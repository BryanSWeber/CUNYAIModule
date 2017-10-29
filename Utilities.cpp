#pragma once
# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

// Gets units last error and prints it directly onscreen.  From tutorial.
void MeatAIModule::PrintError_Unit(const Unit &unit) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// Identifies those moments where a worker is gathering and its unusual subcases.
bool MeatAIModule::isActiveWorker(const Unit &unit){
	bool passive = //BWAPI::Orders::MoveToMinerals &&
		unit->getOrder() == BWAPI::Orders::MoveToGas ||
		unit->getOrder() == BWAPI::Orders::WaitForMinerals ||
		//unit->getOrder() == BWAPI::Orders::WaitForGas && // should never be overstacked on gas.
		unit->getOrder() == BWAPI::Orders::MiningMinerals ||
		unit->getOrder() == BWAPI::Orders::HarvestGas ||
		unit->getOrder() == BWAPI::Orders::ReturnMinerals ||
		unit->getOrder() == BWAPI::Orders::ReturnGas ||
		unit->getOrder() == BWAPI::Orders::ResetCollision;//command is issued promptly when workers finish mining, but must resolve. http://satirist.org/ai/starcraft/blog/archives/220-how-to-beat-Stone,-according-to-AIL.html
	return passive;
}

bool MeatAIModule::isInLine(const Unit &unit){
	bool passive = 
		unit->getOrder() == BWAPI::Orders::WaitForMinerals ||
		unit->getOrder() == BWAPI::Orders::WaitForGas ||
		unit->getOrder() == BWAPI::Orders::ResetCollision;
	return passive;
}

// An improvement on existing idle scripts. Returns true if stuck or finished with most recent task.
bool MeatAIModule::isIdleEmpty(const Unit &unit) {

    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();

    UnitCommandType u_type = unit->getLastCommand().getType();

	bool task_complete = (u_type == UnitCommandTypes::Move && !unit->isMoving()) ||
                         (u_type == UnitCommandTypes::Morph && !unit->isMorphing()) ||
                         (u_type == UnitCommandTypes::Attack_Move && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Attack_Unit && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Return_Cargo && !laden_worker) ||
                         (u_type == UnitCommandTypes::Gather && !unit->isMoving() && !unit->isGatheringGas() && !unit->isGatheringMinerals() && !isInLine(unit)) ||
                         (u_type == UnitCommandTypes::Build && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 5 * 24 && !unit->isMoving() ) || // assumes a command has failed if it hasn't executed in the last 10 seconds.
                         (u_type == UnitCommandTypes::Upgrade && !unit->isUpgrading() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 5 * 24) || // unit is done upgrading.
                          u_type == UnitCommandTypes::None ||
						  u_type == UnitCommandTypes::Unknown;

	bool spam_guard = unit->getLastCommandFrame() + Broodwar->getLatencyFrames() < Broodwar->getFrameCount();

    return ( task_complete || unit->isStuck() ) && !isActiveWorker(unit) && !IsUnderAttack(unit) && spam_guard ;
}

// Did the unit fight in the last 5 seconds?
bool MeatAIModule::isRecentCombatant(const Unit &unit) {
	bool fighting_now = (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Unit);
	bool recent_order = unit->getLastCommandFrame() + 5 * 24 > Broodwar->getFrameCount();
	return fighting_now && recent_order;
}

// Checks for if a unit is a combat unit.
bool MeatAIModule::IsFightingUnit(const Unit &unit)
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

// Outlines the case where UNIT can attack ENEMY; 
bool MeatAIModule::Can_Fight( Unit unit, Stored_Unit enemy ) {
    bool e_invunerable = (enemy.type_.isFlyer() && unit->getType().airWeapon() == WeaponTypes::None) || (!enemy.type_.isFlyer() && unit->getType().groundWeapon() == WeaponTypes::None); // if we cannot attack them.
    if ( enemy.bwapi_unit_  && enemy.bwapi_unit_->exists() ) {
        return !e_invunerable && enemy.bwapi_unit_->isDetected();
    }
    else {
        return !e_invunerable; // also if they are cloaked and can attack us.
    }
}

