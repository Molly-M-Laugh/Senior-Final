import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';

@Component({
  selector: 'app-settings',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './settings.html',
  styleUrl: './settings.css',
})
export class Settings {
  optionsForm:FormGroup;
  router = inject(Router);

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
  }

  home(){
    this.router.navigateByUrl("/home")
  }
}
