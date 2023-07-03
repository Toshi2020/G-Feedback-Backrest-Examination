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
#include <TlHelp32.h>
#include <tchar.h>
#include "r3e.h"

#define	ERR	(-1)
#define	OK 0

// データー出力インターバル[s]
#define	OUTINTERVAL 0.1
// プロセスの生存チェック間隔[s]
#define	CHECKINTERVAL (SHORT)(10.0 /OUTINTERVAL)
// 同じデータが連続した時のフェードアウトまでの時間[s]
#define	FADEOUTTIME (SHORT)(3.0 /OUTINTERVAL)

HANDLE Map_handle = INVALID_HANDLE_VALUE;
r3e_shared* Map_buffer = NULL;
HANDLE hCom = NULL;
DOUBLE Pitch, Roll;
DOUBLE Ax, Ay, Az;			// 運動による加速度
DOUBLE Glon, Glat;			// 重力による加速度 前後G、横G
DOUBLE Alon, Alat, Avert;	// トータル加速度   前後G、横G、上下G、
DOUBLE PitchZ, RollZ;
DOUBLE AxZ, AyZ, AzZ;
CHAR tmpc;
const CHAR IniFile[] = "COMx.txt";			// ポート番号を記述したファイル名
const CHAR ProcessName[] = "RRRE64.exe";	// 監視するプロセス名

// プロトタイプ宣言
SHORT WaitSecUntilKey(SHORT sec);
void AddOnTime(BOOL flag, SHORT* ontime);
BOOL is_process_running(const TCHAR* name);
int map_init();
void map_close();
BOOL map_exists();
HANDLE map_open();
BOOL is_process_running(const TCHAR* name);

/*----------------------------------------------------------------------------
	シミュレータからのデータを読み取ってシリアル送信
----------------------------------------------------------------------------*/
void SimDataReadAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT count = 0;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;
	r3e_ori_f32 angle;
	r3e_vec3_f32 accel;

	timez = clock();
	while (1)
	{
		Sleep(1);		// 一瞬OSに制御を渡す
		if (_kbhit())	// 何かキーが押されたら
		{
			tmpc = _getch();	// キーをダミーリード
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
			if (!is_process_running(CA2CT(ProcessName)))
			{
				break;			// ループを抜ける
			}
		}
	}
	map_close();	// マップ終了
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT ConnectSim()
{
	while (1)
	{
		// シミュレーターと接続してmapを取得
		if (is_process_running(CA2CT(ProcessName)) && map_exists())
		{
			if (map_init())
			{
				printf("mapを取得できません\n");
				return ERR;
			}
			printf("■%sに接続しました■\n", ProcessName);
			return OK;
		}
		else
		{
			printf("%sの起動を待機中...\n", ProcessName);
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
		SimDataReadAndTx();	// データを受信してシリアル送信
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

//----------------------------------------------------------------------------
// 以下は
// https://forum.kw-studios.com/index.php?threads/shared-memory-api.1525/
// より移植
/*----------------------------------------------------------------------------
	プロセスの起動チェック
----------------------------------------------------------------------------*/
BOOL is_process_running(const TCHAR* name)
{
	BOOL result = FALSE;
    HANDLE snapshot = NULL;
    PROCESSENTRY32 entry;

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(PROCESSENTRY32);

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		if (Process32First(snapshot, &entry))
		{
			do
			{
				if (_tcscmp(entry.szExeFile, name) == 0)
				{
					result = TRUE;
					break;
				}
			} while (Process32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);
	}

    return result;
}
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
/*** end of "R3eGetG.cpp" ***/
