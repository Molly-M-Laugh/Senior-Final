#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define SERVICE "ab2d02b4-ad53-400f-bf7e-d603a657d07d"
#define CHAR1_UUID "05ac146f-aee8-4659-aba5-882c1f7e0372"
#define CHAR2_UUID "58bb99f3-75cb-48cb-81e4-346cc4f0687d"

bool connection = false;

BLEServer *BtServer;
BLEService *BtData;
BLECharacteristic *BtHeartbeat;
BLECharacteristic *BtCommand;


float commonValue;
float testingValue;
float commandValue;
static char messenger[6];
static char command[6];



class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* BtServer){
    connection = true;
    Serial.println("New Connection");
  }

  void onDisconnect(BLEServer* BtServer){
    connection = false;
    Serial.println("Disconnect");
  }
};

class MyHeartbeatCallbacks: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* BtHeartbeat){
    String input;
    Serial.println("Received input");
    input = BtHeartbeat->getValue();
    testingValue = input.toFloat();
    if(testingValue == 0){
      testingValue = 1;
    }
    else{
      dtostrf(testingValue,6,2,messenger);
    }
  }
};

class MyCommandCallbacks: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* BtCommand){
    String input;
    Serial.println("Received command input");
    input = BtCommand->getValue();
    commandValue = input.toFloat();
    if(input.toFloat() == 0){
      commandValue = 1;
    }
    else{
      if(input.toFloat() == 12){
        testingValue = 0;
        dtostrf(testingValue,6,2,messenger);
      }
      if(input.toFloat() == 14){
        commandValue = input.toFloat();
        testingValue = 1000;
        dtostrf(testingValue,6,2,messenger);
      }
      dtostrf(commandValue,6,2,command);
      Serial.println(command);
    }
  }
};

void setup() {
  // put your setup code here, to run once:
  commonValue = 1;
  testingValue = 12;
  Serial.begin(115200);
  BLEDevice::init("MyESP32");
  BtServer = BLEDevice::createServer();
  BtServer->setCallbacks(new MyServerCallbacks);
  BtData = BtServer->createService(SERVICE);
  BtHeartbeat = BtData->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtHeartbeat->setCallbacks(new MyHeartbeatCallbacks);
  BtCommand = BtData->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtCommand->setCallbacks(new MyCommandCallbacks);
  BtData->start();
  BLEAdvertising *BtAdvert = BLEDevice::getAdvertising();
  BtAdvert->addServiceUUID(SERVICE);
  BtAdvert->setScanResponse(true);
  BtAdvert->setMinPreferred(0x06);
  BtAdvert->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("All set up. Please test.");
}


void loop() {
  // put your main code here, to run repeatedly:
  if(connection){
    dtostrf(testingValue,6,2,messenger);
    if(commonValue == 1){
      BtHeartbeat->setValue(messenger);
      BtHeartbeat->notify();
      delay(500);
      BtCommand->setValue(command);
      BtCommand->notify();
      Serial.println("12");
      commonValue = 0;
      delay(5000);
    }
    else{
      BtHeartbeat->setValue(messenger);
      BtHeartbeat->notify();
      delay(500);
      BtCommand->setValue(command);
      BtCommand->notify();
      delay(5000);
    }
  }
  delay(100);
}
