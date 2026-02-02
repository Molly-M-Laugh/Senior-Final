import { Routes } from '@angular/router';
import { Home } from './components/home/home';
import { Login } from './components/login/login';
import { Settings } from './components/settings/settings';


// export const routes: Routes = []
export const routes: Routes = [
    {path:'home',component: Home},
    {path:'login', component:Login},
    {path:'settings',component:Settings},
    {path: '', redirectTo:'/login', pathMatch:'full'} // Autoredirect here, and only exact url
];
