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
*
*****************************************************************************/

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <conio.h>
#include <math.h>
#include <atlstr.h>
#include <time.h>
#include "SimConnect.h"

#pragma comment (lib, "SimConnect.lib")

#define	ERR	(-1)
#define	OK 0

// データー出力インターバル[s]
#define	OUTINTERVAL 0.1
// 同じデータが連続した時のフェードアウトまでの時間[s]
#define	FADEOUTTIME (SHORT)(3.0 /OUTINTERVAL)

SHORT quit = 0;
HANDLE hSimConnect = NULL;
HANDLE hCom = NULL;
DOUBLE Pitch, Bank;
DOUBLE Ax, Ay, Az;			// 運動による加速度
DOUBLE Gforce;				// 運動による上下G
DOUBLE Glon, Glat;			// 重力による加速度 前後G、横G
DOUBLE Alon, Alat, Avert;	// トータル加速度   前後G、横G、上下G
DOUBLE PitchZ, BankZ;
DOUBLE AxZ, AyZ, AzZ, GforceZ;
CHAR tmpc;
const CHAR IniFile[] = "COMx.txt";		// ポート番号を記述したファイル名

struct Struct1
{
	double pitch;
	double bank;
	double ax;
//	double ay;
	double az;
	double gforce;
};
SHORT SameCount;

static enum EVENT_ID{
	EVENT_SIM_START,
};

static enum DATA_DEFINE_ID {
	DEFINITION_1,
};

static enum DATA_REQUEST_ID {
	REQUEST_1,
};

// プロトタイプ宣言
SHORT WaitSecUntilKey(SHORT sec);
void AddOnTime(BOOL flag, SHORT* ontime);

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
//					Ay = -(pS->ay) * 0.3048 / 9.8;	// 運動上下G(上が+)[G]
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
			printf("Received:%d\n",pData->dwID);
			break;
	}
}
/*----------------------------------------------------------------------------
	シミュレーターと通信
----------------------------------------------------------------------------*/
void SimDataRequest()
{
	HRESULT hr;
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;

	// Set up the data definition, but do not yet do anything with it
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"PLANE PITCH DEGREES", "Radians");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"PLANE BANK DEGREES", "Radians");
	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
			"ACCELERATION BODY X", "Feet per second squared");
//	hr = SimConnect_AddToDataDefinition(hSimConnect, DEFINITION_1,
//			"ACCELERATION BODY Y", "Feet per second squared");
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
//					  AyZ == Ay &&
					  AzZ == Az &&
					  GforceZ == Gforce,
					  &sameval);

			// 現在値をメモリ―
			PitchZ = Pitch;
			BankZ = Bank;
			AxZ = Ax;
//			AyZ = Ay;
			AzZ = Az;
			GforceZ = Gforce;
			if (sameval < FADEOUTTIME)		// 値が固着していないなら
			{
				// 前後G、横G、上下G
				printf("%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);

				// デバッグ用
//				printf("%f,%f,%f,%f,%f,%f,%f\n",
//						Heading, Pitch, Bank, Az, Ax, Ay, Gforce);

				// シリアルポートへ送信
				sprintf_s(txbuff, "%f,%f,%f,%f\n", itvl, Alon, Alat, Avert);
				WriteFile(hCom, txbuff, (DWORD)strlen(txbuff), &sended, NULL);
			}
			timez = clock();	// 今の時刻をメモリー
		}
	}
	// シミュレータとの接続終了
	hr = SimConnect_Close(hSimConnect);
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT ConnectSim()
{
	while (1)
	{
		// シミュレーターと接続してハンドルを取得
		if (SUCCEEDED(SimConnect_Open(&hSimConnect, "Request Data",
			NULL, 0, 0, 0)))
		{
			printf("■Flight Simulatorに接続しました■\n");
			return OK;
		}
		else
		{
			printf("Flight Simulatorとの接続を待機中...\n");
			if (WaitSecUntilKey(5) == ERR)		// 5秒待つ
			{
				return ERR;
			}
		}
	}
}
/*----------------------------------------------------------------------------
	シリアルポートのオープン
----------------------------------------------------------------------------*/
SHORT OpenComPort()
{
	FILE *fp;
	errno_t err;
	CHAR com[16];

	// ポート名を記述したファイルを開く
	// 開発環境ではソースと同じ場所、実行環境ではexeと同じ場所に置く
	err = fopen_s(&fp, IniFile, "r");
	if (err != 0)
	{
		printf("%sファイルがありません\n", IniFile);
		return ERR;
	}
	// ポート名を得る
	if (fgets(com, 16, fp) == NULL)
	{
		printf("%sファイルにCOMn(nは1～9)と記述してください\n", IniFile);
		fclose(fp);
		return ERR;
	}
	fclose(fp);
	// 末尾に改行があれば除去
	if (com[strlen(com) - 1] == '\n') com[strlen(com) - 1] = '\0';

	 // シリアルポートを開き、ハンドルを取得
	 // (CA2CTマクロのためにatlstr.hのインクルードが必要)
	hCom = CreateFile(CA2CT(com), GENERIC_READ | GENERIC_WRITE,
					0, NULL, OPEN_EXISTING, 0, NULL);
	if (hCom == INVALID_HANDLE_VALUE)
	{
		printf("%sポートがオープンできません。\n", com);
		return ERR;
	}

	DCB dcb;						// シリアルポートの構成情報が入る構造体
	GetCommState(hCom, &dcb);		// 現在の設定値を読み込み
	dcb.BaudRate = 115200;			// ビットレート
	dcb.ByteSize = 8;				// データ長
	dcb.Parity = NOPARITY;			// パリティ
	dcb.StopBits = ONESTOPBIT;		// ストップビット長
	dcb.fOutxCtsFlow = FALSE;		// 送信時CTSフロー
	dcb.fRtsControl = RTS_CONTROL_ENABLE; // RTSフロー
	SetCommState(hCom, &dcb);		// 変更した設定値を書き込み
	return OK;
}
/*----------------------------------------------------------------------------
	メイン処理
----------------------------------------------------------------------------*/
int main()
{
	if (OpenComPort() == ERR)	// シリアルポートを開く
	{
		WaitSecUntilKey(5);
		exit(1);
	}
	if (ConnectSim() == OK)	// Simと接続したら
	{
		SimDataRequest();	// データを受信してシリアル送信
	}
	CloseHandle(hCom);		// シリアルポートを閉じる

	return 0;
}
/*----------------------------------------------------------------------------
	何かキーが押されるまで待つ
----------------------------------------------------------------------------*/
SHORT WaitSecUntilKey(SHORT sec)
{
	for (SHORT i = 0; i < sec * 10; i++)
	{
		if (_kbhit())
		{
			tmpc = _getch();
			return ERR;
		}
		Sleep(100);
	}
	return OK;
}
/*----------------------------------------------------------------------------
	フラグのオン時間の累積
	書式 void AddOnTime(BOOL flag, SHORT* ontime)

	BOOL flag;		フラグ
	SHORT* ontime;	オン時間
----------------------------------------------------------------------------*/
#define	TIMEMAX 30000
#define	TIMEMAXC 255
void AddOnTime(BOOL flag, SHORT* ontime)
{
	if (flag)						// オンしてるなら
	{
		if (*ontime < TIMEMAX)
		{
			(*ontime)++;			// オン時間＋＋
		}
	}
	else
	{
		*ontime = 0;
	}
}
/*** end of "FsimGetG.cpp" ***/
