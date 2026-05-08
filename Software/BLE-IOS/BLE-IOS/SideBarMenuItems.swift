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
    case settings
    case logout
    
    
    var name: String {
        switch self {
        case .home:
            return "Home"
        case .graph:
            return "Graph"
        case .settings:
            return "Settings"
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
        case .settings:
            return .settings
        case .logout:
            return .login
        }
    }
    
}
extension SideBarMenuItems: Identifiable{
    var id: Int {return self.rawValue}
}
