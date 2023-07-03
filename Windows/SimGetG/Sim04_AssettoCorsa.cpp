/*****************************************************************************
*
*	AcGetG.cpp -- Assetto Corsaより車体加速度をシリアル出力する
*
*	Shared Memory Reference ACC↓の情報を利用
*	https://assettocorsamods.net/threads/doc-shared-memory-reference-acc.3061/
*
*	rev1.0	2023/05/06	initial revision by	Toshi
*	rev1.0a	2023/05/11	Sim0405_AssettoCorsa.cppに改修
*
*****************************************************************************/

//----------------------------------------------------------------------------
// 以下は
// https://assettocorsamods.net/threads/doc-shared-memory-reference-acc.3061/
// より移植
struct SPageFilePhysics
{
	int packetId = 0;
	float gas = 0;
	float brake = 0;
	float fuel = 0;
	int gear = 0;
	int rpms = 0;
	float steerAngle = 0;
	float speedKmh = 0;
	float velocity[3];
	float accG[3];
	float wheelSlip[4];
	float wheelLoad[4];
	float wheelsPressure[4];
	float wheelAngularSpeed[4];
	float tyreWear[4];
	float tyreDirtyLevel[4];
	float tyreCoreTemperature[4];
	float camberRAD[4];
	float suspensionTravel[4];
	float drs = 0;
	float tc = 0;
	float heading = 0;
	float pitch = 0;
	float roll = 0;
};
struct SMElement
{
	HANDLE hMapFile;
	unsigned char* mapFileBuffer;
} SMphysics;

// ここまで
//----------------------------------------------------------------------------

// プロトタイプ宣言
SHORT initPhysics();
void MapDismiss();

/*----------------------------------------------------------------------------
	シミュレータからのデータを読み取ってシリアル送信
----------------------------------------------------------------------------*/
BOOL AcSimDataReadAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT count = 0;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	static DOUBLE pitch, roll, ax, ay, az;
	SPageFilePhysics* pf = (SPageFilePhysics*)SMphysics.mapFileBuffer;
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
		Heading = pf->heading;// 方位[rad]
		Pitch = pf->pitch;	// 頭を上で+[rad]
		Roll = pf->roll;	// 右を上で+[rad]
		Ax = -pf->accG[0];	// 運動横G(右が+)[G]
		Ay = pf->accG[1];	// 運動上下G(上が+)[G]
		Az = pf->accG[2];	// 運動前後G(前が+)[G]
		//Ax = Ay = Az = 0;		// デバッグ用

		// 各軸方向の加速度を計算
		Glon = sin(Pitch);	// 抗重力前後G(頭を上で+)[G]
		Glat = sin(Roll);	// 抗重力横G(右を上で+)[G]
		//Glon = Glat = 0;	// デバッグ用

		Alon = Az + Glon;	// トータル前後G(前が+)[G]
		Alat = Ax + Glat;	// トータル横G(右が+)[G]
		Avert = Ay;			// 上下G(抗重力成分なし。上が+)[G])

		// データーが変化しない回数をカウント
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
//			printf("%f,%f,%f,%f,%f\n",Glon, Glat, Ax, Ay, Az);
//			printf("%f,%f,%f,%f,%f,%f\n", Heading, Pitch, Roll, Ax, Ay, Az);

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
	MapDismiss();	// マップ終了

	return fret;
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT AcConnectSim()
{
	printf("%sとの接続を待機中...", ProcessList[ProcessID][0]);

	// mapを取得
	if (initPhysics() == ERR)
	{
		printf("mapを取得できません\n");
		return ERR;
	}
	printf("接続しました。\n");
	return OK;
}
/*----------------------------------------------------------------------------
	メイン処理
----------------------------------------------------------------------------*/
BOOL Sim04_AssettoCorsa()
{
	BOOL fret = FALSE;

	if (AcConnectSim() == OK)	// Simと接続したら
	{
		fret = AcSimDataReadAndTx();	// データを受信してシリアル送信
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
//----------------------------------------------------------------------------
// 以下は
// https://assettocorsamods.net/threads/doc-shared-memory-reference-acc.3061/
// より移植

#define AC_SHARED_MEMORY_NAME "Local\\acpmf_physics"
SHORT initPhysics()
{
	TCHAR szName[] = TEXT("Local\\acpmf_physics");

	SMphysics.hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
											PAGE_READWRITE, 0,
											sizeof(SPageFilePhysics), szName);
	if (!SMphysics.hMapFile)
	{
		printf("CreateFileMappingができませんでした\n");
		return ERR;
	}
	SMphysics.mapFileBuffer = (unsigned char*)MapViewOfFile(
								SMphysics.hMapFile, FILE_MAP_READ, 0, 0,
								sizeof(SPageFilePhysics));
	if (!SMphysics.mapFileBuffer)
	{
		printf("MapViewOfFileができませんでした\n");
		return ERR;
	}
	return OK;
}
void MapDismiss()
{
	UnmapViewOfFile(SMphysics.mapFileBuffer);
	CloseHandle(SMphysics.hMapFile);
}
// ここまで
//----------------------------------------------------------------------------
/*** end of "Sim0405_AssettoCorsa.cpp" ***/
