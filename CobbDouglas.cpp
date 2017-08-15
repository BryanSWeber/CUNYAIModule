#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\CobbDouglas.h"

using namespace std;

//complete but long creator method. Normalizes to 1 automatically.
CobbDouglas::CobbDouglas( double a_army, double army_stk, bool army_possible, double a_tech, double tech_stk, bool tech_possible, double a_econ, double wk_stk, bool econ_possible )
{
    double a_tot = a_econ + a_army + a_tech;

    alpha_econ = a_econ / a_tot;
    alpha_army = a_army / a_tot;
    alpha_tech = a_tech / a_tot;

    worker_stock = wk_stk;
    army_stock = army_stk;
    tech_stock = tech_stk;

    econ_derivative = (alpha_econ / worker_stock) * econ_possible;
    army_derivative = (alpha_army / army_stock) * army_possible;
    tech_derivative = (alpha_tech / tech_stock) * tech_possible;

}

// Protected from failure in divide by 0 case.
double CobbDouglas::getlny()
{
    double ln_y = 0;
    try {
        ln_y = alpha_army * log( army_stock / worker_stock ) + alpha_tech * log( tech_stock / worker_stock );

    }
    catch ( ... ) {
        BWAPI::Broodwar->sendText( "Uh oh. We are out of something critical..." );
    };
    return ln_y;

}
// Protected from failure in divide by 0 case.
double CobbDouglas::getlnY()
{
    double ln_Y = 0;
    try {
        ln_Y = alpha_army * log( army_stock ) + alpha_tech * log( tech_stock ) + alpha_econ * log( worker_stock ); //Analog to GDP
    }
    catch ( ... ) {
        BWAPI::Broodwar->sendText( "Uh oh. We are out of something critical..." );
    };
    return ln_Y;
}

// Identifies the value of our main priority.
double CobbDouglas::getPriority() {
    double derivatives[3] = { econ_derivative , army_derivative, tech_derivative }; 
    double priority = *(max_element( begin( derivatives ), end( derivatives ) ));
    return priority;
}

//Identifies priority type
bool CobbDouglas::army_starved()
{
    if ( army_derivative == getPriority() )
    {
        return true;
    }
    else {
        return false;
    }
}

//Identifies priority type
bool CobbDouglas::econ_starved()
{
    if ( econ_derivative == getPriority() )
    {
        return true;
    }
    else {
        return false;
    }
}
//Identifies priority type
bool CobbDouglas::tech_starved()
{
    if ( tech_derivative == getPriority() )
    {
        return true;
    }
    else {
        return false;
    }
}
