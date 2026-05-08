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
import UserNotifications


struct ContentView: View {
   
    @StateObject private var router = Router()
    private var BLEmanager = BLEHandler()
    @State var NotifManager = NotifDelegate()
    init(){
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .badge, .sound]){
            success,error in
            if success {}
            else if let error {
                print(error.localizedDescription)
            }
        }
        
        
    }
    var body: some View {
        NavigationStack(path: $router.path) {
            loginView (router: router)
                .navigationDestination(for: Route.self){ route in
                    switch route {
                    case .login:
                        loginView(router: router)
                    case .home:
                        homeView(router: router, manager: BLEmanager)
                    case .register:
                        registerView(router: router)
                    case .settings:
                        settingsView(router: router, manager: BLEmanager)
                    case .graph:
                        graphView(router: router, manager: BLEmanager)
                    }
                }
        }
        .navigationBarBackButtonHidden(true)
        .navigationBarHidden(true)
    }
}

#Preview {
    ContentView()
}
