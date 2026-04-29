#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>
#include <SPI.h>
#include <BLEServer.h> // (Gemini)
#include <BLE2902.h> // (Gemini)
#include <BLEHIDDevice.h> // (Gemini)
#include "HIDTypes.h"
#include "can_ctrl.h"
#include <Wire.h>

//These are our Universal Unique Identifiers (UUID).
// These are needed to ensure communication is happening on the same place.
#define SERVICE "ab2d02b4-ad53-400f-bf7e-d603a657d07d"
#define TEMP_UUID "05ac146f-aee8-4659-aba5-882c1f7e0372"
#define ALERT_UUID "58bb99f3-75cb-48cb-81e4-346cc4f0687d"
#define POWER_UUID "825ab071-bbe7-4032-82d3-558c35669279"
#define TEMP_LIMIT_UUID "8dfe11ef-25c0-47ee-a85d-f09d9e842c75"
#define POWER_RAIL_UUID "7908269a-7dfb-43f4-aa67-498191d77b8e"
#define TEMP_REQUEST_UUID "0ab3fd35-5e20-48ec-b5cb-7bf0b7260453"

//For I2C support
#define I2C_Freq 115200
#define SDA_0 18
#define SCL_0 19

// For Windows 11 Support
// Note: USED GEMINI to obtain these variables for a generic instance, as wasn't finding
// in top resources
BLEHIDDevice* hid;
BLECharacteristic* inputReport;
// Dummy variables for Windows 11 
const char* deviceManufacturer = "DOIT_ESP32";
uint16_t vid = 0x05ac; // Apple Vendor ID (often used for compatibility)
uint16_t pid = 0x820a; 
uint16_t version = 0x0210;
uint8_t batteryLevel = 100;
// Minimal HID Report Map (tells Windows this is a generic HID device)
const uint8_t _hidReportDescriptor[] = {
  0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
  0x09, 0x06,                    // USAGE (Keyboard)
  0xa1, 0x01,                    // COLLECTION (Application)
  0x85, 0x01,                    //   REPORT_ID (1)
  0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
  0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
  0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
  0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
  0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
  0x75, 0x01,                    //   REPORT_SIZE (1)
  0x95, 0x08,                    //   REPORT_COUNT (8)
  0x81, 0x02,                    //   INPUT (Data,Var,Abs)
  0xc0                           // END_COLLECTION
};
// Below is the orginial approach that was taken for HID variables, but also would
// have needing to know the generic values listed above
//#define HID_SERVICE "1812"
//#define HID_CHAR_REPORT "2a4d" // HID inputs
//#define HID_CHAR_REPORT_MAP "2a4b" // Device usage
//#define HID_CHAR_CONTROL_POINT "2a4c"
//#define HID_CHAR_HID_INFO "2a4a"
//#define BATTERY_SERVICE "180F"
//#define BATTERY_CHAR "2a19"
//#define DEVICE_INFO_SERVICE "180a"
//#define DEVICE_INFO_CHAR_MANU_NAME "2a29"
//#define DEVICE_INFO_CHAR_MODEL_NUM "2a24"
//#define DEVICE_INFO_CHAR_SYSTEM_ID "2a23"


// Notes for SPI communication
// Pins are set to default and have corresponding preset variables from Arduino
// MOSI = 23
// MISO = 19
// SCK = 18
// SS = 5

// Variable to track state of connection
bool connection = false;
//bool sync = false;


// BLE Server, Service, and Characteristic setup. 
BLEServer *BtServer;
BLEService *BtData;
BLECharacteristic *BtTemp;
BLECharacteristic *BtPower;
BLECharacteristic *BtAlert;

BLEService *BtCommand;
BLECharacteristic *BtPowerOn;
BLECharacteristic *BtTempLimit;
BLECharacteristic *BtTempRequest;


/*
// Communication variables
uint8_t temperature; // Communication value for the temp characteristic
float commandValue; // Value for the command characteristic.
static char messenger[6]; // Message buffer for temp values
static char command[6]; // Message buffer for command values
static int pastValues[10]; // Small storage in case of delay in sending out values
int counter=0; // generic counter for loops, used later
*/

//State variables of the system
float tempLimit;
float recentTemp;



TwoWire I2C_0 = TwoWire(0);

// Event handling for the BLEServer
class MyServerCallbacks : public BLEServerCallbacks
{
  // Event of a connection. It sets the connection to true, prints status.
  void onConnect(BLEServer* BtServer){
    connection = true;
  }


  // Event of a disconnection. Sets connection to false, prints status, and then restarts advertising for a new connection.
  void onDisconnect(BLEServer* BtServer){
    connection = false;
    delay(500); // (Gemini, to account for disconnection before restarting advertising)
    BLEDevice::startAdvertising();
  }
};

// Event Handling on the Characteristic for Temperature
class MyTempCallbacks: public BLECharacteristicCallbacks
{
  // Need to add event handling for when the phone reads a value from this characteristic.
  void onRead(BLECharacteristic* BtTemp){
    //update with reading from I2C    
  }

};

class MyAlertCallbacks: public BLECharacteristicCallbacks
{
  //Reading the alert values
  void onRead(){}
  //ACK for alerts
  void onWrite(){}
}

class MyPowerCallbacks: public BLECharacteristicCallbacks
{
  //Reading the power status of the local device.
  void onRead(){}
}

