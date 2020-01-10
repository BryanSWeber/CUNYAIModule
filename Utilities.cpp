#pragma once
# include "Source\CUNYAIModule.h"
#include <numeric> // std::accumulate
#include <fstream>
#include <algorithm>

using namespace BWAPI;
using namespace Filter;
using namespace std;

// Gets units last error and prints it directly onscreen.  From tutorial.
void CUNYAIModule::PrintError_Unit(const Unit &unit) {
    Position pos = unit->getPosition();
    Error lastErr = Broodwar->getLastError();
    Broodwar->registerEvent( [pos, lastErr]( Game* ) { Broodwar->drawTextMap( pos, "%c%s", Text::Red, lastErr.c_str() ); },   // action
        nullptr,    // condition
        Broodwar->getLatencyFrames() );  // frames to run.
}

// Identifies those moments where a worker is gathering and its unusual subcases.
bool CUNYAIModule::isActiveWorker(const Unit &unit){
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

bool CUNYAIModule::isInLine(const Unit &unit){
    bool passive = 
        unit->getOrder() == BWAPI::Orders::WaitForMinerals ||
        unit->getOrder() == BWAPI::Orders::WaitForGas ||
        unit->getOrder() == BWAPI::Orders::ResetCollision;
    return passive;
}

// An improvement on existing idle scripts. Returns true if stuck or finished with most recent task.
bool CUNYAIModule::isIdleEmpty(const Unit &unit) {

    bool laden_worker = unit->isCarryingGas() || unit->isCarryingMinerals();

    UnitCommandType u_type = unit->getLastCommand().getType();

    bool task_complete = (u_type == UnitCommandTypes::Move && !unit->isMoving() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 30 * 24) ||
                         (u_type == UnitCommandTypes::Morph && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 5 * 24 && !(unit->isMorphing() || unit->isMoving() || unit->isAccelerating())) ||
                         (u_type == UnitCommandTypes::Attack_Move && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Attack_Unit && !unit->isMoving() && !unit->isAttacking()) ||
                         (u_type == UnitCommandTypes::Return_Cargo && !laden_worker && !isInLine(unit) ) ||
                         (u_type == UnitCommandTypes::Gather && !unit->isMoving() && !unit->isGatheringGas() && !unit->isGatheringMinerals() && !isInLine(unit)) ||
                         (u_type == UnitCommandTypes::Build && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 30 * 24 && !( unit->isMoving() || unit->isAccelerating() ) ) || // assumes a command has failed if it hasn't executed in the last 10 seconds.
                         (u_type == UnitCommandTypes::Upgrade && !unit->isUpgrading() && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 15 * 24) || // unit is done upgrading.
                         (u_type == UnitCommandTypes::Burrow && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 3 * 24) ||
                         (u_type == UnitCommandTypes::Unburrow && unit->getLastCommandFrame() < Broodwar->getFrameCount() - 3 * 24) ||
                          u_type == UnitCommandTypes::None ||
                          u_type == UnitCommandTypes::Stop ||
                          u_type == UnitCommandTypes::Unknown;


    return ( task_complete || unit->isStuck() ) && !isActiveWorker(unit) && !IsUnderAttack(unit) && spamGuard(unit);
}

// Did the unit fight in the last few moments?
bool CUNYAIModule::isRecentCombatant(const Stored_Unit &su) {
    bool fighting_now = (su.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (su.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Attack_Unit);
    bool recent_order = su.bwapi_unit_->getLastCommandFrame() + 24 > Broodwar->getFrameCount();
    bool retreat_or_fight = (su.phase_ == Stored_Unit::Retreating || su.phase_ == Stored_Unit::Attacking);
    return (fighting_now || retreat_or_fight) && recent_order;
}

// Checks if a unit is a combat unit.
bool CUNYAIModule::isFightingUnit(const Unit &unit)
{
    if ( !unit )
    {
        return false;
    }

    UnitType u_type = unit->getType();
    return isFightingUnit(u_type);

}

// Checks if a stored unit is a combat unit.
bool CUNYAIModule::isFightingUnit(const Stored_Unit &unit)
{
    if (!unit.valid_pos_)
        return false;
    return isFightingUnit(unit.type_);
}

// Checks if a stored unit is a combat unit.
bool CUNYAIModule::isFightingUnit(const UnitType &unittype)
{
    if (unittype == UnitTypes::Spell_Scanner_Sweep)
        return false;

    // no workers, overlords, or larva...
    if (unittype.isWorker() ||
        //unit.type_.isBuilding() ||
        unittype == BWAPI::UnitTypes::Zerg_Larva ||
        unittype == BWAPI::UnitTypes::Zerg_Overlord)
    {
        return false;
    }

    // This is a last minute check for psi-ops or transports.
    if (unittype.canAttack() ||
        unittype.maxEnergy() > 0 ||
        unittype.isDetector() ||
        unittype == BWAPI::UnitTypes::Terran_Bunker ||
        unittype.spaceProvided() ||
        unittype == BWAPI::UnitTypes::Protoss_Carrier ||
        unittype == BWAPI::UnitTypes::Protoss_Reaver)
    {
        return true;
    }

    return false;
}


void CUNYAIModule::writePlayerModel(const Player_Model &player, const string label)
{
    if constexpr(ANALYSIS_MODE) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        if (Broodwar->getFrameCount() % 96 == 0) {
            //living
            string smashed_unit_types = "";
            string smashed_unit_positions = "";
            string smashed_unit_stock_value = "";
            string smashed_unit_valid_positions = "";
            string smashed_unit_phase = "";
            string place;
            string type;
            string stock_value;
            string valid_pos;
            string last_phase;
            //dead
            string smashed_dead_unit_types = "";
            string smashed_dead_unit_positions = "";
            string smashed_dead_unit_stock_value = "";
            string smashed_dead_unit_valid_positions = "";
            string dead_place;
            string dead_type;
            string dead_stock_value;
            string dead_valid_pos;
            //science
            string smashed_upgrade_types = "";
            string smashed_tech_types = "";
            string smashed_inferred_building_types = "";
            string up_type;
            string tech_type;
            string inferred_building_type;

            //living units - position, type, stock value.
            for (auto u : player.units_.unit_map_) {

                std::stringstream place_translator;
                place_translator << u.second.pos_;
                place = place_translator.str();
                smashed_unit_positions += place + ", ";

                std::stringstream type_translator;
                type_translator << u.second.type_.c_str();
                type = type_translator.str();
                smashed_unit_types += type + ", ";

                std::stringstream stock_value_translator;
                stock_value_translator << u.second.stock_value_;
                stock_value = stock_value_translator.str();
                smashed_unit_stock_value += stock_value + ", ";

                std::stringstream valid_pos_translator;
                valid_pos = u.second.valid_pos_ ? "True" : "False";
                smashed_unit_valid_positions += valid_pos + ", ";

                smashed_unit_phase += u.second.phase_ + ", ";
            }


            //dead units - position, type, stock value.
            for (auto u : player.casualties_.unit_map_) {

                std::stringstream dead_place_translator;
                dead_place_translator << u.second.pos_;
                dead_place = dead_place_translator.str();
                smashed_dead_unit_positions += dead_place + ", ";

                std::stringstream dead_type_translator;
                dead_type_translator << u.second.type_.c_str();
                dead_type = dead_type_translator.str();
                smashed_dead_unit_types += dead_type + ", ";

                std::stringstream dead_stock_value_translator;
                dead_stock_value_translator << u.second.stock_value_;
                dead_stock_value = dead_stock_value_translator.str();
                smashed_dead_unit_stock_value += dead_stock_value + ", "; // might not be relevant.

                std::stringstream dead_valid_pos_translator;
                dead_valid_pos = u.second.valid_pos_ ? "True" : "False";
                smashed_dead_unit_valid_positions += dead_valid_pos + ", "; // might not be relevant.

            }

            //science
            //upgrades
            for (auto u : player.researches_.upgrades_) {
                up_type = u.first.c_str();
                if (u.second > 0) {
                    smashed_upgrade_types += up_type + ", ";
                }
            }
            //tech types
            for (auto u : player.researches_.tech_) {

                tech_type = u.first.c_str();
                if (u.second) {
                    smashed_tech_types += tech_type + ", ";
                }
            }
            // Research-sort Buildings, includes inferred ones.
            for (auto u : player.researches_.tech_buildings_) {
                inferred_building_type = u.first.c_str();
                if (u.second > 0) {
                    smashed_inferred_building_types += inferred_building_type + ", ";
                }
            }

            output.open("..\\write\\" + Broodwar->mapFileName() + "_v_" + Broodwar->enemy()->getName() + "_status.txt", ios_base::app);

            output << label << " Frame Count " << Broodwar->getFrameCount() << endl;
            output << " Unit Types " << smashed_unit_types << endl;
            output << " Positions " << smashed_unit_positions << endl;
            output << " Stock Value " << smashed_unit_stock_value << endl;
            output << " Valid Positions " << smashed_unit_valid_positions << endl;
            output << " Phase " << smashed_unit_phase << endl;
            output << " Dead Unit Types " << smashed_dead_unit_types << endl;
            output << " Dead Positions " << smashed_dead_unit_positions << endl;
            output << " Dead Stock Value " << smashed_dead_unit_stock_value << endl;
            output << " Dead Valid Positions " << smashed_dead_unit_valid_positions << endl;
            output << " Upgrade Types " << smashed_upgrade_types << endl;
            output << " Tech Types " << smashed_upgrade_types << endl;
            output << " Inferred Buildings " << smashed_inferred_building_types << endl;

            if (player.bwapi_player_) {
                output << " Unit Score " << player.bwapi_player_->getUnitScore() << endl;
                output << " Kill Score " << player.bwapi_player_->getKillScore() << endl;
                output << " Building Score " << player.bwapi_player_->getBuildingScore() << endl;
            }

            output << " Labor " << player.spending_model_.worker_stock <<  " alpha_L " << player.spending_model_.alpha_econ  << " gradient " << player.spending_model_.econ_derivative << endl;
            output << " (K)Capital " << player.spending_model_.army_stock << " alpha_K " << player.spending_model_.alpha_army << " gradient " << player.spending_model_.army_derivative << endl;
            output << " Technology " << player.spending_model_.tech_stock << " alpha_T " << player.spending_model_.alpha_tech << " gradient " << player.spending_model_.tech_derivative << endl;
            output << " ln(Y), ln(Utility) " << player.spending_model_.getlnY() << endl;
            output << " Testing Net Worth Function " << player.estimated_net_worth_ << endl;

            
            output.close();
        }
    }
}

// Outlines the case where UNIT cannot attack ENEMY type (air/ground), while ENEMY can attack UNIT.  Essentially bidirectional Can_Fight checks.
//bool CUNYAIModule::Futile_Fight( Unit unit, Unit enemy ) {
//    bool e_invunerable = (enemy->isFlying() && unit->getType().airWeapon() == WeaponTypes::None ) || (!enemy->isFlying() && unit->getType().groundWeapon() == WeaponTypes::None) || unit->getType() == UnitTypes::Terran_Bunker || unit->getType() == UnitTypes::Protoss_Carrier || (unit->getType() == UnitTypes::Protoss_Reaver && !enemy->isFlying()); // if we cannot attack them.
//    bool u_vunerable = (unit->isFlying() && enemy->getType().airWeapon() != WeaponTypes::None) || (!unit->isFlying() && enemy->getType().groundWeapon() != WeaponTypes::None) || enemy->getType() == UnitTypes::Terran_Bunker || enemy->getType() == UnitTypes::Protoss_Carrier || (enemy->getType() == UnitTypes::Protoss_Reaver && !unit->isFlying()); // they can attack us.
//    
//    return ( e_invunerable && u_vunerable ) || ( u_vunerable && !enemy->isDetected() ); // also if they are cloaked and can attack us.
//}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight(UnitType u_type, UnitType e_type) {
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool shoots_without_weapons = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier; // medics do not fight per se.
    bool reaver = u_type == UnitTypes::Protoss_Reaver && !e_type.isFlyer();
    bool could_get_spelled = (u_type == UnitTypes::Protoss_High_Templar || u_type == UnitTypes::Protoss_Dark_Archon || u_type == UnitTypes::Zerg_Defiler) && !e_type.isBuilding();
    bool queen_broodling = u_type == UnitTypes::Zerg_Queen && !e_type.isFlyer() && e_type != UnitTypes::Protoss_Probe && e_type != UnitTypes::Protoss_Reaver && e_type != UnitTypes::Protoss_Archon && e_type != UnitTypes::Protoss_Dark_Archon;
    bool science_irradiate = u_type == UnitTypes::Terran_Science_Vessel && e_type.isOrganic() && !e_type.isBuilding();
    bool e_vunerable = (has_appropriate_weapons || shoots_without_weapons || queen_broodling || reaver || science_irradiate || could_get_spelled); // if we cannot attack them.
    return e_vunerable; // also if they are cloaked and can attack us.
}

