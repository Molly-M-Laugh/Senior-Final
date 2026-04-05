import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';
import { Data } from '../data'; // Import interface

@Injectable({
  providedIn: 'root'
})
export class DataService {
  private http = inject(HttpClient); // Modern inject() in Angular
  private apiUrl = 'http://localhost:8080/api/data-init'; // Used only for local
  //private apiUrl = 'https://senior-t-fd5496756068.herokuapp.com/api/data-init'; // Used only for Heroku?
  // Define return type as Observable<User>
  getData(): Observable<Data[]> {
    // Pass the interface to the get method
    return this.http.get<Data[]>(this.apiUrl);
  }
}
