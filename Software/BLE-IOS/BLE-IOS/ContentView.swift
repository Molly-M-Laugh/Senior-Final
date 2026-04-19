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
    
    
    @State var connectionStatus = false
    @State var number = 0
    @State var state = "Searching"
    @State var loginStatus = false
    @State var registering = false
    @State private var username: String = ""
    @State private var password: String = ""
    @State private var isMenuOpen = false
    @State var router = Router.shared
    var manager = BLEHandler()
    var httpManager = HttpHandler()
    @State var tab = 0
    
    var body: some View{
        NavigationStack{
            ZStack{
                TabView(selection: $tab){
                    loginView()
                        .tag(0)
                    registerView()
                        .tag(1)
                    homeView()
                        .tag(2)
                    graphView()
                        .tag(3)
                }
                SideBarMenuView(menuOpen: $isMenuOpen, tab: $tab)
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
}
#Preview {
    ContentView()
}
