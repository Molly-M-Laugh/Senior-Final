import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { HttpClient } from '@angular/common/http';

@Component({
  selector: 'app-login',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './login.html',
  styleUrl: './login.css',
})
export class Login {
  loginForm:FormGroup;
  creationForm:FormGroup;
  unactiveLogin = false; // No double register submissions
  router = inject(Router);

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.loginForm=this.fb.group({
      username:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]],
      password:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]]
    });
    this.creationForm=this.fb.group({});
  }

  // No auth yet, so running easy (not actual password) for testing routing on press
  login(){
    if (this.loginForm.invalid || this.unactiveLogin) return;

    this.unactiveLogin = true; // disable until response

    this.http.post<{is_match : boolean}>('http://localhost:8080/api/login', this.loginForm.value)
    //this.http.post<{is_match : boolean}>('api/login', this.loginForm.value)
      .subscribe({
        next: response => {
        if (response.is_match) {
          this.router.navigateByUrl("/home");
        } else {
          alert("Invalid login");
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        },
        complete: () => {
          this.unactiveLogin = false; // re-enable after request finishes
        }
      });

      /*
    if (this.loginForm.value.username == "example@ece.com" 
        && this.loginForm.value.password == "passed") {
      this.router.navigateByUrl("/home")
    } else 
    {
      alert("Invalid login")
    }
      */
  }

  toCreate() {
    this.router.navigateByUrl("/new_user");
  }
}
