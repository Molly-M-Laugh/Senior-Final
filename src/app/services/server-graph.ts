import { Injectable } from '@angular/core';
import { AnalyticsService } from './analytics-service';

@Injectable({
  providedIn: 'root',
})
export class ServerGraph implements AnalyticsService {
  trackEvent(name: string): void {
    // Record event on server (ie. input from sensor)
  }
}