// Outlines the case where UNIT can attack ENEMY;
bool CUNYAIModule::Can_Fight( Unit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit->getType();
    if (!enemy->isDetected()) return false;
    return CUNYAIModule::Can_Fight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight( Unit unit, Stored_Unit enemy ) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit->getType();
    if (enemy.bwapi_unit_ && !enemy.bwapi_unit_->isDetected()) return false;
    return CUNYAIModule::Can_Fight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight(Stored_Unit unit, Stored_Unit enemy) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit.type_;
    if (enemy.bwapi_unit_ && !enemy.bwapi_unit_->isDetected()) return false;
    return CUNYAIModule::Can_Fight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::Can_Fight( Stored_Unit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit.type_;
    if (enemy && !enemy->isDetected()) return false;
    return CUNYAIModule::Can_Fight(u_type, e_type);
}

// Returns True if UnitType UT can attack anything in Unit_Inventory ENEMY; 
bool CUNYAIModule::canContributeToFight(const UnitType &ut, const Unit_Inventory enemy) {
    bool shooting_up = ut.airWeapon() != WeaponTypes::None && (enemy.stock_air_fodder_ + enemy.stock_fliers_ > 0);
    bool shooting_down = (ut.groundWeapon() != WeaponTypes::None || ut == UnitTypes::Protoss_Reaver || ut == UnitTypes::Zerg_Lurker) && (enemy.stock_ground_fodder_ + enemy.stock_ground_units_ > 0);
    bool shoots_without_weapons = ut == UnitTypes::Terran_Bunker || ut == UnitTypes::Protoss_Carrier || ut == UnitTypes::Terran_Science_Vessel || ut == UnitTypes::Terran_Medic || ut == UnitTypes::Protoss_High_Templar || ut == UnitTypes::Protoss_Dark_Archon || ut == UnitTypes::Zerg_Defiler || ut == UnitTypes::Zerg_Queen;
    return shooting_up || shooting_down || shoots_without_weapons;
}

// Returns True if UnitType UT is in danger from anything in Unit_Inventory ENEMY. Excludes Psions; 
bool CUNYAIModule::isInDanger(const UnitType &ut, const Unit_Inventory enemy) {
    bool shooting_up = ut.isFlyer() && enemy.stock_shoots_up_ > 0;
    bool shooting_down = !ut.isFlyer() && enemy.stock_shoots_down_ > 0;
    bool shoots_without_weapons = enemy.stock_psion_ > 0;

    return shooting_up || shooting_down || shoots_without_weapons;
}

// Counts all units of one type in existance and owned by enemies. Counts units under construction.
int CUNYAIModule::countUnits( const UnitType &type, const Unit_Inventory &ui )
{
    int count = 0;

    for ( auto & e : ui.unit_map_) {

        //if ( e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type ) { // Count units under construction
        //    count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        //} 
        //else if ( e.second.type_ == type ) {
        //    count++;
        //}

        count += (e.second.type_ == type) + (e.second.type_ != type && e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type) * (1 + e.second.build_type_.isTwoUnitsInOneEgg()) ; // better without if-conditions.
    }

    return count;
}

bool CUNYAIModule::containsUnit(const UnitType &type, const Unit_Inventory &ui) {

    for (auto & e : ui.unit_map_) {
        if(e.second.type_ == type) return true; 
    }
    return false;
}


// Counts all units of one type in existance and owned by enemies. 
int CUNYAIModule::countSuccessorUnits(const UnitType &type, const Unit_Inventory &ui)
{
    int count = 0;

    for (auto & e : ui.unit_map_) {

        //if ( e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type ) { // Count units under construction
        //    count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        //} 
        //else if ( e.second.type_ == type ) {
        //    count++;
        //}

        count += (e.second.type_ == type) + (e.second.type_ != type) * e.second.type_.isSuccessorOf(type); // better without if-conditions.
    }

    return count;
}

// Overload. (Very slow, since it uses BWAPI Unitsets) Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::countUnits( const UnitType &type, const Unitset &unit_set )
{
    int count = 0;
    for ( auto & unit : unit_set )
    {
        //if ( unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type ) { // Count units under construction
        //    count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        //} 
        //else if ( unit->getType() == type ) {
        //    count++;
        //}
        count += (unit->getType() == type) + (unit->getType() != type && unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type) * (1 + unit->getBuildType().isTwoUnitsInOneEgg()); // better without if-conditions.

    }

    return count;
}

// Overload. Counts all units in a set of one type in the reservation system. Does not reserve larva units. 
int CUNYAIModule::countUnits( const UnitType &type, const Reservation &res )
{
    int count = 0;
    for (auto it : res.reservation_map_ ) {
        if( it.second == type ) count++;
    }

    return count;
}

// Counts all units of one type in existance and owned by me. Counts units under construction.
int CUNYAIModule::countUnits(const UnitType &type)
{
    auto c_iter = find(CUNYAIModule::friendly_player_model.unit_type_.begin(), CUNYAIModule::friendly_player_model.unit_type_.end(), type);
    if (c_iter == CUNYAIModule::friendly_player_model.unit_type_.end()) {
        return 0;
    }
    else {
        int distance = std::distance(CUNYAIModule::friendly_player_model.unit_type_.begin(), c_iter);
        return CUNYAIModule::friendly_player_model.unit_count_[distance];
    }

}

// Counts all units of one type in existance and in progress by me. Counts units under construction.
int CUNYAIModule::countUnitsInProgress(const UnitType &type)
{
    auto c_iter = find(CUNYAIModule::friendly_player_model.unit_type_.begin(), CUNYAIModule::friendly_player_model.unit_type_.end(), type);
    if (c_iter == CUNYAIModule::friendly_player_model.unit_type_.end()) {
        return 0;
    }
    else {
        int distance = std::distance(CUNYAIModule::friendly_player_model.unit_type_.begin(), c_iter);
        return CUNYAIModule::friendly_player_model.unit_incomplete_[distance];
    }
}

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::countUnitsDoing(const UnitType &type, const UnitCommandType &u_command_type, const Unitset &unit_set)
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
// Overload. Counts all units in a set of one type owned by player. Includes individual units in production. 
int CUNYAIModule::countUnitsDoing(const UnitType &type, const UnitCommandType &u_command_type, const Unit_Inventory &ui)
{
    int count = 0;
    for (const auto & unit : ui.unit_map_)
    {
        if (unit.second.type_ == UnitTypes::Zerg_Egg && unit.second.build_type_ == type) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        }
        else if (unit.second.type_ == UnitTypes::Zerg_Drone && unit.second.build_type_ == type) { // Count units under construction
            count++;
        }
        else if (unit.second.type_ == type && unit.second.bwapi_unit_ && unit.second.bwapi_unit_->exists() && unit.second.bwapi_unit_->getLastCommand().getType() == u_command_type) {
            count++;
        }
    }

    return count;
}

