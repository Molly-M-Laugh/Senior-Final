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
  
  router = inject(Router);
  loggedIn = new BehaviorSubject<boolean>(false);
  redirectUrl: string | null = null;

  constructor(private http:HttpClient){}

  login(password:string): Observable<any>{
    return this.http.post(`${this.apiUrl}/login`, {password: password}, {withCredentials:true}).pipe(
      tap(() => {
        this.loggedIn.next(true);
      }),
      catchError((error: HttpErrorResponse) => {
        if(error.status === 401){
          this.loggedIn.next(false);
        }
        return throwError(() => error);
      }
    )
  )}

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