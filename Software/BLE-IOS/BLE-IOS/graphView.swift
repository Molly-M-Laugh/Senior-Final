//
//  graphView.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI
import CoreBluetooth
import Charts

struct ChartData: Identifiable {
    let id = UUID()
    let x : Date
    let y : Float
}


struct graphView: View{
    @ObservedObject var router: Router
    @ObservedObject var manager: BLEHandler
    @State private var isMenuOpen = false
    
    var body: some View{
        ZStack{
            VStack{
                Text("\(manager.lastValue)")
                Chart {
                    ForEach(manager.values) { data in
                        LineMark(x: .value("x", data.x, unit:.second), y: .value("y", data.y))
                    }
                }
                .chartXVisibleDomain(length: 60)
                .frame(width: 350, height: 200)
                .padding()
                
                Button("Reset Graph", action:{
                    manager.resetGraph()
                })
                .buttonStyle(.bordered)
            }
            .padding()
            .navigationBarBackButtonHidden(true)
            
            SideBarMenuView(menuOpen: $isMenuOpen, router: router, manager: manager)
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

