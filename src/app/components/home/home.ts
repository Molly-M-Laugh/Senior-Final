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
//import { BluetoothCore } from '@manekinekko/angular-web-bluetooth';
//import { map } from 'rxjs/operators';
import { HttpClient} from '@angular/common/http';

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
  // dps = initial values
  dps = [{x: 1, y: 10}, {x: 2, y: 13}, {x: 3, y: 18}, {x: 4, y: 20}, {x: 5, y: 17},{x: 6, y: 10}, {x: 7, y: 13}, {x: 8, y: 18}, {x: 9, y: 20}, {x: 10, y: 17}];
	chart: any;
	
	chartOptions = {
	  exportEnabled: true,
	  title: {
		text: "Bluetooth random data"
	  },
	  data: [{
		type: "line",
		dataPoints: this.dps
	  }]
	}
	getChartInstance(chart: object) {
		this.chart = chart;
		setTimeout(this.updateChart, 1000); //Chart updated every 1 second
    // Note: Change timeout/add checks if no new data
	}
	updateChart = () => {
    // Add check for disconnect/multiple points, but for now, append only 1 new point
		//var yVal = this.dps[this.dps.length - 1].y +  Math.round(5 + Math.random() *(-5-5));
    var yVal = 0; // Declare initial instance
    this.http.get<{x : Date, y : number}>('http://localhost:8080/api/data')
    //this.http.get<{x : Date, y : number}>('api/data')
      .subscribe({
        next: response => {
        console.log('Data received', response) // Only for testing purposes have this
        if (response != null) {
          yVal = response.y;
          console.log('Data relevant');
        } else {
          yVal = 0;
          console.log('No relevant data');
        }
        },
        error: (err) => {
          alert("Unable to link");
        }
      });
		this.dps.push({x: this.dps[this.dps.length - 1].x + 1, y: yVal});
 
		if (this.dps.length >  10 ) {
			this.dps.shift();
		}
		this.chart.render();
		setTimeout(this.updateChart, 1000); //Chart updated every 1 second
	}

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings");
  }


  // BLE code (example from official documentation)
  // IF WAS NON-SSR
  /*
  getDevice() {
    // call this method to get the connected device
    return this.ble.getDevice$();
  }

  stream() {
    // call this method to get a stream of values emitted by the device for a given characteristic
    return this.ble.streamValues$().pipe(
      map((value: DataView) => value.getInt8(0))
    );
  }

  disconnectDevice() {
    // call this method to disconnect from the device. This method will also stop clear all subscribed notifications
    this.ble.disconnectDevice();
  }

  value() {
    console.log('Getting Battery level...');

    return this.ble
      .value$({
        service: 'battery_service',
        characteristic: 'battery_level'
      });
  }
	  */
}
