//
//  homeView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI


struct homeView: View{
    @ObservedObject var router: Router
    @ObservedObject var manager: BLEHandler
    @State private var isMenuOpen = false
    var body: some View{
        ZStack(){
            VStack(spacing: 16){
                if manager.devices.isEmpty{
                    ProgressView().progressViewStyle(.circular)
                    
                    Text("Scanning: \(manager.isScanning)")
                }
                else if !manager.isConnected{
                    List(manager.devices, id: \.self){ device in
                        Button(action: {
                            manager.connect(to: device)
                        }){
                            Text(device.name)
                        }
                    }
                    Text("Connection Status: \(manager.isConnected)")
                }
                else{
                    //Get current battery reading from CAN on ESP
                    Text("")
                    Text("Currently connected to VitalVest: \(manager.isConnected)")
                    Text("Current battery: \(manager.lastDataValue[2])")
                    
                }
            }
            .padding()
            .navigationTitle("VitalVest")
            .navigationBarBackButtonHidden(true)
            
            SideBarMenuView(menuOpen: $isMenuOpen, router: router, manager: manager)
        }
        .toolbar {
            ToolbarItem(placement: .topBarLeading) {
                Button(action: {
                    isMenuOpen.toggle()
                }, label: {
                    Image(systemName: "line.3.horizontal")
                })
            }
        }
    }
}
