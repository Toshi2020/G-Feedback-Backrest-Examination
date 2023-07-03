/*****************************************************************************
*
*	SimGetG.cpp -- シミュレーターから加速度を得てシリアル出力する
*
*	rev1.0	2023/05/10	initial revision by	Toshi
*
*****************************************************************************/
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS	// for inet_addr()
#define _CRT_SECURE_NO_WARNINGS

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <strsafe.h>
#include <conio.h>
#include <math.h>
#include <atlstr.h>
#include <time.h>
#include <tlhelp32.h>
#include <tchar.h>
#include "r3e.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#define	ERR	(-1)
#define	OK 0
#define QUIT 1
#define PI 3.14159

// データー出力インターバル[s]
#define	OUTINTERVAL 0.1
// プロセスの生存チェック間隔[s]
#define	CHECKINTERVAL (SHORT)(5.0 / OUTINTERVAL)
#define	CHECKINTERVALMS (SHORT)(5.0 * 1000.0)
// 同じデータが連続した時のフェードアウトまでの時間[s]
#define	FADEOUTTIME (SHORT)(3.0 /OUTINTERVAL)
#define BUF_SIZE 1024	// UDPパケットバッファサイズ

HANDLE Map_handle = INVALID_HANDLE_VALUE;
r3e_shared* Map_buffer = NULL;
HANDLE hCom = NULL;
SOCKET Sock;
DOUBLE Heading, Pitch, Roll, Bank;
DOUBLE Ax, Ay, Az;			// 運動による加速度
DOUBLE Gforce;				// 運動による上下G
DOUBLE Glon, Glat;			// 重力による加速度 前後G、横G
DOUBLE Alon, Alat, Avert;	// トータル加速度	前後G、横G、上下G、
DOUBLE HeadingZ, PitchZ, RollZ, BankZ;
DOUBLE AxZ, AyZ, AzZ, GforceZ;
CHAR tmpc;
const CHAR IniFile[] = "COMx.txt";			// ポート番号を記述したファイル名
const CHAR* ProcessName = "AssettoCorsa.exe";	// 監視するプロセス名

// 監視するプロセスのタイトルとexe名
#define PROCESSMAX 5
const CHAR* ProcessList[PROCESSMAX][2] = {
			"Microsoft Flight Simulator X", "fsx.exe",
			"Flight Gear", "fgfs.exe",
			"Live for Speed", "LFS.exe",
			"RaceRoom Racing Experience", "RRRE64.exe",
			"Assetto Corsa", "AssettoCorsa.exe"
};
SHORT ProcessID = -1;	// 起動中のプロセス番号0～PROCESSMAX-1

// プロトタイプ宣言
BOOL WaitUntilKey(SHORT msec);
void AddOnTime(BOOL flag, SHORT* ontime);
BOOL is_process_running(const TCHAR* name);
SHORT initPhysics();
void MapDismiss();
BOOL IsProcessRunning(const CHAR* name);
SHORT SearchProcess();
BOOL WaitProcessEnd();

