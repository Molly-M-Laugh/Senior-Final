import { ComponentFixture, TestBed } from '@angular/core/testing';

import { Trevor } from './trevor';

describe('Trevor', () => {
  let component: Trevor;
  let fixture: ComponentFixture<Trevor>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Trevor]
    })
    .compileComponents();

    fixture = TestBed.createComponent(Trevor);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
