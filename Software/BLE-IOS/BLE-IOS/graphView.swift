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
    
    var manager = BLEHandler()
    
    var body: some View{
        VStack{
            Chart {
                ForEach(manager.values) { data in
                    LineMark(x: .value("x", data.x), y: .value("y", data.y))
                }
            }
            .frame(width: 350, height: 200)
            .padding()
            Button("Reset Graph", action:{
                manager.resetGraph()
            })
            .buttonStyle(.bordered)
        }
    }
}

