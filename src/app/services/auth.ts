import { Injectable, inject, signal} from '@angular/core';
import { HttpClient, HttpErrorResponse} from '@angular/common/http';
import { Router } from '@angular/router';
import { BehaviorSubject, Observable, of, throwError } from 'rxjs';
import { catchError, map, tap } from 'rxjs/operators';


@Injectable({
  providedIn: 'root',
})
export class Auth {
  private apiUrl = 'http://localhost:8080';
  private tokenKey = 'jwt_token';
  router = inject(Router);
  loggedIn = new BehaviorSubject<boolean>(false);
  redirectUrl: string | null = null;

  constructor(private http:HttpClient){}

  login(password:string){
    return this.http.post(`${this.apiUrl}/login`, {password: password}, {withCredentials:true})
    .subscribe((response: any) => {
      localStorage.setItem(this.tokenKey, response.token);
      this.router.navigate(['/dashboard']);
      
    });
  }

  logout(): void{
    this.redirectUrl = null;
    this.http.post(`${this.apiUrl}/logout`, {}, {withCredentials:true}).subscribe(() => this.loggedIn.next(false));
  }

  isAuthenticated(): Observable<any>{
    return this.http.get(`${this.apiUrl}/verify`, {withCredentials:true}).pipe(
      map(() => {
        this.loggedIn.next(true);
        return true;
    }),
    catchError(() => {
      this.loggedIn.next(false);
      return of(false);
    })
  );
  }


}