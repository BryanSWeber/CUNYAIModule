#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\CobbDouglas.h"
#include "Source\PlayerModelManager.h"
#include "Source\Diagnostics.h"
#include <iostream>
#include <fstream>

using namespace std;

class PlayerModel;

void CobbDouglas::setModelParameters(double armyAlpha, double econAlpha, double techAlpha, double adopRate)
{
    alpha_army = armyAlpha; // army starved parameter.
    alpha_econ = econAlpha; // econ starved parameter.
    alpha_tech = techAlpha; // tech starved parameter.
    adoptionRate = adopRate; // rate of adaptation.
}

void CobbDouglas::onStartSelf(LearningManager l)
{
    setModelParameters(l.inspectCurrentBuild().getParameter(BuildParameterNames::ArmyAlpha),
                        l.inspectCurrentBuild().getParameter(BuildParameterNames::EconAlpha),
                        l.inspectCurrentBuild().getParameter(BuildParameterNames::TechAlpha),
                        l.inspectCurrentBuild().getParameter(BuildParameterNames::AdaptationRate));
}

//complete but long creator method. Normalizes to 1 automatically.
void CobbDouglas::setCD(double army_stk, double tech_stk, double wk_stk)
{
    // CD takes the form Y= (A*labor)^alpha_l * capital^(alpha_k). I assume A is a technology augmenting only capital, and takes the form tech^(alpha_t). I can still normalize the sum of alpha_l and alpha_k to 1. 
    double a_tot = alpha_econ + alpha_army;
    alpha_econ = alpha_econ / a_tot;
    alpha_army = alpha_army / a_tot;
    //alpha_tech = alpha_tech; // No change here, it's not normalized.

    worker_stock = std::max(wk_stk, 1.0);
    army_stock = std::max(army_stk, 1.0);
    tech_stock = std::max(tech_stk, 1.0);

    bool army_possible = evalArmyPossible();
    bool econ_possible = evalEconPossible();
    bool tech_possible = evalTechPossible();

    econ_derivative = alpha_econ * pow(tech_stock, alpha_tech * alpha_army) * pow(safeDiv(army_stock, worker_stock), alpha_army) * econ_possible; // worker stock is incorperated on the RHS to save on a calculation.
    army_derivative = alpha_army * pow(safeDiv(worker_stock, army_stock), alpha_econ) * pow(tech_stock, alpha_tech * alpha_army) * army_possible;  // army stock is incorperated on the RHS to save on a calculation.  
    tech_derivative = alpha_tech * alpha_army * pow(worker_stock, alpha_econ) * pow(tech_stock, alpha_tech * alpha_army - 1) * pow(army_stock, alpha_army) * tech_possible;
}

// Identifies the value of our main priority.
double CobbDouglas::getPriority() const {
    double derivatives[3] = { econ_derivative , army_derivative, tech_derivative };
    double priority = *(max_element(begin(derivatives), end(derivatives)));
    return priority;
}

// Protected from failure in divide by 0 case.
double CobbDouglas::getlnYPerCapita() const
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

// Protected from failure in divide by 0 case.
double CobbDouglas::getlnYusing(const double alpha_army, const double alpha_tech) const
{
    double ln_Y = 0;
    try {
        //ln_Y = alpha_army * log( army_stock ) + alpha_tech * log( tech_stock ) + alpha_econ * log( worker_stock ); //Analog to GDP
        ln_Y = alpha_army * log(army_stock) + alpha_army * alpha_tech * log(tech_stock) + (1 - alpha_army) * log(worker_stock); //Analog to GDP
    }
    catch (...) {
        BWAPI::Broodwar->sendText("Uh oh. We are out of something critical...");
    };
    return ln_Y;
}

//Identifies priority type
bool CobbDouglas::army_starved() const
{
    if (army_derivative == getPriority())
    {
        return true;
    }
    else {
        return false;
    }
}

//Identifies priority type
bool CobbDouglas::econ_starved() const
{
    if (econ_derivative == getPriority())
    {
        return true;
    }
    else {
        return false;
    }
}
//Identifies priority type
bool CobbDouglas::tech_starved() const
{
    if (tech_derivative == getPriority())
    {
        return true;
    }
    else {
        return false;
    }
}

double CobbDouglas::getParameter(BuildParameterNames b) const
{
    switch(b){
    case BuildParameterNames::ArmyAlpha :
        return alpha_army;
    case BuildParameterNames::EconAlpha:
        return alpha_econ;
    case BuildParameterNames::TechAlpha:
        return alpha_tech;
    case BuildParameterNames::AdaptationRate:
        return adoptionRate;
    default:
        Diagnostics::DiagnosticText("You're asking for a parameter from the CD model that it doesn't have.");
        return 0.0;
    }
}

double CobbDouglas::getDeriviative(BuildParameterNames b) const
{
    switch (b) {
    case BuildParameterNames::ArmyAlpha:
        return army_derivative;
    case BuildParameterNames::EconAlpha:
        return econ_derivative;
    case BuildParameterNames::TechAlpha:
        return tech_derivative;
    default:
        Diagnostics::DiagnosticText("You're asking for a derivative from the CD model that it doesn't have.");
        return 0.0;
    }
}

double CobbDouglas::getStock(BuildParameterNames b) const
{
    switch (b) {
    case BuildParameterNames::ArmyAlpha:
        return army_stock;
    case BuildParameterNames::EconAlpha:
        return worker_stock;
    case BuildParameterNames::TechAlpha:
        return tech_stock;
    default:
        Diagnostics::DiagnosticText("You're asking for a Stock from the CD model that it doesn't have.");
        return 0.0;
    }
}

