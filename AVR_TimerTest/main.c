/*
 * AVR_TimerTest.c
 *
 * Created: 2023/08/25 金 18:15:17
 * Author : Kenichi Iwai

  8MHzに変更するためにFUSEを変更する
  avrdude -c usbasp -p m8 -U hfuse:w:0xd9:m -U lfuse:w:0xe4:m

  01 - PC6 - NC(RESET)
  02 - PD0 - toComm(RXD)
  03 - PD1 - NC(TXD)
  04 - PD2 - toMAXBet(INT0)
  05 - PD3 - NC
  06 - PD4 - NC
  07 - VCC - VCC
  08 - GND - GND
  09 - PB6 - NC
  10 - PB7 - NC
  11 - PD5 - NC
  12 - PD6 - NC
  13 - PD7 - NC
  14 - PB0 - NC

  15 - PB1 - NC
  16 - PB2 - toReelSensor1
  17 - PB3 - toReelSensor2
  18 - PB4 - toReelSensor3
  19 - PB5 - NC(test)
  20 - AVCC- NC
  21 - AREF- NC
  22 - GND - GND
  23 - PC0 - toCoinLess
  24 - PC1 - toStart
  25 - PC2 - toStop1
  26 - PC3 - toStop2
  27 - PC4 - toStop3
  28 - PC5 - toOption

  テスト機種：
  オーイズミ	Sパチスロひぐらしのなく頃に２PX
 */

#define Length(array) (sizeof(array)/sizeof(array[0]))

#define F_CPU 8000000UL					// 8MHz

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>

typedef unsigned char byte;

volatile byte flag_Data[256];		// volatileは割り込みに使う
volatile byte flag_write = 0;
byte flag_read  = 0;

byte flgC1[38];
byte flgC2[38];
byte flgC3[3];
byte flgC4[1];
byte flgC5[3];
//byte flgC6[3];
//byte flgCA[4];
byte flgCB[1];

byte autoPlayStep	= 0;			// CoinIn Start 1stStop 2ndStop 3rdStop
byte stopPattarn	= 0;			// 0=>123 1=>132 2=>213 3=>312 4=>231 5=>321
int  stopPosition[3]	= {-1, -1, -1};

//byte Mode=0;						//0=通常時、1=ボーナス揃え時、2=バケ中、
									//3=ビッグ中、4=CZ中、5=RTCZ中、6=RT中、7=フリー打ち

bool bbMAXFlg		= true;
bool bonusFlg		= false;

bool autoMode		= false;
bool autoOff		= false;
int  count			= 0;
byte coinin			= 0;

void rs_putc (char c){
	loop_until_bit_is_set(UCSRA, UDRE); //UDREビットが1になるまで待つ
	UDR = c;
}

void rs_puts (char *st)
{
	while (*st) {
		rs_putc (*st);
		if (*(st++) == '\n') rs_putc('\r');
	}
}

bool is_received()
{
	// read位置とwrite位置が異なるならば受信データがあるはず
	return (flag_write !=  flag_read) ? true : false;
}

// データを受信するまで待機する
void wait_for_receiving()
{
	while(!is_received()){
	}
}

// 受信したデータを返す。受信したデータがない場合は受信するまで待機。
byte getReceivedData() //データが無いと待ってしまう
{
	wait_for_receiving();
	rs_putc(flag_Data[flag_read]);
	return flag_Data[flag_read++];
}

