#include <SPI.h>
#include <Wire.h>
#include <RFID.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>

const int STEPPER_PIN_1 = 4;
const int STEPPER_PIN_2 = 5;
const int STEPPER_PIN_3 = 6;
const int STEPPER_PIN_4 = 7;

const int STEPS_PER_REVOLUTION = 2048;
const float DEGREES_PER_STEP = 360.0 / STEPS_PER_REVOLUTION;
const int TARGET_DEGREES = 40;

int Buzzer = 16;
int RedLed = 17;
int GreenLed = 18;
uint32_t tagID = 0;
int ExitPushButton = 3;
int EntracePushButton = 19;
int EmergencyPushButton = 2;
volatile boolean rfidDetected = false;

volatile boolean buttonState1 = HIGH;
volatile boolean lastButtonState1 = HIGH;
volatile unsigned long lastDebounceTime1 = 0;
volatile unsigned long debounceDelay1 = 50;
//volatile boolean objectDetected = false;
int send = 0;

volatile boolean buttonState2 = HIGH;
volatile boolean lastButtonState2 = HIGH;
volatile unsigned long lastDebounceTime2 = 0;
volatile unsigned long debounceDelay2 = 50;

volatile boolean buttonState3 = HIGH;
volatile boolean lastButtonState3 = HIGH;
volatile unsigned long lastDebounceTime3 = 0;
volatile unsigned long debounceDelay3 = 50;

#define SDA_DIO 9
#define RESET_DIO 8

enum State {
  NOT_OCCUPIED,
  OCCUPIED,
  EMERGENCY,
};

State currentState = NOT_OCCUPIED;

SoftwareSerial mySerial(24, 22);
RFID RC522(SDA_DIO, RESET_DIO);
LiquidCrystal_I2C lcd(0x3F, 16, 2);

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(Buzzer, OUTPUT);
  pinMode(RedLed, OUTPUT);
  pinMode(GreenLed, OUTPUT);
  pinMode(STEPPER_PIN_1, OUTPUT);
  pinMode(STEPPER_PIN_2, OUTPUT);
  pinMode(STEPPER_PIN_3, OUTPUT);
  pinMode(STEPPER_PIN_4, OUTPUT);
  pinMode(EntracePushButton, INPUT_PULLUP);
  pinMode(ExitPushButton, INPUT_PULLUP);
  pinMode(EmergencyPushButton, INPUT_PULLUP);

  mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
  updateSerial();

  attachInterrupt(digitalPinToInterrupt(EntracePushButton), buttonInterrupt1, FALLING);
  attachInterrupt(digitalPinToInterrupt(ExitPushButton), buttonInterrupt2, FALLING);
  attachInterrupt(digitalPinToInterrupt(EmergencyPushButton), buttonInterrupt3, FALLING);

  digitalWrite(GreenLed, HIGH);
  digitalWrite(RedLed, LOW);
  digitalWrite(Buzzer, LOW);

  SPI.begin();
  RC522.init();

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("SYSTEM");
  lcd.setCursor(1, 1);
  lcd.print("INITIALIZATION");
  delay(2000);
}

