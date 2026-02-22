import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { HttpClient} from '@angular/common/http';

@Component({
  selector: 'app-new-user',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './new-user.html',
  styleUrl: './new-user.css',
})
export class NewUser {
  userForm:FormGroup;
  unactiveRegistration = false; // No double register submissions
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
    if (this.userForm.invalid || this.unactiveRegistration) return;

    this.unactiveRegistration = true; // disable until response

    this.http.post('http://localhost:8080/api/register', this.userForm.value)
      .subscribe({
        next: response => {
        //console.log('Saved User', response) // Only for testing purposes have this
        this.router.navigateByUrl("/home")
        },
        error: (err) => {
          alert("Registration failed on invalid credentials or unable to link")
        },
        complete: () => {
          this.unactiveRegistration = false; // re-enable after request finishes
        }
  });
  }
  cancelNewUser() {
    this.router.navigateByUrl("/login")
  }
}
