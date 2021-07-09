#include <SoftwareSerial.h>
#include "stdio.h" //Standard Input Output header file

SoftwareSerial softSerial(2, 3); // RX,TX for Arduino
SoftwareSerial bridgeSerial(-1, 13); // RX,TX for web function
#define doorMotor1 11
#define doorMotor2 10
#define IR 8
#define GAS 9
#define BUZZER 12
#define  myLDR A5   //pin A5 analog pin for the LDR
#define SecurityLight 4

const char* allowedNumbers = "08028404987,08096444378";
const char* adminNumber = "08028404987";
const char* senderEmail = "security@electronic.abu.com";
const char* recipientEmail = "medinatapampa@yahoo.com";
const char* apiToken = "_";
int8_t counter = 0;
char requestBuffer[128];
int myLDRthreshold = 150; //setting a threshold value of 150
int LDR = HIGH;

//to place a call by the system
void Call() {
  Serial.println("Calling");             //Show this message on serial monitor
  softSerial.print(F("ATD"));
  softSerial.print(adminNumber);
  softSerial.print(F(";\r\n"));               //
  delay(5000);
  softSerial.print(F("ATH"));         //hang up
}

// function to send SMS
void SendSMS(const char* sms){
  Serial.println("Sending SMS...");      //Show this message on serial monitor
  softSerial.print("AT+CMGF=1\r");                   //Set the module to SMS mode
  delay(100);
  softSerial.print("AT+CMGS=\"");  //Your phone number
  softSerial.print(adminNumber);
  softSerial.print("\"\r");
  delay(500);
  softSerial.print(sms); //This is the text to send to the phone number,
  delay(500);
  softSerial.print((char) 26);       // (decimal equivalent of ctrl+z cut)
  delay(500);
  softSerial.println();
  Serial.println("Text Sent.");
  delay(2000);
}

void setup() {
  pinMode(IR, INPUT);       //it is high, when pressed, it will be low/
  pinMode(GAS, INPUT);      //it is high, when pressed, it will be low/
  pinMode(BUZZER, OUTPUT);
  pinMode(doorMotor1, OUTPUT);
  pinMode(doorMotor2, OUTPUT);
  pinMode(myLDR, INPUT);      //LDRpin as input
  bridgeSerial.begin(115200);
  softSerial.begin(9600); //Module baud rate,it depends on the version

  Serial.begin(9600);

  Serial.println("Starting...");
  delay(1000);
  softSerial.println("AT\r");     //Checking communication between the module and computer
  delay(1000);
  softSerial.print("AT+CMGF=1\r");    //Message format setup
  delay(1000);
  softSerial.print("AT+CMGDA=\"");      //Delete message all message
  softSerial.println("DEL ALL\"");
  Serial.println("all sms deleted");
  delay(1000);
  softSerial.print("AT+CNMI=2,2,0,0,0\r"); //New message indications
  delay(1000);
  softSerial.print("AT+CLIP=1\r"); // IN phone calls,  the presence of a +CLIP commands indicate an incoming call
  delay(1000);

  Serial.println("SIM Module is OK");
}

void OpenDoor(){
  digitalWrite(doorMotor1, HIGH);
  digitalWrite(doorMotor2, LOW);
  Serial.println("Door is open");
  delay(1000);
  softSerial.print("AT+CMGDA=\"");
  softSerial.println("DEL ALL\"");
  Serial.println("all sms deleted.");
  delay(2000);
  digitalWrite(doorMotor1, LOW);
  digitalWrite(doorMotor2, LOW);
  counter = 0;
}

void CloseDoor(){
  digitalWrite(doorMotor1, LOW);
  digitalWrite(doorMotor2, HIGH);
  Serial.println("Door is closed");
  delay(1000);
  softSerial.print("AT+CMGDA=\"");
  softSerial.println("DEL ALL\"");
  Serial.println("all sms deleted.");
  delay(2000);
  digitalWrite(doorMotor1, LOW);
  digitalWrite(doorMotor2, LOW);
  counter = 0;
}

