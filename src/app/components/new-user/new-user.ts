import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { HttpClient } from '@angular/common/http';

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

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.userForm=this.fb.group({
      username:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]],
      password:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]]
    });
    this.returnForm=this.fb.group({});
  }

  // No auth yet, so running easy (not actual password) for testing routing on press
  createUser(){
    this.http.post('http://localhost:3000/api/', this.userForm.value)
      .subscribe(response => console.log('Saved', response));
    /*
    if (this.userForm.value.username == "example@ece.com" 
        && this.userForm.value.password == "passed") {
      this.router.navigateByUrl("/home")
    } else 
    {
      alert("Invalid credentials")
    }
      */
  }
  cancelNewUser() {
    this.router.navigateByUrl("/login")
  }
}
