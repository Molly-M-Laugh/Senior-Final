import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';

@Component({
  selector: 'app-new-user',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './new-user.html',
  styleUrl: './new-user.css',
})
export class NewUser {
  userForm:FormGroup;
  returnForm:FormGroup;
  router = inject(Router);

  constructor(private fb:FormBuilder){
    this.userForm=this.fb.group({
      username:["",[Validators.required,Validators.minLength(3)]],
      password:["",[Validators.required,Validators.minLength(3)]]
    });
    this.returnForm=this.fb.group({});
  }

  // No auth yet, so running easy (not actual password) for testing routing on press
  createUser(){
    if (this.userForm.value.username == "example@ece.com" 
        && this.userForm.value.password == "passed") {
      this.router.navigateByUrl("/home")
    } else 
    {
      alert("Invalid credentials")
    }
  }
  cancelNewUser() {
    this.router.navigateByUrl("/login")
  }
}
