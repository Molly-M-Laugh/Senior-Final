//
//  Router.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import SwiftUI
internal import Combine
import Foundation
import Observation

enum Route: Hashable{
    case home
    case login
    case register
    case graph
}

@MainActor
class Router: ObservableObject{
  
    @Published var path: [Route] = []
    //Need to change this to have dynamic settings between logged in and out
    func setPath(_ newPath: [Route]){
        path = newPath
    }
    
    func reset(){
        path.removeAll()
    }
    func backOne()
    {
       _ = path.popLast()
    }
    func forwardOne(_ route: Route){
        path.append(route)
    }
}

