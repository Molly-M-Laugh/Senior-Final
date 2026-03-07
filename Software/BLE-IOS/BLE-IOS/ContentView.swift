//
//  ContentView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 2/2/26.
//
// Front Facing code handling the view of the app and user interaction


import SwiftUI
import CoreBluetooth

struct ContentView: View {
    
    @State var connectionStatus = false
    @State var number = 0
    @State var state = "Searching"
    var manager = BLEHandler()
    
    

    var body: some View{
        if manager.devices.isEmpty{
            ProgressView("\(state)").progressViewStyle(.circular)
            Text("Debug: \(manager.debugVariable)")
            Text("Scanning: \(manager.isScanning)")
            Button("try to connect"){
                state = "Button pushed, searching"
                manager.startScan()
            }
        }
        else if !manager.isConnected{
            Text("I'm trying to debug this")
            List(manager.devices, id: \.self){ device in
                Button(action: {
                    manager.connect(to: device)
                }){
                    Text(device.name)
                }
            }
            Text("Scanning Status: \(manager.isScanning)")
            Text("Connection Status: \(manager.isConnected)")
        }
        else {
            Text("Debug Variable: \(manager.debugVariable)")
            Text("Our current reading: \(manager.lastValue)")
            Text("Last command sent: \(manager.lastCommandValue)")
            HStack{
                Button("Reset value",action: {
                    manager.sendCommand("12")
                })
                Button("increase value",action: {
                    manager.sendCommand("5")
                })
                Button("Random value",action: {
                    manager.sendCommand("1")
                })
            }.buttonStyle(.bordered)
        }
    }
}

#Preview {
    ContentView()
}