// Overload. Counts all units in a set of one type owned by player. Includes individual units in production.  I have doubts about this function.
int CUNYAIModule::countUnitsInProgress(const UnitType &type, const Unit_Inventory &ui)
{
    int count = 0;
    for (const auto & unit : ui.unit_map_)
    {
        if (unit.second.type_ == UnitTypes::Zerg_Egg && unit.second.build_type_ == type) { // Count units under construction
            count += type.isTwoUnitsInOneEgg() ? 2 : 1; // this can only be lings or scourge, I believe.
        }
        else if (unit.second.type_ == UnitTypes::Zerg_Drone && unit.second.build_type_ == type) { // Count units under construction
            count++;
        }
        else if (unit.second.bwapi_unit_ && unit.second.bwapi_unit_->getBuildType() == type ) { // Count units under construction
            count++;
        }

    }

    return count;
}

int CUNYAIModule::countUnitsAvailableToPerform(const UpgradeType &upType) {
    int count = 0;
    for (auto u : Broodwar->self()->getUnits()) {
        if (u->getType() == upType.whatUpgrades() && !u->isMorphing() && u->isCompleted()) count++;
    }
    return count;
}

int CUNYAIModule::countUnitsAvailableToPerform(const TechType &techType) {
    int count = 0;
    for (auto u : Broodwar->self()->getUnits()) {
        if (u->getType() == techType.whatResearches() && !u->isMorphing() && u->isCompleted()) count++;
    }
    return count;
}

// evaluates the value of a stock of buildings, in terms of pythagorian distance of min & gas & supply. Assumes building is zerg and therefore, a drone was spent on it.
int CUNYAIModule::Stock_Buildings( const UnitType &building, const Unit_Inventory &ui ) {
    int cost = Stored_Unit(building).stock_value_;
    int instances = countUnits( building , ui );
    int total_stock = cost * instances;
    return total_stock;
}

// evaluates the value of a stock of upgrades, in terms of pythagorian distance of min & gas & supply. Counts totals of stacked upgrades like melee/range/armor.
int CUNYAIModule::Stock_Ups( const UpgradeType &ups ) {
    int lvl = Broodwar->self()->getUpgradeLevel( ups ) + static_cast<int>(Broodwar->self()->isUpgrading( ups ));
    int total_stock = 0;
    for ( int i = 1; i <= lvl; i++ ) {
        int cost = static_cast<int>(ups.mineralPrice() + 1.25 * ups.gasPrice());
        total_stock += cost;
    }
    return total_stock;
}

int CUNYAIModule::Stock_Tech(const TechType &tech) {
    bool lvl = Broodwar->self()->hasResearched(tech) + static_cast<int>(Broodwar->self()->isResearching(tech));
    int total_stock = 0;
    if ( lvl ) {
        int cost = static_cast<int>(tech.mineralPrice() + 1.25 * tech.gasPrice());
        total_stock += cost;
    }
    return total_stock;
}

int CUNYAIModule::Stock_Units( const UnitType &unit_type, const Unit_Inventory &ui) {
    int total_stock = 0;

    for ( auto & u : ui.unit_map_ ) {
        if ( u.second.type_ == unit_type ) {  // if you impose valid_pos here many of YOUR OWN UNITS will not be counted.
            total_stock += u.second.current_stock_value_;
        }
    }

    return total_stock;
}

// evaluates the value of a stock of combat units, for all unit types in a unit inventory. Does not count eggs.
int CUNYAIModule::Stock_Combat_Units( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None || ((UnitType)i).groundWeapon() != WeaponTypes::None || ((UnitType)i).maxEnergy() > 0 ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of units, for all unit types in a unit inventory
int CUNYAIModule::Stock_Units_ShootUp( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).airWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// Overload. evaluates the value of a stock of allied units, for all unit types in a unit inventory
int CUNYAIModule::Stock_Units_ShootDown( const Unit_Inventory &ui ) {
    int total_stock = 0;
    for ( int i = 0; i != 173; i++ )
    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
        if ( ((UnitType)i).groundWeapon() != WeaponTypes::None ) {
            total_stock += Stock_Units( ((UnitType)i), ui );
        }
    }
    return total_stock;
}

// evaluates the value of a stock of unit, in terms of supply added.
int CUNYAIModule::Stock_Supply( const UnitType &unit ) {
    int supply = unit.supplyProvided();
    int instances = countUnits( unit);
    int total_stock = supply * instances;
    return total_stock;
}

