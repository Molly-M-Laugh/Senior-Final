import { Component, signal } from '@angular/core';
import { RouterLink, RouterOutlet } from '@angular/router';
import { Home } from './components/home/home';
import { Login } from './components/login/login';
import { Settings } from './components/settings/settings';
//import { WebBluetoothModule } from '@manekinekko/angular-web-bluetooth'; // If was non-SSR

@Component({
  selector: 'app-root',
  imports: [RouterOutlet],
  templateUrl: './app.html',
  styleUrl: './app.css'
})
export class App {
  protected readonly title = signal('vital-vest');
  //protected title = "senior-et";
}
