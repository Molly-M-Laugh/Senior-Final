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
import UserNotifications


class BLEHandler: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate, ObservableObject{
    
    struct BLEDevice: Identifiable, Hashable{
        let id: UUID
        let name: String
        let rssi: Int
    }
    private var httpManager = HttpHandler()
    
    private var centralManager: CBCentralManager!
    private var esp32Peripheral: CBPeripheral?
    
    var dataCharacteristic: CBCharacteristic?
    var commandCharacteristic: CBCharacteristic?
    //Our UUIDs for connecting to the ESP
    var serviceUUID = CBUUID(string:"ab2d02b4-ad53-400f-bf7e-d603a657d07d")
    var dataUUID = CBUUID(string:"05ac146f-aee8-4659-abba-882c1f7e0372")
    var commandUUID = CBUUID(string:"58bb99f3-75cb-48cb-81e4-346cc4f0687d")
    
    var debugVariable = ""
    var isConnected = false
    var isScanning = false
    var lastValue : String
    private var pushSuccess = true
    
    private var bleDelay = false
    private var timer: Timer?
    
    @Published var devices: [BLEDevice] = []
    @Published var lastDataValue : [String]  = []
    @Published var lastCommandValue = "Nothing yet"
    @Published var lastDate : Date = Date()
    @Published var tempValues : [ChartData] = []
    @Published var currentValues : [ChartData] = []
    @Published var voltageValues : [ChartData] = []
    @Published var cleanValue = ""
    @Published var tempThreshold : Float = 100
    @Published var tempAvgValues: [ChartData] = []
    @Published var alertFired = false
    
    
    
    override init(){
        lastValue = "0"
        super.init()
        centralManager = CBCentralManager(delegate: self, queue:nil)
    }
   
    func startScan() {
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
        esp32Peripheral?.writeValue(data, for: commandCharacteristic!, type: .withResponse)
    }
    
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
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
        let newPeripheral = BLEDevice(id: peripheral.identifier, name: peripheral.name ?? "Unknown", rssi: RSSI.intValue)
        if !devices.contains(newPeripheral) {devices.append(newPeripheral)}
    }
    
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
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
         for testing in services where testing.uuid == serviceUUID{
             peripheral.discoverCharacteristics([dataUUID, commandUUID], for: testing)
         }
     }

     func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
         guard let targetChars = service.characteristics else {return}
         for target in targetChars {
             if target.uuid == dataUUID {
                 dataCharacteristic = target
                 peripheral.setNotifyValue(true, for: dataCharacteristic!)
             }
             if target.uuid == commandUUID {
                 commandCharacteristic = target
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
            print(lastValue)
            return
        }
        
        if(bleDelay){
             return
        }
        startTimer()
        
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
        
         lastDataValue = lastValue.components(separatedBy: " | ")
         let inputTrim1 = CharacterSet.init(charactersIn: "T: ")
         let cleanValue = lastDataValue[0].trimmingCharacters(in: inputTrim1)
         var tempChartData = (Float((Double(temp0)/100.0)))
         var tempAvg : Float = 0.0
         if(tempChartData == -1){
             print("Error on this data: \(cleanValue)")
         }
         else{
             tempValues.append(ChartData(sensor: "temp0", x:Date(), y: tempChartData))
             tempAvg += tempChartData
         }
         tempChartData = (Float((Double(temp1)/100.0)))
         if(tempChartData == -1){
             print("Error on this data: \(cleanValue)")
             
         }
         else{
             tempValues.append(ChartData(sensor: "temp1", x:Date(), y: tempChartData))
             tempAvg += tempChartData
         }
         tempChartData = (Float((Double(temp2)/100.0)))
         if(tempChartData == -1){
             print("Error on this data: \(cleanValue)")
         }
         else{
             tempValues.append(ChartData(sensor: "temp2", x:Date(), y: tempChartData))
             tempAvg += tempChartData
         }
         if(tempAvg > 0 && pushSuccess){
             
             tempAvg = tempAvg / 3
             if(tempAvg >= tempThreshold){
                 callNotification()
             }
             tempAvgValues.append(ChartData(sensor: "all", x:Date(), y:tempAvg))
             httpManager.pushToDB(username: "testUser", password: "config2", dataToPush: [tempValues[tempValues.count-3], tempValues[tempValues.count-2], tempValues[tempValues.count-1]], averageData: tempAvgValues[tempAvgValues.count-1]){
                 (working) in
                 self.pushSuccess = working
             }
             
         }
         
         
         
     } else {
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
    
    func addToAvg(newData: [ChartData]){
        if(newData.count > 0){
            for i in 0...(newData.count-1){
                tempAvgValues.append(newData[i])
            }
        }
        tempAvgValues = tempAvgValues.sorted(by: {$0.x > $1.x})
        
    }
    
    private func startTimer(){
        bleDelay = true
        timer = Timer.scheduledTimer(timeInterval: 1.0, target: self, selector: #selector(fire), userInfo: nil, repeats: false)
    }
    
    private func callNotification(){
        if(alertFired){
            return
        }
        let content = UNMutableNotificationContent()
        content.title = "VitalVest Alert"
        content.subtitle = "Temperature Threshold Breached!!"
        content.sound = .default
        
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let request = UNNotificationRequest(identifier: UUID().uuidString, content: content, trigger: trigger)
        alertFired = true
        UNUserNotificationCenter.current().add(request)
        
    }
    
    @objc func fire(){
        bleDelay = false;
        timer?.invalidate()
        timer = nil
    }
    
}

    
