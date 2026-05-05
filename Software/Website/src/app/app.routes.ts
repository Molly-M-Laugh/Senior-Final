import { Routes } from '@angular/router';
import { Home } from './components/home/home';
import { Login } from './components/login/login';
import { Settings } from './components/settings/settings';
import { NewUser } from './components/new-user/new-user';


// export const routes: Routes = []
export const routes: Routes = [
    {path:'', component:Login},
    {path:'home',component: Home},
    {path:'login', component:Login},
    {path:'settings',component:Settings},
    {path:'new_user',component:NewUser}
    //{path: '', redirectTo:'login', pathMatch:'full'} // Autoredirect here, and only exact url
];
