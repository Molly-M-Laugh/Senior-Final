//
//  settingsView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/28/26.
//

import SwiftUI



struct settingsView: View{
    @ObservedObject var router: Router
    @ObservedObject var manager: BLEHandler
    @State private var isMenuOpen = false
    @State var threshold = ""
    var body: some View{
        ZStack(){
            VStack(){
                Text("Change your threshold limit")
                TextField("", text: $threshold)
                    .keyboardType(.decimalPad)
                    .padding()
                    .disableAutocorrection(true)
                    .textInputAutocapitalization(.never)
                Button("Update Settings") {
                    if(Float(threshold) ?? -1 != -1){
                        manager.tempThreshold = Float(threshold)!
                        manager.alertFired = false
                    }
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
