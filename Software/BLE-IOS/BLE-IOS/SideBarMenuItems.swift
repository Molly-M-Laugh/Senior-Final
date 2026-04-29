//
//  SideBarMenuItems.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import Foundation

enum SideBarMenuItems: Int, CaseIterable{
    case home
    case graph
    case logout
    
    var name: String {
        switch self {
        case .home:
            return "Home"
        case .graph:
            return "Graph"
        case .logout:
            return "Logout"
        }
        
    }
    var route: Route {
        switch self {
        case .home:
            return .home
        case .graph:
            return .graph
        case .logout:
            return .login
        }
    }
    
}
extension SideBarMenuItems: Identifiable{
    var id: Int {return self.rawValue}
}
