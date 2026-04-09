import { Injectable, inject, signal} from '@angular/core';
import { HttpClient, HttpErrorResponse} from '@angular/common/http';
import { Router } from '@angular/router';
import { BehaviorSubject, Observable, of, throwError } from 'rxjs';
import { catchError, map, tap } from 'rxjs/operators';


@Injectable({
  providedIn: 'root',
})
export class Auth {
  //private apiUrl = 'http://localhost:8080';
  private tokenKey = 'jwt_token';
  router = inject(Router);
  loggedIn = new BehaviorSubject<boolean>(false);
  redirectUrl: string | null = null;

  constructor(private http:HttpClient){}

  login(username:string, password:string){
    return this.http.post(`api/login`, {username:username, password:password}, {withCredentials:true})
    .pipe(tap(() => {
      this.loggedIn.next(true);}));
    /*
    .subscribe((response: any) => {
      localStorage.setItem(this.tokenKey, response.token);
      this.router.navigate(['/dashboard']);
    });*/
  }

  logout(): void{
    this.redirectUrl = null;
    localStorage.removeItem(this.tokenKey);
    this.http.post(`api/logout`, {}, {withCredentials:true}).subscribe(() => this.loggedIn.next(false));
  }

  isAuthenticated(): boolean{
    const token = localStorage.getItem(this.tokenKey);
    if (!token) return false;
    try {
      const payload = JSON.parse(atob(token.split('.')[1]));
      return payload.exp * 1000 > Date.now();
    }
    catch{
      return false;
    }

  }


}