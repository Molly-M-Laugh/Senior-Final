//
//  BLE-App.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 2/2/26.
//

import SwiftUI

class MyAppDelegate: NSObject, UIApplicationDelegate {
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchConfig: [UIApplication.LaunchOptionsKey: Any]? = nil)-> Bool {
        UNUserNotificationCenter.current().delegate = NotifDelegate.shared
        return true
    }
    
    func application(_ application: UIApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data){
        let tokenParts = deviceToken.map { data in String(format: "%02.2hhx", data)}
        let token = tokenParts.joined()
        print("Device Token: \(token)")
    }
}

class NotifDelegate : NSObject, UNUserNotificationCenterDelegate{
    static let shared = NotifDelegate()
    
    override init(){
        super.init()
        UNUserNotificationCenter.current().delegate = self
    }
    func userNotificationCenter(_ center: UNUserNotificationCenter, willPresent notification: UNNotification, withCompletionHandler completion: @escaping (UNNotificationPresentationOptions) -> Void){
        completion([.banner, .list, .sound])
    }
    
}


@main
struct BLE_App: App {
    @UIApplicationDelegateAdaptor(MyAppDelegate.self) var appDelegate
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
