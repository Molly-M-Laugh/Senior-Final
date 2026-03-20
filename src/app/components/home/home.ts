import { Component, OnDestroy, OnInit, PLATFORM_ID, afterNextRender, DOCUMENT, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
//import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";
//import { Observable } from 'rxjs';
//import $ from 'jquery';
import { isPlatformBrowser, JsonPipe, DatePipe } from '@angular/common';
//import { HighchartsChartComponent, ChartConstructorType } from 'highcharts-angular';
import { CanvasJSAngularChartsModule } from '@canvasjs/angular-charts';
//import { BluetoothCore } from '@manekinekko/angular-web-bluetooth';
import { interval, Subscription } from 'rxjs';
import { HttpClient} from '@angular/common/http';
import { BleService } from '../../services/ble-service';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule, CanvasJSAngularChartsModule, JsonPipe, DatePipe],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home implements OnInit, OnDestroy {
  //private readonly platform = inject(PLATFORM_ID);
  //private readonly document = inject(DOCUMENT);
  private statusSub?: Subscription;
  private dataIntervalSub?: Subscription;
  hasAttemptedConnection = false;
  optionsForm:FormGroup;
  router = inject(Router);
  // dps = initial values
  dps = [{x: 1, y: 10}, {x: 2, y: 13}, {x: 3, y: 18}, {x: 4, y: 20}, {x: 5, y: 17},{x: 6, y: 10}, {x: 7, y: 13}, {x: 8, y: 18}, {x: 9, y: 20}, {x: 10, y: 17}];
	chart: any;

  private ble = inject(BleService);
  items: any = { x: new Date(), y: 0 };
  isConnected = false;
	
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
  ngOnInit () {
    // 1. Existing data subscriber
    this.ble.deviceValue$.subscribe((val: string) => {
    //const numericValue = parseFloat(val);
    //if (isNaN(numericValue)) return;
    if (val == '-0.01' || val == '0.00') return;

    const numericValue = parseFloat(val);
    if (isNaN(numericValue)) return;

    // Update the local items object for the HTML 
    this.items = { x: new Date(), y: val };

    // Update the DataPoints array for the chart
    this.dps.push({ x: this.dps.length + 1, y: numericValue });

    // Keep the chart from getting too crowded (e.g., last 20 points)
    if (this.dps.length > 20) {
      this.dps.shift();
    }

    // Re-render the chart if the instance exists
    if (this.chart) {
      this.chart.render();
    }
  });

  // 2. Monitor connection status automatically
    this.statusSub = this.ble.isConnected$.subscribe(status => {
      this.isConnected = status;
      if (!status) {
        console.log("Polling paused - waiting for reconnection.");
      } else {
        console.log("Polling active.");
      }
    });
  }

  async initBluetooth() {
    //alert("Button clicked!");
    this.hasAttemptedConnection = true;
    try {
      await this.ble.connect();
      //this.isConnected = true; // Set this here so buttons enable immediately

      // Start the automatic polling once connected
      //this.startAutomaticPolling();
      if (!this.dataIntervalSub) {
        this.startAutomaticPolling();
      }

      console.log("Status updated to connected");
    } catch (err) {
      console.error("Pairing failed", err);
      alert("Bluetooth pairing canceled or failed.");
    }
  }

  startAutomaticPolling() {
    // interval(1000) emits every 1 second
    this.dataIntervalSub = interval(1000).subscribe(async () => {
      if (this.isConnected) {
        await this.requestNewData();
        // With returned string, now has -0.01 for disconnect when not recognized yet for the 3-5 polls before recognizes
      }
    });
  }

  async requestNewData() {
    if (!this.isConnected) return;

    // Sending "5" triggers the 'Increment number' logic in your ESP32 onWrite callback
    await this.ble.sendCommand("5"); 
    const latestValue = await this.ble.read();
    this.items = { x: new Date(),y: latestValue};
  
    // The ESP32 will then run BtTemp->notify(), which 
    // automatically updates this.items via the subscription in ngOnInit.
  }

	getChartInstance(chart: object) {
		this.chart = chart;
		//setTimeout(this.updateChart, 1000); //Chart updated every 1 second
    // Note: Change timeout/add checks if no new data
	}
	updateChart = () => {
    /*
    // Add check for disconnect/multiple points, but for now, append only 1 new point
		//var yVal = this.dps[this.dps.length - 1].y +  Math.round(5 + Math.random() *(-5-5));
    //var yVal = 0; // Declare initial instance
    this.http.get<{x : Date, y : number}>('http://localhost:8080/api/data')
    //this.http.get<{x : Date, y : number}>('api/data')
      .subscribe({
        next: response => {
        console.log('Data received', response) // Only for testing purposes have this
        this.items = response;
        if (response != null) {
          //this.val = response;
          //yVal = response.y;
          console.log('Data relevant');
        } else {
          //this.val = 0;
          console.log('No relevant data');
        }
        },
        error: (err) => {
          alert("Unable to link");
        }
    
      });
		//this.dps.push({x: this.dps[this.dps.length - 1].x + 1, y: yVal});
 
		//if (this.dps.length >  10 ) {
		//	this.dps.shift();
		//}
		//this.chart.render();
    
     var output = document.getElementById('outputText');
     if (output) {
      //output.textContent = this.val;
     } else {
      console.error("Output HTML element not found");
     }
		//setTimeout(this.updateChart, 1000); //Chart updated every 1 second
    */
	}

  // CRITICAL: Clean up the timer when the component is destroyed
  ngOnDestroy() {
    this.statusSub?.unsubscribe();
    this.dataIntervalSub?.unsubscribe();
    //if (this.dataIntervalSub) {
    //  this.dataIntervalSub.unsubscribe();
    //}
  }

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings");
  }
}