// Outlines the case where UNIT can attack ENEMY; 
bool MeatAIModule::Can_Fight( Stored_Unit unit, Unit enemy ) {
    bool e_invunerable = (enemy->isFlying() && unit.type_.airWeapon() == WeaponTypes::None) || (!enemy->isFlying() && unit.type_.groundWeapon() == WeaponTypes::None); // if we cannot attack them.
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

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int MeatAIModule::Count_Units_Doing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set)
{
	int count = 0;
	for (const auto & unit : unit_set)
	{
		if (unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type) { // Count units under construction
			count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
		}
		else if (unit->getType() == type && unit->getLastCommand().getType() == u_command_type) {
			count++;
		}
	}

	return count;
}

// evaluates the value of a stock of buildings, in terms of pythagorian distance of min & gas & supply. Assumes building is zerg and therefore, a drone was spent on it.
int MeatAIModule::Stock_Buildings( const UnitType &building, const Unit_Inventory &ui ) {
    int cost = (int)sqrt( pow( building.mineralPrice() + UnitTypes::Zerg_Drone.mineralPrice(), 2 ) + pow( 1.25 * building.gasPrice()+ UnitTypes::Zerg_Drone.gasPrice(), 2 ) + pow( 25 * UnitTypes::Zerg_Drone.supplyRequired(), 2 ) );
    int instances = Count_Units( building , ui );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of pythagorian distance of min & gas & supply. Counts totals of stacked upgrades like melee/range/armor.
int MeatAIModule::Stock_Ups( const UpgradeType &ups ) {
    int lvl = Broodwar->self()->getUpgradeLevel( ups ) + (int)Broodwar->self()->isUpgrading( ups );
    int total_stock = 0;
    for ( int i = 1; i <= lvl; i++ ) {
        int cost = (int)sqrt( pow( ups.mineralPrice(), 2 ) + pow( 1.25 * ups.gasPrice(), 2 ) );
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

//Gets pointer to closest unit to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* MeatAIModule::getClosestStored( Unit_Inventory &ui, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    double temp_dist = 999999;
    Stored_Unit *return_unit = nullptr;

    if ( !ui.unit_inventory_.empty() ) {
        for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
            temp_dist = (*e).second.pos_.getDistance( origin );
            if ( temp_dist <= min_dist ) {
                min_dist = temp_dist;
                return_unit = &(e->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit of a type to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* MeatAIModule::getClosestStored(Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999) {
	int min_dist = dist;
	double temp_dist = 999999;
	Stored_Unit *return_unit = nullptr;

	if (!ui.unit_inventory_.empty()) {
		for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
			if (e->second.type_ == u_type){
				temp_dist = (*e).second.pos_.getDistance(origin);
				if (temp_dist <= min_dist) {
					min_dist = temp_dist;
					return_unit = &(e->second);
				}
			}
		}
	}

	return return_unit;
}

//Gets pointer to closest unit to point in Resource_inventory. Checks range. Careful about visiblity.
Stored_Resource* MeatAIModule::getClosestStored(Resource_Inventory &ri, const Position &origin, const int &dist = 999999) {
	int min_dist = dist;
	double temp_dist = 999999;
	Stored_Resource *return_unit = nullptr;

	if (!ri.resource_inventory_.empty()) {
		for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
			temp_dist = (*r).second.pos_.getDistance(origin);
			if (temp_dist <= min_dist) {
				min_dist = temp_dist;
				return_unit = &(r->second);
			}
		}
	}

	return return_unit;
}

//Gets pointer to closest attackable unit to point in Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr.
Stored_Unit* MeatAIModule::getClosestAttackableStored( Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    bool can_attack;
    double temp_dist = 999999;
    Stored_Unit *return_unit = nullptr; 

    if ( !ui.unit_inventory_.empty() ) {
        for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
            can_attack = (u_type.airWeapon() != WeaponTypes::None && e->second.type_.isFlyer()) || (u_type.groundWeapon() != WeaponTypes::None && !e->second.type_.isFlyer());
            if ( can_attack ) {
                temp_dist = e->second.pos_.getDistance( origin );
                if ( temp_dist <= min_dist ) {
                    min_dist = temp_dist;
                    return_unit = &(e->second); 
                }
            }
        }
    }

    return return_unit;
}
//Gets pointer to closest attackable unit to point in Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters.
Stored_Unit* MeatAIModule::getClosestThreatOrTargetStored( Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    double temp_dist = 999999;
    Stored_Unit *return_unit = nullptr;

    if ( !ui.unit_inventory_.empty() ) {
        for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
            can_attack = (u_type.airWeapon() != WeaponTypes::None && e->second.type_.isFlyer()) || (u_type.groundWeapon() != WeaponTypes::None && !e->second.type_.isFlyer());
            can_be_attacked_by = (e->second.type_.airWeapon() != WeaponTypes::None && u_type.isFlyer()) || (e->second.type_.groundWeapon() != WeaponTypes::None && !u_type.isFlyer());
            if ( (can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() ) {
                temp_dist = e->second.pos_.getDistance( origin );
                if ( temp_dist <= min_dist ) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Unit_Inventory MeatAIModule::getUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unit_Inventory ui_out;
    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.pos_.getDistance( origin ) <= dist ) {
            ui_out.addStored_Unit( (*e).second ); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr. Overloaded for specifi types.
Unit_Inventory MeatAIModule::getUnitInventoryInRadius(const Unit_Inventory &ui, const UnitType u_type, const Position &origin, const int &dist) {
	Unit_Inventory ui_out;
	for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
		if ((*e).second.pos_.getDistance(origin) <= dist && (*e).second.type_== u_type) {
			ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
		}
	}
	return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Resource_Inventory MeatAIModule::getResourceInventoryInRadius(const Resource_Inventory &ri, const Position &origin, const int &dist) {
	Resource_Inventory ri_out;
	for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
		if ((*r).second.pos_.getDistance(origin) <= dist) {
			ri_out.addStored_Resource( (*r).second ); // if we take any distance and they are in inventory.
		}
	}
	return ri_out;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
bool MeatAIModule::checkOccupiedArea( const Unit_Inventory &ui, const Position &origin, const int &dist ) {

    for ( auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++ ) {
        if( (*e).second.pos_.x < origin.x + dist && (*e).second.pos_.x > origin.x - dist &&
            (*e).second.pos_.y < origin.y + dist && (*e).second.pos_.y > origin.y - dist ) {
            return true;
        }
    }

    return false;
}

//Searches an inventory for buildings of within a range. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
bool MeatAIModule::checkBuildingOccupiedArea( const Unit_Inventory &ui, const Position &origin ) {

    for ( auto & e : ui.unit_inventory_) {
        if ( e.second.type_.isBuilding() ) {
            if ( e.second.pos_.x < origin.x + e.second.type_.dimensionLeft() && e.second.pos_.x > origin.x - e.second.type_.dimensionRight() &&
                e.second.pos_.y < origin.y + e.second.type_.dimensionUp() && e.second.pos_.y > origin.y - e.second.type_.dimensionDown() ) {
                return true;
            }
        }
    }

    return false;
}

//Searches if a particular unit is within a range of the position. Returns TRUE if the area is occupied or nearly so. Checks retangles for performance reasons rather than radius.
bool MeatAIModule::checkUnitOccupiesArea( const Unit &unit, const Position &origin, const int & dist ) {

        if ( unit->getType().isBuilding() ) {
            Position pos = unit->getPosition();
            UnitType type = unit->getType();
            if ( pos.x < origin.x + type.dimensionLeft() + dist && pos.x > origin.x - type.dimensionRight() - dist &&
                pos.y < origin.y + type.dimensionUp() + dist && pos.y > origin.y - type.dimensionDown() - dist ) {
                return true;
            }
        }


    return false;
}

bool MeatAIModule::isOnScreen( const Position &pos ) {
    bool inrange_x = Broodwar->getScreenPosition().x < pos.x && Broodwar->getScreenPosition().x + 640 > pos.x;
    bool inrange_y = Broodwar->getScreenPosition().y < pos.y && Broodwar->getScreenPosition().y + 480 > pos.y;

    return inrange_x && inrange_y;
}

//checks if there is a smooth path to target. in minitiles
//bool MeatAIModule::isClearRayTrace( const Position &initial, const Position &final, const Inventory &inv ) // see Brehsam's Algorithm
//{
//    int dx = abs( final.x - initial.x ) / 8;
//    int dy = abs( final.y - initial.y ) / 8;
//    int x = initial.x / 8;
//    int y = initial.y / 8;
//    int n = 1 + dx + dy;
//    int x_inc = (final.x > initial.x) ? 1 : -1;
//    int y_inc = (final.y > initial.y) ? 1 : -1;
//    int error = dx - dy;
//    dx *= 2;
//    dy *= 2;
//
//    for ( ; n > 0; --n )
//    {
//        if ( inv.smoothed_barriers_[x][y] == 1 ) {
//            return false;
//        }
//
//        if ( error > 0 )
//        {
//            x += x_inc;
//            error -= dy;
//        }
//        else
//        {
//            y += y_inc;
//            error += dx;
//        }
//    }
//
//    return true;
//} 

//checks if there is a smooth path to target. in minitiles
bool MeatAIModule::isClearRayTrace(const Position &initialp, const Position &finalp, const Inventory &inv){ // see Brehsam's Algorithm for all 8 octants.
	int x, y, dx, dy, dx1, dy1, px, py, xe, ye, map_x, map_y;
	WalkPosition final = WalkPosition(finalp);
	WalkPosition initial = WalkPosition(initialp);

	dx = (final.x - initial.x);
	dy = (final.y - initial.y);
	dx1 = abs(dx);
	dy1 = abs(dy);
	px = 2 * dy1 - dx1;
	py = 2 * dx1 - dy1;
	map_x = Broodwar->mapWidth() * 4;
	map_y = Broodwar->mapHeight() * 4;

	if (dy1 <= dx1)
	{
		if (dx >= 0)
		{
			x = initial.x;
			y = initial.y;
			xe = final.x;
		}
		else
		{
			x = final.x;
			y = final.y;
			xe = initial.x;
		}

		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y ;
		if ( safety_check && inv.smoothed_barriers_[x][y] == 1) {
			return false;
		}

		for (int i = 0; x<xe; i++)
		{
			x = x + 1;
			if (px<0)
			{
				px = px + 2 * dy1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					y++;
				}
				else
				{
					y--;
				}
				px = px + 2 * (dy1 - dx1);
			}

			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
				return false;
			}
		}
	}
	else
	{
		if (dy >= 0)
		{
			x = initial.x;
			y = initial.y;
			ye = final.y;
		}
		else
		{
			x = final.x;
			y = final.y;
			ye = initial.y;
		}
		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
			return false;
		}

		for (int i = 0; y<ye; i++)
		{
			y = y + 1;
			if (py <= 0)
			{
				py = py + 2 * dx1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					x++;
				}
				else
				{
					x--;
				}
				py = py + 2 * (dx1 - dy1);
			}
			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
				return false;
			}

		}
	}

	return true;

}

//Counts the number of minitiles in a smooth path to target. in minitiles
int MeatAIModule::getClearRayTraceSquares( const Position &initialp, const Position &finalp, const Inventory &inv ) // see Brehsam's Algorithm. Is likely bugged in current state.
{
	int x, y, dx, dy, dx1, dy1, px, py, xe, ye, map_x, map_y, squares_counted;
	WalkPosition final = WalkPosition(finalp);
	WalkPosition initial = WalkPosition(initialp);

	squares_counted = 0;
	dx = (final.x - initial.x);
	dy = (final.y - initial.y);
	dx1 = abs(dx);
	dy1 = abs(dy);
	px = 2 * dy1 - dx1;
	py = 2 * dx1 - dy1;
	map_x = Broodwar->mapWidth() * 4;
	map_y = Broodwar->mapHeight() * 4;

	if (dy1 <= dx1)
	{
		if (dx >= 0)
		{
			x = initial.x;
			y = initial.y;
			xe = final.x;
		}
		else
		{
			x = final.x;
			y = final.y;
			xe = initial.x;
		}

		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
			squares_counted++;
		}

		for (int i = 0; x<xe; i++)
		{
			x = x + 1;
			if (px<0)
			{
				px = px + 2 * dy1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					y++;
				}
				else
				{
					y--;
				}
				px = px + 2 * (dy1 - dx1);
			}

			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
				squares_counted++;
			}
		}
	}
	else
	{
		if (dy >= 0)
		{
			x = initial.x;
			y = initial.y;
			ye = final.y;
		}
		else
		{
			x = final.x;
			y = final.y;
			ye = initial.y;
		}
		bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
		if (safety_check && inv.smoothed_barriers_[x][y] == 1) {
			squares_counted++;
		}

		for (int i = 0; y<ye; i++)
		{
			y = y + 1;
			if (py <= 0)
			{
				py = py + 2 * dx1;
			}
			else
			{
				if ((dx<0 && dy<0) || (dx>0 && dy>0))
				{
					x++;
				}
				else
				{
					x--;
				}
				py = py + 2 * (dx1 - dy1);
			}
			bool safety_check = x > 1 && x < map_x && y > 1 && y < map_y;
			if ( safety_check && inv.smoothed_barriers_[x][y] == 1) {
				squares_counted++;
			}

		}
	}

	return squares_counted;
}

