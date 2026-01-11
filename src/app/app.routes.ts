import { Routes } from '@angular/router';
import { Dashboard } from './components/dashboard/dashboard';
import { Colin } from './components/colin/colin';
import { Molly } from './components/molly/molly';
import { Trevor } from './components/trevor/trevor';
import { Dani } from './components/dani/dani';


// export const routes: Routes = []
export const routes: Routes = [
    {path:'dashboard',component: Dashboard},
    {path: '', component: Dashboard},
    {path:'colin', component:Colin},
    {path:'molly',component:Molly},
    {path:'trevor',component:Trevor},
    {path:'dani',component:Dani}
];
