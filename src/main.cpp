#include <Arduino.h>
#include <CAN.h>

// BASED ON: https://raw.githubusercontent.com/Lukilink/Cruise_ECU/master/a_smarter_cruise_ECU.ino


//______________BUTTONS AND SWITCHES
int button3 = 9;  // SPD+
int button2 = 6;  // SPD-
int button1 = 8;  // ON/OFF

int pedal = A4; // UNUSED
boolean pedalstate = false;


int buttonstate3;
int lastbuttonstate3;
unsigned long debounceTime3 = 0;

int buttonstate2;
int lastbuttonstate2;
unsigned long debounceTime2 = 0;

int buttonstate1;
int lastbuttonstate1;
unsigned long debounceTime1 = 0;

//______________VALUES SEND ON CAN
unsigned long lastCanSend=0L;

boolean OP_ON = false;
uint8_t set_speed = 0x0;
uint8_t last_set_speed = 90;

int gas_pedal_state = 0; // TODO: Remove gas_pedal_state
int brake_pedal_state = 0; // TODO: Remove brake_pedal_state
double average = 0; 
boolean blinker_left = true;
boolean blinker_right = true;

//______________FOR SMOOTHING SPD
const int numReadings = 160;
float readings[numReadings];
int readIndex = 0; 
double total = 0;

//______________FOR READING VSS SENSOR
const byte interruptPin = 3;
int inc = 0;
int half_revolutions = 0;
int spd;
unsigned long lastmillis;
unsigned long duration;
uint8_t encoder = 0;

void rpm() {
  half_revolutions++;
  if (encoder > 255)
  {
    encoder = 0;
  }
  encoder++;
}

//TOYOTA CAN CHECKSUM
int can_cksum (uint8_t *dat, uint8_t len, uint16_t addr) {
  uint8_t checksum = 0;
  checksum = ((addr & 0xFF00) >> 8) + (addr & 0x00FF) + len + 1;
  //uint16_t temp_msg = msg;
  for (int ii = 0; ii < len; ii++) {
    checksum += (dat[ii]);
  }
  return checksum;
}


void setup() {
  Serial.begin(115200);
  CAN.setClockFrequency(8E6);

  if (!CAN.begin(500E3)) {
    while (1) {
      Serial.println("Starting CAN failed!");
    }
  }

  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), rpm, FALLING);
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);

  //______________initialize smoothing inputs
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

