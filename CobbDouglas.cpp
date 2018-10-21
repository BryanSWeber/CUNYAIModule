#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\CobbDouglas.h"
#include "Source\PlayerModelManager.h"
#include <iostream>
#include <fstream>

using namespace std;

struct Player_Model;

//complete but long creator method. Normalizes to 1 automatically.
void CobbDouglas::evaluateCD(double army_stk, double tech_stk, double wk_stk )
{
    // CD takes the form Y= (A*labor)^alpha_l * capital^(alpha_k). I assume A is a technology augmenting only capital, and takes the form tech^(alpha_t). I can still normalize the sum of alpha_l and alpha_k to 1. 
    double a_tot = alpha_econ + alpha_army;
    alpha_econ = alpha_econ / a_tot;
    alpha_army = alpha_army / a_tot;
    //alpha_tech = alpha_tech; // No change here, it's not normalized.

    worker_stock = wk_stk;
    army_stock = army_stk;
    tech_stock = tech_stk;

    bool army_possible = evalArmyPossible();
    bool econ_possible = evalEconPossible();
    bool tech_possible = evalTechPossible();

    econ_derivative = alpha_econ *                                                           pow(tech_stock, alpha_tech * alpha_army) * pow(army_stock/worker_stock, alpha_army) * econ_possible; // worker stock is incorperated on the RHS to save on a calculation.
    army_derivative = alpha_army *              pow(worker_stock / army_stock, alpha_econ) * pow(tech_stock, alpha_tech * alpha_army) * army_possible;  // army stock is incorperated on the RHS to save on a calculation.  
    tech_derivative = alpha_tech * alpha_army * pow(worker_stock, alpha_econ) *              pow(tech_stock, alpha_tech * alpha_army - 1 ) * pow(army_stock, alpha_army) * tech_possible;
}

// Identifies the value of our main priority.
double CobbDouglas::getPriority() {
    double derivatives[3] = { econ_derivative , army_derivative, tech_derivative }; 
    double priority = *(max_element( begin( derivatives ), end( derivatives ) ));
    return priority;
}

