import { mergeApplicationConfig, ApplicationConfig } from '@angular/core';
import { provideServerRendering, withRoutes } from '@angular/ssr';
import { appConfig } from './app.config';
import { serverRoutes } from './app.routes.server';

import { AnalyticsService } from './services/analytics-service';
import { ServerGraph } from './services/server-graph';

const serverConfig: ApplicationConfig = {
  providers: [
    provideServerRendering(withRoutes(serverRoutes)),
    {provide: AnalyticsService, useClass: ServerGraph}
  ]
};

export const config = mergeApplicationConfig(appConfig, serverConfig);
