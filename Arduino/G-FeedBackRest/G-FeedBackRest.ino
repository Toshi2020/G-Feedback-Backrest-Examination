/*****************************************************************************
*
*	G-FeedBackRest.ino -- Gフィードバック・バックレスト(背もたれ)
*
*	rev1.0	2023/04/29	initial revision by	Toshi
*	Rev1.1	2023/05/25	テストモードを統合
*
*****************************************************************************/

// TEST種類
// 0:通常動作
// 1:シリアル入力したパルス幅[μs]をサーボに出力する
// 2:シリアル入力したForce FMIN～FMAXをサーボに出力する
// 3:FMIN～FMAXをサーボに出力する
#define TEST 0

#include <stdlib.h>
#include <string.h>
#include <Servo.h>	// サーボライブラリ

/*** 定数とマクロ ***/
#define	TRUE	1
#define	FALSE	0
#define	ERR	(-1)
#define	OK		0
#define	YES		1
#define	NO		0
#define BOOL unsigned char
#define CHAR char
#define UCHAR unsigned char
#define SHORT int
#define USHORT unsigned int
#define LONG long
#define ULONG unsigned long
#define FLOAT float
#define DOUBLE double
#define BOOLSET(x) (BOOL)(((x) != 0) ? TRUE : FALSE)

// Arduino ピン設定
#define SVRH 4		// 右上(右肩) サーボピン(いづれも外部でプルアップが必要)
#define SVLH 5		// 左上(左肩)
#define SVRL 6		// 右下(右尻)
#define SVLL 7		// 左下(左尻)
#define LED 13
#define LED_ON digitalWrite(LED, 1);
#define LED_OFF digitalWrite(LED, 0);
#define SW A0
#define SWLO A2		// スイッチを基板に直付けしたので片側をGNDに落とす代わり

// サーボ関連
#define AMAX 1800	// アクチュエータ全開時の角度 180[deg]相当x10
#define AMIN 900	// アクチュエータ全閉時の角度 90[deg]相当x10
#define FMAX 4000	// アクチュエータ全開時の力 [G]相当x1000
#define FMIN -2000	// アクチュエータ全閉時の力 [G]相当x1000

#define TIMEOUT 30		// 受信タイムアウト回数 3[sec]x10
#define FILT_G 0.1		// Gフィルタ定数

/********* グローバル変数 *********/
ULONG LastTime;			// ループ時間計測用
SHORT RxTime = 30000;	// 受信間隔計測用(通電時はタイムアウト状態)
Servo servoRH;			// 右上(サーボ#1)
Servo servoLH;			// 左上(サーボ#2)
Servo servoRL;			// 右下(サーボ#3)
Servo servoLL;			// 左下(サーボ#4)
FLOAT Dt;				// 受信したインターバル[s]
FLOAT Glon, Glat, Gvert;// 受信した前後G, 横G, 上下G[G]
FLOAT GlonFilt, GlatFilt, GvertFilt;// フィルタした前後G, 横G, 上下G[G]
SHORT ForceRH;			// サーボ出力(1G→1000)
SHORT ForceLH;
SHORT ForceRL;
SHORT ForceLL;
BOOL fRxTimeout;		// 受信タイムアウト
BOOL fRxStart;			// 受信開始
FLOAT Roll, Pitch;

// amazon購入のMG995サーボ(180deg品)
#define SVMIN 350		// サーボ最少パルス幅[us]
#define SVMAX 2650		// サーボ最大パルス幅[us]
#define DELTA90 1000	// サーボ90deg当たりの幅[us]
// サーボに個体差があり、センター位置になるようにTEST=1で値を得る
const SHORT Trim[4] = {1500, 1550, 1550, 1570};

// プロトタイプ宣言
void CalcFadeinoutForce(SHORT* force, SHORT tgt);
void ServoWrite10RH(SHORT angle);
void ServoWrite10LH(SHORT angle);
void ServoWrite10RL(SHORT angle);
void ServoWrite10LL(SHORT angle);
LONG map2(LONG x, LONG in_min, LONG in_max, LONG out_min, LONG out_max);
void Filter(FLOAT* filt, FLOAT dat, FLOAT fact);
void AddOnTime(BOOL flag, SHORT* ontime);

