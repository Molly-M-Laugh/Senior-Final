//
//  ContentView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 2/2/26.
//
// Front Facing code handling the view of the app and user interaction


import SwiftUI
import CoreBluetooth
import Charts

struct ChartData: Identifiable {
    let id = UUID()
    let x : Date
    let y : Float
}

struct ContentView: View {
    
    
    @State var connectionStatus = false
    @State var number = 0
    @State var state = "Searching"
    @State var loginStatus = false
    @State var registering = false
    @State private var username: String = ""
    @State private var password: String = ""
    var manager = BLEHandler()
    var httpManager = HttpHandler()
    
    
    
    var body: some View{
        if(loginStatus){
            if manager.devices.isEmpty{
                ProgressView("\(state)").progressViewStyle(.circular)
                Text("Scanning: \(manager.isScanning)")
                
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
                //Text("Debug Variable: \(manager.debugVariable)")
                Text("Our current reading: \(manager.lastValue)")
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
                Chart {
                    ForEach(manager.values) { data in
                        LineMark(x: .value("x", data.x), y: .value("y", data.y))
                    }
                }
                .frame(width: 350, height: 200)
                .padding()
                Button("Reset Graph", action:{
                    manager.resetGraph()
                }).buttonStyle(.bordered)
            }
        }
        else if (registering == false){
            Text("Please login:")
                .font(.largeTitle)
            TextField("Username:", text: $username)
            TextField("Password", text: $password)
            Button("Login", action:{
                httpManager.login(username: username, password: password)
                
                loginStatus = httpManager.loginResults
            })
            Button("Refresh", action:{
                loginStatus = httpManager.loginResults
            })
            Button("Register New User", action:{
                registering = true
                httpManager.loginResults = false
                httpManager.registerResults = false
            })
            
        }
        else{
            Text("Please enter your new information:")
                .font(.largeTitle)
            TextField("New Username:", text: $username)
            TextField("New Password", text: $password)
            Button("Create Account", action:{
                httpManager.register(username: username, password: password)
                
            })
            Button("Refresh", action:{
                registering = !(httpManager.registerResults)
            })
            
        }
    }
}
#Preview {
    ContentView()
}
