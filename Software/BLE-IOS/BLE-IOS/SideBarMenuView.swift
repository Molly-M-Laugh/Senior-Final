//
//  SideBarMenuView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//


import SwiftUI


struct SideBarMenuView: View {
    @Binding var menuOpen: Bool
    @ObservedObject var router: Router
    var manager: BLEHandler
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
                            Text("VitalVest")
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
                                    if(option.route == .login){
                                        manager.disconnectFromPeripheral()
                                    }
                                    router.setPath([option.route])
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
                    .background()
                }
            }
        }
        .transition(.move(edge: .leading))
        .animation(.easeInOut, value: menuOpen)
        
    }
}

