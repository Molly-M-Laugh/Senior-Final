import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { HttpClient} from '@angular/common/http';

@Component({
  selector: 'app-settings',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './settings.html',
  styleUrl: './settings.css',
})
export class Settings {
  optionsForm1:FormGroup;
  optionsForm2:FormGroup;
  homeForm:FormGroup;
  router = inject(Router);
  unactiveUpdate = false; // No double register submissions

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.optionsForm1=this.fb.group({
      username:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]]
    });
    this.optionsForm2=this.fb.group({
      password:["",[Validators.required,Validators.minLength(3), Validators.maxLength(30)]]
    });
    this.homeForm = this.fb.group({});
  }

  home(){
    this.router.navigateByUrl("/home")
  }


  // Implement once cookies are a thing
  updateUsername() {
    //if (this.optionsForm1.invalid || this.unactiveUpdate) return;

    //this.unactiveUpdate = true; // disable until response (for either update)

    //this.http.post<{is_match : boolean}>('http://localhost:8080/api/login', this.optionsForm1.value)
    //this.http.post<{is_match : boolean}>('api/register', this.loginForm.value)
  }

  updatePassword() {

  }
}
