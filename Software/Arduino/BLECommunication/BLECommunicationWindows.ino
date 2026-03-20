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


//These are our Universal Unique Identifiers (UUID).
// These are needed to ensure communication is happening on the same place.
#define SERVICE "ab2d02b4-ad53-400f-bf7e-d603a657d07d"
#define CHAR1_UUID "05ac146f-aee8-4659-aba5-882c1f7e0372"
#define CHAR2_UUID "58bb99f3-75cb-48cb-81e4-346cc4f0687d"

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
BLECharacteristic *BtCommand;

// Communication variables
float testingValue; // Communication value for the temp characteristic
float commandValue; // Value for the command characteristic.
static char messenger[6]; // Message buffer for temp values
static char command[6]; // Message buffer for command values
static int pastValues[10]; // Small storage in case of delay in sending out values
int counter=0; // generic counter for loops, used later





// Event handling for the BLEServer
class MyServerCallbacks : public BLEServerCallbacks
{
  // Event of a connection. It sets the connection to true, prints status.
  void onConnect(BLEServer* BtServer){
    connection = true; 
    Serial.println("New Connection");  
  }


  // Event of a disconnection. Sets connection to false, prints status, and then restarts advertising for a new connection.
  void onDisconnect(BLEServer* BtServer){
    connection = false;
    delay(500); // (Gemini, to account for disconnection before restarting advertising)
    Serial.println("Disconnect");
    BLEDevice::startAdvertising();
  }
};

// Event Handling on the Characteristic for Temperature
class MyTempCallbacks: public BLECharacteristicCallbacks
{
  // Need to add event handling for when the phone reads a value from this characteristic.
  void onRead(BLECharacteristic* BtTemp){
    // Create decision tree on sync vs standard read
  }

};

// Event handling for the command characteristic.
class MyCommandCallbacks: public BLECharacteristicCallbacks
{
  // Event of a command being written to the characteristic.
  // It reads the command, then checks to make sure it is a correct value. 
  // Currently it only changes testingValue, as part of testing this feature.
  // Future changes: Need to implement actual commands that interact with the system.
  void onWrite(BLECharacteristic* BtCommand){
    String input;
    input = BtCommand->getValue();
    commandValue = input.toFloat();
    if(input.toFloat() == 0){
      commandValue = 1;
    }
    else{
      //insert command value effects
      if(commandValue == 12){
        Serial.println("Resetting number");
        testingValue = 12;
      }
      else if(commandValue == 1){
        testingValue = random(1,20);
        Serial.println("Random number");
      }
      /*
      //Not used yet
      else if(commandValue == 2){
        // Begin Sync Process
        sync = true;
        // Set up read process.
      }
      else if(commandValue == 20){
        // ACK for sync process
        sync = false;
      }
      */
      else{
        testingValue += 1;
      }
    }
  }
};


// Setup code!
void setup() {
  // Setup for testing, can remove later.
  testingValue = 12;
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
  BtTemp = BtData->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BtTemp->addDescriptor(new BLE2902()); // (Gemini)
  BtTemp->setCallbacks(new MyTempCallbacks);
  BtCommand = BtData->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtCommand->addDescriptor(new BLE2902()); // (Gemini)
  BtCommand->setCallbacks(new MyCommandCallbacks);
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
  //Serial.println("Looping"); // Because if there's no connection, testing loop - Molly
  // Loop does nothing if there's no connection to anything. 
  if(connection){
    // it currently updates the phone every 5 seconds, and prints out any changes that occur. 
    dtostrf(testingValue,6,2,messenger);
    BtTemp->setValue(messenger);
    BtTemp->notify();
    //Serial.println("Connected in the loop");
    delay(500);
  } else {
    //Serial.println("Not connected in the loop");
  }
  delay(100);
}