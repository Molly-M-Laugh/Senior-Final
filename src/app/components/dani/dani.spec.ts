import { ComponentFixture, TestBed } from '@angular/core/testing';

import { Dani } from './dani';

describe('Dani', () => {
  let component: Dani;
  let fixture: ComponentFixture<Dani>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Dani]
    })
    .compileComponents();

    fixture = TestBed.createComponent(Dani);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
