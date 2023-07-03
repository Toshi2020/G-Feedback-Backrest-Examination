/*****************************************************************************
*
*	R3eGetG.cpp -- RRREより車体加速度をシリアル出力する
*
*	Shared Memory API↓の情報を利用
*	https://forum.kw-studios.com/index.php?threads/shared-memory-api.1525/
*	こちら↓からr3e.hをローカルにコピーしておく
*	https://github.com/sector3studios/r3e-api
*
*	rev1.0	2023/05/06	initial revision by	Toshi
*	rev1.0a	2023/05/11	Sim03_RRRE.cppに改修
*
*****************************************************************************/

#include "r3e.h"

// プロトタイプ宣言
int map_init();
void map_close();
BOOL map_exists();
HANDLE map_open();

/*----------------------------------------------------------------------------
	シミュレータからのデータを読み取ってシリアル送信
----------------------------------------------------------------------------*/
BOOL R3eSimDataReadAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT count = 0;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	r3e_ori_f32 angle;
	r3e_vec3_f32 accel;
	BOOL fret = FALSE;

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
		// 前回処理してからのインターバル
		itvl = (FLOAT)(clock() - timez) / CLOCKS_PER_SEC;
		if (itvl < OUTINTERVAL)	// 時間が来るまでは
		{
			continue;			// ループ
		}

		// データを読み取る
		angle = Map_buffer->car_orientation;
		accel = Map_buffer->local_acceleration;

		Pitch = angle.pitch;	// 頭を上で+[rad]
		Roll = -angle.roll;		// 右を上で+[rad]
		Ax = -accel.x / 9.8;	// 運動横G(右が+)[G]
		Ay = accel.y / 9.8;		// 運動上下G(上が+)[G]
		Az = -accel.z / 9.8;	// 運動前後G(前が+)[G]
		//Ax = Ay = Az = 0;		// デバッグ用

		// 各軸方向の加速度を計算
		Glon = sin(Pitch);	// 抗重力前後G(頭を上で+)[G]
		Glat = sin(Roll);	// 抗重力横G(右を上で+)[G]
		//Glon = Glat = 0;	// デバッグ用

		Alon = Az + Glon;	// トータル前後G(前が+)[G]
		Alat = Ax + Glat;	// トータル横G(右が+)[G]
		Avert = Ay;			// 上下G(抗重力成分なし。上が+)[G])

		// データーが変化しない回数をカウント
		AddOnTime(PitchZ == Pitch &&
				  RollZ == Roll &&
				  AxZ == Ax &&
				  AyZ == Ay &&
				  AzZ == Az,
				  &sameval);

		// 現在値をメモリ―
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
//			printf("%f,%f,%f,%f,%f\n",Pitch, Roll, Az, Ax, Ay);

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
	map_close();	// マップ終了

	return fret;
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT R3eConnectSim()
{
	printf("%sとの接続を待機中...", ProcessList[ProcessID][0]);
	while (1)
	{
		// mapを取得
		if (map_exists())
		{
			if (map_init())
			{
				printf("mapを取得できませんでした。\n");
				return ERR;
			}
			printf("接続しました。\n");
			return OK;
		}
		else
		{
			// キーが押されるか時間が経過するまでしばらく待つ
			if (WaitUntilKey(CHECKINTERVALMS))
			{
				return QUIT;	// キーによる中断
			}
		}
	}
}
/*----------------------------------------------------------------------------
	メイン処理
----------------------------------------------------------------------------*/
BOOL Sim03_RRRE()
{
	BOOL fret = FALSE;
	SHORT stat;

	stat = R3eConnectSim();
	if (stat == OK)	// Simと接続したら
	{
		fret = R3eSimDataReadAndTx();	// データを受信してシリアル送信
	}
	else if (stat == QUIT)	// キーによる中断なら
	{
		fret = TRUE;		// TRUEを返す
	}
	else	// 接続不可なら
	{
		// プロセスの終了を待つ
		printf("%sが終了するのを待ちます。\n", ProcessList[ProcessID][0]);
		if (WaitProcessEnd())		// キーによる中断なら
		{
			return FALSE;			// 終了メッセージ無しでここで戻る
		}
		printf("%sが終了しました。\n", ProcessList[ProcessID][0]);
	}

	return fret;
}
//----------------------------------------------------------------------------
// 以下は
// https://forum.kw-studios.com/index.php?threads/shared-memory-api.1525/
// より移植
HANDLE map_open()
{
	HANDLE handle = NULL;

	handle = OpenFileMapping(FILE_MAP_READ, FALSE,
							TEXT(R3E_SHARED_MEMORY_NAME));
	return handle;
}

BOOL map_exists()
{
	HANDLE handle = map_open();

	if (handle != NULL)
		CloseHandle(handle);
		
	return handle != NULL;
}

int map_init()
{
	Map_handle = map_open();

	if (Map_handle == NULL)
	{
		wprintf_s(L"Failed to open mapping\n");
		return 1;
	}

	Map_buffer = (r3e_shared*)MapViewOfFile(Map_handle, FILE_MAP_READ,
					0, 0, sizeof(r3e_shared));
	if (Map_buffer == NULL)
	{
		wprintf_s(L"Failed to map buffer\n");
		return 1;
	}

	return 0;
}

void map_close()
{
	if (Map_buffer) UnmapViewOfFile(Map_buffer);
	if (Map_handle) CloseHandle(Map_handle);
}
/*** end of "Sim03_RRRE.cpp" ***/
