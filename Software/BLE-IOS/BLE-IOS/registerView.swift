//
//  registerView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI

struct registerView: View {
    @ObservedObject var router: Router
    @State private var username: String = ""
    @State private var password: String = ""
    @State var registering = false
    var httpManager = HttpHandler()
    
    var body: some View {
        VStack{
            Text("Please Enter Your Information")
                .font(.largeTitle)
                .bold()
                .foregroundColor(.blue)
            TextField("New Username:", text: $username)
                .padding()
                .disableAutocorrection(true)
                .textInputAutocapitalization(.never)
                .textFieldStyle(.roundedBorder)
            TextField("New Password", text: $password)
                .padding()
                .disableAutocorrection(true)
                .textInputAutocapitalization(.never)
                .textFieldStyle(.roundedBorder)
            Button(action:{
                httpManager.register(username: username, password: password){
                    (working) in
                    if((working)){
                        router.setPath([.login])
                    }
                }
            }){
                Text("Create New Account")
                    .foregroundColor(.white)
            }
            .buttonStyle(.bordered)
            .cornerRadius(8)
            .background(Color.blue)
        }
    }
}
