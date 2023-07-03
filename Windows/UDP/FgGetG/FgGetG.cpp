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
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment (lib, "ws2_32.lib")
#pragma comment (lib, "mswsock.lib")
#pragma comment (lib, "advapi32.lib")

#define	ERR	(-1)
#define	OK 0
#define PI 3.14159

// データー出力インターバル[s]
#define	OUTINTERVAL 0.1
// 同じデータが連続した時のフェードアウトまでの時間[s]
#define	FADEOUTTIME (SHORT)(3.0 /OUTINTERVAL)

#define SERVER_ADDR "127.0.0.1"	// cfg.txtと同じにする
#define SERVER_PORT 16661

SOCKET Sock;
HANDLE hCom = NULL;
DOUBLE Heading, Pitch, Roll;
DOUBLE Ax, Ay, Az;			// 運動による加速度
DOUBLE Glon, Glat;			// 重力による加速度 前後G、横G
DOUBLE Alon, Alat, Avert;	// トータル加速度	前後G、横G、上下G、
DOUBLE AngVx, AngVy, AngVz;
DOUBLE Vx, Vy, Vz;
DOUBLE HeadingZ, PitchZ, RollZ;
DOUBLE AxZ, AyZ, AzZ;
CHAR tmpc;
const CHAR IniFile[] = "COMx.txt";		// ポート番号を記述したファイル名

struct FgSimPack	// Property browserからそれと思われる値
{
	FLOAT	Heading;
	FLOAT	Pitch;
	FLOAT	Roll;
	FLOAT	AccelX;
	FLOAT	AccelY;
	FLOAT	AccelZ;
};

// プロトタイプ宣言
SHORT WaitSecUntilKey(SHORT sec);
void AddOnTime(BOOL flag, SHORT* ontime);

/*----------------------------------------------------------------------------
	シミュレータからのデータを受信してシリアル送信
----------------------------------------------------------------------------*/
#define BUF_SIZE 1024
void SimDataReceiveAndTx()
{
	CHAR txbuff[128];
	DWORD sended; // 実際に送信したバイト数
	SHORT recv_size;
	CHAR recv_buf[BUF_SIZE];
	FgSimPack pack;
	FLOAT itvl;
	clock_t timez;
	static SHORT sameval = FADEOUTTIME;

	timez = clock();
	while (1)
	{
		Sleep(1);		// 一瞬OSに制御を渡す
		if (_kbhit())	// 何かキーが押されたら
		{
			tmpc = _getch();	// キーをダミーリード
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
		}
	}
	// ソケット終了処理
	closesocket(Sock);
	WSACleanup();
}
/*----------------------------------------------------------------------------
	シミュレーターと接続
----------------------------------------------------------------------------*/
SHORT ConnectSim()
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

	printf("■FlightGearとの通信準備完了しました■\n");
	return OK;
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
		SimDataReceiveAndTx();	// データを受信してシリアル送信
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
/*** end of "FgGetG.cpp" ***/
