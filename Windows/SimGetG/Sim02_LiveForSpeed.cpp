/*****************************************************************************
*
*	LfsGetG.cpp -- Live for Speedより車体加速度をシリアル出力する
*
*	・LFS/docフォルダのInSim.txtを参考に
*	・LFSフォルダのcfg.txtで以下のように変更
*	OutSim Mode 1
*	OutSim Delay 1
*	OutSim IP 127.0.0.1
*	OutSim Port 4123
*	OutSim ID 1
*	OutSim Opts 0
*
*	rev1.0	2023/05/01	initial revision by	Toshi
*	rev1.0a	2023/05/10	Sim02_LiveForSpeed.cppに改修
*
*****************************************************************************/

#define LFSSERVER_ADDR "127.0.0.1"	// cfg.txtと同じにする
#define LFSSERVER_PORT 4123			// SimToolsがこのポートだったので揃えた

typedef struct
{
	int x;
	int y;
	int z;
} Vec;

typedef struct
{
	float x;
	float y;
	float z;
} Vector;

struct OutSimPack	// InSim.txtよりコピー
{
	unsigned	Time;		// time in milliseconds (to check order)
	Vector		AngVel;		// 3 floats, angular velocity vector
	float		Heading;	// anticlockwise from above (Z)
	float		Pitch;		// anticlockwise from right (X)
	float		Roll;		// anticlockwise from front (Y)
	Vector		Accel;		// 3 floats X, Y, Z
	Vector		Vel;		// 3 floats X, Y, Z
	Vec			Pos;		// 3 ints	X, Y, Z (1m = 65536)
	int			ID;			// optional - only if OutSim ID is specified
};

/*----------------------------------------------------------------------------
	シミュレータからのデータを受信してシリアル送信
----------------------------------------------------------------------------*/
BOOL LfsSimDataReceiveAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT recv_size;
	CHAR recv_buf[BUF_SIZE];
	OutSimPack pack;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	static DOUBLE ax, ay, az;
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
		if (recv_size == 0)	// 受信していないなら-1が返る
		{
			// 受信サイズが0の場合は相手が接続を閉じていると判断
			printf("接続が閉じられました\n");
			break;
		}
		else if (recv_size == sizeof(pack))	// 受信した？
		{
			// データを受け取る
			memcpy(&pack, recv_buf, sizeof(pack));
			Heading = pack.Heading;		// 左旋回で+[rad]
			Pitch = pack.Pitch;			// 頭を上で+[rad]
			Roll = -pack.Roll;			// 右を上で+[rad]
			ax = pack.Accel.x / 9.8;	// 運動横G(右が+)[G] (Gはワールド座標)
			ay = pack.Accel.y / 9.8;	// 運動前後G(前が+)[G]
			az = pack.Accel.z / 9.8;	// 運動上下G(上が+)[G]
			// ローカル座標に変換
			Ax = ax * cos(Heading) + ay * sin(Heading);
			Ay = ay * cos(Heading) - ax * sin(Heading);
			Az = az;
			//Ax = Ay = Az = 0;	// デバッグ用

			// 各軸方向の加速度を計算
			Glon = sin(Pitch);	// 抗重力前後G(頭を上で+)[G]
			Glat = sin(Roll);	// 抗重力横G(右を上で+)[G]
			//Glon = Glat = 0;	// デバッグ用

			Alon = Ay + Glon;	// トータル前後G(前が+)[G]
			Alat = Ax + Glat;	// トータル横G(右が+)[G]
			Avert = Az;			// 上下G(抗重力成分なし。上が+)[G])

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
//				printf("%f,%f,%f,%f,%f,%f\n",Heading,Pitch, Roll, ax, ay, az);
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
SHORT LfsConnectSim()
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
	addr.sin_port = htons((WORD)LFSSERVER_PORT);
	addr.sin_addr.s_addr = inet_addr(LFSSERVER_ADDR);

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
BOOL Sim02_LiveForSpeed()
{
	BOOL fret = FALSE;

	if (LfsConnectSim() == OK)	// Simと接続したら
	{
		fret = LfsSimDataReceiveAndTx();	// データを受信してシリアル送信
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
/*** end of "Sim02_LiveForSpeed.cpp" ***/
