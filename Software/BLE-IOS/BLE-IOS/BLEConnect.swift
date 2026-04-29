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

@Observable
class BLEHandler: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    struct BLEDevice: Identifiable, Hashable{
        let id: UUID
        let name: String
        let rssi: Int
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
    var devices: [BLEDevice] = []
    var lastValue : Any
    var lastCommandValue = "Nothing yet"
    var values: [String] = []

    
    
    override init(){
        lastValue = 0
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
        values.append(lastValue as! String)
        esp32Peripheral?.writeValue(data, for: commandCharacteristic!, type: .withResponse)
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        debugVariable = "Bluetooth on"
        if central.state == .poweredOn {
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
     guard let data = characteristic.value else { return }
    
     if characteristic.uuid == dataUUID {
        // Check for fault frame (prefixed with 0x02 type byte)
        if data.first == 0x02 && data.count >= 2 {
            let faultFlags = data[1]
            lastValue = "FAULT: 0x\(String(format: "%02X", faultFlags))"
            return
        }
        
        // Parse as can_telemetry_t — must be exactly 16 bytes
        guard data.count >= 16 else {
            lastValue = "Bad frame: \(data.count) bytes"
            return
        }
        
        let temp0  = data.subdata(in: 0..<2).withUnsafeBytes { $0.load(as: Int16.self) }
        let temp1  = data.subdata(in: 2..<4).withUnsafeBytes { $0.load(as: Int16.self) }
        let temp2  = data.subdata(in: 4..<6).withUnsafeBytes { $0.load(as: Int16.self) }
        let cur0   = data.subdata(in: 6..<8).withUnsafeBytes  { $0.load(as: UInt16.self) }
        let cur1   = data.subdata(in: 8..<10).withUnsafeBytes { $0.load(as: UInt16.self) }
        let cur2   = data.subdata(in: 10..<12).withUnsafeBytes { $0.load(as: UInt16.self) }
        let volt0  = data.subdata(in: 12..<14).withUnsafeBytes { $0.load(as: UInt16.self) }
        let volt1  = data.subdata(in: 14..<16).withUnsafeBytes { $0.load(as: UInt16.self) }
        // volt2 would be bytes 16..<18 if you want it — struct is 18 bytes not 16, see note below
        
        lastValue = "T: \(Double(temp0)/100.0)° \(Double(temp1)/100.0)° \(Double(temp2)/100.0)°C | " +
                    "I: \(cur0) \(cur1) \(cur2)mA | " +
                    "V: \(volt0) \(volt1)mV"
     } else {
        lastCommandValue = String(decoding: data, as: UTF8.self)
     }
}



/*
     func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
         guard let data = characteristic.value else {return}
         if(characteristic == dataCharacteristic){
             lastValue = String(decoding: data, as: UTF8.self)
             debugVariable = "Hey we read a variable"
         }
         else{
             lastCommandValue = String(decoding: data, as: UTF8.self)
         }
     }
 */
    
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?){
        if error != nil {
            lastCommandValue = "ERROR"
            return
        }
        lastCommandValue = "We sent something"
    }
}