String PhoneNumberNormalize(String number){
  if (number.indexOf("+234") == 0){
    number = "0" + number.substring(4);
  }
  return number;
}

uint32_t lastCall = 0;
String smsSender;
uint32_t lastGasSMSTime;
uint32_t lastPIRSMSTime;

void GasLeakageHandle(){
  if (counter < 0)
    return;
  if (lastGasSMSTime == 0 || (millis() - lastGasSMSTime > 30000)){
    Serial.print("Gas leakage count: ");
    Serial.println(counter+1);
    lastGasSMSTime = millis();
    if (counter < 3){
      digitalWrite(BUZZER, HIGH);
      Serial.println("Gas leakage...");          //Shows this message on the serial monitor
      delay(1000);                              //Small delay to avoid detecting the button press many times
      SendSMS("Gas leakage suspected, please inform the appropriate authority");                          //SMS function is called
      BridgeSendEmail(apiToken, senderEmail, recipientEmail, "Gas", "Gas leakage suspected, please inform the appropriate authority"); //EMAIL MESSAGE
      delay(2000);
      Call();
      digitalWrite(BUZZER, LOW);
      counter++;
    }else {
      OpenDoor();
      counter = -1;
      SendSMS("Door opened due to gas leakage");
      BridgeSendEmail(apiToken, senderEmail, recipientEmail, "Gas", "Door opened due to gas leakage");
    }
  }
}
///////////////////////////////////////handles pir
void PIRandLDR(){
  if (counter < 0)
    return;
  if (lastPIRSMSTime == 0 || (millis() - lastPIRSMSTime > 30000)){
    Serial.print("Intruder count: ");
    Serial.println(counter+1);
    lastPIRSMSTime = millis();
    if (counter < 3){
      digitalWrite(BUZZER, HIGH);
      digitalWrite(SecurityLight, HIGH);
      Serial.println("INTRUSION SUSPECTED...");          //Shows this message on the serial monitor
      //delay(1000);                              //Small delay to avoid detecting the button press many times
      SendSMS("Intrusion suspected, please inform the appropriate authority");                          //SMS function is called
      BridgeSendEmail(apiToken, senderEmail, recipientEmail, "Intrusion", "Intrusion suspected, please inform the appropriate authority"); //EMAIL MESSAGE
      delay(5000);
      digitalWrite(BUZZER, LOW);
      delay(5000);
      digitalWrite(SecurityLight, LOW);
      counter++;
    } else {
      Call();
      digitalWrite(SecurityLight, HIGH);
      counter = -1;
      BridgeSendEmail(apiToken, senderEmail, recipientEmail, "Gas", "Door opened due to gas leakage");
    }
  }
}
///////////////////////////
//setup for email functionality
uint8_t put8(uint8_t c, uint8_t sum){
//      Serial.print("0x");
//      Serial.print((int)c, 16);
//      Serial.print(", ");
//
  bridgeSerial.write(c);
  sum+=c;
  return sum;
}

uint8_t put16(uint16_t c, uint8_t sum){
  sum = put8(c&0xFF, sum);
  sum = put8((c>>8)&0xFF, sum);
  return sum;
}

uint8_t putLen(uint16_t len, uint8_t sum){
  if (len < 128)
    sum = put8(len, sum);
  else{
    sum = put8((len&0x7F) | 0x80, sum);
    sum = put8(len>>7, sum);
  }
  return sum;
}

void BridgeSend(uint8_t type, const uint8_t* data, uint16_t len, bool sendLength){
  put16(0x23B7, 0);
  uint8_t sum = 0;
  uint16_t l = len+1+(sendLength?1:0);
  sum = putLen(l, sum);
  sum = put8(type, sum);
  if (sendLength){
    sum = putLen(len, sum);
  }
  while(len--){
    sum = put8(*data, sum);
    data++;
  }
  put8((0xFF^sum)+1, 0);
}

