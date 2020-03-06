/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

// Setup the BLE characteristics
#define DigitalPinService BLEUUID((uint16_t)0x1815)
BLECharacteristic DigitalPinCharacteristic(BLEUUID((uint16_t)0x2A56), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
BLEDescriptor PresentPin1Descriptor(BLEUUID((uint16_t)0x2901));

// Setup the LCD
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int PIN_LID_CLOSED = 23;
const int PIN_PHONE_PRESENT = 22;
const int DISPLAY_MOTIVATION = -1;

int _presentTimer = 0;
unsigned long _sessionStartTime = 0;
bool _BLEClientConnected = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Present device...");

  InitBLE();
  Serial.println("Bluetooth server started.");

  pinMode(PIN_LID_CLOSED, INPUT);
  Serial.println("Lid sensor connected.");
  pinMode(PIN_PHONE_PRESENT, INPUT);
  Serial.println("Phone sensor connected.");

  if (!EEPROM.begin(4)) {
    Serial.println("EEPROM connected failed."); 
    delay(1000000);
  }
  Serial.println("EEPROM connected.");

  lcd.begin(16, 2);
  Serial.println("LCD connected.");

  _presentTimer = loadTimer();
  Serial.println("Storage loaded.");
  
  Serial.println("Started Present device.");
}

void loop() {

  // Check if the phone is in the box with the lid closed
  if (lidClosed() && phonePresent()) {
    // Make sure the timer is running
    startTimer();
    // Update the display
    updateDisplay(getPresentTimer());
    // Store the new value
    storeTimer(getPresentTimer());
  } else {
    // Stop the timer
    stopTimer();
    // Motivate them to put the phone in the box
    updateDisplay(DISPLAY_MOTIVATION);
  }
  
  // If the phone is connected via bluetooth, update it.
  if (_BLEClientConnected) {
    updateBLEDevice(getPresentTimer());
  }
  
  delay(100);
}

bool lidClosed() {
  return digitalRead(PIN_LID_CLOSED);
}

bool phonePresent() {
  return digitalRead(PIN_PHONE_PRESENT);  
}

void startTimer() {
  if (!timerRunning()) {
    _sessionStartTime = millis();
  }
}

bool timerRunning() {
  return _sessionStartTime != 0;  
}

int getPresentTimer() {
  unsigned long secondsElapsed = 0;
  // If the timer is running.
  if (timerRunning()) {
    unsigned long elapsedTime = millis() - _sessionStartTime;
    secondsElapsed = elapsedTime / 1000;
  }
  return _presentTimer + secondsElapsed;
}

void stopTimer() {
  if (timerRunning()) {
    // Store the new value
    unsigned long elapsedTime = millis() - _sessionStartTime;
    unsigned long secondsElapsed = elapsedTime / 1000;
    _presentTimer += secondsElapsed;
    storeTimer(_presentTimer);
    
    _sessionStartTime = 0;
  }
}

void updateDisplay(int timerValue) {
  if (timerValue == DISPLAY_MOTIVATION) {
    lcd.setCursor(0, 0);
    lcd.print("Present");
    lcd.setCursor(0, 1);
    lcd.print("The gift of time.");
  } else {
    // Break out the dimensions of the time to show
    int valueRemover = timerValue;
    int days = valueRemover / 60 / 60 / 24;
    valueRemover -= days * 24 * 60 * 60;
    int hours = valueRemover / 60 / 60;
    valueRemover -= hours * 60 * 60;
    int minutes = valueRemover / 60;
    valueRemover -= minutes * 60;
    int seconds = valueRemover;

    //Print the values
    lcd.setCursor(0, 0);
    lcd.print(days);
    lcd.print("d ");
    lcd.setCursor(0, 1);
    lcd.print(hours);
    lcd.print("h ");
    lcd.setCursor(0, 1);
    lcd.print(minutes);
    lcd.print("m ");
    lcd.setCursor(0, 1);
    lcd.print(seconds);
    lcd.print("s");
    lcd.setCursor(0, 1);

    lcd.print("The gift of time.");
  }
}

void storeTimer(int timerValue) {
  byte storageBytes[4];

  storageBytes[3] = (timerValue >> 24) & 0xFF;
  storageBytes[2] = (timerValue >> 16) & 0xFF;
  storageBytes[1] = (timerValue >> 8) & 0xFF;
  storageBytes[0] = timerValue & 0xFF;

  EEPROM.write(0, storageBytes[0]);
  EEPROM.write(1, storageBytes[1]);
  EEPROM.write(2, storageBytes[2]);
  EEPROM.write(3, storageBytes[3]);
}

int loadTimer() {
  byte storageBytes[4];
  storageBytes[0] = EEPROM.read(0);
  storageBytes[1] = EEPROM.read(1);
  storageBytes[2] = EEPROM.read(2);
  storageBytes[3] = EEPROM.read(3);

  int timerValue = *(int*)storageBytes;
  return timerValue;
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      _BLEClientConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      _BLEClientConnected = false;
    }
};

void InitBLE() {
  BLEDevice::init("Present-0000001");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(DigitalPinService);
  
  pService->addCharacteristic(&DigitalPinCharacteristic);
 
  PresentPin1Descriptor.setValue("Present Timer Value");
  DigitalPinCharacteristic.addDescriptor(&PresentPin1Descriptor);
  DigitalPinCharacteristic.setValue("0");
    
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(DigitalPinService);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connection issues
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

void updateBLEDevice(int timerValue) {
    DigitalPinCharacteristic.setValue(String(timerValue).c_str());
}

