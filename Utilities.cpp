#pragma once
# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

// Gets units last error and prints it directly onscreen.  From tutorial.
void MeatAIModule::PrintError_Unit( Unit unit ) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// An improvement on existing idle scripts. Returns true if stuck or finished with most recent task.
bool MeatAIModule::isIdleEmpty( Unit unit ) {

    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();

    UnitCommandType u_type = unit->getLastCommand().getType();

    bool task_complete = (u_type == UnitCommandTypes::Move && !unit->isMoving()) ||
                         (u_type == UnitCommandTypes::Morph && !unit->isMorphing()) ||
                         (u_type == UnitCommandTypes::Attack_Move && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Attack_Unit && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Return_Cargo && !laden_worker) ||
                         (u_type == UnitCommandTypes::Gather && !unit->isMoving() && !unit->isGatheringGas() && !unit->isGatheringMinerals()) ||
                         (u_type == UnitCommandTypes::Build && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 10 * 24) || // assumes a command has failed if it hasn't executed in the last 10 seconds.
                         (u_type == UnitCommandTypes::Upgrade && !unit->isUpgrading() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 10 * 24) || // unit is done upgrading.
                          u_type == UnitCommandTypes::None;
    return task_complete || unit->isStuck();
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
    if ( _ANALYSIS_MODE ) {
        if ( isOnScreen( s_pos ) || isOnScreen( f_pos ) ) {
            Broodwar->drawLineMap( s_pos, f_pos, col );
        }
    }
}

// Outlines the case where UNIT cannot attack ENEMY type (air/ground), while ENEMY can attack UNIT.  Essentially bidirectional Can_Fight checks.
bool MeatAIModule::Futile_Fight( Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && unit->getType().airWeapon() == WeaponTypes::None ) || (!enemy->isFlying() && unit->getType().groundWeapon() == WeaponTypes::None); // if we cannot attack them.
    bool u_vunerable = (unit->isFlying() && enemy->getType().airWeapon() != WeaponTypes::None) || (!unit->isFlying() && enemy->getType().groundWeapon() != WeaponTypes::None); // they can attack us.
    return ( e_invunerable && u_vunerable ) || ( u_vunerable && !enemy->isDetected() ); // also if they are cloaked and can attack us.
}

// Outlines the case where UNIT can attack ENEMY;
bool MeatAIModule::Can_Fight( Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && unit->getType().airWeapon() == WeaponTypes::None) || (!enemy->isFlying() && unit->getType().groundWeapon() == WeaponTypes::None); // if we cannot attack them.
    return !e_invunerable && enemy->isDetected(); // also if they are cloaked and can attack us.
}

// Counts all units of one type in existance and owned by enemies. Counts units under construction.
int MeatAIModule::Count_Units( const UnitType &type, const Unit_Inventory &ui )
{
    int count = 0;

    for ( const auto & e : ui.unit_inventory_) {

        if ( e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type ) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        } 
        else if ( e.second.type_ == type ) {
            count++;
        }
    }

    return count;
}

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int MeatAIModule::Count_Units( const UnitType &type, const Unitset &unit_set )
{
    int count = 0;
    for ( const auto & unit : unit_set )
    {
        if ( unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type ) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        } 
        else if ( unit->getType() == type ) {
            count++;
        }
    }

    return count;
}



