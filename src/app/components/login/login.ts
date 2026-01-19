import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { RouterOutlet } from '@angular/router';

@Component({
  selector: 'app-login',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './login.html',
  styleUrl: './login.css',
})
export class Login {
  loginForm:FormGroup;

  constructor(private fb:FormBuilder){
    this.loginForm=this.fb.group({
      password:["",[Validators.required,Validators.minLength(3)]]
    });
  }

  login(){}
}
