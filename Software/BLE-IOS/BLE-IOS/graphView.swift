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
    var manager: BLEHandler
    var times = ["Last 5 Minutes", "Last Hour", "Last 24 hours", "Last week"]
    @State var currentDuration = "Last 5 Minutes"
    @State private var isMenuOpen = false
    
    var body: some View{
        ZStack{
            VStack{
                Picker("Duration", selection: $currentDuration){
                    //change option based on duration
                    ForEach(times, id: \.self){ time in
                        Text(time)
                    }
                }
                Text("\(manager.lastDataValue[0])")
                Chart {
                    ForEach(manager.tempValues) { data in
                        LineMark(x: .value("x", data.x, unit:.second), y: .value("y", data.y))
                    }
                }
                .chartXVisibleDomain(length: 60)
                .frame(width: 350, height: 200)
                .padding()
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

