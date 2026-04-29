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
            TextField("New Username:", text: $username)
                .padding()
            TextField("New Password", text: $password)
                .padding()
            Button("Create Account", action:{
                httpManager.register(username: username, password: password){
                    (working) in
                    if((working)){
                        router.setPath([.login])
                    }
                }
            }).buttonStyle(.bordered)
        }
    }
}