void CobbDouglas::setEnemyCD(int e_army_stock, int e_tech_stock, int e_worker_stock) // FOR MODELING ENEMIES ONLY
{
    worker_stock = std::max(e_worker_stock, 1);
    army_stock = std::max(e_army_stock, 1);
    tech_stock = std::max(e_tech_stock, 1);

    double K_over_L = safeDiv(army_stock, worker_stock); // avoid NAN's
    alpha_army = CUNYAIModule::bindBetween(K_over_L / static_cast<double>(1.0 + K_over_L), 0.05, 0.95);
    alpha_econ = CUNYAIModule::bindBetween(1 - alpha_army, 0.05, 0.95);
    alpha_tech = CUNYAIModule::bindBetween(safeDiv(tech_stock, worker_stock) * alpha_econ / alpha_army, 0.05, 2.95);
}

//Sets enemy utility function parameters based on known information.
void CobbDouglas::enemy_mimic(const PlayerModel & enemy) {
    //If optimally chose, the derivatives will all be equal.

    Diagnostics::DiagnosticWrite("We're updating our model to mimic our opponents.");
    Diagnostics::DiagnosticWrite("Alpha_army: Enemy %4.2f, Self %4.2f", enemy.spending_model_.alpha_army, alpha_army);
    Diagnostics::DiagnosticWrite("Alpha_econ: Enemy %4.2f, Self %4.2f", enemy.spending_model_.alpha_econ, alpha_econ);
    Diagnostics::DiagnosticWrite("Alpha_tech: Enemy %4.2f, Self %4.2f", enemy.spending_model_.alpha_tech, alpha_tech);

    //Shift alpha towards enemy choices.
    alpha_army += adoptionRate * (enemy.spending_model_.alpha_army - alpha_army);
    alpha_econ += adoptionRate * (enemy.spending_model_.alpha_econ - alpha_econ);
    alpha_tech += adoptionRate * (enemy.spending_model_.alpha_tech - alpha_tech);

    bool army_possible = evalArmyPossible();
    bool econ_possible = evalEconPossible();
    bool tech_possible = evalTechPossible();

    if (isnan(alpha_army))  alpha_army = 0;
    if (isnan(alpha_econ))  alpha_econ = 0;
    if (isnan(alpha_tech))  alpha_tech = 0; // this safety might be permitting undiagnosed problems.


    //reevaluate our tech choices.
    econ_derivative = alpha_econ * pow(tech_stock, alpha_tech * alpha_army) * pow(safeDiv(army_stock, worker_stock), alpha_army) * econ_possible; // worker stock is incorperated on the RHS to save on a calculation.
    army_derivative = alpha_army * pow(safeDiv(worker_stock, army_stock), alpha_econ) * pow(tech_stock, alpha_tech * alpha_army) * army_possible;  // army stock is incorperated on the RHS to save on a calculation.  
    tech_derivative = alpha_tech * alpha_army * pow(worker_stock, alpha_econ) *              pow(tech_stock, alpha_tech * alpha_army - 1) * pow(army_stock, alpha_army) * tech_possible;

    if (isnan(econ_derivative)) econ_derivative = 0;
    if (isnan(army_derivative)) army_derivative = 0;
    if (isnan(tech_derivative)) tech_derivative = 0;
};


void CobbDouglas::printModelParameters() const { // we have poorly named parameters, alpha army is in CUNYAIModule as well.
    std::ofstream GameParameters;
    GameParameters.open("..\\write\\GameParameters.txt", ios::app | ios::ate);
    if (GameParameters.is_open()) {
        GameParameters.seekp(0, ios::end); //to ensure the put pointer is at the end
        GameParameters << getlnY() << "," << getlnYPerCapita() << "," << alpha_army << "," << alpha_econ << "," << alpha_tech << "," << econ_derivative << "," << army_derivative << "," << tech_derivative << "\n";
        GameParameters.close();
    }
    else {
        Broodwar->sendText("Failed to find GameParameters.txt");
    }
}

bool CobbDouglas::evalArmyPossible() const
{

    double K_over_L = safeDiv(army_stock, worker_stock); // avoid NAN's

    bool can_build_army = false;

    // I can build army if any of these are suitible/viable.
    for (auto unitPair : CUNYAIModule::friendly_player_model.getCombatUnitCartridge()) {
        if (CUNYAIModule::checkWilling(unitPair.first, true)) {
            can_build_army = true;
            break;
        }

    }

    return Broodwar->self()->supplyUsed() < 399 && can_build_army; // can't be army starved if you are maxed out (or close to it), Or if you have a wild K/L ratio. Or if you have nothing in production? These seem like freezers.

}

bool CobbDouglas::evalEconPossible() const
{
    bool enough_mines_exist = CUNYAIModule::countUnits(UnitTypes::Zerg_Drone) <= static_cast<int>(Broodwar->getMinerals().size() * 2 + Broodwar->getGeysers().size() * 3 + 1);
    bool not_enough_miners_for_mines = (CUNYAIModule::countUnits(UnitTypes::Zerg_Drone) <= CUNYAIModule::land_inventory.countLocalMinPatches() * 2 + CUNYAIModule::countUnits(UnitTypes::Zerg_Extractor) * 3);
    bool not_excessive_workers = CUNYAIModule::countUnits(UnitTypes::Zerg_Drone) < (CUNYAIModule::enemy_player_model.getEstimatedWorkers() + 12);
    return enough_mines_exist && not_excessive_workers && Broodwar->self()->supplyUsed() < 399 && CUNYAIModule::assemblymanager.checkNewUnitWithinMaximum(UnitTypes::Zerg_Drone); // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
                                                                  //bool vision_possible = true; // no vision cutoff ATM.
}

bool CobbDouglas::evalTechPossible() const
{
    return CUNYAIModule::techmanager.checkTechAvail(); // if you have no tech available, you cannot be tech starved.
}

