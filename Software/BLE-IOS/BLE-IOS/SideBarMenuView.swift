//
//  SideBarMenuView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//


import SwiftUI

struct SideBarMenuView: View {
    @Binding var menuOpen: Bool
    @State var router = Router.shared
    
    var body: some View {
        ZStack(alignment: Alignment(horizontal: .leading, vertical: .top)){
            if menuOpen{
                Rectangle()
                    .opacity(0.5)
                    .ignoresSafeArea()
                    .onTapGesture {
                        menuOpen.toggle()
                    }
                HStack{
                    VStack(alignment: .leading){
                        VStack(alignment: .leading, spacing: 0.3){
                            Text("T-Temp")
                                .padding()
                                .font(.subheadline)
                            Text("UI testing")
                                .padding()
                                .font(.footnote)
                                .tint(.gray)
                        }
                        VStack{
                            ForEach(SideBarMenuItems.allCases){ option in
                                Button(action: {
                                    router.tab = option.rawValue
                                    menuOpen.toggle()
                                }, label: {
                                    SideBarItemView(item: option)
                                })
                            }
                        }
                        
                        Spacer()
                    }
                    .padding()
                    .frame(width: 200, alignment: .leading)
                    .background(.white)
                }
            }
        }
        .transition(.move(edge: .leading))
        .animation(.easeInOut, value: menuOpen)
        
    }
}

#Preview{
    SideBarMenuView(menuOpen: .constant(true))
}
