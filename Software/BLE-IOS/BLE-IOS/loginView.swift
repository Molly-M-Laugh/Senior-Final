//
//  loginView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI

struct loginView: View {
    
    @State var loginStatus = false
    @State var registering = false
    @State private var username: String = ""
    @State private var password: String = ""
    var manager = BLEHandler()
    var httpManager = HttpHandler()
    
    var body: some View{
        VStack{
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
    }
}

#Preview{
    loginView()
}
