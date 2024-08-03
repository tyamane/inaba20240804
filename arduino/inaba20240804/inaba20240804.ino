/*
   -- RemoteController --
   
   This source code of graphical user interface 
   has been generated automatically by RemoteXY editor.
   To compile this code using RemoteXY library 3.1.8 or later version 
   download by link http://remotexy.com/en/library/
   To connect using RemoteXY mobile app by link http://remotexy.com/en/download/                   
     - for ANDROID 4.11.1 or later version;
     - for iOS 1.9.1 or later version;
    
   This source code is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.    
*/

//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP8266WIFI_LIB_POINT
#include <ESP8266WiFi.h>


#include <RemoteXY.h>

// RemoteXY connection settings 
#define REMOTEXY_WIFI_SSID "INABA02"
#define REMOTEXY_WIFI_PASSWORD "12345678"
#define REMOTEXY_SERVER_PORT 6377

// RemoteXY configurate  
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 52 bytes
  { 255,3,0,13,0,45,0,16,31,1,5,37,4,42,51,51,2,26,31,66,
  1,48,6,7,16,2,26,66,1,7,5,7,16,2,26,67,4,17,5,29,
  5,2,26,11,4,160,19,14,24,7,6,26 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  int8_t joystick_1_x; // from -100 to 100  
  int8_t joystick_1_y; // from -100 to 100  
  int8_t adjust; // =-100..100 slider position 

    // output variables
  int8_t level_right; // =0..100 level position 
  int8_t level_left; // =0..100 level position 
  char text_1[11];  // string UTF8 end zero 

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0 

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

#include <EEPROM.h>

// ビルド設定
// ボードタイプ WEMOS D1 R2 & mini
// 通信速度 115200
// CPU速度 160MHz
ADC_MODE(ADC_VCC);

//////////////////////////////////////////////
//        L298N　mini                       //
//////////////////////////////////////////////
/* defined the right motor control pins */
#define PIN_MOTOR_RIGHT_UP 14  // ①IN1 オレンジ
#define PIN_MOTOR_RIGHT_DN 12  // ②IN2　黄色

/* defined the left motor control pins */
#define PIN_MOTOR_LEFT_UP 0 // ③IN3　緑
#define PIN_MOTOR_LEFT_DN 2 // ④IN4　青

/* defined two arrays with a list of pins for each motor */
unsigned char RightMotor[3] = 
  {PIN_MOTOR_RIGHT_UP, PIN_MOTOR_RIGHT_DN};
unsigned char LeftMotor[3] = 
  {PIN_MOTOR_LEFT_UP, PIN_MOTOR_LEFT_DN};

// EEPROM保存データ
#define MAGIC_NO (0x12345678L)
struct {
  unsigned long magic_no;
  int8_t adjust; // モータ左右バランス調整
} eeprom_data;

/* defined the LED pin */
#define PIN_LED 2
/*
   speed control of the motor
   motor - pointer to an array of pins
   v - motor speed can be set from -100 to 100
*/

void Wheel (unsigned char * motor, int v)
{
  if (v>100) v=100;
  if (v<-100) v=-100;
  if (v>0) {
    // 正転
    analogWrite(motor[0], abs(v*2.55));
    analogWrite(motor[1], 0);
  }
  else if (v<0) {
    // 逆転
    analogWrite(motor[0], 0);
    analogWrite(motor[1], abs(v*2.55));
  }
  else {
    //停止
    analogWrite(motor[0], 0);
    analogWrite(motor[1], 0);
  }
}

// ジョイスティックコントロール
#define STOP 0
#define FORWARD 1
#define BACK 2
#define RIGHT 3
#define LEFT 4
#define DELAY 5
int control_xy( int x, int y)
{
  static int delay = 0;
  static int state = 0; // 0:stop, 1:Forward, 2:Back, 3:(Turn)Right, 4:(Turn)Left, 5:delay
  static int old_state = 0;
  static unsigned long t = 0;
  int d=0;

  if (t != 0){
    d = millis() - t; // 前回の呼び出しからの経過時間(ミリ秒)
  }
  t = millis(); // 現在時刻
  
  if ( state == DELAY ){
    delay -= d; // 待機時間減算
    if ( delay > 0 ){
      // 待機時間中はモーターを止める
      //Wheel (RightMotor, 0);
      //Wheel (LeftMotor, 0);
      //sprintf(RemoteXY.text_3, "**DELAY**\n");
      return state;
    }
    //sprintf(RemoteXY.text_3, "\n");
    // 待機期間終了
    state = old_state;
  }

  // ジョイスティック指示
  if ( x == 0 && y == 0){
    // ジョイスティックを手放した状態
    state = STOP;
  }
  else{
    if ( y > 30 /*&& abs(x) < 90*/ ){
      state = FORWARD;
    }
    else if ( y < -30 /*&& abs(x) < 90*/ ){
      state = BACK;
    }
    else{
      if (x > 20 ){
        state = RIGHT;
      }
      else if ( x < -20 ){
        state = LEFT;
      }
      else{
        // 微妙なところはモーターだけ止める
        Wheel (RightMotor, 0);
        Wheel (LeftMotor, 0);
        return state;  
      }
    }  
  }
  if ( old_state != state && state != STOP && old_state != STOP){
    // 動作が切り替わるときに0.5秒の待機期間を設ける
    delay = 500;
    old_state = state;
    state = DELAY;
    return state;
  }

  // モーターバランス調整パラメータ
  // eeprom_data.adjust -100 .. +100
  x += (eeprom_data.adjust / 10);
  switch(state){
    case 0: // stop
      delay = 0;
      Wheel (RightMotor, 0);
      Wheel (LeftMotor, 0);
      break;
    case FORWARD:
    case BACK:
      Wheel (RightMotor, y + x/2); // ジョイスティックx方向の影響度を小さくする（曲がりにくい）
      Wheel (LeftMotor, y - x/2);
      break;
    case LEFT:
    case RIGHT:
      Wheel (RightMotor, (y + x) );// 旋回速度を落とす
      Wheel (LeftMotor, (y - x) );
      break;
  }
  old_state = state;
  return state;
}

void setup() 
{
  Serial.begin(115200);

  /* initialization pins */
  pinMode (PIN_MOTOR_RIGHT_UP, OUTPUT);
  pinMode (PIN_MOTOR_RIGHT_DN, OUTPUT);
  pinMode (PIN_MOTOR_LEFT_UP, OUTPUT);
  pinMode (PIN_MOTOR_LEFT_DN, OUTPUT);
  pinMode (PIN_LED, OUTPUT);
  
  RemoteXY_Init(); 
  
  RemoteXY.level_right = 50;
  RemoteXY.level_left = 50;

  // EEPROM初期化
  EEPROM.begin(sizeof(eeprom_data));
  EEPROM.get(0, eeprom_data);
  if (eeprom_data.magic_no != MAGIC_NO){
    // 保存値を初期化する
    eeprom_data.magic_no = MAGIC_NO;
    eeprom_data.adjust = 0; //center
    EEPROM.put(0, eeprom_data); // EEPROMを更新する
    EEPROM.commit();
  }
  // EEPROMに保存されている値でUIを更新する
  RemoteXY.adjust = eeprom_data.adjust;
}

const char* txt [] = {
  "STOP","FORWARD", "BACK", "RIGHT", "LEFT", "DELAY"
};

void loop() 
{ 
  RemoteXY_Handler ();

  if (RemoteXY.adjust != eeprom_data.adjust){
    eeprom_data.adjust = RemoteXY.adjust;
    EEPROM.put(0, eeprom_data); // EEPROMを更新する
    EEPROM.commit();
  }
  int state = control_xy(RemoteXY.joystick_1_x, RemoteXY.joystick_1_y);
  sprintf(RemoteXY.text_1, "%s", txt[state]);

  RemoteXY.level_left = 50 + (RemoteXY.joystick_1_y + RemoteXY.joystick_1_x) / 2;
  RemoteXY.level_right = 50 + (RemoteXY.joystick_1_y - RemoteXY.joystick_1_x) / 2;
  
  Serial.printf("x=%d, y=%d\n",RemoteXY.joystick_1_x, RemoteXY.joystick_1_y);
}