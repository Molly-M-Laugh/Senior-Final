import { ComponentFixture, TestBed } from '@angular/core/testing';

import { Colin } from './colin';

describe('Colin', () => {
  let component: Colin;
  let fixture: ComponentFixture<Colin>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Colin]
    })
    .compileComponents();

    fixture = TestBed.createComponent(Colin);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
