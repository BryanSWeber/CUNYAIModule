#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\ConstantElasticityofSubstitution.h"

using namespace std;

//complete but long creator method. Normalizes to 1 automatically.
CES::CES( double a_army, double army_stk, bool army_possible, double a_tech, double tech_stk, bool tech_possible, double a_econ, double wk_stk, bool econ_possible )
{
    // CES using form:    Y = ((pi_0)*(tech^alpha_tech * labor) ^ ((sigma - 1) / sigma) + (1 - pi_0)(1 * capital) ^ ((sigma - 1) / sigma)) ^ )(1 - sigma) / sigma);  see: Klump et al. (2007),  E.C.B: WORKING PAPER SERIES  NO 1294 / FEBRUARY 2011

    pi_0 = 
    sigma = 
    alpha_tech = a_tech;

    worker_stock = wk_stk;
    army_stock = army_stk;
    tech_stock = tech_stk;

    econ_derivative = (alpha_econ / worker_stock) * econ_possible;
    army_derivative = (alpha_army / army_stock) * army_possible;
    tech_derivative = (alpha_tech / tech_stock) * tech_possible;

}

// Protected from failure in divide by 0 case.
double CES::getlny()
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
double CES::getlnY()
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
double CES::getPriority() {
    double derivatives[3] = { econ_derivative , army_derivative, tech_derivative };
    double priority = *(max_element( begin( derivatives ), end( derivatives ) ));
    return priority;
}

//Identifies priority type
bool CES::army_starved()
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
bool CES::econ_starved()
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
bool CES::tech_starved()
{
    if ( tech_derivative == getPriority() )
    {
        return true;
    }
    else {
        return false;
    }
}