// evaluates the value of a stock of buildings, in terms of pythagorian distance of min & gas & supply. Assumes building is zerg and therefore, a drone was spent on it.
int MeatAIModule::Stock_Buildings( const UnitType &building, const Unit_Inventory &ui ) {
    int cost = sqrt( pow( building.mineralPrice() + UnitTypes::Zerg_Drone.mineralPrice(), 2 ) + pow( 1.25 * building.gasPrice()+ UnitTypes::Zerg_Drone.gasPrice(), 2 ) + pow( 25 * UnitTypes::Zerg_Drone.supplyRequired(), 2 ) );
    int instances = Count_Units( building , ui );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of pythagorian distance of min & gas & supply. Counts totals of stacked upgrades like melee/range/armor.
int MeatAIModule::Stock_Ups( const UpgradeType &ups ) {
    int lvl = Broodwar->self()->getUpgradeLevel( ups ) + (int)Broodwar->self()->isUpgrading( ups );
    int total_stock = 0;
    for ( int i = 1; i <= lvl; i++ ) {
        int cost = sqrt( pow( ups.mineralPrice(), 2 ) + pow( 1.25 * ups.gasPrice(), 2 ) );
        total_stock += cost;
    }
    return total_stock;
}

int MeatAIModule::Stock_Units( const UnitType &unit_type, const Unit_Inventory &ui) {
    int total_stock = 0;

    for ( auto & u : ui.unit_inventory_ ) {
        if ( u.second.type_ == unit_type ) {
            total_stock += u.second.current_stock_value_;
        }
    }

    return total_stock;
}

// evaluates the value of a stock of combat units, for all unit types in a unit inventory. Does not count eggs.
int MeatAIModule::Stock_Combat_Units( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and MeatAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None || ((UnitType)i).groundWeapon() != WeaponTypes::None || ((UnitType)i).maxEnergy() > 0 ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of units, for all unit types in a unit inventory
int MeatAIModule::Stock_Units_ShootUp( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and MeatAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of allied units, for all unit types in a unit inventory
int MeatAIModule::Stock_Units_ShootDown( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and MeatAI "knows" about. 
        if ( ((UnitType)i).groundWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int MeatAIModule::Stock_Supply( const UnitType &unit, const Unit_Inventory &ui ) {
    int supply = unit.supplyProvided();
    int instances = Count_Units( unit, ui );
    int total_stock = supply * instances;
    return total_stock;
}

// Announces to player the name and type of all units in the unit inventory. Bland but practical.
void MeatAIModule::Print_Unit_Inventory( const int &screen_x, const int &screen_y, const Unit_Inventory &ui ) {
    int another_sort_of_unit = 0;
    for ( int i = 0; i != 229; i++ )
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = Count_Units( ((UnitType)i), ui );
        if ( u_count > 0 ) {
            Broodwar->drawTextScreen( screen_x, screen_y, "Inventoried Units:" );  //
            Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10 , "%s: %d", noRaceName( ((UnitType)i).c_str()), u_count );  //
            another_sort_of_unit++;
        }
    }
}

// Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
void MeatAIModule::Print_Build_Order_Remaining( const int &screen_x, const int &screen_y, const Building_Gene &bo ) {
    int another_sort_of_unit = 0;
    if ( !bo.building_gene_.empty() ) {
        for ( auto i : bo.building_gene_ ) { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            Broodwar->drawTextScreen( screen_x, screen_y, "Build Order:" );  //
            if ( i.getUnit() != UnitTypes::None ) {
                Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s", noRaceName( i.getUnit().c_str() ) );  //
            }
            else if ( i.getUpgrade() != UpgradeTypes::None ) {
                Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s", i.getUpgrade().c_str() );  //
            }
            another_sort_of_unit++;
        }
    }
    else {
        Broodwar->drawTextScreen( screen_x, screen_y, "Build Order:" );  //
        Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_unit * 10, "Build Order Empty");  //
    }
}

// Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void MeatAIModule::Print_Upgrade_Inventory( const int &screen_x, const int &screen_y ) {
    int another_sort_of_upgrade = 0;
    for ( int i = 0; i != 62; i++ )
    { // iterating through all upgrades.
        int up_count = Broodwar->self()->getUpgradeLevel( ((UpgradeType)i) ) + (int)Broodwar->self()->isUpgrading( ((UpgradeType)i) );
        if ( up_count > 0 ) {
            Broodwar->drawTextScreen( screen_x, screen_y, "Upgrades:" );  //
            Broodwar->drawTextScreen( screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: %d", ((UpgradeType)i).c_str() , up_count );  //
            another_sort_of_upgrade++;
        }
    }
}


//Strips the RACE_ from the front of the unit type string.
const char * MeatAIModule::noRaceName( const char *name ) { //From N00b
    for ( const char *c = name; *c; c++ )
        if ( *c == '_' ) return ++c;
    return name;
}

//Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
Unitset MeatAIModule::getUnit_Set( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unitset e_set;
    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.pos_.getDistance( origin ) <= dist ) {
            e_set.insert( (*e).second.bwapi_unit_ ); // if we take any distance and they are in inventory.
        }
    }
    return e_set;
}

//Searches an enemy inventory for units of a type within a range. Returns enemy inventory meeting that critera. Returns pointers even if the unit is lost, but the pointers are empty.
Unit_Inventory MeatAIModule::getUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unit_Inventory ui_out;
    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.pos_.getDistance( origin ) <= dist ) {
            ui_out.addStored_Unit( (*e).second ); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

bool MeatAIModule::isOnScreen( const Position &pos ) {
    bool inrange_x = Broodwar->getScreenPosition().x < pos.x && Broodwar->getScreenPosition().x + 640 > pos.x;
    bool inrange_y = Broodwar->getScreenPosition().y < pos.y && Broodwar->getScreenPosition().y + 480 > pos.y;

    return inrange_x && inrange_y;
}

//checks if there is a smooth path to target. in minitiles
bool MeatAIModule::isClearRayTrace( const Position &initial, const Position &final, const Inventory &inv ) // see Brehsam's Algorithm
{
    int dx = abs( final.x - initial.x ) / 8;
    int dy = abs( final.y - initial.y ) / 8;
    int x = initial.x / 8;
    int y = initial.y / 8;
    int n = 1 + dx + dy;
    int x_inc = (final.x > initial.x) ? 1 : -1;
    int y_inc = (final.y > initial.y) ? 1 : -1;
    int error = dx - dy;
    dx *= 2;
    dy *= 2;

    for ( ; n > 0; --n )
    {
        if ( inv.smoothed_barriers_[x][y] == 1 ) {
            return false;
        }

        if ( error > 0 )
        {
            x += x_inc;
            error -= dy;
        }
        else
        {
            y += y_inc;
            error += dx;
        }
    }

    return true;
} 
//Zerg_Carapace = 3,
//Zerg_Melee_Attacks = 10,
//Zerg_Missile_Attacks = 11,
//Antennae = 25,
//Pneumatized_Carapace = 26,
//Metabolic_Boost = 27,
//Adrenal_Glands = 28,
//Muscular_Augments = 29,
//Grooved_Spines = 30,
//Chitinous_Plating = 52,
//Anabolic_Synthesis = 53,


