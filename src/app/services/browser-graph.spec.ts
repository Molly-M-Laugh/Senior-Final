import { TestBed } from '@angular/core/testing';

import { BrowserGraph } from './browser-graph';

describe('BrowserGraph', () => {
  let service: BrowserGraph;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(BrowserGraph);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
