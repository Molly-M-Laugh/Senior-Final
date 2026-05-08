//
//  HttpHandler.swift
//  BLE-IOS
//
//  Created by Colin Sampey on 3/4/26.
//
import Foundation
import Alamofire
import SwiftUI

struct pushRequest: Encodable, Sendable{
    let a_user_id : Int
    let b_record_date : String
    let c_temp_avg : String
    let d_temp_1 : String
    let e_temp_2 : String
    let f_temp_3 : String
}

nonisolated struct pullRequest: Decodable, Sendable{
    let record_date : String
    let temperature_avg : String
}


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
        
        registerAttempt = true
        let url = baseAPI + "register"
        let currentLogin : Parameters = [
            "username": username,
            "password": password
        ]
        
        
        AF.request(url, method: .post, parameters: currentLogin, encoding: JSONEncoding.default, headers:nil).responseData { response in
            var working = Bool()
            switch response.result {
            case .success(_):
            
                self.registerResults = true
                working = true
                
            case .failure(let error):
                print(error)
                self.registerResults = false
                working = false
            }
            completion(working)
        }
        
        return
    }
    
    
    
    
    func pullFromDB(username:String, password:String, completion: @escaping ([ChartData]) -> Void){
        let url = baseAPI + "data-init"
        let currentLogin: Parameters = [
            "username": username,
            "password": password
        ]
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        
        //let date = dateFormatter.date(from:isoDate)!
        AF.request(url, method:.get).responseDecodable(of: [pullRequest].self){ response in
                
                var result : [ChartData] = []
                
                switch response.result{
                case .success(_):
                    for i in 0...(response.value!.count - 1){
                        print(response.value![i].record_date)
                        let date = formatter.date(from:response.value![i].record_date)!
                        result.append(ChartData(sensor: "all", x: date, y: Float(response.value![i].temperature_avg)!))
                    }
                case.failure(let error):
                    
                    print(error)
                }
                completion(result)
                
            }
        return
        
    }
    
    func pushToDB(username:String, password:String, dataToPush: [ChartData], averageData: ChartData, completion: @escaping (Bool) -> Void){
        let url = baseAPI + "data"
        
        let currentTime = DateFormatter()
        currentTime.dateFormat = "yyyy-MM-dd hh:mm:ss"
    
        let parameters : Parameters = [
            "user" : 1,
            "time": currentTime.string(from:dataToPush[dataToPush.count-3].x),
            "temp_avg": averageData.y,
            "temp_1" : dataToPush[dataToPush.count-3].y,
            "temp_2" : dataToPush[dataToPush.count-2].y,
            "temp_3" : dataToPush[dataToPush.count-1].y
        ]
        
        AF.request(
            url,
            method: .post,
            parameters: parameters,
            encoding: JSONEncoding(options: .sortedKeys),
            headers: nil
        ).response {response in
                var working = Bool()
                switch response.result {
                case .success(_):
                    working = true
                    
                case .failure(let error):
                    working = false
                }
                completion(working)
            }
        return
    }
    
}
