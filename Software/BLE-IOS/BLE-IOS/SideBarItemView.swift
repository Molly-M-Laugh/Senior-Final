//
//  SideBarItemView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI



struct SideBarItemView: View{
    
    let item: SideBarMenuItems
    
    var body: some View{
        HStack{
            Text(item.name)
                .font(.subheadline)
                
        }
        .padding(.leading)
        .frame(height: 44)
    }
}