class MyPowerOnCallbacks: public BLECharacteristicCallbacks
{
  //this is written with example values, the actual which I don't currently know
  // 
  void onWrite(BLECharacteristic* BtPowerOn){
    String input;
    input = BtPowerOn->getValue();
    if(input.compareTo("on") == 0){
      //send message to turn power on
    }
    else if(input.compareTo("off") == 0){
      //send message to turn power rail off
    }
    else{
      //ignore, input matches no expected value
    }
  }
};


// Event handling for the TempLimit characteristic.
class MyTempLimitCallbacks: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* BtTempLimit){
    String input;
    input = BtTempLimit->getValue();
    if(input.toFloat() == 0){
      tempLimit = input.toFloat();
      //Added I2C to push limit to CAN ESP
    }
  }
};

class MyTempRequestCallbacks: public BLECharacteristicCallbacks
{

  void onWrite(BLECharacteristic* BtTempRequest){
    String input;
    input = BtTempRequest->getValue();
    if(input.compareTo("sync") == 0){
      //begin sync process
      //find length of remaining data locally, then repeatedly query I2C for local values from FRAM
      //Send values over in a combined format if possible
    }
    else if(input.compareTo("now") == 0){
      //Immediate value

      //find if we have the immediate in local queue
      //then either send it or request from I2C what value to send
    }
    else{
      //Disregard any input here
    }
    
  }
}



// Setup code!
void setup() {
  I2C_0.begin(SDA_0, SCL_0, I2C_Freq);
  // Setup for testing, can remove later.
  // Starting the baud rate to 115200, necessary for BLE readings.
  Serial.begin(115200);
  // We initialize the BLE device, and then create a server on this device. Set callbacks as well.
  BLEDevice::init("MyESP32");
  BtServer = BLEDevice::createServer();
  BtServer->setCallbacks(new MyServerCallbacks);

  // HID (Majority as template from GitHub BLEKeyboard)
  hid = new BLEHIDDevice(BtServer);
  inputReport = hid->inputReport(1); // Report ID 1 (Gemini)
  hid->manufacturer()->setValue(deviceManufacturer);
  hid->pnp(0x02, vid, pid, version);
  hid->hidInfo(0x00, 0x01);
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  hid->startServices();
  if (hid != 0)
    hid->setBatteryLevel(batteryLevel);

  // Creating a service to be run. This is run on the server. Refer to diagram on interface documentation for how the service is set up.
  BtData = BtServer->createService(SERVICE);
  // Setting up 2 characteristics to be a part of the BtData service. Again, refer to diagram in interface documentation for how the service is set up.
  BtTemp = BtData->createCharacteristic(TEMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BtTemp->addDescriptor(new BLE2902()); // (Gemini)
  BtTemp->setCallbacks(new MyTempCallbacks);

  BtPower = BtData->createCharacteristic(POWER_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BtPower->addDescriptor(new BLE2902()); // (Gemini)
  BtPower->setCallbacks(new MyPowerCallbacks);

  BtAlert = BtData->createCharacteristic(ALERT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BtAlert->addDescriptor(new BLE2902()); // (Gemini)
  BtAlert->setCallbacks(new MyAlertCallbacks);

  BtPowerOn = BtData->createCharacteristic(POWER_RAIL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtPowerOn->addDescriptor(new BLE2902()); // (Gemini)
  BtPowerOn->setCallbacks(new MyPowerOnCallbacks);

  BtTempLimit = BtData->createCharacteristic(TEMP_LIMIT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtTempLimit->addDescriptor(new BLE2902()); // (Gemini)
  BtTempLimit->setCallbacks(new MyTempLimitCallbacks);

  BtTempRequest = BtData->createCharacteristic(TEMP_REQUEST_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtTempRequest->addDescriptor(new BLE2902()); // (Gemini)
  BtTempRequest->setCallbacks(new MyTempRequestCallbacks);
  // Starting the BLE stack
  BtData->start();
  // HID Service start
  hid->startServices();
  //onStarted(BtServer);
  // Setting up advertising for a device to try to connect to this.
  BLEAdvertising *BtAdvert = BLEDevice::getAdvertising();
  BtAdvert->addServiceUUID(SERVICE); //Adding the service UUID to what the ESP is advertising.
  // Need to confirm if these settings are necessary for iPhone communication. I had found that these were preferred settings, but they might be unnecessary
  BtAdvert->setScanResponse(true);
  BtAdvert->setMinPreferred(0x06);
  BtAdvert->setMinPreferred(0x12);
  //Additional HID advertising
  BtAdvert->addServiceUUID(hid->hidService()->getUUID());
  BtAdvert->setAppearance(GENERIC_HID); // Crucial: Tell Windows it's a HID device (Gemini)
  BtAdvert->setAdvertisementType(ADV_TYPE_IND); // (Gemini) Make sure don't have invisible connection
  // Device actually starts advertising.
  // And to find associated values quicker, providing labels to services, characteristics
  BLEAdvertisementData advertisementData;
  advertisementData.setName("MyESP32"); // In case if some scanners don't catch it all the time
  advertisementData.setManufacturerData("DoIt");
  advertisementData.setServiceData(BLEUUID(SERVICE), "Temperature Service");

  BLEDevice::startAdvertising();
  Serial.println("All set up. Please test.");
  //SPI setup, using default pins
  SPI.begin();
}

// General loop to constantly run.
void loop() {
  // Loop does nothing if there's no connection to anything. 
  if(connection){
    delay(1000)
  } else {
    //Serial.println("Not connected in the loop");
  }
  delay(100);
}