// returns helpful_friendly and helpful_enemy units from respective inventories.
//vector<int> CUNYAIModule::getUsefulStocks(const Unit_Inventory & friend_loc, const Unit_Inventory & enemy_loc)
//{
//    int helpful_e, helpful_u;
//
//        //helpful_e = min(enemy_loc.stock_shoots_down_, friend_loc.stock_ground_units_ * 2) + min(enemy_loc.stock_shoots_up_, friend_loc.stock_fliers_ * 2) - min(min(enemy_loc.stock_both_up_and_down_, friend_loc.stock_fliers_ * 2), friend_loc.stock_ground_units_ * 2); // A+B - A
//        //helpful_u = min(friend_loc.stock_shoots_down_, enemy_loc.stock_ground_units_ * 2) + min(friend_loc.stock_shoots_up_, enemy_loc.stock_fliers_ * 2) - min(min(friend_loc.stock_both_up_and_down_, enemy_loc.stock_fliers_ * 2), enemy_loc.stock_ground_units_ * 2); // A+B - A
//
//
//        helpful_e = enemy_loc.stock_shoots_down_ * (friend_loc.stock_ground_units_ > 0) + enemy_loc.stock_shoots_up_ * (friend_loc.stock_fliers_ > 0) - enemy_loc.stock_both_up_and_down_ * (friend_loc.stock_fliers_ > 0) * (friend_loc.stock_ground_units_ > 0); // A+B - A Union B
//           //if (friend_loc.stock_ground_units_ == 0) {
//            //    helpful_e = enemy_loc.stock_shoots_up_;
//            //}
//            //else if (friend_loc.stock_fliers_ == 0) {
//            //    helpful_e = enemy_loc.stock_shoots_down_;
//            //}
//        helpful_u = friend_loc.stock_shoots_down_ * (enemy_loc.stock_ground_units_ > 0) + friend_loc.stock_shoots_up_ * (enemy_loc.stock_fliers_ > 0) - friend_loc.stock_both_up_and_down_ * (enemy_loc.stock_fliers_ > 0) * (enemy_loc.stock_ground_units_ > 0); // A+B - A
//            //if (enemy_loc.stock_ground_units_ == 0) {
//            //    helpful_u = friend_loc.stock_shoots_up_;
//            //}
//            //else if (enemy_loc.stock_fliers_ == 0) {
//            //    helpful_u = friend_loc.stock_shoots_down_;
//            //}
//            
//        vector<int> return_vec = { helpful_u, helpful_e };
//        return return_vec;
//}

int CUNYAIModule::getTargetableStocks(const Unit & u, const Unit_Inventory & enemy_loc)
{
    int targetable_e = 0;
    targetable_e = (u->getType().airWeapon() != WeaponTypes::None) * (enemy_loc.stock_fliers_ + enemy_loc.stock_air_fodder_ ) + (u->getType().groundWeapon() != WeaponTypes::None) * (enemy_loc.stock_ground_units_ + enemy_loc.stock_ground_fodder_);
    return targetable_e;
}

int CUNYAIModule::getThreateningStocks(const Unit & u, const Unit_Inventory & enemy_loc)
{
    int threatening_e = 0;
    threatening_e = u->getType().isFlyer() * enemy_loc.stock_shoots_up_  +  !u->getType().isFlyer() * enemy_loc.stock_shoots_down_;
    return threatening_e;
}

//Strips the RACE_ from the front of the unit type string.
const char * CUNYAIModule::noRaceName( const char *name ) { //From N00b
    for ( const char *c = name; *c; c++ )
        if ( *c == '_' ) return ++c;
    return name;
}

//Converts a unit inventory into a unit set directly. Checks range. Careful about visiblity.
Unitset CUNYAIModule::getUnit_Set( const Unit_Inventory &ui, const Position &origin, const int &dist ) {
    Unitset e_set;
    for ( auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++ ) {
        if (static_cast<int>((*e).second.pos_.getDistance( origin )) <= dist ) {
            e_set.insert( (*e).second.bwapi_unit_ ); // if we take any distance and they are in inventory.
        }
    }
    return e_set;
}

Stored_Unit * CUNYAIModule::getStoredUnit(const Unit_Inventory & ui, const Unit & u)
{
	auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);
	bool found = found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end();
	if (found) return &found_item->second;
}

