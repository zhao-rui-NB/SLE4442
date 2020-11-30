#include "memory.h"
/*arduino 
 * 
 * D2 PD2 ==>(INT0)>   RST
 * D3 PD3 ==>(INT1)>   CLK sda
 * D4 PD4 ==>      >   IO  scl
 * 
 * 
 * 
 * 
 */
#define RST PD2 
#define CLK PD3 
#define IO PD4 

static inline void setIO_DDR(bool DDR){ //0-in 1-out 
  if(DDR)DDRD |= (1<<IO); //PD4 =>io
  else   DDRD &= ~(1<<IO); 
}
static inline void setIO_D(bool D){ //digitalWrite
  if(D)PORTD |= (1<<IO); //PD4 =>io
  else PORTD &= ~(1<<IO); 
}

#define cmd_read_main       0x30  //讀主儲存器
#define cmd_write_main      0x38  //寫入主儲存器(不用密碼)
#define cmd_read_protect    0x34  //沒屁用 沒有做
#define cmd_write_protect   0x3C  //沒屁用 沒有做
#define cmd_read_security   0x31  //如果沒有驗證完成 屏蔽123byte
#define cmd_write_security  0x39  //不鎖直接寫 byte0 write 0b00000111 unlock
#define cmd_PSC             0x33  //不需要我不鎖卡(完全略過)
#define cmd_MY_ATR          0xFF  //自訂的指令 用來輸出 ATR 

#define MODE_ATR  1
#define MODE_CMD  2
#define MODE_WAIT_START 3
#define MODE_WAIT_STOP  4
#define MODE_OUT  5

volatile uint8_t MODE = 2;

volatile bool Reset; 
volatile bool in_data; 

volatile uint8_t left_wait_clk;

volatile bool lock = 1;

volatile uint8_t in_cmd;
volatile uint8_t data_out;

volatile uint8_t bit_pointer;

volatile uint8_t counter;
volatile uint8_t cmd;
volatile uint16_t addr;
volatile uint8_t data;

volatile unsigned char *selmemory = main_menory;



void setup() {
  //LED_setup();
  
  DDRD &= ~(1<<RST);  //set RST input 
  DDRD &= ~(1<<CLK);  //set CLK input 
  DDRD &= ~(1<<IO);  //set IO  input  
  EICRA = 0B00000111; //中斷觸發方式 rst-rising clk-logical change  
  EIMSK = 0B00000011; //中斷的開關
}

ISR(INT0_vect){//static inline void reset_in() { //reset 
  Reset = 1 ;
}

ISR(INT1_vect){//static inline void clk_in(){
  if( (PIND>>CLK)&1 ){  // (PORTD>>CLK)&1) read clk 
    posedge();
  }
  else{ 
    negedge();
  }
}

static inline void posedge(){
  
  in_data = (PIND>>IO)&1;
  if(MODE == MODE_WAIT_START&& in_data==1 ){ //CLK中 IO負緣==>真的開始
     MODE = MODE_CMD;
     counter=0;
     setIO_DDR(0);
  }

  
  else if(MODE == MODE_CMD){
    in_cmd = in_cmd<<1 | in_data;
    counter++;
    
    if(counter==8){
      cmd = DECODE[in_cmd];
    }
    if(counter==16){
      addr = DECODE[in_cmd];
    }
    if(counter==24){
      data = DECODE[in_cmd];
      MODE = MODE_WAIT_STOP;
      counter==0;
    }
  } 
     
  else if(MODE==MODE_WAIT_STOP){
    bit_pointer=0;
    if(cmd==cmd_read_main){     //
       selmemory = main_menory;
       MODE = MODE_OUT;
    }
    else if(cmd==cmd_write_main){
       selmemory = main_menory; 
       selmemory[addr]=data;
       MODE = MODE_WAIT_START;  
    }
    else if(cmd==cmd_read_security   ){     //
       if(lock){ selmemory = zero_memory; }
       else{ selmemory = security_memory; }
       addr = 0;
       MODE = MODE_OUT;
    }
    else if(cmd==cmd_write_security     ){
       selmemory= security_memory;
       selmemory[addr]=data;
       MODE = MODE_WAIT_START;
       if(addr==0 && data==0b00000111){
        lock = 0;  
       }
    }
    else if(cmd==cmd_PSC){ // 直接略過
       MODE = MODE_WAIT_START;
    }
    else{
      MODE = MODE_WAIT_START;
    }
    
  }

  
  
}

static inline void negedge(){
  //setIO_D( !((PORTD>>IO)&1) );
  data_out = (selmemory[addr]>>bit_pointer) & 0x1;
  if(MODE==MODE_ATR || MODE==MODE_OUT ){
    setIO_DDR(1);
    if(cmd == cmd_MY_ATR ){
      setIO_D(data_out);
      if(addr==4 && bit_pointer==0){
        MODE = MODE_WAIT_START;
        setIO_DDR(0);
      }
    }

    if(cmd == cmd_read_main){
     setIO_D(data_out);
     if(addr==256 && bit_pointer==0){
        MODE = MODE_WAIT_START;
        setIO_DDR(0);
     }
    }

    if(cmd == cmd_read_security){
       setIO_D(data_out);
     if(addr==4 && bit_pointer==0){
       MODE = MODE_WAIT_START;
       setIO_DDR(0);
     }
    }


    bit_pointer++;
    if(bit_pointer==8){
      bit_pointer=0;
      addr++;
    }
  }
   
}


void loop() {
  if(Reset){
    setIO_DDR(1);
    MODE = MODE_ATR;
    cmd = cmd_MY_ATR;
    addr = 0;
    bit_pointer = 0;
    Reset = 0;
    counter = 0;
  }
//LED_output( cmd );
  
}

//////////////////////////////////////////debug////////////////////////////////////////////////////////
//////////////////////////////////////////debug////////////////////////////////////////////////////////
//////////////////////////////////////////debug////////////////////////////////////////////////////////
/*
void LED_setup(){
  #define LED7 A2
  #define LED6 12
  #define LED5 A3
  #define LED4 11
  #define LED3 A4
  #define LED2 10
  #define LED1 A5
  #define LED0 9

  pinMode(LED7,OUTPUT);
  pinMode(LED6,OUTPUT);
  pinMode(LED5,OUTPUT);
  pinMode(LED4,OUTPUT);
  pinMode(LED3,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(LED1,OUTPUT);
  pinMode(LED0,OUTPUT);
  
}

void LED_output(char cmd){
  digitalWrite(LED7 , ~(cmd>>7)&1 );
  digitalWrite(LED6 , ~(cmd>>6)&1 );
  digitalWrite(LED5 , ~(cmd>>5)&1 );
  digitalWrite(LED4 , ~(cmd>>4)&1 );
  digitalWrite(LED3 , ~(cmd>>3)&1 );
  digitalWrite(LED2 , ~(cmd>>2)&1 );
  digitalWrite(LED1 , ~(cmd>>1)&1 );
  digitalWrite(LED0 , ~(cmd>>0)&1 );
}
*/
