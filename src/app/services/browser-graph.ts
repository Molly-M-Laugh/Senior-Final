import { Injectable } from '@angular/core';
import { AnalyticsService } from './analytics-service';

@Injectable({
  providedIn: 'root',
})
export class BrowserGraph implements AnalyticsService {
  trackEvent(name: string): void {
    // Retrieves/sends out event from server (ie. input from sensor into graph form)
  }
}
