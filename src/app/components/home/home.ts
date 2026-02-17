import { Component, PLATFORM_ID, afterNextRender, DOCUMENT, inject } from '@angular/core';
import { FormBuilder, FormGroup, FormsModule, ReactiveFormsModule, Validators } from '@angular/forms';
import { Router, RouterOutlet } from '@angular/router';
import * as Plot from "@observablehq/plot";
import {JSDOM} from "jsdom";
//import { Observable } from 'rxjs';
//import $ from 'jquery';
import { isPlatformBrowser } from '@angular/common';
import { AnalyticsService } from '../../services/analytics-service';

@Component({
  selector: 'app-home',
  imports: [RouterOutlet,FormsModule,ReactiveFormsModule],
  templateUrl: './home.html',
  styleUrl: './home.css',
})
export class Home{
  private readonly platform = inject(PLATFORM_ID);
  private readonly document = inject(DOCUMENT);
  optionsForm:FormGroup;
  router = inject(Router);
  chart: any;
  //svg: any;
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
  plotHTML : string = "";
  private analytics = inject(AnalyticsService);
  /*data$ : Observable<any[]> = new Observable(subscriber => { 
    subscriber.next(
      [
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
    ]);
  });
  @ViewChild('chart', { static: true }) chartDiv!: ElementRef;

  ngAfterViewInit(): void {
    this.data$.subscribe(data : any => {
      this.plot(data);
    });
  }

  plot(data: any[]) {
    const plot = Plot.plot({
      marks: [
        Plot.lineY(data, {x: 'date', y: 'value', stroke: 'blue'}),
        Plot.dot(data, {x: 'date', y: 'value'})
      ]
    });
    this.chartDiv.nativeElement.append(plot);
  }
    */

  constructor(private fb:FormBuilder){
    this.optionsForm=this.fb.group({});
    //const {this.document} = new JSDOM("<!DOCTYPE html").window.document;
    if (isPlatformBrowser(this.platform)) {
      // Note: To convert from nodejs to angular interpretation did use google ai
      this.document = new JSDOM("<!DOCTYPE html").window.document
      afterNextRender(() => {
        const plot = Plot.plot({
          document: this.document,
          marks: [Plot.lineY(this.data, {x: 'time (s)', y: 'value (any)'})]
        });
        this.plotHTML = plot.outerHTML;
        /*
        this.chart = $('.chart')
        requestAnimationFrame( () =>{
          this.update();
        });
        */
      });
    }
  }

  /*
  ngAfterViewInit(): void {
    if (isPlatformBrowser(this.platform)) {
      this.chart = $('.chart')
      requestAnimationFrame( () =>{
        this.update();
      });
    }
  }
    */

  onAction() {
    this.analytics.trackEvent('action');
  }


  //////////////
  /*
  update() {
    if (isPlatformBrowser(this.platform)) {
      // Fill in later as get synch data
      this.plot();
      requestAnimationFrame(() => this.update());
    }
  }

  plot() {
    if (isPlatformBrowser(this.platform)) {
      const svg = Plot.plot({
        marks: [
          Plot.frame(),
          Plot.lineY(this.data, {x: "time (s)", y: "random value"})
        ]
      });
      this.chart.html(svg);
    }
  }
    */
  //////////////////
  settings(){
    this.router.navigateByUrl("/settings")
  }
}
