//
//  homeView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI

struct homeView: View{
    @State var router = Router.shared
    
    var body: some View{
        VStack{
            Text("Still Building")
            Button("Move pages", action: {
                router.tab = 2
                print(router.tab)
            })
        }
    }
}
