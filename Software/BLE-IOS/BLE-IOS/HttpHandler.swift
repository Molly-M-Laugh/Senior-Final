//
//  HttpHandler.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 3/4/26.
//
import Foundation
import Alamofire
import SwiftUI

class HttpHandler: NSObject{
    
    
    
    
    @State var loginAttempt = false
    @State var registerAttempt = false
    @State var syncAttempt = false
    
    var loginResults = false
    var registerResults = false
    
    private var baseAPI: String = "https://senior-t-fd5496756068.herokuapp.com/api/"
    
    
    
    
    
    
    func login(username:String, password:String, completion: @escaping (Bool) -> Void){
        if loginAttempt == true {
            return
        }
        loginAttempt = true
        let url = baseAPI + "login"
        let currentLogin: Parameters = [
            "username": username,
            "password": password
        ]
        print("attempting request")
        AF.request(url, method: .post, parameters: currentLogin, encoding: JSONEncoding.default, headers:nil).responseData { (response) in
            var working = Bool()
            switch response.result {
            case .success(_):
                if(response.response?.statusCode == 200){
                    self.loginResults = true
                    working = true
                }
                else{
                    self.loginResults = false
                    working = false
                }
                
            case .failure(_):
                print("login failed")
                self.loginResults = false
                working = false
            }
            completion(working)
        }
        return
    }
    
    func register(username:String, password:String, completion: @escaping (Bool) -> Void){
        if registerAttempt == true {
            return
        }
        print("Starting Register")
        registerAttempt = true
        let url = baseAPI + "register"
        let currentLogin : Parameters = [
            "username": username,
            "password": password
        ]
        print("Register 1, username is \(username) and password is \(password)")
        
        AF.request(url, method: .post, parameters: currentLogin, encoding: JSONEncoding.default, headers:nil).responseData { response in
            var working = Bool()
            switch response.result {
            case .success(_):
                print("Register 2")
                self.registerResults = true
                working = true
                
            case .failure(let error):
                print("Register 3")
                print(error)
                self.registerResults = false
                working = false
            }
            completion(working)
        }
        
        return
    }
    
}
