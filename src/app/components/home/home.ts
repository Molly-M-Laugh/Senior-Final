import { Component, PLATFORM_ID, afterNextRender, DOCUMENT, inject } from '@angular/core';
import { CommonModule } from '@angular/common';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
//import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";
//import { Observable } from 'rxjs';
//import $ from 'jquery';
import { isPlatformBrowser } from '@angular/common';
import { HighchartsChartComponent, ChartConstructorType } from 'highcharts-angular';

import { CanvasJSAngularChartsModule } from '@canvasjs/angular-charts';
import { CanvasJSAngularStockChartsModule } from '@canvasjs/angular-stockcharts';


@Component({
  selector: 'app-home',
  standalone: true,
  imports: [CommonModule, RouterOutlet, FormsModule, ReactiveFormsModule, CanvasJSAngularStockChartsModule],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home{
  //private readonly platform = inject(PLATFORM_ID);
  //private readonly document = inject(DOCUMENT);
  optionsForm:FormGroup;
  router = inject(Router);
  chart: any;
  
  chartOptions = {
    title: { text:'Testing a new graph'},
    theme: 'light2',
    charts:[{
      data: [{
        type:"line",
        dataPoints : [
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
        ],
      }]
    }],
  }
  //chartConstructor: ChartConstructorType = 'chart';

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings")
  }
}
