#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define F_CPU 16000000UL
#define THRESHOLD 200

#define ON 1
#define OFF 0

#define STOP  0
#define SING  1
#define FAST  2
#define BACK  3

volatile int tone;
volatile int tmpTone;
volatile int state = STOP;
volatile int buzzState = OFF;
volatile int singStart = OFF;

char t_table[16] = {
  255, //미사용
  239, //도
  213, //레 
  190, //미
  179, //파
  159, //솔
  142, //라
  128, //시
  119, //도
  106, //레
  95, //미
  89, //파
  79, //솔
  71, //라
  64, //시
  255, //미사용
};

//재생 버튼
SIGNAL(SIG_INTERRUPT4) {
  if(state == STOP) state = SING;
  else if(state == BACK) state = STOP;
}
//정지/되감기 버튼
SIGNAL(SIG_INTERRUPT5) {
  if(state == STOP) state = BACK;
  else if(state == SING) state = STOP;
}
//타이머 인터럽트
SIGNAL(SIG_OVERFLOW0) {
  if (buzzState == ON) {
    PORTB &= 0xEF;
    buzzState = OFF;
  } else if(tone != 0){
    PORTB |= 0x10;
    buzzState = ON;
  }
  TCNT0 = 256-t_table[tone];
}

void init(){
  //Infrared
  DDRF=0x3f;   // PF7 IN, PF6 IN
  ADCSRA=0x87; // ADC Enable
  //Interrupt
  DDRE = 0xcf;    //INT4, 5
  EICRB = 0x0a;   //INT4 falling edge, INT5 falling edge
  EIMSK = 0x30;   //INT4 enable,     INT5 enable
  SREG |= 1 << 7; //Global Int enable
  //Motor
  DDRB|=0x20;
  PORTB|=0x20;
  TCCR1A=0x82;
  TCCR1B=0x1A;
  ICR1=19999;     //TOP 0부터 시작
  OCR1A=3000;    //정지
  //Buzzer
  DDRB |= 0x10;
  TCCR0 = 0x03; // 32분주
  TIMSK = 0x01; // Overflow
  TCNT0 = 255;
}

unsigned short read_adc(int port) {
  unsigned char adc_low, adc_high;
  unsigned short value;
  ADMUX = port==7?0x07:0x06;
  ADCSRA |= 0x40; // ADC start conversion, ADSC = '1'
  while ((ADCSRA & 0x10) != 0x10) // ADC 변환 완료 검사
  ;
  adc_low = ADCL; // 변환된 Low 값 읽어오기
  adc_high = ADCH; // 변환된 High 값 읽어오기
  value = (adc_high << 8) | adc_low;
  // 흰색은 1, 검은색은 0
  if(value<THRESHOLD) return 1;
  else return 0;
}

int main() {
  volatile int iter = 0; // 0~3 반복자
  volatile int sigBool = 1; // 0/1 반복
  volatile int newSigBool = 1; // 신호 변화 비교용
  tone = 0;
  tmpTone=0;
  init();
  while (1) {
    if (state == STOP){
      tone=0;
      tmpTone=0;
      iter=0;
      singStart=OFF;
      OCR1A=3000;     //정지
      _delay_ms(300);
    }
    if (state == BACK){
      tone=0;
      OCR1A=3500;     //반시계
      _delay_ms(300);
      OCR1A=3000;
    }
    if (state == SING && singStart == OFF) { //시작점 찾기
      OCR1A = 3200; // 반시계
      _delay_ms(3);
      OCR1A = 3000;
      _delay_ms(1);
      newSigBool = read_adc(7);
      if(newSigBool!=sigBool){ // 뒷면이 바뀔때마다
        if(read_adc(6)==0){ // 앞면이 0이라면 노래 시작부분
          singStart =ON;
          tmpTone=0;
          iter = 1;
        }
      }
      sigBool = newSigBool;
    }
    if (state == SING && singStart == ON){
      OCR1A = 3200; // 반시계
      _delay_ms(3);
      OCR1A = 3000;
      _delay_ms(1);
      newSigBool = read_adc(7);
      if(newSigBool!=sigBool){ // 뒷면이 바뀔때마다
        if(iter==0)tmpTone=0;
        if(read_adc(6)==1){ // 앞면이 1이라면 값합산
          switch(iter){
            case 0:tmpTone|=1;break;
            case 1:tmpTone|=2;break;
            case 2:tmpTone|=4;break;
            case 3:tmpTone|=8;break;
          }
        }
        iter++;
        if(tmpTone==15){ // 1111: 노래끝
          state = STOP;
          continue;
        }
        if(iter==4){ // 4비트 인식 후 출력
          tone=tmpTone;
          _delay_ms(500);
          tone=0;
          iter=0;
        }
        sigBool = newSigBool;
      }
    }
  }
}