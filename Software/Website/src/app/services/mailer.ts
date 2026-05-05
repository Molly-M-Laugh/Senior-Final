import { HttpClient } from '@angular/common/http';
import { Injectable, inject } from '@angular/core';
import { Router } from '@angular/router';

@Injectable({
  providedIn: 'root',
})
export class Mailer {
  private apiUrl = 'http://localhost:8080/email';
  
  router = inject(Router);
  constructor(private http: HttpClient){}

  sendEmail(teamMember:string, from:string, message:string){
    return this.http.post(`${this.apiUrl}`, {teamMember: teamMember, sender:from, message:message}, {withCredentials:true});
  }



}