void switchOnOff(byte button){
	switch(button){
		case 0:		// CoinIn
			PORTC &= ~(1 << PORTC0);
			_delay_ms(100); //100ms待つ
			PORTC |= 1 << PORTC0;
			break;
		case 1:		// Start
			//_delay_ms(100); //100ms待つ ここで同期を取るとフラグ狙い打ちできる？？？
			PORTC |= 1 << PORTC1;
			_delay_ms(10); //10ms待つ
			PORTC &= ~(1 << PORTC1);
			break;
		case 2:		// 1stStop
			PORTC &= ~(1 << PORTC2);
			_delay_ms(5); //10ms待つ
			PORTC |= 1 << PORTC2;
			break;
		case 3:		// 2ndStop
			PORTC &= ~(1 << PORTC3);
			_delay_ms(5); //10ms待つ
			PORTC |= 1 << PORTC3;
			break;
		case 4:		// 3rdStop
			PORTC &= ~(1 << PORTC4);
			_delay_ms(5); //10ms待つ
			PORTC |= 1 << PORTC4;
			break;
		case 5:		// Option
//			PORTC |= 1 << PORTC5;
//			_delay_ms(10); //100ms待つ
			PORTC &= ~(1 << PORTC5);
			_delay_ms(100); //100ms待つ
			PORTC |= 1 << PORTC5;
//			_delay_ms(10); //100ms待つ
			break;
	}
}

ISR(INT0_vect){
	_delay_ms(10);

	if(bit_is_clear(PIND, PIND2)){
		if(autoMode){
			autoMode = false;
			autoOff  = false;
			autoPlayStep = 0;
			PORTC = (1<<PC0)|(0<<PC1)|(1<<PC2)|(1<<PC3)|(1<<PC4)|(1<<PC5);
		}
		else{
			// Coinlessへ信号出力
			switchOnOff(0);
		}
	}
	else{
		count = 0;
	}
}

void PORT_init(void){
	DDRB  = 0b11100011;	// 未使用はOUTにする
	PORTB = 0b00011111;

	DDRC  = 0b11111111;	// 未使用はOUTにする
	PORTC = 0b00111101;
	// PD0 - (IN) RXD
	// PD2 - (IN) toMAXBet
	DDRD  = (0<<PD0)|(0<<PD2);
	PORTD = (0<<PD0)|(1<<PD2);
	//DDRD  &= ~(1<<PD2);	// INT0入力設定
	//PORTD |=  (1<<PD2);	// INT0 Pull-up設定
	GICR  = (1<<INT0);	// INT0割り込み許可
	GIFR  = (1<<INT0);	// INT0割り込み要求フラグON
	MCUCR = (1<<ISC00)|(0<<ISC01);	//INT0ピンの立下りエッジで発生
}


volatile unsigned int led0;
volatile byte status = 0;


ISR(TIMER0_OVF_vect) //timer0でのコンペアマッチAの割り込み関数
{
	//ここに割り込み時の処理を書く
	TCNT0 = 0;
	if(led0 > 50){
		if(status == 1){
			switchOnOff(1);
			status++;
		}
		led0 = 0;
	}
	else{
		led0++;
	}
}

int main(void)
{
	PORT_init();
	//timer0 制御レジスタA
	//TCCR1A = 0b10000010;  //10:コンペアマッチAでLOW, 10:CTCモード
	//timer0 制御レジスタB
	//TCCR1B = 0b00000001;  //分周なし
	//timer0 割り込み設定
	//TIMSK0 = 0b00000010;  //コンペアマッチAの割り込みを設定

	TIMSK = 0x01;
	//コンペアマッチするタイミング
	//OCR0A = (50000-1)*8;  //32.5msでコンペアマッチ @1MHz
	TCNT0 = 131;
	//TCCR0 = 0b00000001;
	TCCR0 = 0x04;

	sei();										// 全体の割り込み許可

    while (1)
    {
		switch(status){
			case 0:
				switchOnOff(0);
				status++;
				break;
			case 2:
				switchOnOff(2);
				_delay_ms(50);
				status++;
				break;
			case 3:
				switchOnOff(3);
				_delay_ms(50);
				status++;
				break;
			case 4:
				switchOnOff(4);
				_delay_ms(50);
				status=0;
				break;
		}
	}

	return 0;
}

#include <avr/io.h>

ISR(TIMER0_COMPA_vect){

}

int main(void)
{
    /* Replace with your application code */
    while (1)
    {
    }
}