/*****************************************************************************
*
*	FgGetG.cpp -- FlightGearより機体加速度をシリアル出力する
*
*	・FlightGearをインストールしたフォルダの下の\data\Protocolフォルダに
*	　FgGetG.xmlをコピーしておく。このファイルにUDPで送って欲しい変数を
*	　記述しておく。
*	　変数はFlightGearの画面から'/'キーを押すと出てくる画面から選択した
*	・FlightGearのコマンドラインオプションに以下を追加して起動
*		--generic=socket,out,100,127.0.0.1,16661,udp,FgGetG
*
*	rev1.0	2023/05/07	initial revision by	Toshi
*	rev1.0a	2023/05/10	Sim01_FlightGear.cppに改修
*
*****************************************************************************/

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "mswsock.lib")
#pragma comment (lib, "advapi32.lib")

#define SERVER_ADDR "127.0.0.1"	// cfg.txtと同じにする
#define SERVER_PORT 16661

struct FgSimPack	// Property browserからそれと思われる値
{
	FLOAT	Heading;
	FLOAT	Pitch;
	FLOAT	Roll;
	FLOAT	AccelX;
	FLOAT	AccelY;
	FLOAT	AccelZ;
};

/*----------------------------------------------------------------------------
	シミュレータからのデータを受信してシリアル送信
----------------------------------------------------------------------------*/
BOOL FgSimDataReceiveAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT recv_size;
	CHAR recv_buf[BUF_SIZE];
	FgSimPack pack;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	BOOL fret = FALSE;
	SHORT count = 0;

	timez = clock();
	while (1)
	{
		Sleep(1);		// 一瞬OSに制御を渡す
		if (_kbhit())	// 何かキーが押されたら
		{
			tmpc = _getch();	// キーをダミーリード
			fret = TRUE;
			break;				// ループを抜ける
		}

		// サーバーからのUDPパケットを受信
		ZeroMemory(&recv_buf, sizeof(pack));
		recv_size = recv(Sock, recv_buf, sizeof(pack), 0);
		if (recv_size == 0)
		{
			// 受信サイズが0の場合は相手が接続を閉じていると判断
			printf("接続が閉じられました\n");
			break;
		}
		else if (recv_size == sizeof(pack))	// 受信した？
		{
			// データを受け取る
			memcpy(&pack, recv_buf, sizeof(pack));
			Heading = pack.Heading * PI / 180.0;	// 左旋回で+[rad]
			Pitch = pack.Pitch * PI / 180.0;		// 頭を上で+[rad]
			Roll = -pack.Roll * PI / 180.0;			// 右を上で+[rad]

			Ax = pack.AccelX;		// 運動前後G(前が+)[G]
			Ay = pack.AccelY;		// 運動横G(右が+)[G]
			Az = pack.AccelZ;		// 運動上下G(上が+)[G]
			if (Az != 0.0) Az += 1.0;// UFOでないなら重力加速度分を補正
			//Ax = Ay = Az = 0;		// デバッグ用

			// 各軸方向の加速度を計算
			Glon = sin(Pitch);	// 抗重力前後G(頭を上で+)[G]
			Glat = sin(Roll);	// 抗重力横G(右を上で+)[G]
			//Glon = Glat = 0;	// デバッグ用

			Alon = Ax + Glon;	// トータル前後G(前が+)[G]
			Alat = Ay + Glat;	// トータル横G(右が+)[G]
			Avert = -Az;		// 上下G(抗重力成分なし。上が+)[G])

			// たまってしまったパケットを捨てる
			while (recv(Sock, recv_buf, sizeof(pack), 0) == sizeof(pack));
		}
		// 前回処理してからのインターバル
		itvl = (FLOAT)(clock() - timez) / CLOCKS_PER_SEC;
		if (itvl >= OUTINTERVAL)	// 時間が来たら
		{
			// データーが変化しない回数
			AddOnTime(HeadingZ == Heading &&
					  PitchZ == Pitch &&
					  RollZ == Roll &&
					  AxZ == Ax &&
					  AyZ == Ay &&
					  AzZ == Az,
					  &sameval);

			// 現在値をメモリ―
			HeadingZ = Heading;
			PitchZ = Pitch;
			RollZ = Roll;
			AxZ = Ax;
			AyZ = Ay;
			AzZ = Az;
			if (sameval < FADEOUTTIME)		// 値が固着していないなら
			{
				// 前後G、横G、上下G
				printf("%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);

				// デバッグ用
//				printf("%f,%f,%f,%f,%f,%f\n",Heading,Pitch, Roll, Ax, Ay, Az);
//				printf("%f,%f,%f,%f,%f,%f\n",AngVx,AngVy,AngVz,Vx,Vy,Vz);

				// シリアルポートへ送信
				sprintf_s(txbuff, "%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);
				WriteFile(hCom, txbuff, (DWORD)strlen(txbuff), &sended, NULL);
			}
			timez = clock();	// 今の時刻をメモリー

			// たまにプロセスの生存をチェック
			if (++count > CHECKINTERVAL)
			{
				count = 0;
				// プロセスが走っていないなら
				if (!IsProcessRunning(ProcessList[ProcessID][1]))
				{
					printf("%sが終了しました。\n", ProcessList[ProcessID][0]);
					break;			// ループを抜ける
				}
			}
		}
	}
	// ソケット終了処理
	closesocket(Sock);
	WSACleanup();
	return fret;
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT FgConnectSim()
{
	struct sockaddr_in addr;
	WSADATA wsaData;

	// winsock2の初期化
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
	{
		printf("WINSOCK失敗\n");
		WSACleanup();
		return ERR;
	}

	// UDPソケットを作成
	Sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (Sock == INVALID_SOCKET)
	{
		printf("ソケットの作成ができません\n");
		WSACleanup();
		return ERR;
	}

	// 構造体を全て0にセット
	ZeroMemory(&addr, sizeof(addr));

	// サーバーのIPアドレスとポートの情報を設定
	addr.sin_family = AF_INET;
	addr.sin_port = htons((WORD)SERVER_PORT);
	addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

	// バインドする
	if (bind(Sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		printf("ソケットをバインドできません\n");
		WSACleanup();
		return ERR;
	}
	// ノンブロッキングに設定する
	DWORD val = 1;
	ioctlsocket(Sock, FIONBIO, &val);

	printf("%sに接続しました。\n", ProcessList[ProcessID][0]);
	return OK;
}
/*----------------------------------------------------------------------------
	メイン処理
----------------------------------------------------------------------------*/
BOOL Sim01_FlightGear()
{
	BOOL fret = FALSE;

	if (FgConnectSim() == OK)			// Simと接続したら
	{
		fret = FgSimDataReceiveAndTx();	// データを受信してシリアル送信
	}
	else	// 接続不可なら
	{
		// プロセスの終了を待つ
		printf("%sが終了するのを待ちます。\n", ProcessList[ProcessID][0]);
		if (WaitProcessEnd())		// キーによる中断なら
		{
			return TRUE;			// 終了メッセージ無しでここで戻る
		}
		printf("%sが終了しました。\n", ProcessList[ProcessID][0]);
	}

	return fret;
}
/*** end of "Sim01_FlightGear.cpp" ***/
