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

struct ContentView: View {
    @StateObject private var router = Router()
    @StateObject private var BLEmanager = BLEHandler()
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