//finds nearest choke or best location within 100 minitiles.
Position MeatAIModule::getNearestChoke( const Position &initial, const Position &final, const Inventory &inv ) {
    WalkPosition e_position = WalkPosition( final );
    WalkPosition wk_postion = WalkPosition( initial );
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );

    int max_observed = inv.map_veins_[wk_postion.x][wk_postion.y];
    Position nearest_choke; 

    for ( auto i = 0; i < 100; ++i ) {
        for ( int x = -1; x <= 1; ++x ) {
            for ( int y = -1; y <= 1; ++y ) {

                int testing_x = wk_postion.x + x;
                int testing_y = wk_postion.y + y;

                if ( !(x == 0, y == 0) &&
                    testing_x < map_dim.x &&
                    testing_y < map_dim.y &&
                    testing_x > 0 &&
                    testing_y > 0 ) { // check for being within reference space.

                    int temp = inv.map_veins_[testing_x][testing_y];

                    if ( temp >= max_observed ) {
                        max_observed = temp;
                        nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
                        wk_postion = WalkPosition( nearest_choke ); //search from there.
                        if ( max_observed > 175 ) {
                            return nearest_choke;
                            break;
                        }
                    }
                }
            }
        }
    }

    //another attempt
    //int x_dist_to_e = e_position.x - wk_postion.x;
    //int y_dist_to_e = e_position.y - wk_postion.y;

    //int dx = x_dist_to_e > 0 ? 1 : -1;
    //int dy = y_dist_to_e > 0 ? 1 : -1;

    //for ( auto i = 0; i < 50; ++i ) {
    //    for ( int x = 0; x <= 1; ++x ) {
    //        for ( int y = 0; y <= 1; ++y ) {

    //            int testing_x = wk_postion.x + x * dx;
    //            int testing_y = wk_postion.y + y * dy;

    //            if ( !(x == 0, y == 0) &&
    //                testing_x < map_dim.x &&
    //                testing_y < map_dim.y &&
    //                testing_x > 0 &&
    //                testing_y > 0 ) { // check for being within reference space.

    //                int temp = inv.map_veins_[testing_x][testing_y];

    //                if ( temp > max_observed ) {
    //                    max_observed = temp;
    //                    nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
    //                    wk_postion = WalkPosition( nearest_choke ); //search from there.
    //                    if ( max_observed > 275 ) {
    //                        return nearest_choke;
    //                        break;
    //                    }
    //                }
    //                else if ( y_dist_to_e == 0 || abs( x_dist_to_e / y_dist_to_e ) > 1 ) {
    //                    dx = x_dist_to_e > 0 ? 1 : -1;
    //                }
    //                else {
    //                    dy = y_dist_to_e > 0 ? 1 : -1;
    //                }
    //            }
    //        }
    //    }
    //}
     //another attempt
    //int x_dist_to_e, y_dist_to_e, dx, dy, x_inc, y_inc;

    //dx = x_dist_to_e > 0 ? 1 : -1;
    //dy = y_dist_to_e > 0 ? 1 : -1;

    //for ( auto i = 0; i < 50; ++i ) {

    //    x_dist_to_e = e_position.x - wk_postion.x;
    //    y_dist_to_e = e_position.y - wk_postion.y;

    //    int testing_x = wk_postion.x + dx;
    //    int testing_y = wk_postion.y + dy;

    //    if ( testing_x < map_dim.x &&

    //        testing_y < map_dim.y &&
    //        testing_x > 0 &&
    //        testing_y > 0 ) { // check for being within reference space.

    //        int temp = inv.map_veins_[testing_x][testing_y];

    //        if ( temp > max_observed ) {
    //            max_observed = temp;
    //            nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
    //            wk_postion = WalkPosition( nearest_choke ); //search from there.
    //            if ( max_observed > 275 ) {
    //                return nearest_choke;
    //                break;
    //            }
    //        }
    //        else if ( abs(y_dist_to_e / x_dist_to_e) < 1 ) {
    //            dx += x_dist_to_e > 0 ? 1 : -1;
    //        }
    //        else {
    //            dy += y_dist_to_e > 0 ? 1 : -1;
    //        }
    //    }
    //}

    return nearest_choke;
}

Position MeatAIModule::getUnit_Center(Unit unit){
	return Position(unit->getPosition().x + unit->getType().dimensionLeft(), unit->getPosition().y + unit->getType().dimensionUp());
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


