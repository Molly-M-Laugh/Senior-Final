<<<<<<< HEAD
import { Component, signal } from '@angular/core';
import { RouterLink, RouterOutlet } from '@angular/router';
import { Dashboard } from './components/dashboard/dashboard';
import { Colin } from './components/colin/colin';
import { Molly } from './components/molly/molly';
import { Trevor } from './components/trevor/trevor';
import { Dani } from './components/dani/dani';

@Component({
  selector: 'app-root',
  imports: [RouterLink, RouterOutlet],
  templateUrl: './app.html',
  styleUrl: './app.css'
})
export class App {
  protected readonly title = signal('team_14_portfolios');
  //protected title = "team_14_portfolios";
}
=======
import { Component, signal } from '@angular/core';
import { RouterLink, RouterOutlet } from '@angular/router';
import { Home } from './components/home/home';
import { Login } from './components/login/login';
import { Settings } from './components/settings/settings';

@Component({
  selector: 'app-root',
  imports: [RouterLink, RouterOutlet],
  templateUrl: './app.html',
  styleUrl: './app.css'
})
export class App {
  protected readonly title = signal('senior-et');
}
>>>>>>> 971498cd181e9cdaacc976631b6fab10b0fa4580
