import { Component, OnDestroy, OnInit, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import { CanvasJSAngularChartsModule } from '@canvasjs/angular-charts';
import { interval, Subscription } from 'rxjs';
import { HttpClient} from '@angular/common/http';
import { BleService } from '../../services/ble-service';
import { DataService } from '../../services/data-service';
import { Data } from '../../data';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule, CanvasJSAngularChartsModule],
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
  chart: any;
  private startTime: Date = new Date('2023-10-27T10:00:00'); // Keep as such but reassign when get first date value

  private ble = inject(BleService);
  // Items is for testing only, delete instances when moving to finalize
  items: any = { x: new Date().toUTCString(), xSeconds: new Date().getTime(), difference: 0, y: 0 };
  isConnected = false;
  private scale = 5 * 60 * 1000; // Default scale is last 5 minutes (1000 ms/s * 60 s/min * 5 min)
  private launchTime = new Date();
  private formatString = "HH:mm:ss"
	
  // Note: Find way to create new chart instance as hard time shifting down frame
  // as well as implement data shifting frame when updating graph and not just on scale shift
	chartOptions = {
	  exportEnabled: true,
	  title: {
		text: "Bluetooth random data"
	  },
    axisX: {
      title: "Time Passed",
      valueFormatString: this.formatString
    },
	  data: [{
		type: "line",
    xValueType: "dateTime",
		dataPoints: [] as { x: Date; y: number }[]
	  }]
	}

  ngOnInit(): void {
    setTimeout(() => {
      this.dataService.getData()
      .subscribe({
        next: (response: any[]) => {
          if (response != null) {
            // Change the scale later, but for seconds stay to last 30 minutes - actually last 5 minutes
            const now = new Date().getTime();
            let thirtyMinutesAgo = now - this.scale;

            this.chartOptions.data[0].dataPoints =response
              .map(item => {
                // Postgres strings like "2026-04-19 21:20:33.724+00" 
                // are usually parsed correctly by new Date() as UTC, 
                // but let's be explicit.
                const recordDate = new Date(item.record_date);
    
                return {
                  x: recordDate,
                  y: parseFloat(item.temperature)
                };
              })
              .filter(point => point.x.getTime() >= thirtyMinutesAgo) // Filter AFTER mapping
              .slice(-100);
            /*(response
              .filter(item => {
                const itemDate = new Date(item.record_date);
                return itemDate >= thirtyMinutesAgo; // Only keep recent data
              })
              .map(item => {
                console.log(item)
                console.log("")
                const dateStr = item.record_date;
                // Strip milliseconds and Z to be a valid date
                const cleanDate = dateStr.includes('.') ? dateStr.split('.')[0] : dateStr;
                console.log(item)
                console.log("")

                return {
                  x: new Date(cleanDate),
                  y: parseFloat(item.temperature)
                };
              })).slice(-100); // Keep only last 20 values for visability
              */

              console.log("Initial Load")
              console.log(this.chartOptions.data[0].dataPoints)
              console.log("")
              console.log(response)

        } else {
          console.log("Response is empty");
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        }
      });
      // Update axis as well and render full chart
      let minDate = new Date(this.launchTime.getTime() - this.scale);
      this.chart.axisX[0].set("viewportMinimum", minDate.getTime());
      this.chart.axisX[0].set("viewportMaximum", new Date().getTime());
      this.chart.render();
    }, 1000);

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
  
  getChartInstance(chart: object) {
		this.chart = chart;
    this.updateChart();
    // Interval rather set in ngOnInit
	}

  // BLE will update at its own rate, but repeat data retrieval every second
  // Would need to re-insert logic to pause graph when disconnected if wanted
  updateChart() {
    // Get new data
    this.dataService.getData()
      .subscribe({
        next: (response: any[]) => {
          if (response) {
            let thirtyMinutesAgo = new Date(Date.now() - this.scale);

            // Update the dataPoints reference
            this.chartOptions.data[0].dataPoints = response
              .filter(item => new Date(item.record_date) >= thirtyMinutesAgo)
              .map(item => ({
                x: new Date(item.record_date.endsWith('Z') ? item.record_date : item.record_date + 'Z'),
                //x: new Date(item.record_date.replace(' ', 'T')), // Ensure ISO format
                y: parseFloat(item.temperature)
              }))
              .slice(-100);

            if (this.chart) {
              let minDate = new Date(this.launchTime.getTime() - this.scale);
              this.chart.axisX[0].set("viewportMinimum", minDate.getTime());
              this.chart.axisX[0].set("viewportMaximum", new Date().getTime());
              this.chart.render();
            }
          }
        }
      });
  }

  onSelectionChange(value: string) {
    this.launchTime = new Date();
    this.launchTime = new Date(this.launchTime.getTime() + 10 * 1000); // While rest builds, put up to 10 seconds ahead

    if (value.match('s')) {
      this.scale = 5 * 60 * 1000; // 1000 ms/s * 60 s/min * 5 min = Last 5 minutes
      this.formatString = "HH:mm:ss"
    } else if (value.match('m')) {
      this.scale = 60 * 60 * 1000; // 1000 ms/s * 60 s/min * 60 min/hr = Last hour
      this.formatString = "HH:mm:ss"
    } else if (value.match('h')) {
      this.scale = 24 * 60 * 60 * 1000; // 1000 ms/s * 60 s/min * 60 min/hr * 24 hr = Last day
      this.formatString = "MMM D HH:mm:ss"
    } else if (value.match('W')) {
      this.scale = 7 * 24 * 60 * 60 * 1000; // 1000 ms/s * 60 s/min * 60 min/hr * 24 hr/days * 7 days = Last week
      this.formatString = "MMM D HH:mm:ss"
    } else {
      return;
    }

    // Reset the graph scale
    let minDate = new Date(this.launchTime.getTime() - this.scale);
    this.chart.axisX[0].set("valueFormatString", this.formatString);
    this.chart.axisX[0].set("viewportMinimum", minDate.getTime());
    this.chart.axisX[0].set("viewportMaximum", new Date().getTime());

    this.dataService.getData()
      .subscribe({
        next: (response: any[]) => {
          if (response) {
            const thirtyMinutesAgo = new Date(Date.now() - this.scale);

            // Update the dataPoints reference
            this.chartOptions.data[0].dataPoints = response
              .filter(item => new Date(item.record_date) >= thirtyMinutesAgo)
              .map(item => ({
                x: new Date(item.record_date.endsWith('Z') ? item.record_date : item.record_date + 'Z'),
                //x: new Date(item.record_date.replace(' ', 'T')), // Ensure ISO format
                y: parseFloat(item.temperature)
              }))
              .slice(-100);

            if (this.chart) {
              this.chart.render();
            }
          } else {
          console.log("Response is empty");
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        }
      });
  }

  // General (form) items  below --------------------------------------------------------------
  constructor(private fb:FormBuilder, private http: HttpClient){
    this.optionsForm=this.fb.group({});
  }

  settings(){
    this.router.navigateByUrl("/settings");
  }

  // BLE items below -------------------------------------------------------------------------
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
        } else {
          this.updateChart();
        }
        },
        error: (err) => {
          alert("Login failed on invalid credentials or unable to link");
        }
      });

    //this.items = { x: dateItem.toUTCString(),xSeconds: dateItem.getTime(), y: latestValue};
  
    // The ESP32 will then run BtTemp->notify(), which 
    // automatically updates this.items via the subscription in ngOnInit/updateChart.
  }
}