// Protected from failure in divide by 0 case.
double CobbDouglas::getlny() const
{
    double ln_y = 0;
    try {
        //ln_y = alpha_army * log( army_stock / worker_stock ) + alpha_tech * log( tech_stock / worker_stock );
        ln_y = log((pow(army_stock * pow(tech_stock, alpha_tech), alpha_army) + pow(worker_stock, alpha_econ)) / worker_stock); //theory is messier than typical constant growth assumptions. A direct calculation instead.
    }
    catch (...) {
        BWAPI::Broodwar->sendText("Uh oh. We are out of something critical...");
    };
    return ln_y;

}
// Protected from failure in divide by 0 case.
double CobbDouglas::getlnY() const
{
    double ln_Y = 0;
    try {
        //ln_Y = alpha_army * log( army_stock ) + alpha_tech * log( tech_stock ) + alpha_econ * log( worker_stock ); //Analog to GDP
        ln_Y = alpha_army * log(army_stock) + alpha_army * alpha_tech * log(tech_stock) + alpha_econ * log(worker_stock); //Analog to GDP
    }
    catch (...) {
        BWAPI::Broodwar->sendText("Uh oh. We are out of something critical...");
    };
    return ln_Y;
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

void CobbDouglas::estimateCD(int e_army_stock, int e_tech_stock, int e_worker_stock) // FOR MODELING ENEMIES ONLY
{
    double K_over_L = (double)(e_army_stock + 1) / (double)(e_worker_stock + 1); // avoid NAN's
    alpha_army = CUNYAIModule::bindBetween(K_over_L / (double)(1.0 + K_over_L), 0.05, 0.95);
    alpha_econ = CUNYAIModule::bindBetween(1 - alpha_army, 0.05, 0.95);
    alpha_tech = CUNYAIModule::bindBetween(e_tech_stock / (double)(e_worker_stock + 1) * alpha_econ / alpha_army, 0.05, 0.95 );

    army_stock = e_army_stock;
    tech_stock = e_tech_stock;
    worker_stock = e_worker_stock;
}

//Sets enemy utility function parameters based on known information.
void CobbDouglas::enemy_mimic(const Player_Model & enemy, const double adaptation_rate) {
    //If optimally chose, the derivatives will all be equal.

    //Shift alpha towards enemy choices.
    alpha_army += adaptation_rate * (enemy.spending_model_.alpha_army - alpha_army);
    alpha_econ += adaptation_rate * (enemy.spending_model_.alpha_econ - alpha_econ);
    alpha_tech += adaptation_rate * (enemy.spending_model_.alpha_tech - alpha_tech);

    bool army_possible = evalArmyPossible();
    bool econ_possible = evalEconPossible();
    bool tech_possible = evalTechPossible();

    if (isnan(alpha_army))  alpha_army = 0; 
    if (isnan(alpha_econ))  alpha_econ = 0; 
    if (isnan(alpha_tech))  alpha_tech = 0; // this safety might be permitting undiagnosed problems.


    //reevaluate our tech choices.
    econ_derivative = alpha_econ * pow(tech_stock, alpha_tech * alpha_army) * pow(army_stock / worker_stock, alpha_army) * econ_possible; // worker stock is incorperated on the RHS to save on a calculation.
    army_derivative = alpha_army * pow(worker_stock / army_stock, alpha_econ) * pow(tech_stock, alpha_tech * alpha_army) * army_possible;  // army stock is incorperated on the RHS to save on a calculation.  
    tech_derivative = alpha_tech * alpha_army * pow(worker_stock, alpha_econ) *              pow(tech_stock, alpha_tech * alpha_army - 1) * pow(army_stock, alpha_army) * tech_possible;

    if (isnan(econ_derivative)) econ_derivative = 0;
    if (isnan(army_derivative)) army_derivative = 0;
    if (isnan(tech_derivative)) tech_derivative = 0;
};


void CobbDouglas::printModelParameters() { // we have poorly named parameters, alpha army is in CUNYAIModule as well.
    std::ofstream GameParameters;
    GameParameters.open(".\\bwapi-data\\write\\GameParameters.txt", ios::app | ios::ate);
    if (GameParameters.is_open()) {
        GameParameters.seekp(0, ios::end); //to ensure the put pointer is at the end
        GameParameters << getlnY() << "," << getlny() << "," << alpha_army << "," << alpha_econ << "," << alpha_tech << "," << econ_derivative << "," << army_derivative << "," << tech_derivative << "\n";
        GameParameters.close();
    }
    else {
        Broodwar->sendText("Failed to find GameParameters.txt");
    }
}

bool CobbDouglas::evalArmyPossible()
{
    double K_over_L = (double)(CUNYAIModule::friendly_player_model.units_.stock_fighting_total_ + 1) / (double)(CUNYAIModule::friendly_player_model.units_.worker_count_ * Stored_Unit(UnitTypes::Zerg_Zergling).stock_value_ + 1); // avoid NAN's
    int units_on_field = CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Spawning_Pool)
        + CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hydralisk_Den)
        + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Spire)
        + CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Ultralisk_Cavern);
   return (Broodwar->self()->supplyUsed() < 400 && K_over_L < 5 * alpha_army / alpha_tech) || units_on_field <= 0; // can't be army starved if you are maxed out (or close to it), Or if you have a wild K/L ratio. Or if you have nothing in production? These seem like freezers.
}

bool CobbDouglas::evalEconPossible()
{
    bool not_enough_miners = (CUNYAIModule::current_map_inventory.min_workers_ <= CUNYAIModule::current_map_inventory.min_fields_ * 2);
    bool not_enough_workers = CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) < 85;
    return not_enough_miners && not_enough_workers; // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
                                                                  //bool vision_possible = true; // no vision cutoff ATM.
}

bool CobbDouglas::evalTechPossible()
{
    return CUNYAIModule::Tech_Avail(); // if you have no tech available, you cannot be tech starved.
}