void loop() {
  
  //______________READ SPD SENSOR
  attachInterrupt(1,rpm, FALLING);

  if (half_revolutions >= 1) {
      detachInterrupt(1);
      duration = (micros() - lastmillis);
      spd = half_revolutions * (0.000135 / (duration * 0.000001)) * 3600;
      lastmillis = micros(); 
      half_revolutions = 0;
      attachInterrupt(1, rpm, FALLING);
    }

  //______________SMOOTH SPD TO AVERAGE
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = spd;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  average = total / numReadings;
  // send it to the computer as ASCII digits

  //______________READING BUTTONS AND SWITCHES
  pedalstate = digitalRead(pedal);
  buttonstate3 = digitalRead(button3);
  buttonstate2 = digitalRead(button2);
  buttonstate1 = digitalRead(button1);

  if (buttonstate1 != lastbuttonstate1) {
    debounceTime1=millis();
  }
  if (buttonstate2 != lastbuttonstate2) {
    debounceTime2=millis();
  }
  if (buttonstate3 != lastbuttonstate3) {
    debounceTime3=millis();
  }
  
  lastbuttonstate1 = buttonstate1;
  lastbuttonstate2 = buttonstate2;
  lastbuttonstate3 = buttonstate3;


  if (buttonstate1==LOW && debounceTime1!=0 && (millis()-debounceTime1>=50L || millis()<debounceTime1)) {
    debounceTime1=0;
    if (OP_ON == true)
    {
      OP_ON = false;
    }
    else if(OP_ON == false)
    {
      OP_ON = true;
      set_speed = last_set_speed;
      
    }
    lastCanSend=0L;
  }

  if (buttonstate2==LOW && debounceTime2!=0 && (millis()-debounceTime2>=50L || millis()<debounceTime2)) {
    debounceTime2=0;
    set_speed -= 5;
    last_set_speed=set_speed;
  }

  if (buttonstate3==LOW && debounceTime3!=0 && (millis()-debounceTime3>=50L || millis()<debounceTime3)) {
    debounceTime3=0;
    set_speed += 5;
    last_set_speed=set_speed;
  }
  

/*   if (buttonstate3 == LOW)
    {
    blinker_right = false;
    }
    else
    {
    blinker_right = true;
    }

  if (buttonstate2 == LOW)
    {
    blinker_left = false;
    }
    else
    {
    blinker_left = true;
    } 

      
  if (pedalstate == LOW)
    {

    }*/



  if (millis()-lastCanSend>=15L || millis()<lastCanSend) {
    lastCanSend=millis();
    //______________SENDING_CAN_MESSAGES
    //0x1d2 msg PCM_CRUISE
    uint8_t dat[8];
    dat[0] = (OP_ON << 5) & 0x20 | (!gas_pedal_state << 4) & 0x10;
    dat[1] = 0x0;
    dat[2] = 0x0;
    dat[3] = 0x0;
    dat[4] = 0x0;
    dat[5] = 0x0;
    dat[6] = (OP_ON << 7) & 0x80;
    dat[7] = can_cksum(dat, 7, 0x1d2);
    CAN.beginPacket(0x1d2);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat[ii]);
    }
    CAN.endPacket();

    //0x1d3 msg PCM_CRUISE_2
    uint8_t dat2[8];
    dat2[0] = 0x0;
    dat2[1] = (OP_ON << 7) & 0x80 | 0x28;
    dat2[2] = set_speed;
    dat2[3] = 0x0;
    dat2[4] = 0x0;
    dat2[5] = 0x0;
    dat2[6] = 0x0;
    dat2[7] = can_cksum(dat2, 7, 0x1d3);
    CAN.beginPacket(0x1d3);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat2[ii]);
    }
    CAN.endPacket();

    //0xaa msg defaults 1a 6f WHEEL_SPEEDS
    average=25;
    uint8_t dat3[8];
    uint16_t wheelspeed = 0x1a6f + (average * 100);
    dat3[0] = (wheelspeed >> 8) & 0xFF;
    dat3[1] = (wheelspeed >> 0) & 0xFF;
    dat3[2] = (wheelspeed >> 8) & 0xFF;
    dat3[3] = (wheelspeed >> 0) & 0xFF;
    dat3[4] = (wheelspeed >> 8) & 0xFF;
    dat3[5] = (wheelspeed >> 0) & 0xFF;
    dat3[6] = (wheelspeed >> 8) & 0xFF;
    dat3[7] = (wheelspeed >> 0) & 0xFF;
    CAN.beginPacket(0xaa);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat3[ii]);
    }
    CAN.endPacket();

    //0x3b7 msg ESP_CONTROL
    uint8_t dat5[8];
    dat5[0] = 0x0;
    dat5[1] = 0x0;
    dat5[2] = 0x0;
    dat5[3] = 0x0;
    dat5[4] = 0x0;
    dat5[5] = 0x0;
    dat5[6] = 0x0;
    dat5[7] = 0x08;
    CAN.beginPacket(0x3b7);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat5[ii]);
    }
    CAN.endPacket();

    //0x620 msg STEATS_DOORS
    uint8_t dat6[8];
    dat6[0] = 0x10;
    dat6[1] = 0x0;
    dat6[2] = 0x0;
    dat6[3] = 0x1d;
    dat6[4] = 0xb0;
    dat6[5] = 0x40;
    dat6[6] = 0x0;
    dat6[7] = 0x0;
    CAN.beginPacket(0x620);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat6[ii]);
    }
    CAN.endPacket();

    // 0x3bc msg GEAR_PACKET
    uint8_t dat7[8];
    dat7[0] = 0x0;
    dat7[1] = 0x0;
    dat7[2] = 0x0;
    dat7[3] = 0x0;
    dat7[4] = 0x0;
    dat7[5] = 0x80;
    dat7[6] = 0x0;
    dat7[7] = 0x0;
    CAN.beginPacket(0x3bc);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat7[ii]);
    }
    CAN.endPacket();

    // 0x2c1 msg GAS_PEDAL
    uint8_t dat10[8];
    dat10[0] = (!gas_pedal_state << 3) & 0x08;
    dat10[1] = 0x0;
    dat10[2] = 0x0;
    dat10[3] = 0x0;
    dat10[4] = 0x0;
    dat10[5] = 0x0;
    dat10[6] = 0x0;
    dat10[7] = 0x0;
    CAN.beginPacket(0x2c1);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat10[ii]);
    }
    CAN.endPacket();

    //0x224 msg fake brake module
    uint8_t dat11[8];
    dat11[0] = 0x0;
    dat11[1] = 0x0;
    dat11[2] = 0x0;
    dat11[3] = 0x0;
    dat11[4] = 0x0;
    dat11[5] = 0x0;
    dat11[6] = 0x0;
    dat11[7] = 0x8;
    CAN.beginPacket(0x224);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat11[ii]);
    }
    CAN.endPacket();

    //0x614 msg steering_levers
    uint8_t dat614[8];
    dat614[0] = 0x29;
    dat614[1] = 0x0;
    dat614[2] = 0x01;
    dat614[3] = (blinker_left << 5) & 0x20 |(blinker_right << 4) & 0x10;
    dat614[4] = 0x0;
    dat614[5] = 0x0;
    dat614[6] = 0x76;
    dat614[7] = can_cksum(dat614, 7, 0x614);
    CAN.beginPacket(0x614);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat614[ii]);
    }
    CAN.endPacket(); 


    //0x262 fake EPS_STATUS
    uint8_t dat8[8];
    dat8[0] = 0x0;
    dat8[1] = 0x0;
    dat8[2] = 0x0;
    dat8[3] = 0x3;
    dat8[4] = 0x6c;

    CAN.beginPacket(0x262);
    for (int ii = 0; ii < 5; ii++) {
      CAN.write(dat8[ii]);
    }
    CAN.endPacket();

    //0x260 fake STEER_TORQUE_SENSOR  
    uint8_t dat9[8];
    dat9[0] = 0x08;
    dat9[1] = 0xff;
    dat9[2] = 0xfb;
    dat9[3] = 0x0;
    dat9[4] = 0x0;
    dat9[5] = 0xff;
    dat9[6] = 0xdc;
    dat9[7] = 0x47;
      
    CAN.beginPacket(0x260);
    for (int ii = 0; ii < 8; ii++) {
      CAN.write(dat9[ii]);
    }
    CAN.endPacket();
  }
}