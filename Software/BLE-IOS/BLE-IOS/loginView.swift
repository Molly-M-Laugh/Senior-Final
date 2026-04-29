//
//  loginView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI

struct loginView: View {
    @ObservedObject var router: Router
    @State var loginStatus = false
    @State var registering = false
    @State private var username: String = ""
    @State private var password: String = ""
    var manager = BLEHandler()
    var httpManager = HttpHandler()
    
    var body: some View{
        VStack{
            
            
            Text("VitalVest")
                .font(.largeTitle)
            Text("")
            Text("")
            Text("Please login:")
                .font(.title)
            TextField("Username:", text: $username)
                .padding()
                .disableAutocorrection(true)
                .textInputAutocapitalization(.never)
            TextField("Password", text: $password)
                .padding()
                .disableAutocorrection(true)
                .textInputAutocapitalization(.never)
            Button("Login", action:{
                httpManager.login(username: username, password: password){
                    (working) in
                    print("working is \(working)")
                    loginStatus = (working)
                    if((working)){
                        router.setPath([.home])
                    }
                }
                router.setPath([.home])
                
            })
            .buttonStyle(.bordered)
            .bold()
            
            Button("Register New User", action:{
                httpManager.loginResults = false
                httpManager.registerResults = false
                router.forwardOne(.register)
            })
            .buttonStyle(.bordered)
        }
        .navigationBarBackButtonHidden(true)
        
    }
        
}

