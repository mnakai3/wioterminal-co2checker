#include "SparkFun_SCD30_Arduino_Library.h"
SCD30 SCD;

#define WIOOFFSET	2.2			// SCD30の温度オフセット(WIO)

#define tempL	(-20.)			// 温度の有効最低値
#define AVESIZE	10
#define BUFSIZE 240
#define BRIGHTNESS 127			// 0-255

bool isSCD = false;				// SCD30 or CDM7160

int ss = 0;
unsigned int ssON = 0;			// 電源ONの残り時間(秒) / LCD点灯時間(秒)
unsigned long lastmillis = 0;

#define XOF	2					// LCDのX方向オフセット(WIOは左端が見えない!)
#define YOF	0					// LCDのY方向オフセット
#define LightL	5				// 明るさの下閾値
#define LightH	10				// 明るさの上閾値

////////////////////////////////////////////////////////////////////////////////
// Button

#define isPressed(b)		(S[b]==1||S[b]==2||S[b]==3)
#define isReleased(b)		(S[b]==0||S[b]==4||S[b]==5)
#define wasPressed(b)		(S[b]==1)
#define wasReleased(b)		(S[b]==4)
#define pressedFor(b,t)		(S[b]==3 && millis()>Btnmillis[b]+t)

int Btnpin[3] = { WIO_KEY_C, WIO_KEY_B, WIO_KEY_A };
int S[3] = { 0, 0, 0 };				// ボタンの状態
unsigned long Btnmillis[3] = { 0, 0, 0 };

