//
//  registerView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI

struct registerView: View {
    @State private var username: String = ""
    @State private var password: String = ""
    @State var registering = false
    var httpManager = HttpHandler()
    
    var body: some View {
        VStack{
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