#include "Sim00_FlightSimulatorX.cpp"
#include "Sim01_FlightGear.cpp"
#include "Sim02_LiveForSpeed.cpp"
#include "Sim03_R3E.cpp"
#include "Sim04_AssettoCorsa.cpp"

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
	BOOL fquit = FALSE;
	BOOL fstart = TRUE;

	if (OpenComPort() == ERR)		// シリアルポートを開く
	{
		WaitUntilKey(5000);
		exit(1);
	}
	while (1)
	{
		if (fstart)	// 初回？
		{
			printf("\nプロセスの起動待ち(何かキーを押すと終了します)...");
			fstart = FALSE;
		}
		ProcessID = SearchProcess();	// プロセスのサーチ
		if (ProcessID >= 0)
		{
			printf("%sが起動しました。\n", ProcessList[ProcessID][0]);
			// 各simごとの処理。終わるまで戻ってこない
			// キーが押されて終了した時はTRUE、それ以外はFALSEを返す
			switch (ProcessID)
			{
				case 0:	// Microsoft Flight Simulator X
						fquit = Sim00_FlightSimulatorX();
						break;
				case 1:	// Flight Gear
						fquit = Sim01_FlightGear();
						break;
				case 2:	// Live for Speed
						fquit = Sim02_LiveForSpeed();
						break;
				case 3:	// RaceRoom Racing Experience
						fquit = Sim03_RRRE();
						break;
				case 4:	// Assetto Corsa
						fquit = Sim04_AssettoCorsa();
						break;
				default:
						break;
			}
			if (fquit)	// キーによる終了？
			{
				break;
			}
			else
			{
				fstart = TRUE;	// 最初に戻る
			}
		}
		else if (WaitUntilKey(3000))
		{
			break;
		}
	}
	CloseHandle(hCom);		// シリアルポートを閉じる

	return 0;
}
/*----------------------------------------------------------------------------
	何かキーが押されるまで指定された時間待つ。キーが押されたらTRUEで戻る
----------------------------------------------------------------------------*/
BOOL WaitUntilKey(SHORT msec)
{
	clock_t timez = clock();

	while ((SHORT)((clock() - timez) / CLOCKS_PER_SEC * 1000) < msec)
	{
		if (_kbhit())
		{
			tmpc = _getch();
			return TRUE;
		}
		Sleep(1);
	}
	return FALSE;
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

/*----------------------------------------------------------------------------
	プロセスの起動チェック
----------------------------------------------------------------------------*/
BOOL IsProcessRunning(const CHAR* name)
{
	BOOL fret = FALSE;
	HANDLE hsnapshot = NULL;
	PROCESSENTRY32 pe32;

	// 全てのプロセスのスナップショットを得る
	hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hsnapshot != INVALID_HANDLE_VALUE)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);	// 使う前にサイズをセット
		if (Process32First(hsnapshot, &pe32))	// プロセスが存在するなら
		{
			do
			{	// 同じ名前を発見した？
				if (_tcscmp(pe32.szExeFile, CA2CT(name)) == 0)
				{
					fret = TRUE;
					break;			// ここまでとする
				}
			} while (Process32Next(hsnapshot, &pe32));
		}
		CloseHandle(hsnapshot);
	}
	return fret;
}
/*----------------------------------------------------------------------------
	プロセスの終了を待つ
----------------------------------------------------------------------------*/
BOOL WaitProcessEnd()
{
	BOOL fret = FALSE;

	// プロセスの終了を待つ
	while (IsProcessRunning(ProcessList[ProcessID][1]))
	{
		if (WaitUntilKey(CHECKINTERVALMS))	// キーによる中断？
		{
			fret = TRUE;					// ここで戻る
			break;
		}
	}
	return fret;
}
/*----------------------------------------------------------------------------
	プロセスのサーチ
----------------------------------------------------------------------------*/
SHORT SearchProcess()
{
	HANDLE hsnapshot = NULL;
	PROCESSENTRY32 pe32;

	// 全てのプロセスのスナップショットを得る
	hsnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hsnapshot != INVALID_HANDLE_VALUE)
	{
		pe32.dwSize = sizeof(PROCESSENTRY32);	// 使う前にサイズをセット
		if (Process32First(hsnapshot, &pe32))	// プロセスが存在するなら
		{
			do
			{	// 探すべきプロセス
				for (SHORT i = 0; i < PROCESSMAX; i++)
				{
					// 同じ名前を発見した？
					if (_tcscmp(pe32.szExeFile, CA2CT(ProcessList[i][1]))== 0)
					{
						CloseHandle(hsnapshot);
						return i;			// ここまでとする
					}
				}
			} while (Process32Next(hsnapshot, &pe32));
		}
		CloseHandle(hsnapshot);
	}
	return ERR;
}
/*** end of "SimGetG.cpp" ***/