// 0:BtnA, 1:BtnB, 2:BtnC
void Btnread(int i)
{
	const int val = digitalRead(Btnpin[i]);
	switch (S[i])
	{
	case 0:
		if (val == LOW)
		{
			Btnmillis[i] = millis();
			S[i] = 1;
		}
		break;
	case 1:
		S[i] = 2;
		break;
	case 2:
		if (millis() > Btnmillis[i] + 50)
		{
			S[i] = 3;
		}
		break;
	case 3:
		if (val == HIGH)
		{
			Btnmillis[i] = millis();
			S[i] = 4;
		}
		break;
	case 4:
		S[i] = 5;
		break;
	case 5:
		if (millis() > Btnmillis[i] + 50)
		{
			S[i] = 0;
		}
		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Tone

// Hz, ms
void toneEx(int freq, int duration)
{
	const int t_us = 1000000L / freq;
	for (long i = 0; i < duration * 1000L; i += t_us)
	{
		digitalWrite(WIO_BUZZER, 1);
		delayMicroseconds(t_us / 2);
		digitalWrite(WIO_BUZZER, 0);
		delayMicroseconds(t_us / 2);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Measure

// 平均値算出用バッファ
int pave = 0;
int co2buf[AVESIZE];
int humibuf[AVESIZE];
float tempbuf[AVESIZE];
float tempREFbuf[AVESIZE];
float wbgtbuf[AVESIZE];

int co2ave;
int humiave;
float tempave;
float tempREFave;
float wbgt;

// グラフ表示用バッファ
int pBUF = 0;
int CO2BUF[BUFSIZE];
int WBGTBUF[BUFSIZE];

// 結果はco2ave, tempave, humiave, wbgt
void measure()
{
	float temp = tempL - 1.;
	float tempREF = tempL - 1.;
	int co2 = -1;
	int humi = -1;
	if (isSCD)							// ----SCD30------------
	{
		if (SCD.dataAvailable())
		{
			co2 = SCD.getCO2();										// CO2
			if (co2 >= 10000 || co2 < 200) co2 = -1;				// 200未満は無効
			temp = SCD.getTemperature() - WIOOFFSET;			// 温度(補正値)
			humi = SCD.getHumidity();							// 湿度
		}
	}												// ---------------------
//	Serial.println(String(temp)+","+humi+","+co2);	// 測定値の確認
	co2buf[pave] = co2;
	humibuf[pave] = humi;
	tempbuf[pave] = temp;
	tempREFbuf[pave] = tempREF;

	int co2count = 0;
	int humicount = 0;
	int tempcount = 0;
	int tempREFcount = 0;
	co2ave = 0;
	humiave = 0;
	tempave = 0.;
	tempREFave = 0.;
	for (int i = 0; i < AVESIZE; ++i)
	{
		if (co2buf[i]     >= 0    ){ co2ave     += co2buf[i]    ; co2count++;     }
		if (humibuf[i]    >= 0    ){ humiave    += humibuf[i]   ; humicount++;    }
		if (tempbuf[i]    >= tempL){ tempave    += tempbuf[i]   ; tempcount++;    }
		if (tempREFbuf[i] >= tempL){ tempREFave += tempREFbuf[i]; tempREFcount++; }
	}
	co2ave     = (co2count     > 0 ? (co2ave / co2count + 5) / 10 * 10: -1          );	// 直近10秒間(5～6個)の平均(1の位を四捨五入)
	humiave    = (humicount    > 0 ? humiave    / humicount           : -1          );	// 直近10秒間(5～6個)の平均
	tempave    = (tempcount    > 0 ? tempave    / tempcount           : (tempL - 1.));	// 直近10秒間(5～6個)の平均
	tempREFave = (tempREFcount > 0 ? tempREFave / tempREFcount        : (tempL - 1.));	// *****
//-----------------------------------------------------------------------------------
//	co2ave=1270;	tempave=26.3;	humiave=63;	// 撮影用
//	co2ave=random(0,5000)/10*10;tempave=random(-20*10,40*10)/10.;humiave=random(10,100);humicount=tempcount=1;	// 表示チェック用
//-----------------------------------------------------------------------------------
	if (humicount > 0 && tempcount > 0)						// WBGTの計算(日本生気象学会の表)
	{
		wbgt = -1.7 + .693 * tempave + .0388 * humiave + .00355 * humiave * tempave;
	}
	else
	{
		wbgt = -1.;
	}
	wbgtbuf[pave] = wbgt;
	pave = (pave + 1) % AVESIZE;		// pave: 0～9
	if (ss % 15 == 0)					// 15秒毎
	{
		CO2BUF[pBUF] = co2ave;			// バッファに保存(グラフ表示用)
		WBGTBUF[pBUF] = wbgt;
		pBUF = (pBUF + 1) % BUFSIZE;	// pBUF: 0～239(1H)
	}
}

////////////////////////////////////////////////////////////////////////////////
// Display

#include <LovyanGFX.hpp>
static LGFX lcd;

#define FONT123	&fonts::Font8
#define FONTABC	&fonts::Font4
#define setCursorFont(x,y,font,mag)	{lcd.setCursor(x,y);lcd.setFont(font);lcd.setTextSize(mag);}

#define P8	8./14.
#define P10	10./14.
#define P14	14./14.
#define P16	16./55.
#define P40	40./55.
#define P48	48./55.

int mode = 1;	// 0:OFF 1:温度・湿度・CO2(冬モード) 2:WBGT・CO2(夏モード) 3:CO2グラフ 4: WBGTグラフ

// co2値のy座標
int yco2(int co2)
{
	const int y = 239 - co2 * 2 / 25 - YOF;
	return (y < 0 ? 0 : y);
}

// wbgt値のy座標
int ywbgt(float val)
{
	int y = 239 - (int)((val - 10.) * 8.) - YOF;
	if (y >= 240) y = 239 - YOF;
	return (y < 0 ? 0 : y);
}

// co2値の表示色
int co2Color(int co2)
{
	if (co2 >= 1500)		return TFT_RED;			// 1500～
	else if (co2 >= 1000)	return TFT_YELLOW;		// 1000～1500
	else if (co2 >= 0)		return TFT_GREEN;		// ～1000
	else					return TFT_BLACK;
}

// wbgt値の表示色
int wbgtColor(float val)
{
	if (val >= 31.)			return TFT_RED;			// 31～
	else if (val >= 28.)	return TFT_ORANGE;		// 28～31
	else if (val >= 25.)	return TFT_YELLOW;		// 25～28
	else if (val >= 0.)		return TFT_GREEN;		// 0～25
	else					return TFT_BLACK;
}

// humi値の表示色
int humiColor(int val)
{
	if (val >= 80)			return TFT_CYAN;		// 80～
	else if (val >= 30)		return TFT_GREEN;		// 30～80
	else if (val >= 0)		return TFT_WHITE;		// 0～30
	else					return TFT_BLACK;
}

// temp値の表示色
int tempColor(float val)
{
	if (val >= 28.)			return TFT_ORANGE;		// 28～
	else if (val >= 17.)	return TFT_GREEN;		// 17～28
	else if (val >= tempL)	return TFT_CYAN;		// -20～17
	else					return TFT_BLACK;
}

// CO2表示 ---------
void co2print()
{
	switch (mode)
	{
	case 1:	// 温度・湿度・CO2(冬モード)
		setCursorFont(132, 174, FONT123, P40);
		if (co2ave >= 100) lcd.printf(co2ave < 1000 ? "%5d" : "%4d", co2ave);
		else               lcd.print("      - ");				// 8

		setCursorFont(260, 144, FONTABC, P14);
		lcd.print("ppm");

		lcd.fillRect(0, 164, 110, 76, co2Color(co2ave));	// インジケータ
		break;
	case 2:	// WBGT・CO2(夏モード)
		setCursorFont(120, 150, FONT123, P48);
		if (co2ave >= 100) lcd.printf(co2ave < 1000 ? "%5d" : "%4d", co2ave);
		else               lcd.print("      - ");				// 8

		setCursorFont(260, 120, FONTABC, P14);
		lcd.print("ppm");

		lcd.fillRect(0, 123, 110, 116, co2Color(co2ave));	// インジケータ
		break;
	case 3:	// CO2グラフ
		setCursorFont(241 + XOF, 10, FONTABC, P14);
		lcd.print(" CO2");

		setCursorFont(241 + XOF, 80, FONT123, P16);
		if (co2ave >= 100) lcd.printf(co2ave < 1000 ? "%5d" : "%4d", co2ave);
		else               lcd.print("      - ");				// 8

		setCursorFont(280, 60, FONTABC, P10);
		lcd.print("ppm");
		break;
	}
}

// WBGT表示 --------
void wbgtprint()
{
	switch (mode)
	{
	case 2:	// WBGT・CO2(夏モード)
		setCursorFont(138, 24, FONT123, P48);
		if (wbgt >= 0.) lcd.printf(wbgt < 10. ? "%5.1f" : "%4.1f", wbgt);
		else            lcd.print("     - ");						// 7

		setCursorFont(296, 0, FONTABC, P14);
		lcd.print("C");

		lcd.fillRect(0, 0, 110, 116, wbgtColor(wbgt));		// インジケータ
		break;
	case 4:	// WBGTグラフ
		setCursorFont(241 + XOF, 10, FONTABC, P14);
		lcd.print("WBGT");

		setCursorFont(241 + XOF, 80, FONT123, P16);
		if (wbgt >= 0.) lcd.printf(wbgt < 10. ? "%5.1f" : "%4.1f", wbgt);
		else            lcd.print("     - ");						// 7

		setCursorFont(300, 70, FONTABC, P10);
		lcd.print("C");
		break;
	}
}

// 温度表示 --------
void tempprint()
{
	setCursorFont(128, 10, FONT123, P40);
	if (tempave >= tempL)
	{
		lcd.printf(tempave > -10. && tempave < 10. ? "%6.1f" : "%5.1f", tempave);	// なければ温度
	}
	else           lcd.print("      - ");								// 8

	setCursorFont(296, 0, FONTABC, P14);
	lcd.print("C");

	setCursorFont(0, 10, FONT123, P40);
	lcd.fillRect(0, 0, 110, 76, tempColor(tempave));					// なければインジケータ
}

// 湿度表示 --------
void humiprint()
{
	setCursorFont(150, 92, FONT123, P40);
	if (humiave >= 0) lcd.printf("%2d", humiave);
	else              lcd.print("  - ");								// 4

	setCursorFont(260, 82, FONTABC, P14);
	lcd.print("%RH");

	lcd.fillRect(0, 82, 110, 76, humiColor(humiave));			// インジケータ
}

void co2graph()
{
	lcd.setFont(FONTABC);
	lcd.setTextSize(P8);
	for (int i = 0; i <= BUFSIZE; ++i)
	{
		if (i % (10 * 4) == 0)				// 縦の補助線
		{
			lcd.drawFastVLine(i + XOF, 0, 239 - YOF, (i == 0 ? TFT_WHITE : TFT_DARKGREY));
			if (i == 40)
			{
				for (int co2 = 1000; co2 <= 3000; co2 += 1000)
				{
					lcd.setCursor(1 + XOF, yco2(co2) + 1);
					lcd.print(co2);
				}
			}
		}
		else
		{
			const int co2 = CO2BUF[(pBUF + i) % BUFSIZE];
			if (co2 > 0)						// plot co2!!
			{
				lcd.drawFastVLine(i + XOF, 1        , yco2(co2)      , TFT_BLACK    );
				lcd.drawFastVLine(i + XOF, yco2(co2), 238 - yco2(co2), co2Color(co2));
			}
			else
			{
				lcd.drawFastVLine(i + XOF, 1, 238 - YOF, TFT_BLACK);	// 無効データ
			}
			for (int u = 0; u <= 3000; u += 500)
			{
				if (u == 0)                    lcd.drawPixel(i + XOF, yco2(u), TFT_WHITE   );	// X軸
				else if (u == 1000 && co2 < u) lcd.drawPixel(i + XOF, yco2(u), TFT_YELLOW  );	// 1000ppm
				else if (u == 1500 && co2 < u) lcd.drawPixel(i + XOF, yco2(u), TFT_RED     );	// 1500ppm
				else                           lcd.drawPixel(i + XOF, yco2(u), TFT_DARKGREY);
			}
		}
	}
}

void wbgtgraph()
{
	lcd.setFont(FONTABC);
	lcd.setTextSize(P8);
	for (int i = 0; i <= BUFSIZE; ++i)
	{
		if (i % (10 * 4) == 0)				// 縦の補助線
		{
			lcd.drawFastVLine(i + XOF, 0, 239 - YOF, (i == 0 ? TFT_WHITE : TFT_DARKGREY));
			if (i == 40)
			{
				for (int val = 10; val <= 40; val += 10)
				{
					lcd.setCursor(1 + XOF, ywbgt(val) + (val == 10 ? -18 : 1));
					lcd.print(val);
				}
			}
		}
		else
		{
			const float val = WBGTBUF[(pBUF + i) % BUFSIZE];
			if (val > 0.)						// plot wbgt!!
			{
				lcd.drawFastVLine(i + XOF, 1         , ywbgt(val)      , TFT_BLACK     );
				lcd.drawFastVLine(i + XOF, ywbgt(val), 238 - ywbgt(val), wbgtColor(val));
			}
			else
			{
				lcd.drawFastVLine(i + XOF, 1, 238 - YOF, TFT_BLACK);			// 無効データ
			}
			for (int u = 10; u <= 40; u += 10)
			{
				if (u == 10) lcd.drawPixel(i + XOF, ywbgt(u), TFT_WHITE   );				// X軸
				else         lcd.drawPixel(i + XOF, ywbgt(u), TFT_DARKGREY);				// 20,30,40度
				lcd.drawPixel(i + XOF, ywbgt(25), (val < 25 ? TFT_YELLOW : TFT_DARKGREY));	// 25度
				lcd.drawPixel(i + XOF, ywbgt(28), (val < 28 ? TFT_ORANGE : TFT_DARKGREY));	// 28度
				lcd.drawPixel(i + XOF, ywbgt(31), (val < 31 ? TFT_RED    : TFT_DARKGREY));	// 31度
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// setup and loop

void setup()
{
	for (int i = 0; i < 3; ++i) pinMode(Btnpin[i], INPUT_PULLUP);
	pinMode(WIO_BUZZER, OUTPUT);
	pinMode(WIO_LIGHT, INPUT);

	lcd.init();
	lcd.setRotation(1);
	lcd.clear();
	lcd.setBrightness(BRIGHTNESS);	// 0-255
	lcd.setFont(FONT123);
	lcd.setTextColor(TFT_WHITE, TFT_BLACK);
	Wire.begin();
	isSCD = SCD.begin();		// SCD30の有無

	for (int i = 0; i < AVESIZE; ++i)
	{
		co2buf[i] = -1;
		tempbuf[i] = tempL - 1.;
		humibuf[i] = -1;
		wbgtbuf[i] = -1.;
	}
	for (int i = 0; i < BUFSIZE; ++i)
	{
		CO2BUF[i] = -1;
		WBGTBUF[i] = -1.;
	}
	lastmillis = millis();
}

void loop()
{
	if (millis() > lastmillis + 1000)								// 1秒ごとに
	{
		lastmillis = millis();
		measure();													// 結果はco2ave, tempave, humiave, wbgt

		switch (mode)
		{
		case 1:	// mode 1:温度・湿度・CO2(冬モード)
			tempprint();											// 温度は1秒毎に表示更新
			humiprint();											// 湿度は1秒毎に表示更新
			if (ss % 5 == 0) co2print();							// co2値は5秒毎に表示更新
			break;
		case 2:	// mode 2:WBGT・CO2(夏モード)
			wbgtprint();											// wbgt値は1秒毎に表示更新
			if (ss % 5 == 0) co2print();							// co2値は5秒毎に表示更新
			break;
		case 3:	// mode 3:co2グラフ
		case 4:	// mode 4:wbgtグラフ
			wbgtprint();											// wbgt値を表示更新
			if (ss % 5 == 0) co2print();							// co2値は5秒毎に表示更新
			if (mode == 3 && ss % 15 == 0) co2graph();				// co2グラフは15秒毎に表示更新
			if (mode == 4 && ss % 15 == 0) wbgtgraph();				// wbgtグラフは15秒毎に表示更新
			break;
		}

		ss = (ss + 1) % 60;											// ss: 0～59sec
		const int light = analogRead(WIO_LIGHT);
		if (mode > 0)												// モード1～4で
		{
			if (light >= LightH)									// 明るければ
			{
				ssON = 10;
				lcd.setBrightness(BRIGHTNESS);						// LCD点灯
			}
			else if (light < LightL)
			{
				if (ssON > 0 && --ssON == 0)
				{
					lcd.setBrightness(0);							// LCD消灯
				}
			}
		}
	}

	Btnread(0);														// BtnA
	if (wasReleased(0))												// BtnAが押されたら
	{
		mode = (mode + 1) % 5;										// 表示モード切替 0-4
		for (int i = 0; i < mode; ++i)								// ピピッ
		{
			toneEx(1000, 50);
			delay(100);
		}
		lcd.clear();
		if (mode == 0)												// modeが0なら
		{
			ssON = 0;
			lcd.setBrightness(0);									// LCD消灯
			toneEx(1000, 500);										// ピーッ
		}
		else														// modeが1～4なら
		{
			ssON = 10;												// 暗くても10秒は点灯
			lcd.setBrightness(BRIGHTNESS);							// LCD点灯
			switch (mode)
			{
			case 1:	// 温度・湿度・CO2(冬モード)
				tempprint();
				humiprint();
				co2print();
				break;
			case 2:	// WBGT・CO2(夏モード)
				wbgtprint();
				co2print();
				break;
			case 3:	// co2グラフ
				co2graph();
				co2print();
				break;
			case 4:	// wbgtグラフ
				wbgtgraph();
				wbgtprint();
				break;
			}
		}
	}
}
