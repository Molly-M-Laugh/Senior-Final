import { ComponentFixture, TestBed } from '@angular/core/testing';

import { Molly } from './molly';

describe('Molly', () => {
  let component: Molly;
  let fixture: ComponentFixture<Molly>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      imports: [Molly]
    })
    .compileComponents();

    fixture = TestBed.createComponent(Molly);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
