import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';

@Component({
  selector: 'app-login',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './login.html',
  styleUrl: './login.css',
})
export class Login {
  loginForm:FormGroup;
  creationForm:FormGroup;
  router = inject(Router);

  constructor(private fb:FormBuilder){
    this.loginForm=this.fb.group({
      username:["",[Validators.required,Validators.minLength(3)]],
      password:["",[Validators.required,Validators.minLength(3)]]
    });
    this.creationForm=this.fb.group({});
  }

  // No auth yet, so running easy (not actual password) for testing routing on press
  login(){
    if (this.loginForm.value.username == "example@ece.com" 
        && this.loginForm.value.password == "passed") {
      this.router.navigateByUrl("/home")
    } else 
    {
      alert("Invalid login")
    }
  }

  toCreate() {
    this.router.navigateByUrl("/new_user")
  }
}
