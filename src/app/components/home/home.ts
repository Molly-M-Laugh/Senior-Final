import { Component, PLATFORM_ID, afterNextRender, DOCUMENT, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
//import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";
//import { Observable } from 'rxjs';
//import $ from 'jquery';
import { isPlatformBrowser } from '@angular/common';
import { HighchartsChartComponent, ChartConstructorType } from 'highcharts-angular';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule, HighchartsChartComponent],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home{
  //private readonly platform = inject(PLATFORM_ID);
  //private readonly document = inject(DOCUMENT);
  optionsForm:FormGroup;
  router = inject(Router);
  chart: any;
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
  chartOptions: Highcharts.Options = { 
    title: {
          style: {
            color: 'tomato',
          },
        },
    legend: {
          enabled: false,
        },
    series: [
      {
        data: [1, 2, 3],
        type: 'line',
      },
    ],};
  chartConstructor: ChartConstructorType = 'chart';

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings")
  }
}
