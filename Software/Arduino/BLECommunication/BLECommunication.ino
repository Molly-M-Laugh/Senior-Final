#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>


//These are our Universal Unique Identifiers (UUID).
// These are needed to ensure communication is happening on the same place.
#define SERVICE "ab2d02b4-ad53-400f-bf7e-d603a657d07d"
#define CHAR1_UUID "05ac146f-aee8-4659-aba5-882c1f7e0372"
#define CHAR2_UUID "58bb99f3-75cb-48cb-81e4-346cc4f0687d"

// Variable to track state of connection
bool connection = false;

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
    Serial.println("Disconnect");
    BLEDevice::startAdvertising();
  }
};

// Event Handling on the Characteristic for Temperature
class MyTempCallbacks: public BLECharacteristicCallbacks
{
  // Need to add event handling for when the phone reads a value from this characteristic.
  void onRead(BLECharacteristic* BtTemp){
    
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
  // Creating a service to be run. This is run on the server. Refer to diagram on interface documentation for how the service is set up.
  BtData = BtServer->createService(SERVICE);
  // Setting up 2 characteristics to be a part of the BtData service. Again, refer to diagram in interface documentation for how the service is set up.
  BtTemp = BtData->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BtTemp->setCallbacks(new MyTempCallbacks);
  BtCommand = BtData->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  BtCommand->setCallbacks(new MyCommandCallbacks);
  // Starting the BLE stack, and setting up advertising for a device to try to connect to this.
  BtData->start();
  BLEAdvertising *BtAdvert = BLEDevice::getAdvertising();
  BtAdvert->addServiceUUID(SERVICE); //Adding the service UUID to what the ESP is advertising.
  // Need to confirm if these settings are necessary for iPhone communication. I had found that these were preferred settings, but they might be unnecessary
  BtAdvert->setScanResponse(true);
  BtAdvert->setMinPreferred(0x06);
  BtAdvert->setMinPreferred(0x12);
  // Device actually starts advertising.
  BLEDevice::startAdvertising();
  Serial.println("All set up. Please test.");
}

// General loop to constantly run.
void loop() {
  // Loop does nothing if there's no connection to anything. 
  if(connection){
    // it currently updates the phone every 5 seconds, and prints out any changes that occur. 
    dtostrf(testingValue,6,2,messenger);
    BtTemp->setValue(messenger);
    BtTemp->notify();
    delay(500);
  }
  delay(100);
}