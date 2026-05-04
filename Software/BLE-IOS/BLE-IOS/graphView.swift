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

enum durations: String, CaseIterable, Identifiable {
    case last5, last60, last24h, lastWeek
    var id: Self {self}
    var duration: Double {
        switch self {
        case .last5:
            return 300
        case .last60:
            return 3600
        case .last24h:
            return 84600
        case .lastWeek:
            return 604800
        }
    }
}



struct graphView: View{
    @ObservedObject var router: Router
    @ObservedObject var manager: BLEHandler
    var times = ["Last 5 Minutes", "Last Hour", "Last 24 hours", "Last week"]
    @State var currentDuration : durations = .last5
    @State private var isMenuOpen = false
    
    var body: some View{
        ZStack{
            VStack{
                Picker("Duration", selection: $currentDuration){
                    //change option based on duration
                    Text("Last 5 Minutes").tag(durations.last5)
                    Text("Last Hour").tag(durations.last60)
                    Text("Last 24 hours").tag(durations.last24h)
                    Text("Last Week").tag(durations.lastWeek)
                }
                .pickerStyle(.segmented)
                Text("\(manager.lastDataValue)")
                Chart {
                    ForEach(manager.tempValues) { data in
                        LineMark(x: .value("x", data.x, unit:.second), y: .value("y", data.y))
                    }
                }
                .chartXScale(domain: ClosedRange(uncheckedBounds: (Date() - currentDuration.duration, Date())))
                .frame(width: 350, height: 200)
                .padding()
                .clipped()
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