void BridgeSendStatus(const uint8_t* data, uint16_t len){
  BridgeSend(1, data, len, false);
}

void BridgeSetName(const char* name){
  BridgeSend(2, (uint8_t*)name, strlen(name), true);
}

int BridgePack(const char* value, int i){
  uint8_t len = strlen(value);
  requestBuffer[i++] = len;
  strcpy(&requestBuffer[i], value);
  i+= len;
  return i;
}

void BridgeSendEmail(const char* token, const char* sender, const char* recipient, const char* title, const char* message){
  uint16_t len = BridgePack(token, 0);
  len = BridgePack(sender, len);
  len = BridgePack(recipient, len);
  len = BridgePack(title, len);
  len = BridgePack(message, len);
//  Serial.print("Len: ");
//  Serial.print(len);
  BridgeSend(128, (const uint8_t*)requestBuffer, len, false);
}

void loop() {
  String d;

  while (softSerial.available()) {
    delay(3);
    d = softSerial.readString();
    Serial.print("> ");
    Serial.println(d);
  }

  bool ir = digitalRead(IR); // We are constantly reading the input state
  bool gas = digitalRead(GAS); // We are constantly reading the input state
  ////////////////////////////////
  
 if (analogRead(myLDR)>= myLDRthreshold) {
    LDR = LOW;
    Serial.print("pin LOW A5: ");
    Serial.println(myLDR);
    delay(5000);
  }
  if (!(ir || LDR) == HIGH){
     PIRandLDR();   
  }
  if ((!ir && LDR) == HIGH){
    Serial.println("Someone at the door..."); //Shows this message on the serial monitor
    SendSMS("Someone at the door");                          //And this function is called
    delay(5000);
    BridgeSendEmail(apiToken, senderEmail, recipientEmail, "IR Sensor", "Someone at the door");
  }
  if (gas == LOW) {
    GasLeakageHandle();
  }

  if (d.indexOf("+CMT") >= 0){
    int start = d.indexOf("\"");
    int end  = d.indexOf("\"", start+1);
    smsSender = d.substring(start+1, end);
    smsSender = PhoneNumberNormalize(smsSender);
    Serial.print("SMS from: ");
    Serial.println(smsSender);
  }
  //Serial.println(d);
  if (d.indexOf("doorOP") >= 0) {
    if (String(allowedNumbers).indexOf(smsSender) >= 0){
      OpenDoor();
    }else{
      sprintf(requestBuffer, "Security Alert! SMS sent by an UNKNOWN INDIVIDUAL to OPEN DOOR: %s", smsSender.c_str());
      SendSMS(requestBuffer);
    }
  }

  else if (d.indexOf("doorCL") >= 0) {
    if (String(allowedNumbers).indexOf(smsSender) >= 0){
      CloseDoor();
    }else{
      sprintf(requestBuffer, "Security Alert! SMS sent by an UNKNOWN INDIVIDUAL to CLOSE DOOR: %s", smsSender.c_str());
      Serial.println(requestBuffer);
      SendSMS(requestBuffer);
    }
  }
  
// conditional statememt for system call recieving
      lastCall = millis();
    }else{
  if (d.indexOf("+CLIP") >= 0 && (lastCall == 0 || (millis() - lastCall > 1*30000))){ // IN phone calls,  the presence of a +CLIP commands indicate an incoming call

    int start = d.indexOf("\"");
    int end  = d.indexOf("\"", start+1);
    String phoneNumber = d.substring(start+1, end);
    phoneNumber = PhoneNumberNormalize(phoneNumber);
    softSerial.println("ATH");                                                        // configure hang up
    delay(2000);
    if (String(allowedNumbers).indexOf(phoneNumber) >= 0){
      Serial.println("Accredited member calling");
      SendSMS("Accredited member calling");
      OpenDoor();
      delay(5000);
      CloseDoor();
      sprintf(requestBuffer, "Unknown individual calling: %s", phoneNumber.c_str());
      Serial.println(requestBuffer);
      SendSMS(requestBuffer);
    }
  }
}