/*----------------------------------------------------------------------------
	セットアップ
----------------------------------------------------------------------------*/
void setup()
{
	pinMode(LED, OUTPUT);
	pinMode(SW, INPUT_PULLUP);
	pinMode(SWLO, OUTPUT);
	digitalWrite(SWLO, 0);

	Serial.begin(115200);	// ハードウェアシリアル115.2kbps
	while (!Serial);

	// サーボを組み込む
#if TEST == 1	// 調整のため500～2500より少し外側を設定しておく
	servoRH.attach(SVRH, SVMIN, SVMAX);
	servoLH.attach(SVLH, SVMIN, SVMAX);
	servoRL.attach(SVRL, SVMIN, SVMAX);
	servoLL.attach(SVLL, SVMIN, SVMAX);
#else	// 通常はトリム調整後の値を設定する
	servoRH.attach(SVRH, Trim[0] - DELTA90, Trim[0] + DELTA90);
	servoLH.attach(SVLH, Trim[1] - DELTA90, Trim[1] + DELTA90);
	servoRL.attach(SVRL, Trim[2] - DELTA90, Trim[2] + DELTA90);
	servoLL.attach(SVLL, Trim[3] - DELTA90, Trim[3] + DELTA90);
#endif //TEST

	ForceRH = ForceLH = ForceRL = ForceLL = FMIN;

	LastTime = micros();
}
/*----------------------------------------------------------------------------
	メインループ
----------------------------------------------------------------------------*/
void loop()
{
	if (micros() - LastTime >= 10000L)	// 10[ms]x1000
	{
		LastTime = micros();
LED_ON
#if TEST == 0	// 通常モードのビルドなら
		Job10ms();		// 10ms処理(rev1.0処理時間の実測は440μsほど)
#elif TEST == 3
		Job10msTest03();
#endif //TEST
LED_OFF
	}

	RxData();			// データ受信
}
/*----------------------------------------------------------------------------
	データ受信
----------------------------------------------------------------------------*/
#define BUFSIZ 64		// 受信バッファサイズ
void RxData()
{
	CHAR c, *p;
	static CHAR rxbuff[BUFSIZ];	// 受信バッファ
	static CHAR pbuff;			// バッファポインタ
	SHORT count, i;
	SHORT rxdata;

	while (Serial.available() > 0)	// 文字を受信していれば
	{
		c = Serial.read();			// 1文字受信
		if (pbuff < BUFSIZ - 2)		// バッファに余裕があるなら
		{
			rxbuff[pbuff++] = c;	// 格納
			if (c == '\n' && pbuff > 1)	// 行末か？
			{
				rxbuff[pbuff - 1] = '\0';// 文字列をターミネート
				pbuff = 0;			// ポインタを先頭に

#if TEST == 1	// テストモードのビルドなら
				Serial.println(rxbuff);
				rxdata = atoi(rxbuff);	// 文字→数値変換
				servoRH.writeMicroseconds(rxdata);
				servoLH.writeMicroseconds(rxdata);
				servoRL.writeMicroseconds(rxdata);
				servoLL.writeMicroseconds(rxdata);
#elif TEST == 2
				Serial.println(rxbuff);
				rxdata = atoi(rxbuff);	// 文字→数値変換
				ServoWrite10RH(map2(rxdata, FMIN, FMAX, AMIN, AMAX));
				ServoWrite10LH(map2(rxdata, FMIN, FMAX, AMIN, AMAX));
				ServoWrite10RL(map2(rxdata, FMIN, FMAX, AMIN, AMAX));
				ServoWrite10LL(map2(rxdata, FMIN, FMAX, AMIN, AMAX));
#else
				// 1行に含まれる','の数を数える
				for (count = i = 0; rxbuff[i] != '\0'; i++)
				{
					if (rxbuff[i] == ',')
					{
						count++;
					}
				}
				if (count == 3)	// SimGetGからの正常受信か？
				{
					RxTime = 0;			// 受信時間クリア
					// 文字列→数値変換
					p = strtok(rxbuff, ",");
					Dt = atof(p);			// simインターバル
					p = strtok(NULL, ",");
					Glon = atof(p);			// 前後G(前が正)
					p = strtok(NULL, ",");
					Glat = atof(p);			// 横G(右が正)
					p = strtok(NULL, ",");
					Gvert = atof(p);		// 上下G(上が正)
				}
				else if (count == 4)	// SimToolsからの正常受信か？
				{
					RxTime = 0;			// 受信時間クリア
					// 文字列→数値変換
					p = strtok(rxbuff, ",");
					Roll = (atof(p) - 32767.0) / 32767.0;	// ロール
					p = strtok(NULL, ",");
					Pitch = (atof(p) - 32767.0) / 32767.0;	// ピッチ
					p = strtok(NULL, ",");
					Glon = (atof(p) - 32767.0) / 32767.0;	// 前後G(前が正)
					p = strtok(NULL, ",");
					Glat = (atof(p) - 32767.0) / 32767.0;	// 横G(右が正)
					p = strtok(NULL, ",");
					Gvert = (atof(p) - 32767.0) / 32767.0;	//上下G(上が正)
				}
#endif //TEST
			}
		}
		else	// バッファフルなら
		{
			pbuff = 0;	// ポインタを先頭に
		}
	}
}
/*----------------------------------------------------------------------------
	10ms処理
----------------------------------------------------------------------------*/
void Job10ms()
{
	static BOOL ftoutz, fmin;
	static SHORT frh, flh, frl, fll;

	// 受信したGをフィルタリング
	Filter(&GlonFilt, Glon, FILT_G);
	Filter(&GlatFilt, Glat, FILT_G);
	Filter(&GvertFilt, Gvert, FILT_G);

	if (!digitalRead(SW))	// SWオン？
	{
		fmin = TRUE;		// サーボを目いっぱい戻す要求
	}

	// 正常受信してからの経過時間
	AddOnTime(TRUE, &RxTime);
	fRxTimeout = BOOLSET(RxTime >= TIMEOUT);	// 受信タイムアウト？
	if (ftoutz && !fRxTimeout)	// 受信タイムアウト解除？
	{
		fRxStart = TRUE;	// 初期化シーケンス開始要求
		GlonFilt = Glon;	// Gフィルタの初期化
		GlatFilt = Glat;
		GvertFilt = Gvert;
		CalcForce();		// 通常時のサーボ指示を得る
		frh = ForceRH;		// 新しい指示値
		flh = ForceLH;
		frl = ForceRL;
		fll = ForceLL;
		ForceRH = 0;		// 今は0に戻っていると仮定
		ForceLH = 0;
		ForceRL = 0;
		ForceLL = 0;
	}
	ftoutz = fRxTimeout;

	if (fmin)	// サーボを目いっぱい戻す要求？
	{	// サーボを目いっぱい戻すサーボ指示を作成
		CalcFadeinoutForce(&ForceRH, FMIN);
		CalcFadeinoutForce(&ForceLH, FMIN);
		CalcFadeinoutForce(&ForceRL, FMIN);
		CalcFadeinoutForce(&ForceLL, FMIN);
		// すべてのサーボが戻り切った状態でSWオン？
		if (ForceRH == FMIN && ForceLH == FMIN &&
			ForceRL == FMIN && ForceLL == FMIN && !digitalRead(SW))
		{
			fmin = FALSE;	// 通常動作に戻る
		}
	}
	else if (fRxStart)	// 初期化シーケンス？
	{
		CalcFadeinoutForce(&ForceRH, frh);
		CalcFadeinoutForce(&ForceLH, flh);
		CalcFadeinoutForce(&ForceRL, frl);
		CalcFadeinoutForce(&ForceLL, fll);
		// すべてのサーボが新たな位置に到達した？
		if (ForceRH == frh && ForceLH == flh &&
			ForceRL == frl && ForceLL == fll)
		{
			fRxStart = FALSE;
		}
	}
	else if (fRxTimeout)	// 受信が途絶えた？
	{
		// 0Gの位置まで徐々に増減するサーボ指示を作成
		CalcFadeinoutForce(&ForceRH, 0);
		CalcFadeinoutForce(&ForceLH, 0);
		CalcFadeinoutForce(&ForceRL, 0);
		CalcFadeinoutForce(&ForceLL, 0);
	}
	else	// 通常受信時
	{
		CalcForce();	// 通常時のサーボ指示を作成
	}

	// サーボ出力
	ServoWrite10RH(map2(ForceRH, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10LH(map2(ForceLH, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10RL(map2(ForceRL, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10LL(map2(ForceLL, FMIN, FMAX, AMIN, AMAX));
}
/*----------------------------------------------------------------------------
	100ms処理テスト03用
----------------------------------------------------------------------------*/
#define GDELTA 0.05
void Job10msTest03()
{
	static BOOL fmin;
	static SHORT count;

	Glat = Gvert = 0;

	if (count++ >= 10)
	{
		count = 0;
		if (!fmin)
		{
			Glon += GDELTA;
			if (Glon >= FMAX / 1000.0)
			{
				fmin = TRUE;
			}
		}
		else
		{
			Glon -= GDELTA;
			if (Glon <= FMIN / 1000.0)
			{
				fmin = FALSE;
			}
		}
	}

	// Gをフィルタリング
	Filter(&GlonFilt, Glon, FILT_G);
	Filter(&GlatFilt, Glat, FILT_G);
	Filter(&GvertFilt, Gvert, FILT_G);

	CalcForce();	// 通常時のサーボ指示を作成

//	// サーボ出力
	ServoWrite10RH(map2(ForceRH, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10LH(map2(ForceLH, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10RL(map2(ForceRL, FMIN, FMAX, AMIN, AMAX));
	ServoWrite10LL(map2(ForceLL, FMIN, FMAX, AMIN, AMAX));
}
/*----------------------------------------------------------------------------
	指示を徐々に目標まで増減するためのサーボ指示を作成
	書式 void CalcFadeinoutForce(SHORT* force, SHORT tgt);

	SHORT*_force;	力[G]x1000
	SHORT tgt;		力[G]x1000
----------------------------------------------------------------------------*/
#define FDELTA 10	// 1サンプリングあたりに変化させる力[G]x1000
void CalcFadeinoutForce(SHORT* force, SHORT tgt)
{
	if (*force - tgt > FDELTA)
	{
		*force -= FDELTA;
	}
	else if (*force - tgt < -FDELTA)
	{
		*force += FDELTA;
	}
	else
	{
		*force = tgt;
	}
}
/*----------------------------------------------------------------------------
	通常時のサーボ指示を作成
----------------------------------------------------------------------------*/
void CalcForce()
{
	SHORT flon, flat, fvert;

	// G→力変換(単純に1000倍した整数値)
	flon = (SHORT)(GlonFilt * 1000.0);
	flat = (SHORT)(GlatFilt * 1000.0);
	fvert = (SHORT)(GvertFilt * 1000.0);

	// 前後G
	// すべてのサーボを±に動かす
	ForceRH = ForceLH = ForceRL = ForceLL = flon;

	// 横G
#if 1
	if (flat > 0) // 右に加速中の場合は
	{
		ForceLH += flat;	// 左側のサーボを+に動かす
		ForceLL += flat;
	}
	else // 左に加速中の場合は
	{
		ForceRH -= flat;	// 右側のサーボを+に動かす
		ForceRL -= flat;
	}
#else
	ForceLH += flat;	// 左側のサーボを±に動かす
	ForceLL += flat;
	ForceRH -= flat;	// 右側のサーボを逆に動かす
	ForceRL -= flat;
#endif
	// 上下G
#if 1
	if (fvert > 0) // 上に加速中の場合は
	{
		ForceRL += fvert;	// 下側のサーボを+に動かす
		ForceLL += fvert;
	}
	else // 下に加速中の場合は
	{
		ForceRH += fvert;	// 上側のサーボを-に動かす
		ForceLH += fvert;
	}
#else
	ForceRL += fvert;	// 下側のサーボを±に動かす
	ForceLL += fvert;
	ForceRH -= fvert;	// 上側のサーボを逆に動かす
	ForceLH -= fvert;
#endif
}
/*----------------------------------------------------------------------------
	サーボに角度を指示(角度はdegを10倍した整数)
	書式 void ServoWrite10xx(SHORT angle);

	SHORT angle;	角度0～180[deg]x10
----------------------------------------------------------------------------*/
void ServoWrite10RH(SHORT angle)
{
	servoRH.writeMicroseconds(map(angle, 0, AMAX,
								Trim[0] - DELTA90, Trim[0] + DELTA90));
#if TEST == 3
//	Serial.println(angle);
	Serial.println(map(angle, 0, AMAX,
								Trim[0] - DELTA90, Trim[0] + DELTA90));
#endif //TEST
}
void ServoWrite10LH(SHORT angle)
{
	servoLH.writeMicroseconds(map(angle, AMAX, 0,
								Trim[1] - DELTA90, Trim[1] + DELTA90));
}
void ServoWrite10RL(SHORT angle)
{
	servoRL.writeMicroseconds(map(angle, 0, AMAX,
								Trim[2] - DELTA90, Trim[2] + DELTA90));
}
void ServoWrite10LL(SHORT angle)
{
	servoLL.writeMicroseconds(map(angle, AMAX, 0,
								Trim[3] - DELTA90, Trim[3] + DELTA90));
}
/*----------------------------------------------------------------------------
	最大最少をリミット付きmap関数()
----------------------------------------------------------------------------*/
LONG map2(LONG x, LONG in_min, LONG in_max, LONG out_min, LONG out_max)
{
	LONG ret;

	ret = map(x, in_min, in_max, out_min, out_max);
	ret = constrain(ret, out_min, out_max);
	return ret;
}
/*----------------------------------------------------------------------------
	フィルタ
	書式 void Filter(FLOAT* filt, FLOAT dat, FLOAT fact);

	FLOAT filt;		入力(1サンプル前)→出力
	FLOAT dat;		入力(今回)
	FLOAT fact;		フィルタ定数
----------------------------------------------------------------------------*/
void Filter(FLOAT* filt, FLOAT dat, FLOAT fact)
{
	*filt = (1.0 - fact) * *filt + fact * dat;
}
/*----------------------------------------------------------------------------
	フラグのオン時間の累積
	書式 void AddOnTime(BOOL flag, SHORT* ontime)

	BOOL flag;		フラグ
	SHORT* ontime;	オン時間
----------------------------------------------------------------------------*/
#define	TIMEMAX 30000
void AddOnTime(BOOL flag, SHORT* ontime)
{
	if (flag)							/* オンしてるなら */
	{
		if (*ontime < TIMEMAX)
		{
			(*ontime)++;				/*オン時間＋＋ */
		}
	}
	else
	{
		*ontime = 0;
	}
}
/*** end of "G-FeedBackRest.ino" ***/
