import { Component, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import * as Plot from "@observablehq/plot";
import $ from "jquery";

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home{
  optionsForm:FormGroup;
  router = inject(Router);
  chart: any;
  svg: any;
  data = [
    {x:0, y:1},
    {x:1, y:2},
    {x:2, y:3},
    {x:3, y:4},
    {x:4, y:8},
    {x:5, y:5},
    {x:6, y:6},
    {x:7, y:7},
    {x:8, y:8},
    {x:9, y:9},
    {x:10, y:10}
];

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
  }

  ngAfterViewInit(): void {
    this.chart = $('.chart')
    requestAnimationFrame( () =>{
      this.update();
    });
  }

  update() {
    // Fill in later as get synch data
    this.plot();
    requestAnimationFrame(() => this.update());
  }

  plot() {
    const svg = Plot.plot({
      marks: [
        Plot.frame(),
        Plot.lineY(this.data, {x: "time (s)", y: "random value"})
      ]
    });
    this.chart.html(svg);
  }

  settings(){
    this.router.navigateByUrl("/settings")
  }
}