void loop() {
  //checkButtonPresses();
  switch (currentState) {
    case NOT_OCCUPIED:
      digitalWrite(RedLed, LOW);
      digitalWrite(GreenLed, HIGH);
      digitalWrite(Buzzer, LOW);
      send = 0;
      idle();
      break;

    case OCCUPIED:
      digitalWrite(RedLed, HIGH);
      digitalWrite(GreenLed, LOW);
      digitalWrite(Buzzer, LOW);
      toilet_occupied();
      break;

    case EMERGENCY:
      digitalWrite(RedLed, HIGH);
      digitalWrite(GreenLed, HIGH);
      digitalWrite(Buzzer, HIGH);
      Emergency();
      break;
  }
}
void idle() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("TOILET STATUS");
  lcd.setCursor(5, 1);
  lcd.print("NORMAL");

  buttonInterrupt1();
  buttonInterrupt2();
  buttonInterrupt3();
  delay(500);
}
void toilet_occupied() {
  buttonInterrupt1();
  buttonInterrupt2();
  buttonInterrupt3();
}
void Emergency() {
  readTag();
  if (rfidDetected) {
    rotateClockwise();
    currentState = NOT_OCCUPIED;
    rfidDetected = false;
    Serial.println("NOT_OCCUPIED");
  }
}
void readTag() {
  if (RC522.isCard()) {
    RC522.readCardSerial();
    Serial.println("Card detected:");
    currentState = NOT_OCCUPIED;
    for (int i = 0; i < 5; i++) {
      Serial.print(RC522.serNum[i], DEC);
    }
    Serial.println();
    Serial.println();

    rfidDetected = true;
  }
  delay(500);
}
void buttonInterrupt1() {
  if (millis() - lastDebounceTime1 > debounceDelay1) {
    buttonState1 = digitalRead(EntracePushButton);
    if (buttonState1 == LOW && lastButtonState1 == HIGH) {
      Serial.println("Button 1 Pressed!");
      if (currentState == NOT_OCCUPIED) {
        rotateClockwise();
        currentState = OCCUPIED;
      }
    }
    lastButtonState1 = buttonState1;
    lastDebounceTime1 = millis();
  }
}
void buttonInterrupt2() {
  if (millis() - lastDebounceTime2 > debounceDelay2) {
    buttonState2 = digitalRead(ExitPushButton);
    if (buttonState2 == LOW && lastButtonState2 == HIGH) {
      Serial.println("Button 2 Pressed!");
      if (currentState == OCCUPIED) {
        rotateClockwise();
        currentState = NOT_OCCUPIED;
      }
    }
    lastButtonState2 = buttonState2;
    lastDebounceTime2 = millis();
  }
}
void buttonInterrupt3() {
  if (millis() - lastDebounceTime3 > debounceDelay3) {
    buttonState3 = digitalRead(EmergencyPushButton);
    if (buttonState3 == LOW && lastButtonState3 == HIGH) {
      Serial.println("Button 3 Pressed!");
      if (currentState == OCCUPIED) {
        if (send == 0) {
          SendMessage();
          send = 1;
        }
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("EMERGENCY ALERT");
        lcd.setCursor(2, 1);
        lcd.print("TOILET NO: 2");
        currentState = EMERGENCY;
        delay(1000);
        readTag();  // Check for RFID tag when transitioning to EMERGENCY
      }
    }
    lastButtonState3 = buttonState3;
    lastDebounceTime3 = millis();
  }
}
void checkButtonPresses() {
  buttonInterrupt1();
  buttonInterrupt2();
  buttonInterrupt3();
}
void SendMessage() {
  mySerial.println("AT");  //Once the handshake test is successful, it will back to OK
  updateSerial();

  mySerial.println("AT+CMGF=1");  // Configuring TEXT mode
  updateSerial();
  mySerial.println("AT+CMGS=\"+254748613509\"");  //change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  mySerial.print("EMERGENCY ALERT TOILET NO: 2");  //text content
  updateSerial();
  mySerial.write(26);
}
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    mySerial.write(Serial.read());  //Forward what Serial received to Software Serial Port
  }
  while (mySerial.available()) {
    Serial.write(mySerial.read());  //Forward what Software Serial received to Serial Port
  }
}
void rotateClockwise() {
  int targetSteps = TARGET_DEGREES / DEGREES_PER_STEP;
  const int stepSequence[8][4] = {
    {HIGH, LOW, LOW, LOW},
    {HIGH, HIGH, LOW, LOW},
    {LOW, HIGH, LOW, LOW},
    {LOW, HIGH, HIGH, LOW},
    {LOW, LOW, HIGH, LOW},
    {LOW, LOW, HIGH, HIGH},
    {LOW, LOW, LOW, HIGH},
    {HIGH, LOW, LOW, HIGH}
  };
  for (int i = 0; i < targetSteps; i++) {
    for (int j = 0; j < 8; j++) {
      digitalWrite(STEPPER_PIN_1, stepSequence[j][0]);
      digitalWrite(STEPPER_PIN_2, stepSequence[j][1]);
      digitalWrite(STEPPER_PIN_3, stepSequence[j][2]);
      digitalWrite(STEPPER_PIN_4, stepSequence[j][3]);
      delayMicroseconds(1000);
    }
  }
  delay(500);
  for (int i = 0; i < targetSteps; i++) {
    for (int j = 7; j >= 0; j--) {
      digitalWrite(STEPPER_PIN_1, stepSequence[j][0]);
      digitalWrite(STEPPER_PIN_2, stepSequence[j][1]);
      digitalWrite(STEPPER_PIN_3, stepSequence[j][2]);
      digitalWrite(STEPPER_PIN_4, stepSequence[j][3]);
      delayMicroseconds(1000);
    }
  }
  digitalWrite(STEPPER_PIN_1, LOW);
  digitalWrite(STEPPER_PIN_2, LOW);
  digitalWrite(STEPPER_PIN_3, LOW);
  digitalWrite(STEPPER_PIN_4, LOW);
}