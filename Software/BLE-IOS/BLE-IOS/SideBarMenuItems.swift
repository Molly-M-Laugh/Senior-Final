//
//  SideBarMenuItems.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 4/13/26.
//

import Foundation

enum SideBarMenuItems: Int, CaseIterable{
    case login
    case register
    case home
    case graph
    
    var name: String {
        switch self {
        case .login:
            return "Login"
        case .register:
            return "Register New User"
        case .home:
            return "Home"
        case .graph:
            return "Graph"
        }
    }
    
}
extension SideBarMenuItems: Identifiable{
    var id: Int {return self.rawValue}
}
