# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

// evaluates the value of a stock of buildings, in terms of total cost (min+gas). Assumes building is zerg and therefore, a drone was spent on it.
int MeatAIModule::Stock_Buildings( UnitType building ) {
    int cost = building.mineralPrice() + building.gasPrice() + 50;
    int instances = Count_Units( building );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of total cost (min+gas).
int MeatAIModule::Stock_Ups( UpgradeType ups ) {
    int cost = ups.mineralPrice() + ups.gasPrice();
    int instances = Broodwar->self()->getUpgradeLevel( ups ) + Broodwar->self()->isUpgrading(ups) ;
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of units, in terms of total cost min+gas. Doesn't consider the counterfactual larva. Is set to considers the unit's condition. Use only for combat units.
int MeatAIModule::Stock_Units( UnitType unit ) {
    int cost = unit.mineralPrice() + unit.gasPrice();
    int instances = Count_Units( unit );

    //int curr_hp = 0;
    //int count = 0;
    //for ( const auto & u : Broodwar->self()->getUnits() ) {
    //    if ( u->getType() == unit ) {
    //        curr_hp += u->getHitPoints();  // how do you call the unitinterface class? 
    //        count++;
    //    }
    //}
    //double curr_hp_pct = (double)curr_hp / (double)(count * unit.maxHitPoints());
    //int total_stock = curr_hp > 0 ? cost * instances * curr_hp_pct : cost * instances;
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int MeatAIModule::Stock_Supply( UnitType unit ) {
    int supply = unit.supplyProvided();
    int instances = Count_Units( unit );
    int total_stock = supply * instances;
    return total_stock;
}


// Counts all units of one type in existance. Includes individual units in production. 
int MeatAIModule::Count_Units( UnitType type )
{
    int count = 0;
    for ( const auto & unit : Broodwar->self()->getUnits() )
    {
        if ( unit->getType() == type )
        {
            count++;
        }

        // Count units under construction
        if ( unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type )
        {
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings, I believe.
        }

    }

    return count;
}

// Gets units last error and prints it directly onscreen.  From tutorial.
void MeatAIModule::PrintError_Unit( Unit unit ) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// An improvement on existing idle scripts. Returns true if it is idle. Checks if it is a laden worker, idle, or stopped. 
bool MeatAIModule::isIdleEmpty( Unit unit ) {
    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();
    bool idle = unit->isIdle() || !unit->isMoving();
    return idle && !laden_worker;
}

// Checks for if a unit is a combat unit.
bool MeatAIModule::IsFightingUnit( Unit unit )
{
    if ( !unit )
    {
        return false;
    }

    // no workers or buildings allowed. Or overlords, or larva..
    if ( unit && unit->getType().isWorker() ||
        unit->getType().isBuilding() ||
        unit->getType() == BWAPI::UnitTypes::Zerg_Larva ||
        unit->getType() == BWAPI::UnitTypes::Zerg_Overlord )
    {
        return false;
    }

    // This is a last minute check for psi-ops. I removed a bunch of these. Observers and medics are not combat units per se.
    if ( unit->getType().canAttack() ||
        unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
        unit->isFlying() && unit->getType().spaceProvided() > 0 )
    {
        return true;
    }

    return false;
}

// This function limits the drawing that needs to be done by the bot.
void MeatAIModule::Diagnostic_Line( Position s_pos, Position f_pos, Color col = Colors::White ) {
    if ( diagnostic_mode ) {
        Broodwar->drawLineMap( s_pos, f_pos, col );
    }
}

// Outlines the case where you cannot attack their type (air/ground), while they can attack you.
bool MeatAIModule::Futile_Fight( Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && !unit->getType().airWeapon().targetsAir()) || (!enemy->isFlying() && !unit->getType().groundWeapon().targetsGround()); // if we cannot attack them.
    bool u_vunerable = (unit->isFlying() && !enemy->getType().airWeapon().targetsAir()) || (!unit->isFlying() && enemy->getType().groundWeapon().targetsGround()); // they can attack us.
    return ( e_invunerable && u_vunerable ) || ( u_vunerable && !enemy->isDetected() ); // also if they are cloaked and can attack us.
}

bool MeatAIModule::Can_Fight( Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && !unit->getType().airWeapon().targetsAir()) || (!enemy->isFlying() && !unit->getType().groundWeapon().targetsGround()); // if we cannot attack them.
    return !e_invunerable && enemy->isDetected(); // also if they are cloaked and can attack us.
}