//Gets pointer to closest unit to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStored( Unit_Inventory &ui, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if ( !ui.unit_map_.empty() ) {
        for ( auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++ ) {
            temp_dist = static_cast<int>((*e).second.pos_.getDistance( origin ));
            if ( temp_dist <= min_dist && e->second.valid_pos_ ) {
                min_dist = temp_dist;
                return_unit = &(e->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit of a type to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStored(Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (e->second.type_ == u_type && e->second.valid_pos_ ){
                temp_dist = static_cast<int>((*e).second.pos_.getDistance(origin));
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
Stored_Resource* CUNYAIModule::getClosestStored(Resource_Inventory &ri, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            temp_dist = static_cast<int>((*r).second.pos_.getDistance(origin));
            if (temp_dist <= min_dist ) {
                min_dist = temp_dist;
                return_unit = &(r->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit of a type to point in Unit_inventory EXCLUDING the unit. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStored(const Unit unit, Unit_Inventory &ui, const UnitType &u_type, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (e->second.type_ == u_type && e->second.valid_pos_ && e->first != unit) {
                temp_dist = static_cast<int>((*e).second.pos_.getDistance(unit->getPosition()));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

Stored_Resource* CUNYAIModule::getClosestGroundStored(Resource_Inventory &ri, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            temp_dist = CUNYAIModule::current_map_inventory.getDifferentialDistanceOutFromHome(r->second.pos_, origin); // can't be const because of this line.
            if (temp_dist <= min_dist) {
                min_dist = temp_dist;
                return_unit = &(r->second);
            }
        }
    }

    return return_unit;
}

// Allows type -specific- selection. 
Stored_Resource* CUNYAIModule::getClosestGroundStored(Resource_Inventory &ri,const UnitType type, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            temp_dist = CUNYAIModule::current_map_inventory.getDifferentialDistanceOutFromHome(r->second.pos_, origin); // can't be const because of this line.
            bool right_type = (type == r->second.type_ || type.isMineralField() && r->second.type_.isMineralField()); //WARNING:: Minerals have 4 types.
            if (temp_dist <= min_dist && right_type ) { 
                min_dist = temp_dist;
                return_unit = &(r->second);
            }
        }
    }

    return return_unit;
}

Stored_Unit* CUNYAIModule::getClosestGroundStored(Unit_Inventory &ui, const Position &origin) {

    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            temp_dist = CUNYAIModule::current_map_inventory.getDifferentialDistanceOutFromHome(u->second.pos_, origin); // can't be const because of this line.
            if (temp_dist <= min_dist && !u->second.is_flying_ && u->second.valid_pos_) {
                min_dist = temp_dist;
                return_unit = &(u->second);
            }
        }
    }

    return return_unit;
}

Stored_Unit* CUNYAIModule::getClosestAirStored(Unit_Inventory &ui, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            temp_dist = CUNYAIModule::current_map_inventory.getDifferentialDistanceOutFromHome(u->second.pos_, origin); // can't be const because of this line.
            if (temp_dist <= min_dist && u->second.is_flying_ && u->second.valid_pos_) {
                min_dist = temp_dist;
                return_unit = &(u->second);
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit to point in Unit_inventory. Checks range. Careful about visiblity.
Stored_Unit* CUNYAIModule::getClosestStoredBuilding(Unit_Inventory &ui, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (e->second.type_.isBuilding()) {
                temp_dist = static_cast<int>((*e).second.pos_.getDistance(origin));
                if (temp_dist <= min_dist && e->second.valid_pos_) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets position of closest occupied expo to position. Checks range. Careful about visiblity.
//Position CUNYAIModule::getClosestExpo(const Map_Inventory &inv, const Unit_Inventory &ui, const Position &origin, const int &dist) {
//    //int min_dist = dist;
//    //int temp_dist = 999999;
//    //Position return_pos = Positions::Origin;
//    //vector<Position> hatchery_positions;
//
//    ////get all the hatcheries (of any type.).
//    //for (auto & potential_hatchery : ui.unit_inventory_) {
//    //    if (potential_hatchery.second.type_.isResourceDepot()) hatchery_positions.push_back(potential_hatchery.second.pos_);
//    //}
//
//    //for (auto & expo = inv.expo_positions_complete_.begin(); expo != inv.expo_positions_complete_.end() && !inv.expo_positions_complete_.empty(); expo++) {
//    //    Position expo_pos = Position(*expo);
//    //    temp_dist = expo_pos.getDistance(return_pos);
//    //    
//    //    bool occupied_expo = false;
//    //    //If it is occupied, we can count it.
//    //    for (auto potential_occupant : hatchery_positions) {
//    //        occupied_expo = potential_occupant.getDistance(expo_pos) < 500;
//    //        if (occupied_expo) break;
//    //    }
//
//    //    if (temp_dist <= min_dist && expo_pos.isValid() && occupied_expo) {
//    //        min_dist = temp_dist;
//    //        return_pos = expo_pos;
//    //    }
//    //}
//    //
//
//    //return return_pos;
//}

//Gets pointer to closest unit to point in Resource_inventory. Checks range. Careful about visiblity.
Stored_Resource* CUNYAIModule::getClosestStored(Resource_Inventory &ri, const UnitType &r_type, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    Stored_Resource* return_unit = nullptr;

    if (!ri.resource_inventory_.empty()) {
        for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
            if (r->second.type_ == r_type && r->second.valid_pos_) {
                temp_dist = static_cast<int>((*r).second.pos_.getDistance(origin));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(r->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit from unit in Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr.
Stored_Unit* CUNYAIModule::getClosestAttackableStored(Unit_Inventory &ui, const Unit unit, const int &dist = 999999) {
    int min_dist = dist;
    bool can_attack;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = CUNYAIModule::Can_Fight(unit, e->second);
            if (can_attack && e->second.pos_.isValid() && e->second.valid_pos_) {
                temp_dist = static_cast<int>(e->second.pos_.getDistance(unit->getPosition()));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatOrTargetStored( Unit_Inventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;

    if ( !ui.unit_map_.empty() ) {
        for ( auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++ ) {
            can_attack = Can_Fight(u_type, e->second.type_) && e->second.bwapi_unit_;
            can_be_attacked_by = Can_Fight(e->second.type_, u_type);
            if ( (can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = static_cast<int>(e->second.pos_.getDistance( origin ));
                if ( temp_dist <= min_dist ) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatOrTargetStored(Unit_Inventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = Can_Fight(unit, e->second);
            can_be_attacked_by = Can_Fight(e->second, unit);

            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = static_cast<int>(e->second.pos_.getDistance(origin));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatOrTargetExcluding(Unit_Inventory &ui, const UnitType ut, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = Can_Fight(unit, e->second);
            can_be_attacked_by = Can_Fight(e->second, unit);

            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && e->second.type_ != ut) {
                temp_dist = static_cast<int>(e->second.pos_.getDistance(origin));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest threat to unit within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getClosestThreatStored(Unit_Inventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_be_attacked_by;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_be_attacked_by = Can_Fight(e->second, unit);

            if (can_be_attacked_by && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = static_cast<int>(e->second.pos_.getDistance(origin));
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest threat/target unit from home within Unit_inventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
Stored_Unit* CUNYAIModule::getMostAdvancedThreatOrTargetStored(Unit_Inventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by, we_are_a_flyer;
    int temp_dist = 999999;
    Stored_Unit* return_unit = nullptr;
    Position origin = unit->getPosition();
    we_are_a_flyer = unit->getType().isFlyer();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = Can_Fight(unit, e->second);
            can_be_attacked_by = Can_Fight(e->second, unit);
            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                if (we_are_a_flyer) {
                    temp_dist = unit->getDistance(e->second.pos_);
                }
                else {
                    temp_dist = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromHome(e->second.pos_);
                }

                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Unit_Inventory CUNYAIModule::getThreateningUnitInventoryInRadius( const Unit_Inventory &ui, const Position &origin, const int &dist, const bool &air_attack ) {
    Unit_Inventory ui_out;
    if (air_attack) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if ((*e).second.pos_.getDistance(origin) <= dist && e->second.valid_pos_ && Can_Fight(e->second.type_, UnitTypes::Zerg_Overlord)) {
                ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
            }
        }
    }
    else {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if ((*e).second.pos_.getDistance(origin) <= dist && e->second.valid_pos_ && Can_Fight(e->second.type_, UnitTypes::Zerg_Drone)) {
                ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
            }
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInRadius(const Unit_Inventory &ui, const Position &origin, const int &dist) {
    Unit_Inventory ui_out;
    for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
        if ((*e).second.pos_.getDistance(origin) <= dist && e->second.valid_pos_) {
            ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr. Overloaded for specifi types.
Unit_Inventory CUNYAIModule::getUnitInventoryInRadius(const Unit_Inventory &ui, const UnitType u_type, const Position &origin, const int &dist) {
    Unit_Inventory ui_out;
    for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
        if ((*e).second.pos_.getDistance(origin) <= dist && (*e).second.type_== u_type && e->second.valid_pos_ ) {
            ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an resource inventory for units within an area. Returns resource inventory meeting that critera. Can return nullptr.
Resource_Inventory CUNYAIModule::getResourceInventoryInRadius(const Resource_Inventory &ri, const Position &origin, const int &dist) {
    Resource_Inventory ri_out;
	for (auto & e = ri.resource_inventory_.begin(); e != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); e++) {
		if ((*e).second.pos_.getDistance(origin) <= dist) {
			ri_out.addStored_Resource((*e).second); // if we take any distance and they are in inventory.
		}
	}
	return ri_out;
}

//Searches an resource inventory for units within an area. Returns resource inventory meeting that critera. Can return nullptr.
Resource_Inventory CUNYAIModule::getResourceInventoryInArea(const Resource_Inventory &ri, const Position &origin) {
	Resource_Inventory ri_out;
	auto area = BWEM::Map::Instance().GetArea(TilePosition(origin));
	if (area) {
		int area_id = area->Id();
		for (auto & r = ri.resource_inventory_.begin(); r != ri.resource_inventory_.end() && !ri.resource_inventory_.empty(); r++) {
			if (r->second.areaID_ == area_id) {
				ri_out.addStored_Resource((*r).second); // if we take any distance and they are in inventory.
			}
		}
	}
	return ri_out;
}

//Searches an enemy inventory for units within a range. Returns units that are not in weapon range but are in inventory. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitsOutOfReach(const Unit_Inventory &ui, const Unit &target) {
    Unit_Inventory ui_out;
    for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
        if (u->second.valid_pos_ && ( !(*u).second.bwapi_unit_->canMove() && !(*u).second.bwapi_unit_->isInWeaponRange(target) ) ) {
            ui_out.addStored_Unit((*u).second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInArea(const Unit_Inventory &ui, const Position &origin) {
    Unit_Inventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                if (e->second.areaID_ == area_id) {
                    ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
                }
            }
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInArea(const Unit_Inventory &ui, const int AreaID) {
    Unit_Inventory ui_out;

    for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
        if (e->second.areaID_ == AreaID) {
            ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
        }
    }

    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Neighborhood. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInNeighborhood(const Unit_Inventory &ui, const Position &origin) {
    Unit_Inventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                for (auto & a : area->AccessibleNeighbours()) {
                    if (e->second.areaID_ == area_id || e->second.areaID_ == a->Id()) {
                        ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
                    }
                }
            }
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Returns enemy inventory meeting that critera. Can return nullptr.
Unit_Inventory CUNYAIModule::getUnitInventoryInArea(const Unit_Inventory &ui, const UnitType ut, const Position &origin) {
    Unit_Inventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                if (e->second.areaID_ == area_id && e->second.type_ == ut) {
                    ui_out.addStored_Unit((*e).second); // if we take any distance and they are in inventory.
                }
            }
        }
    }
    return ui_out;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. 
bool CUNYAIModule::checkOccupiedArea( const Unit_Inventory &ui, const Position &origin ) {
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                if (e->second.areaID_ == area_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

//Searches an inventory for units of within a neighborhood (grouping of areas). Returns TRUE if the neighborhood is occupied. 
bool CUNYAIModule::checkOccupiedNeighborhood(const Unit_Inventory &ui, const Position &origin) {
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                for (auto & a : area->AccessibleNeighbours()) {
                    if (e->second.areaID_ == area_id || e->second.areaID_ == a->Id()) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

//Searches an inventory for units of a type.. Returns TRUE if the area is occupied. 
bool CUNYAIModule::checkOccupiedArea(const Unit_Inventory &ui, const UnitType type, const Position &origin) {
    auto area = BWEM::Map::Instance().GetArea(TilePosition(origin));
    if (area) {
        int area_id = area->Id();
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (e->second.type_ == type) {
                if (e->second.areaID_ == area_id) {
                    return true;
                }
            }
        }
    }
    return false;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. Checks retangles for performance reasons rather than radius.
//bool CUNYAIModule::checkThreatenedArea(const Unit_Inventory &ui, const UnitType &type, const Position &origin, const int &dist) {
//
//    for (auto & e = ui.unit_inventory_.begin(); e != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); e++) {
//        if (e->second.pos_.x < origin.x + dist && e->second.pos_.x > origin.x - dist &&
//            e->second.pos_.y < origin.y + dist && e->second.pos_.y > origin.y - dist &&
//            CUNYAIModule::Can_Fight(e->second.type_, Stored_Unit(type) ) ) {
//            return true;
//        }
//    }
//
//    return false;
//}

bool CUNYAIModule::isOnScreen( const Position &pos , const Position &screen_pos) {
    bool inrange_x = screen_pos.x < pos.x && screen_pos.x + 640 > pos.x;
    bool inrange_y = screen_pos.y < pos.y && screen_pos.y + 480 > pos.y;
    return inrange_x && inrange_y;
}

//Returns TRUE if the unit is ready to move and false if the unit should be ignored for now.
bool CUNYAIModule::spamGuard(const Unit &unit, int cd_frames_chosen) {

    bool ready_to_move = true;
    bool wait_for_cooldown = false;
    int cd_frames = 0;

    if (cd_frames_chosen == 99) {// if default value, then we assume 0 cd frames. This is nearly always the case.
        cd_frames = 0;
    } 
    else { // if the person has selected some specific delay they are looking for, check that.
        ready_to_move = unit->getLastCommandFrame() < Broodwar->getFrameCount() - cd_frames_chosen;
        return ready_to_move;
    }

    bool unit_fighting = unit->isStartingAttack();
    if (unit_fighting) {
        return false; //unit is not ready to move.
    }

    UnitCommandType u_command = unit->getLastCommand().getType();

    if ( u_command == UnitCommandTypes::Attack_Unit || u_command == UnitCommandTypes::Attack_Move ) {
        UnitType u_type = unit->getType();
        //cd_frames = Broodwar->getLatencyFrames();
        //if (u_type == UnitTypes::Zerg_Drone) {
        //    cd_frames = 1;
        //}
        //else if (u_type == UnitTypes::Zerg_Zergling) {
        //    cd_frames = 5;
        //}
        //else if (u_type == UnitTypes::Zerg_Hydralisk) {
        //    cd_frames = 7;
        //}
        //else if (u_type == UnitTypes::Zerg_Lurker) {
        //    cd_frames = 2;
        //}
        //else if (u_type == UnitTypes::Zerg_Mutalisk) {
        //    cd_frames = 1;
        //}
        //else if (u_type == UnitTypes::Zerg_Ultralisk) {
        //    cd_frames = 15;
        //}
        //wait_for_cooldown = unit->getGroundWeaponCooldown() > 0 || unit->getAirWeaponCooldown() > 0;
        if (u_type == UnitTypes::Zerg_Devourer) {
            cd_frames = 28; // this is an INSANE cooldown.
        }
    }

    if (u_command == UnitCommandTypes::Burrow || u_command == UnitCommandTypes::Unburrow) {
        cd_frames = 14;
    }

    if (u_command == UnitCommandTypes::Morph || u_command == UnitCommandTypes::Build) {
        cd_frames = 24;
    }

    if (u_command == UnitCommandTypes::Move) {
        cd_frames = 5;
    }

    //if (u_command == UnitCommandTypes::Hold_Position) {
    //    cd_frames = 5;
    //}
    //if (u_command == UnitCommandTypes::Attack_Move) {
    //    cd_frames += 2; // an ad-hoc delay for aquiring targets, I don't know what it is formally atm.
    //}
    //if (u_order == Orders::Move && ( (!unit->isMoving() && !unit->isAccelerating()) || unit->isBraking() ) ) {
    //    cd_frames = 7; // if it's not moving, accellerating or IS breaking.
    //}

    //if ( u_order == Orders::AttackMove) {
    //    cd_frames = 12;
    //}

    //if (cd_frames < Broodwar->getLatencyFrames() ) {
    //    cd_frames = Broodwar->getLatencyFrames();
    //}

    ready_to_move = Broodwar->getFrameCount() - unit->getLastCommandFrame() > cd_frames + Broodwar->getLatencyFrames();
    return ready_to_move; // we must wait at least 5 frames before issuing them a new command regardless.

}

//checks if there is a smooth path to target. in minitiles
//bool CUNYAIModule::isClearRayTrace( const Position &initial, const Position &final, const Map_Inventory &inv ) // see Brehsam's Algorithm
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

//checks if there is a smooth path to target. in minitiles. May now choose the map directly, and threshold will break as FALSE for values greater than or equal to. More flexible than previous versions.
bool CUNYAIModule::isClearRayTrace(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold){ // see Brehsam's Algorithm for all 8 octants.
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
        if ( safety_check && target_map[x][y] >= threshold) {
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
            if (safety_check && target_map[x][y] >= threshold) {
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
        if (safety_check && target_map[x][y] >= threshold) {
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
            if (safety_check && target_map[x][y] >= threshold) {
                return false;
            }

        }
    }

    return true;

}

//Counts the number of minitiles in a path to target. in minitiles
int CUNYAIModule::getClearRayTraceSquares(const Position &initialp, const Position &finalp, const vector<vector<int>> &target_map, const int &threshold) // see Brehsam's Algorithm. Is likely bugged in current state.
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
        if (safety_check && target_map[x][y] >= 1) {
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
            if (safety_check && target_map[x][y] >= 1) {
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
        if (safety_check && target_map[x][y] >= 1) {
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
            if ( safety_check && target_map[x][y] >= 1) {
                squares_counted++;
            }

        }
    }

    return squares_counted;
}


double CUNYAIModule::getProperSpeed( const Unit u ) {
    UnitType u_type = u->getType();
    Player owner = u->getPlayer();

    double base_speed = u_type.topSpeed();
    if (u_type == UnitTypes::Zerg_Zergling && owner->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Overlord && owner->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Muscular_Augments) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Zerg_Ultralisk && owner->getUpgradeLevel(UpgradeTypes::Anabolic_Synthesis) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Protoss_Scout && owner->getUpgradeLevel(UpgradeTypes::Gravitic_Thrusters) > 0) {
        base_speed *= 1.5;
    }
    else if (u_type == UnitTypes::Protoss_Zealot && owner->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0) {
        base_speed *= 1.5;
    }

    return base_speed;
}

double CUNYAIModule::getProperSpeed(const UnitType &type, const Player owner) {
    double base_speed = type.topSpeed();

    if (type == UnitTypes::Zerg_Zergling && owner->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Overlord && owner->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Muscular_Augments) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Zerg_Ultralisk && owner->getUpgradeLevel(UpgradeTypes::Anabolic_Synthesis) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Protoss_Scout && owner->getUpgradeLevel(UpgradeTypes::Gravitic_Thrusters) > 0) {
        base_speed *= 1.5;
    }
    else if (type == UnitTypes::Protoss_Zealot && owner->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) > 0) {
        base_speed *= 1.5;
    }
    return base_speed;
}

int CUNYAIModule::getExactRange(const Unit u) {

    UnitType u_type = u->getType();
    Player owner = u->getPlayer();

    int base_range = max(u_type.groundWeapon().maxRange(), u_type.airWeapon().maxRange());

    if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Dragoon && owner->getUpgradeLevel(UpgradeTypes::Singularity_Charge) > 0) {
        base_range += 2 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Reaver) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Carrier) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Terran_Marine && owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Terran_Goliath && owner->getUpgradeLevel(UpgradeTypes::Charon_Boosters) > 0) {
        base_range += 3 * 32;
    }
    else if ( u_type == UnitTypes::Terran_Barracks ) {
        base_range = UnitTypes::Terran_Marine.groundWeapon().maxRange() + 32 + (owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) * 32;
    }

    return base_range;
}

int CUNYAIModule::getExactRange(const UnitType u_type, const Player owner) {
    int base_range = max(u_type.groundWeapon().maxRange(), u_type.airWeapon().maxRange());
    if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Dragoon && owner->getUpgradeLevel(UpgradeTypes::Singularity_Charge) > 0) {
        base_range += 2 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Reaver) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Protoss_Carrier) {
        base_range += 8 * 32;
    }
    else if (u_type == UnitTypes::Terran_Marine && owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) {
        base_range += 1 * 32;
    }
    else if (u_type == UnitTypes::Terran_Goliath && owner->getUpgradeLevel(UpgradeTypes::Charon_Boosters) > 0) {
        base_range += 3 * 32;
    }
    else if ( u_type == UnitTypes::Terran_Barracks ) {
        base_range = UnitTypes::Terran_Marine.groundWeapon().maxRange() + 32 + (owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) * 32;
    }

    return base_range;
}

int CUNYAIModule::getFunctionalRange(const UnitType u_type, const Player owner) {
    return max(getExactRange(u_type, owner), 32 );
}

int CUNYAIModule::getFunctionalRange(const Unit u) {
    return max(getExactRange(u), 32);
}

//How far can the unit move in one MAFAP sim (120 frames)? Currently too large.
int CUNYAIModule::getChargableDistance(const Unit & u)
{
    int size_array[] = { u->getType().dimensionDown(), u->getType().dimensionUp(), u->getType().dimensionLeft(), u->getType().dimensionRight() };
    return (u->getType() != UnitTypes::Zerg_Lurker) * static_cast<int>(CUNYAIModule::getProperSpeed(u) * FAP_SIM_DURATION) + CUNYAIModule::getFunctionalRange(u) + *std::max_element( size_array, size_array + 4 ); //lurkers have a proper speed of 0. 96 frames is length of MAfap sim.

}


//finds nearest choke or best location within 100 minitiles.
Position CUNYAIModule::getNearestChoke( const Position &initial, const Position &final, const Map_Inventory &inv ) {
    WalkPosition e_position = WalkPosition( final );
    WalkPosition wk_postion = WalkPosition( initial );
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );

    int max_observed = CUNYAIModule::current_map_inventory.map_veins_[wk_postion.x][wk_postion.y];
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

                    int temp = CUNYAIModule::current_map_inventory.map_veins_[testing_x][testing_y];

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

Position CUNYAIModule::getUnit_Center(Unit unit){
    return Position(unit->getPosition().x + unit->getType().dimensionLeft(), unit->getPosition().y + unit->getType().dimensionUp());
}

// checks if a location is safe and doesn't block minerals.
bool CUNYAIModule::checkSafeBuildLoc(const Position pos) {
    auto area = BWEM::Map::Instance().GetArea(TilePosition(pos));
    auto area_home = BWEM::Map::Instance().GetArea(TilePosition(CUNYAIModule::current_map_inventory.front_line_base_));
    bool it_is_home_ = false;
    Unit_Inventory e_neighborhood = getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, pos);
    Unit_Inventory friend_loc = getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, pos);
    Stored_Unit* e_closest = getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, UnitTypes::Zerg_Drone, pos, 9999999);

    int radial_distance_to_closest_enemy = 0;
    int radial_distance_to_build_position = 0;
    bool enemy_has_not_penetrated = true;
    bool have_to_save = false;

    e_neighborhood.updateUnitInventorySummary();
    friend_loc.updateUnitInventorySummary();
    
    if (e_neighborhood.building_count_ > 0) return false; // don't build where they have buildings.

    if (!checkSuperiorFAPForecast(friend_loc, e_neighborhood) && e_closest) { // if they could overrun us if they organized and we did not.
        radial_distance_to_closest_enemy = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromHome(e_closest->pos_);
        radial_distance_to_build_position = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromHome(pos);
        enemy_has_not_penetrated = radial_distance_to_closest_enemy > radial_distance_to_build_position;
        if (area && area_home) {
            it_is_home_ = (area == area_home);
        }
        have_to_save = CUNYAIModule::land_inventory.getLocalMinPatches() <= 12 || radial_distance_to_build_position < 500 || CUNYAIModule::current_map_inventory.hatches_ == 1;
    }


    return it_is_home_ || enemy_has_not_penetrated || have_to_save;
}


bool CUNYAIModule::checkSafeMineLoc(const Position pos, const Unit_Inventory &ui, const Map_Inventory &inv) {

    bool desperate_for_minerals = CUNYAIModule::land_inventory.getLocalMinPatches() < 6;
    bool safe_mine = checkOccupiedArea(ui, pos);
    return  safe_mine || desperate_for_minerals;
}

//Checks if enemy air units represent a potential problem. Note: does not check if they HAVE air units. Now defunct, replaced by build FAP evaluations
//bool CUNYAIModule::checkWeakAgainstAir(const Unit_Inventory &ui, const Unit_Inventory &ei) {
//    //bool u_relatively_weak_against_air = ei.stock_fliers_ / (double)(ui.stock_shoots_up_ + 1) vs ei.stock_ground_units_ / (double)(ui.stock_shoots_down_ + 1); // div by zero concern. The larger one is the BIGGER problem.
//    return -ei.stock_fliers_ / (double)pow((ui.stock_shoots_up_ + 1), 2) < -ei.stock_ground_units_ / (double)pow((ui.stock_shoots_down_ + 1), 2); // div by zero concern. Derivative of the above equation, which ratio is shrunk the most?
//}

double CUNYAIModule::bindBetween(double x, double lower_bound, double upper_bound) {
    if (lower_bound >= upper_bound) {
        throw std::invalid_argument("lower bound is greater than or equal to upper bound");
    }
    if (x > upper_bound) {
        return upper_bound;
    }
    else if (x < lower_bound) {
        return lower_bound;
    }
    return x;
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

//Some safety checks if it can't find FAP objects, say at game start.
int CUNYAIModule::getFAPScore(FAP::FastAPproximation<Stored_Unit*> &fap, bool friendly_player) {
    if (friendly_player && fap.getState().first && !fap.getState().first->empty())                       return std::accumulate(fap.getState().first->begin(), fap.getState().first->end(), 0,   [](int currentScore, auto FAPunit) { return static_cast<int>(currentScore + FAPunit.data->stock_value_ * static_cast<double>(FAPunit.health + FAPunit.shields) / static_cast<double>(FAPunit.maxHealth + FAPunit.maxShields)); });
    else if(!friendly_player && fap.getState().second && !fap.getState().second->empty())                return std::accumulate(fap.getState().second->begin(), fap.getState().second->end(), 0, [](int currentScore, auto FAPunit) { return static_cast<int>(currentScore + FAPunit.data->stock_value_ * static_cast<double>(FAPunit.health + FAPunit.shields) / static_cast<double>(FAPunit.maxHealth + FAPunit.maxShields)); });
    else return 0;
}

//bool CUNYAIModule::checkSuperiorFAPForecast(const Unit_Inventory &ui, const Unit_Inventory &ei) {
//    return  //((ui.stock_fighting_total_ - ui.moving_average_fap_stock_) * ei.stock_fighting_total_ < (ei.stock_fighting_total_ - ei.moving_average_fap_stock_) * ui.stock_fighting_total_ && ui.squadAliveinFuture(24)) || // Proportional win. fixed division by crossmultiplying. Added squadalive in future so the bot is more reasonable in combat situations.
//        //(ui.moving_average_fap_stock_ - ui.future_fap_stock_) < (ei.moving_average_fap_stock_ - ei.future_fap_stock_) || //Win by damage.
//        ui.moving_average_fap_stock_ > ei.moving_average_fap_stock_; //Antipcipated victory.
//}

bool CUNYAIModule::checkMiniFAPForecast(Unit_Inventory &ui, Unit_Inventory &ei, const bool equality_is_win) {
    FAP::FastAPproximation<Stored_Unit*> MiniFap; // integrating FAP into combat with a produrbation.
    ui.addToBuildFAP(MiniFap, true, CUNYAIModule::friendly_player_model.researches_);
    ei.addToBuildFAP(MiniFap, false, CUNYAIModule::enemy_player_model.researches_);
    MiniFap.simulate(FAP_SIM_DURATION);
    ui.pullFromFAP(*MiniFap.getState().first);
    ei.pullFromFAP(*MiniFap.getState().second);
    ui.updateUnitInventorySummary();
    ei.updateUnitInventorySummary();
    return checkSuperiorFAPForecast(ui, ei, equality_is_win);
}

bool CUNYAIModule::checkSuperiorFAPForecast(const Unit_Inventory &ui, const Unit_Inventory &ei, const bool equality_is_win) {
    int total_surviving_ui = 0;
    int total_surviving_ui_up = 0;
    int total_surviving_ui_down = 0;
    int total_dying_ui = 0;
    int total_surviving_ei = 0;
    int total_surviving_ei_up = 0;
    int total_surviving_ei_down = 0;
    int total_dying_ei = 0;

    for (auto u : ui.unit_map_) {
        if (!u.first->isBeingConstructed()) { // don't count constructing units.
            bool fighting_may_save = u.second.phase_ != Stored_Unit::Phase::Retreating && u.second.type_ != UnitTypes::Zerg_Scourge && u.second.type_ != UnitTypes::Zerg_Infested_Terran; // Retreating units are sunk costs, they cannot inherently be saved.
            total_dying_ui += (u.second.stock_value_ - (u.second.type_ == UnitTypes::Terran_Bunker * 100)) * Stored_Unit::unitDeadInFuture(u.second, 6) * fighting_may_save * CUNYAIModule::canContributeToFight(u.second.type_, ei); // remember, FAP ignores non-fighting units. Bunkers leave about 100 minerals worth of stuff behind them.
            //total_surviving_ui += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * fighting_may_save;
            total_surviving_ui_up += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_up_ * fighting_may_save;
            total_surviving_ui_down += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_down_ * fighting_may_save;
        }
    }
    for (auto e : ei.unit_map_) {
        //if (!e.first->isBeingConstructed()) { // don't count constructing units.
            total_dying_ei += (e.second.stock_value_ - (e.second.type_ == UnitTypes::Terran_Bunker * 100)) * Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second);
            //total_surviving_ei += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second);
            total_surviving_ei_up += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_up_;
            total_surviving_ei_down += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_down_;
        //}
    }

    // Calculate if the surviving side can destroy the fodder:
    //if (total_surviving_ei_up > 0) total_dying_ui += ui.stock_air_fodder_;
    //if (total_surviving_ei_down > 0) total_dying_ui += ui.stock_ground_fodder_;
    if (total_surviving_ui_up > 0) total_dying_ei += ei.stock_air_fodder_;
    if (total_surviving_ui_down > 0) total_dying_ei += ei.stock_ground_fodder_;

    if (equality_is_win)
        return total_dying_ui <= total_dying_ei;
    else
        return total_dying_ui < total_dying_ei;

    //((ui.stock_fighting_total_ - ui.moving_average_fap_stock_) <= (ei.stock_fighting_total_ - ei.moving_average_fap_stock_)) || // If my losses are smaller than theirs..
    //(ui.moving_average_fap_stock_ - ui.future_fap_stock_) < (ei.moving_average_fap_stock_ - ei.future_fap_stock_) || //Win by damage.
    //(ei.moving_average_fap_stock_ == 0 && ui.moving_average_fap_stock_ > 0) || // or the enemy will get wiped out.
    //(total_surviving_ui > total_surviving_ei) ||
    //(total_dying_ui < total_dying_ei); //|| // If my losses are smaller than theirs..
    //(local && total_dying_ui == 0); // || // there are no losses.
    //ui.moving_average_fap_stock_ > ei.moving_average_fap_stock_; //Antipcipated victory.
}

int CUNYAIModule::getFAPDamageForecast(const Unit_Inventory &ui, const Unit_Inventory &ei, const bool fodder) {
    int total_surviving_ui = 0;
    int total_surviving_ui_up = 0;
    int total_surviving_ui_down = 0;
    int total_dying_ui = 0;
    int total_surviving_ei = 0;
    int total_surviving_ei_up = 0;
    int total_surviving_ei_down = 0;
    int total_dying_ei = 0;

    for (auto u : ui.unit_map_) {
        total_dying_ui += u.second.stock_value_ * Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second); // remember, FAP ignores non-fighting units.
        total_surviving_ui += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second);
        total_surviving_ui_up += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_up_;
        total_surviving_ui_down += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, 6) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_down_;
    }
    for (auto e : ei.unit_map_) {
        total_dying_ei += e.second.stock_value_ * Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei_up += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_up_;
        total_surviving_ei_down += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, 6) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_down_;
    }

    // Calculate if the surviving side can destroy the fodder:
    if (total_surviving_ei_up > 0) total_dying_ui += ui.stock_air_fodder_;
    if (total_surviving_ei_down > 0) total_dying_ui += ui.stock_ground_fodder_;

    //Return the damage prediction. This may not always accurately resemble the victor, however.  
    //FAP should follow lancaster's laws for ranged and melee combat but doesn't accurately reflect the blend of the two, since melee units create space for ranged, surrounds are less helpful if surface area is not a factor in combat, etc.
    //It's also not clear the fodder is going to die.
    return total_dying_ui;
}

int CUNYAIModule::getFAPSurvivalForecast(const Unit_Inventory & ui, const Unit_Inventory & ei, const int duration, const bool fodder)
{
    int total_surviving_ui = 0;
    int total_surviving_ui_up = 0;
    int total_surviving_ui_down = 0;
    int total_dying_ui = 0;
    int total_surviving_ei = 0;
    int total_surviving_ei_up = 0;
    int total_surviving_ei_down = 0;
    int total_dying_ei = 0;

    for (auto u : ui.unit_map_) {
        bool fighting_may_save = u.second.phase_ != Stored_Unit::Phase::Retreating && u.second.type_ != UnitTypes::Zerg_Scourge && u.second.type_ != UnitTypes::Zerg_Infested_Terran; // Retreating units are sunk costs, they cannot inherently be saved.
        total_surviving_ui += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, duration) * CUNYAIModule::isFightingUnit(u.second) * fighting_may_save;
        total_surviving_ui_up += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, duration) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_up_ * fighting_may_save;
        total_surviving_ui_down += u.second.stock_value_ * !Stored_Unit::unitDeadInFuture(u.second, duration) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_down_ * fighting_may_save;
    }
    for (auto e : ei.unit_map_) {
        total_dying_ei += e.second.stock_value_ * Stored_Unit::unitDeadInFuture(e.second, duration) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, duration) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei_up += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, duration) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_up_;
        total_surviving_ei_down += e.second.stock_value_ * !Stored_Unit::unitDeadInFuture(e.second, duration) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_down_;
    }

    // Calculate if the surviving side can destroy the fodder:
    if (total_surviving_ei_up == 0) total_surviving_ui += ui.stock_air_fodder_;
    if (total_surviving_ei_down == 0) total_surviving_ui += ui.stock_ground_fodder_;

    //Return the survival differential. This may not always accurately resemble the victor, however.  
    //FAP should follow lancaster's laws for ranged and melee combat but doesn't accurately reflect the blend of the two, since melee units create space for ranged, surrounds are less helpful if surface area is not a factor in combat, etc.
    //It's also not clear the fodder is going to die.
    return total_surviving_ui;
}

bool CUNYAIModule::checkUnitTouchable(const Unit &u) {
    // Ignore the unit if it no longer exists
    // Make sure to include this block when handling any Unit pointer!
    if (!u || !u->exists())
        return false;
    // Ignore the unit if it has one of the following status ailments
    if (u->isLockedDown() ||
        u->isMaelstrommed() ||
        u->isStasised())
        return false;
    // Ignore the unit if it is in one of the following states
    if (u->isLoaded() ||
        !u->isPowered() /*|| u->isStuck()*/)
        return false;
    // Ignore the unit if it is incomplete or busy constructing
    //if (!u->isCompleted() ||
    //    u->isConstructing())
    //    return false;

    if (!CUNYAIModule::spamGuard(u)) {
        return false;
    }

    return true;
}

bool CUNYAIModule::updateUnitPhase(const Unit &u, const Stored_Unit::Phase phase) {
    auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);
    if (found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end()) {
        Stored_Unit& morphing_unit = found_item->second;
        morphing_unit.phase_ = phase;
        morphing_unit.updateStoredUnit(u);
        return true;
    }
    return false;
}

bool CUNYAIModule::updateUnitBuildIntent(const Unit &u, const UnitType &intended_build_type, const TilePosition &intended_build_tile) {
    auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);
    if (found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end()) {
        Stored_Unit& morphing_unit = found_item->second;
        morphing_unit.stopMine();
        morphing_unit.phase_ = Stored_Unit::Prebuilding;
        morphing_unit.intended_build_type_ = intended_build_type;
        morphing_unit.intended_build_tile_ = intended_build_tile;
        morphing_unit.updateStoredUnit(u);
        return true;
    }
    return false;
}

bool CUNYAIModule::checkDangerousArea(const UnitType ut, const Position pos) {
    Unit_Inventory ei_temp;
    ei_temp = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, pos);
    ei_temp.updateUnitInventorySummary();

    if (CUNYAIModule::isInDanger(ut, ei_temp)) return false;
    return true;
}

bool CUNYAIModule::checkDangerousArea(const UnitType ut, const int AreaID) {
    Unit_Inventory ei_temp;
    ei_temp = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, AreaID);
    ei_temp.updateUnitInventorySummary();

    if (CUNYAIModule::isInDanger(ut, ei_temp)) return false;
    return true;
}

string CUNYAIModule::safeString(string input)
{
    input.erase(std::remove_if(input.begin(), input.end(), [](char c) { return !isalpha(c); }), input.end());

    return input;
}

