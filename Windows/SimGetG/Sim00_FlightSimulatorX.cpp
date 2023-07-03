/*****************************************************************************
*
*	FsimGetG.cpp -- FlightSimulatorXより機体加速度をシリアル出力する
*	(SDKのRequest Dataをベースに改修)
*
*	・SDKのパスはこちら↓
*	  C:\Program Files (x86)\Microsoft Games\Microsoft Flight Simulator X SDK
*	・SDKフォルダよりSimConnect.hとSimConnect.libをソースと同じ場所にコピー
*	・ソリューションプラットホームはX86を選択(2020のSDKの場合はX64らしい)
*
*	rev1.0	2023/04/17	initial revision by	Toshi
*	rev1.0a	2023/05/10	Sim00_FlightSimulatorX.cppに改修
*
*****************************************************************************/

#include "SimConnect.h"

#pragma comment (lib, "SimConnect.lib")

SHORT quit = 0;
HANDLE hSimConnect = NULL;

struct Struct1
{
	double pitch;
	double bank;
	double ax;
	double az;
	double gforce;
};

enum EVENT_ID{
	EVENT_SIM_START,
};

enum DATA_DEFINE_ID {
	DEFINITION_1,
};

enum DATA_REQUEST_ID {
	REQUEST_1,
};

/*----------------------------------------------------------------------------
	シミュレーターからのコールバック
----------------------------------------------------------------------------*/
void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData,
				void *pContext)
{
	HRESULT hr;
	
	switch (pData->dwID)
	{
		case SIMCONNECT_RECV_ID_EVENT:
		{
			SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;

			switch (evt->uEventID)
			{
				case EVENT_SIM_START:
					hr = SimConnect_RequestDataOnSimObject(hSimConnect,
						REQUEST_1, DEFINITION_1,
						SIMCONNECT_OBJECT_ID_USER,
						//SIMCONNECT_PERIOD_SIM_FRAME,
						SIMCONNECT_PERIOD_VISUAL_FRAME,
						SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);
					break;

				default:
				   break;
			}
			break;
		}

		// case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
		case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
		{
			SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE *pObjData = 
						(SIMCONNECT_RECV_SIMOBJECT_DATA_BYTYPE*)pData;
			DWORD ObjectID = pObjData->dwObjectID;
			Struct1 *pS = (Struct1*)&pObjData->dwData;

			switch (pObjData->dwRequestID)
			{
				case REQUEST_1:
					// データを受け取る
					Pitch = -pS->pitch;		// 頭を上で+[rad]
					Bank = pS->bank;		// 右を上で+[rad]
					Az = (pS->az) * 0.3048 / 9.8;	// 運動前後G(前が+)[G]
					Ax = -(pS->ax) * 0.3048 / 9.8;	// 運動横G(右が+)[G]
					// 運動上下G(上が+)[G] →Gメーターの値から重力成分を引く
					Gforce = pS->gforce - cos(Pitch) * cos(Bank);
					//Ax = Ay = Az = Gforce = 0;	// デバッグ用

					// 各軸方向の加速度を計算
					Glon = sin(Pitch);	// 抗重力前後G(頭を上で+)[G]
					Glat = sin(Bank);	// 抗重力横G(右を上で+)[G]
					//Glon = Glat = 0;	// デバッグ用

					Alon = Az + Glon;	// トータル前後G(前が+)[G]
					Alat = Ax + Glat;	// トータル横G(右が+)[G]
					Avert = Gforce;		// 上下G(抗重力成分なし。上が+)[G])
					break;

				default:
				   break;
			}
			break;
		}

		case SIMCONNECT_RECV_ID_QUIT:
			quit = 1;
			break;

		default:
			break;
	}
}
/*----------------------------------------------------------------------------
	シミュレーターと通信
----------------------------------------------------------------------------*/
BOOL FsxSimDataRequest()
{
	HRESULT hr;
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	BOOL fret = FALSE;

	// Set up the data definition, but do not yet do anything with it
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"PLANE PITCH DEGREES", "Radians");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"PLANE BANK DEGREES", "Radians");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"ACCELERATION BODY X", "Feet per second squared");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"ACCELERATION BODY Z", "Feet per second squared");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"G FORCE", "GForce");

	// Request an event when the simulation starts
	hr = SimConnect_SubscribeToSystemEvent(hSimConnect, EVENT_SIM_START,
			"SimStart");

	timez = clock();
	while (quit == 0)
	{
		// データをアップデート
		SimConnect_CallDispatch(hSimConnect, MyDispatchProcRD, NULL);

		Sleep(1);		// 一瞬OSに制御を渡す
		if (_kbhit())	// 何かキーが押されたら
		{
			tmpc = _getch();	// キーをダミーリード
			fret = TRUE;
			break;				// ループを抜ける
		}

		// 前回処理してからのインターバル
		itvl = (FLOAT)(clock() - timez) / CLOCKS_PER_SEC;
		if (itvl >= OUTINTERVAL)	// 時間が来たら
		{
			// データーが変化しない回数
			AddOnTime(PitchZ == Pitch &&
					  BankZ == Bank &&
					  AxZ == Ax &&
					  AzZ == Az &&
					  GforceZ == Gforce,
					  &sameval);

			// 現在値をメモリ―
			PitchZ = Pitch;
			BankZ = Bank;
			AxZ = Ax;
			AzZ = Az;
			GforceZ = Gforce;
			if (sameval < FADEOUTTIME)		// 値が固着していないなら
			{
				// 前後G、横G、上下G
				printf("%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);

				// デバッグ用
//				printf("%f,%f,%f,%f,%f\n", Pitch, Bank, Az, Ax, Gforce);

				// シリアルポートへ送信
				sprintf_s(txbuff, "%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);
				WriteFile(hCom, txbuff, (DWORD)strlen(txbuff), &sended, NULL);
			}
			timez = clock();	// 今の時刻をメモリー
		}
	}
	// シミュレータとの接続終了
	hr = SimConnect_Close(hSimConnect);

	// プロセスの終了を待つ
	while (IsProcessRunning(ProcessList[ProcessID][1]))
	{
		if (WaitUntilKey(CHECKINTERVALMS))	// キーによる中断？
		{
			fret = TRUE;					// ここで戻る
			break;
		}
	}
	printf("%sが終了しました。\n", ProcessList[ProcessID][0]);
	return fret;
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT FsxConnectSim()
{
	printf("%sとの接続を待機中...", ProcessList[ProcessID][0]);
	while (1)
	{
		// シミュレーターと接続してハンドルを取得
		if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Request Data",
			NULL, 0, 0, 0)))
		{
			printf("接続しました。\n");
			return OK;
		}
		else
		{
			// キーが押されるか時間が経過するまでしばらく待つ
			if (WaitUntilKey(CHECKINTERVALMS))
			{
				printf("\n");
				return QUIT;	// キーによる中断
			}
		}
	}
}
/*----------------------------------------------------------------------------
	メイン処理
----------------------------------------------------------------------------*/
BOOL Sim00_FlightSimulatorX()
{
	BOOL fret = FALSE;

	if (FsxConnectSim() == OK)		// Simと接続したら
	{
		fret = FsxSimDataRequest();	// データを受信してシリアル送信
	}
	else	// キーによる中断なら
	{
		fret = TRUE;
	}

	return fret;
}
/*** end of "Sim00_FlightSimulatorX.cpp" ***/
