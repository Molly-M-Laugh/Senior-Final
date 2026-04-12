import { Component, OnDestroy, OnInit, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { JsonPipe, DatePipe } from '@angular/common';
import { CanvasJSAngularChartsModule } from '@canvasjs/angular-charts';
import { interval, Subscription } from 'rxjs';
import { HttpClient} from '@angular/common/http';
import { BleService } from '../../services/ble-service';
import { DataService } from '../../services/data-service';
import { Data } from '../../data';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule, CanvasJSAngularChartsModule, JsonPipe, DatePipe],
  templateUrl: './home.html',
  styleUrl: './home.css',
})

// OnInit and OnDestroy in order to keep track of BLE polling for doing when only on the page
export class Home implements OnInit, OnDestroy {
  private statusSub?: Subscription;
  private dataIntervalSub?: Subscription;
  hasAttemptedConnection = false;
  optionsForm:FormGroup;
  router = inject(Router);
  private dataService = inject(DataService);
  data: Data[] = [];
  // dps = graph pushed values, below were templated initial values
  //dps = [{x: 1, y: 10}, {x: 2, y: 13}, {x: 3, y: 18}, {x: 4, y: 20}, {x: 5, y: 17},{x: 6, y: 10}, {x: 7, y: 13}, {x: 8, y: 18}, {x: 9, y: 20}, {x: 10, y: 17}];
	dps: any[] = new Array(10).fill(null).map(() => ({})); // Empty array for then adding to for the chart
  chart: any;
  private startTime: Date = new Date('2023-10-27T10:00:00'); // Keep as such but reassign when get first date value

  private ble = inject(BleService);
  // Items is for testing only, delete instances when moving to finalize
  items: any = { x: new Date(), xSeconds: new Date().getTime(), difference: 0, y: 0 };
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
    // Items under update chart went here before
    // Otherwise, will put the initial values loaded here
    /*
    // Will eventually implement, but for now, need to figure out how to signal to get the return will be array of json
    // (and have services, interfaces to initially try), but for time, will only implement data load for Heroku
    this.dataService.getData()
    //this.http.get('api/data-init', dataValue)
      .subscribe({
        next: response => {
        if (response != null) {
          this.data = response;
          // First see what is returned
          console.log(response[0]);
          //new Date(isoString) // For later converting data back to string
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        }
      });
    */
  }

  async initBluetooth() {
    this.hasAttemptedConnection = true;
    try {
      await this.ble.connect();

      if (!this.dataIntervalSub) {
        this.startAutomaticPolling();
      }

      console.log("Status updated to connected");
    } catch (err) {
      console.error("Pairing failed", err);
      alert("Bluetooth pairing canceled or failed.");
    }
  }

  // This will automatically poll device at least once a second
  startAutomaticPolling() {
    // interval(1000) emits every 1 second
    this.dataIntervalSub = interval(1000).subscribe(async () => {
      if (this.isConnected) {
        await this.requestNewData();
      }
    });
  }

  async requestNewData() {
    if (!this.isConnected) return;

    // Sending "5" triggers the 'Increment number' logic 
    await this.ble.sendCommand("5"); 
    const latestValue = await this.ble.read();
    const dateItem = new Date();
    const dataValue = { user: 1, time: dateItem.toISOString(), temp: latestValue };

    // Make insert to database before displaying debug code
    //this.http.post<{is_inserted : boolean}>('http://localhost:8080/api/data', dataValue)
    this.http.post<{is_inserted : boolean}>('api/data', dataValue)
      .subscribe({
        next: response => {
        if (!response.is_inserted) {
          alert("A data value failed to insert!");
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        }
      });

    this.items = { x: dateItem,xSeconds: dateItem.getTime(), y: latestValue};
  
    // The ESP32 will then run BtTemp->notify(), which 
    // automatically updates this.items via the subscription in ngOnInit/updateChart.
  }

	getChartInstance(chart: object) {
		this.chart = chart;
		setTimeout(this.updateChart, 1000); //Chart updated every 1 second
    // Note: Change timeout/add checks if no new data(?)
	}

	updateChart = () => {
    // 1. Existing data subscriber
    this.ble.deviceValue$.subscribe((val: string) => {
      if (val == '-0.01' || val == '0.00') return;

      const numericValue = parseFloat(val);
      if (isNaN(numericValue)) return;

      // Update the local items object for the HTML (testing) 
      // And otherwise update the graph data object
      const xVal: Date = new Date();
      this.items = { x: xVal, xSeconds: xVal.getTime(), y: val };

      if (this.startTime.getTime() === new Date('2023-10-27T10:00:00').getTime()) { // If first time of data, put 1st as load
        this.startTime = xVal;
        this.dps.push({ x: 0, y: numericValue });
      } else {
        const timeDifSec: number = (xVal.getTime() - this.startTime.getTime()) / 1000;
        this.dps.push({ x: timeDifSec, y:numericValue });

      }

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

  // Clean up the timer when the component is destroyed
  ngOnDestroy() {
    this.statusSub?.unsubscribe();
    this.dataIntervalSub?.unsubscribe();
  }

  constructor(private fb:FormBuilder, private http: HttpClient){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings");
  }
}