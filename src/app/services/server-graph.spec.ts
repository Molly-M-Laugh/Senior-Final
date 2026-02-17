import { TestBed } from '@angular/core/testing';

import { ServerGraph } from './server-graph';

describe('ServerGraph', () => {
  let service: ServerGraph;

  beforeEach(() => {
    TestBed.configureTestingModule({});
    service = TestBed.inject(ServerGraph);
  });

  it('should be created', () => {
    expect(service).toBeTruthy();
  });
});
