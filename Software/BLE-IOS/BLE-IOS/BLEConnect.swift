//
//  BLEConnect.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 2/3/26.
//

// Work for CoreBluetooth and handling all connections


import Foundation
import CoreBluetooth
import Observation
internal import Combine



class BLEHandler: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate, ObservableObject{
    
    struct BLEDevice: Identifiable, Hashable{
        let id: UUID
        let name: String
        let rssi: Int
    }
    
    struct ChartData: Identifiable{
        let id = UUID()
        let x : Date
        let y : Float
    }
    
    
    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    var dataCharacteristic: CBCharacteristic?
    var commandCharacteristic: CBCharacteristic?
    //Our UUIDs for connecting to the ESP
    var serviceUUID = CBUUID(string:"ab2d02b4-ad53-400f-bf7e-d603a657d07d")
    var dataUUID = CBUUID(string:"05ac146f-aee8-4659-aba5-882c1f7e0372")
    var commandUUID = CBUUID(string:"58bb99f3-75cb-48cb-81e4-346cc4f0687d")
    
    var debugVariable = ""
    var isConnected = false
    var isScanning = false
    @Published var devices: [BLEDevice] = []
    @Published var lastValue : String
    @Published var lastCommandValue = "Nothing yet"
    @Published var lastDate : Date = Date()
    @Published var values : [ChartData] = []
    @Published var int1 : Float = 0.0
    @Published var cleanValue = ""
    

    
    
    override init(){
        lastValue = "0"
        
        super.init()
        centralManager = CBCentralManager(delegate: self, queue:nil)
    }
   
    func startScan() {
        debugVariable = "scan started"
        isScanning = true
        devices.removeAll()
        centralManager.scanForPeripherals(withServices: [serviceUUID], options:nil);
    }
    
    func connect(to bleDevice: BLEDevice){
        guard let newPeripheral = centralManager.retrievePeripherals(withIdentifiers: [bleDevice.id]).first else {return}
        centralManager.stopScan()
        isScanning = false
        esp32Peripheral = newPeripheral
        newPeripheral.delegate = self
        centralManager.connect(newPeripheral, options:nil)
        isConnected = true
    }
    
    
    func sendCommand(_ command: String){
        let data = Data(command.utf8)
        debugVariable = "trying to send command"
        esp32Peripheral?.writeValue(data, for: commandCharacteristic!, type: .withResponse)
    }
    
    func resetGraph(){
        values = []
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        debugVariable = "Bluetooth on"
        if central.state == .poweredOn {
            startScan()
        }
    }
    
    func getDate() -> Date {
        let date = Date()
        return date
    }
    
    func disconnectFromPeripheral(){
        if let peripheral = esp32Peripheral{
            centralManager.cancelPeripheralConnection(peripheral)
            isConnected = false
            esp32Peripheral = nil
            dataCharacteristic = nil
            commandCharacteristic = nil
            devices = []
            startScan()
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber){
        debugVariable = "Attempting to connect to something"

        let newPeripheral = BLEDevice(id: peripheral.identifier, name: peripheral.name ?? "Unknown", rssi: RSSI.intValue)
        if !devices.contains(newPeripheral) {devices.append(newPeripheral)}
    }
    
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        debugVariable = "connection made"
        isConnected = true
        peripheral.discoverServices([serviceUUID])
     }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral){
        isConnected = false
        esp32Peripheral = nil
        dataCharacteristic = nil
        commandCharacteristic = nil
        devices = []
    }

     func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
         guard let services = peripheral.services else {return}
         debugVariable = "Discovered Services successfully"
         for testing in services where testing.uuid == serviceUUID{
             peripheral.discoverCharacteristics([dataUUID, commandUUID], for: testing)
         }
     }

     func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
         guard let targetChars = service.characteristics else {return}
         debugVariable = "Trying to connect the characteristics"
         for target in targetChars {
             if target.uuid == dataUUID {
                 dataCharacteristic = target
                 peripheral.setNotifyValue(true, for: dataCharacteristic!)
                 debugVariable = "Set data"
             }
             if target.uuid == commandUUID {
                 commandCharacteristic = target
                 debugVariable = "Set command"
             }
         }
     }

     func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
         guard let data = characteristic.value else {return}
         if(characteristic == dataCharacteristic){
             lastValue = String(decoding: data, as: UTF8.self)
             debugVariable = "Hey we read a variable"
             cleanValue = lastValue.trimmingCharacters(in: .whitespacesAndNewlines)
             int1 = (Float(cleanValue) ?? -1)
             lastDate = getDate()
             values.append(ChartData(x: lastDate, y: int1))
         }
         else{
             lastCommandValue = String(decoding: data, as: UTF8.self)
         }
     }

    
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?){
        if error != nil {
            lastCommandValue = "ERROR"
            return
        }
        lastCommandValue = "We sent something"
    }
}
