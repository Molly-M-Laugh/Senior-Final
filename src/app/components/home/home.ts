import { Component, PLATFORM_ID, afterNextRender, DOCUMENT, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
//import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";
//import { Observable } from 'rxjs';
//import $ from 'jquery';
import { isPlatformBrowser } from '@angular/common';
//import { HighchartsChartComponent, ChartConstructorType } from 'highcharts-angular';
import { CanvasJSAngularChartsModule } from '@canvasjs/angular-charts';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule, CanvasJSAngularChartsModule],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home{
  //private readonly platform = inject(PLATFORM_ID);
  //private readonly document = inject(DOCUMENT);
  optionsForm:FormGroup;
  router = inject(Router);
  dps = [{x: 1, y: 10}, {x: 2, y: 13}, {x: 3, y: 18}, {x: 4, y: 20}, {x: 5, y: 17},{x: 6, y: 10}, {x: 7, y: 13}, {x: 8, y: 18}, {x: 9, y: 20}, {x: 10, y: 17}];
	chart: any;
	
	chartOptions = {
	  exportEnabled: true,
	  title: {
		text: "Angular Dynamic Chart"
	  },
	  data: [{
		type: "line",
		dataPoints: this.dps
	  }]
	}
	getChartInstance(chart: object) {
		this.chart = chart;
		setTimeout(this.updateChart, 1000); //Chart updated every 1 second
	}
	updateChart = () => {
		var yVal = this.dps[this.dps.length - 1].y +  Math.round(5 + Math.random() *(-5-5));
		this.dps.push({x: this.dps[this.dps.length - 1].x + 1, y: yVal});
 
		if (this.dps.length >  10 ) {
			this.dps.shift();
		}
		this.chart.render();
		setTimeout(this.updateChart, 1000); //Chart updated every 1 second
	}

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings")
  }
}
