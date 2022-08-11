#pragma once
# include "Source\CUNYAIModule.h"
#include <numeric> // std::accumulate
#include <fstream>
#include <algorithm>
#include <functional>
#include "Utilities.h"

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
bool CUNYAIModule::isRecentCombatant(const StoredUnit &su) {
    bool fighting_now = (su.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (su.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Attack_Unit);
    bool recent_order = su.bwapi_unit_->getLastCommandFrame() + 24 > Broodwar->getFrameCount();
    bool retreat_or_fight = (su.phase_ == StoredUnit::Retreating || su.phase_ == StoredUnit::Attacking);
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
bool CUNYAIModule::isFightingUnit(const StoredUnit &unit)
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


void CUNYAIModule::onFrameWritePlayerModel(PlayerModel &player, const string label)
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
            for (auto u : player.researches_.getUpgrades()) {
                up_type = u.first.c_str();
                if (u.second > 0) {
                    smashed_upgrade_types += up_type + ", ";
                }
            }
            //tech types
            for (auto u : player.researches_.getTech()) {
                tech_type = u.first.c_str();
                if (u.second) {
                    smashed_tech_types += tech_type + ", ";
                }
            }
            // Research-sort Buildings, includes inferred ones.
            for (auto u : player.researches_.getTechBuildings()) {
                inferred_building_type = u.first.c_str();
                if (u.second > 0) {
                    smashed_inferred_building_types += inferred_building_type + ", ";
                }
            }

            output.open(learnedPlan.getWriteDir() + Broodwar->mapFileName() + "_v_" + Broodwar->enemy()->getName() + "_status.txt", ios_base::app);

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

            //if (player.getPlayer()) {
            //    output << " Unit Score " << player.getPlayer()->getUnitScore() << endl;
            //    output << " Kill Score " << player.getPlayer()->getKillScore() << endl;
            //    output << " Building Score " << player.getPlayer()->getBuildingScore() << endl;
            //}

            output << " Labor " << player.spending_model_.getStock(BuildParameterNames::EconAlpha) <<  " alpha_L " << player.spending_model_.getParameter(BuildParameterNames::EconAlpha) << " gradient " << player.spending_model_.getDeriviative(BuildParameterNames::EconAlpha) << endl;
            output << " (K)Capital " << player.spending_model_.getStock(BuildParameterNames::ArmyAlpha) << " alpha_K " << player.spending_model_.getParameter(BuildParameterNames::ArmyAlpha) << " gradient " << player.spending_model_.getDeriviative(BuildParameterNames::ArmyAlpha) << endl;
            output << " Technology " << player.spending_model_.getStock(BuildParameterNames::TechAlpha) << " alpha_T " << player.spending_model_.getParameter(BuildParameterNames::TechAlpha) << " gradient " << player.spending_model_.getDeriviative(BuildParameterNames::TechAlpha) << endl;
            output << " ln(Y), ln(Utility) " << player.spending_model_.getlnY() << endl;
            //output << " Testing Net Worth Function " << player.getNetWorth() << endl;

            
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
bool CUNYAIModule::checkCanFight(UnitType u_type, UnitType e_type) {
    bool has_appropriate_weapons = (e_type.isFlyer() && u_type.airWeapon() != WeaponTypes::None) || (!e_type.isFlyer() && u_type.groundWeapon() != WeaponTypes::None);
    bool shoots_without_weapons = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier; // medics do not fight per se.
    bool reaver = u_type == UnitTypes::Protoss_Reaver && !e_type.isFlyer();
    bool could_get_spelled = (u_type == UnitTypes::Protoss_High_Templar || u_type == UnitTypes::Protoss_Dark_Archon || u_type == UnitTypes::Zerg_Defiler) && !e_type.isBuilding();
    bool queen_broodling = u_type == UnitTypes::Zerg_Queen && !e_type.isFlyer() && e_type != UnitTypes::Protoss_Probe && e_type != UnitTypes::Protoss_Reaver && e_type != UnitTypes::Protoss_Archon && e_type != UnitTypes::Protoss_Dark_Archon;
    bool science_irradiate = u_type == UnitTypes::Terran_Science_Vessel && e_type.isOrganic() && !e_type.isBuilding();
    bool e_vunerable = (has_appropriate_weapons || shoots_without_weapons || queen_broodling || reaver || science_irradiate || could_get_spelled); // if we cannot attack them.
    return e_vunerable; // also if they are cloaked and can attack us.
}

// Outlines the case where UNIT can attack Anything; 
bool CUNYAIModule::checkCanFight(UnitType u_type) {
    bool has_appropriate_weapons = (u_type.airWeapon() != WeaponTypes::None) || (u_type.groundWeapon() != WeaponTypes::None);
    bool shoots_without_weapons = u_type == UnitTypes::Terran_Bunker || u_type == UnitTypes::Protoss_Carrier; // medics do not fight per se.
    bool reaver = u_type == UnitTypes::Protoss_Reaver;
    bool could_get_spelled = (u_type == UnitTypes::Protoss_High_Templar || u_type == UnitTypes::Protoss_Dark_Archon || u_type == UnitTypes::Zerg_Defiler);
    bool queen_broodling = u_type == UnitTypes::Zerg_Queen;
    bool science_irradiate = u_type == UnitTypes::Terran_Science_Vessel;
    bool e_vunerable = (has_appropriate_weapons || shoots_without_weapons || queen_broodling || reaver || science_irradiate || could_get_spelled); // if we cannot attack them.
    return e_vunerable; // also if they are cloaked and can attack us.
}


// Outlines the case where UNIT can attack ENEMY;
bool CUNYAIModule::checkCanFight( Unit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit->getType();
    if (!enemy->isDetected()) return false;
    return CUNYAIModule::checkCanFight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::checkCanFight( Unit unit, StoredUnit enemy ) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit->getType();
    if (enemy.bwapi_unit_ && !enemy.bwapi_unit_->isDetected()) return false;
    return CUNYAIModule::checkCanFight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::checkCanFight(StoredUnit unit, StoredUnit enemy) {
    UnitType e_type = enemy.type_;
    UnitType u_type = unit.type_;
    if (enemy.bwapi_unit_ && !enemy.bwapi_unit_->isDetected()) return false;
    return CUNYAIModule::checkCanFight(u_type, e_type);
}

// Outlines the case where UNIT can attack ENEMY; 
bool CUNYAIModule::checkCanFight( StoredUnit unit, Unit enemy ) {
    UnitType e_type = enemy->getType();
    UnitType u_type = unit.type_;
    if (enemy && !enemy->isDetected()) return false;
    return CUNYAIModule::checkCanFight(u_type, e_type);
}

// Returns True if UnitType UT can attack anything in UnitInventory ENEMY; 
bool CUNYAIModule::canContributeToFight(const UnitType &ut, const UnitInventory enemy) {
    bool shooting_up = ut.airWeapon() != WeaponTypes::None && enemy.stock_fliers_ + enemy.stock_air_fodder_ > 0;
    bool shooting_down = (ut.groundWeapon() != WeaponTypes::None || ut == UnitTypes::Protoss_Reaver || ut == UnitTypes::Zerg_Lurker) && enemy.stock_ground_fodder_ + enemy.stock_ground_units_ > 0;
    bool shoots_without_weapons = ut == UnitTypes::Terran_Bunker || ut == UnitTypes::Protoss_Carrier || ut == UnitTypes::Terran_Science_Vessel || ut == UnitTypes::Terran_Medic || ut == UnitTypes::Protoss_High_Templar || ut == UnitTypes::Protoss_Dark_Archon || ut == UnitTypes::Zerg_Defiler || ut == UnitTypes::Zerg_Queen;
    return shooting_up || shooting_down || shoots_without_weapons;
}

// Returns True if UnitType UT is in danger from anything in UnitInventory ENEMY. Excludes Psions; 
bool CUNYAIModule::isInPotentialDanger(const UnitType &ut, const UnitInventory enemy) {
    bool shooting_up = ut.isFlyer() && enemy.stock_shoots_up_ > 0;
    bool shooting_down = !ut.isFlyer() && enemy.stock_shoots_down_ > 0;
    bool shoots_without_weapons = CUNYAIModule::containsUnit(UnitTypes::Terran_Bunker, enemy) + CUNYAIModule::containsUnit(UnitTypes::Protoss_Carrier, enemy) + CUNYAIModule::containsUnit(UnitTypes::Terran_Science_Vessel, enemy) + CUNYAIModule::containsUnit(UnitTypes::Terran_Medic, enemy) + CUNYAIModule::containsUnit(UnitTypes::Protoss_High_Templar, enemy) + CUNYAIModule::containsUnit(UnitTypes::Protoss_Dark_Archon, enemy) + CUNYAIModule::containsUnit(UnitTypes::Zerg_Defiler, enemy) + CUNYAIModule::containsUnit(UnitTypes::Zerg_Queen, enemy);

    return shooting_up || shooting_down || shoots_without_weapons;
}

//bool CUNYAIModule::isInDanger(const Unit &u) {
//    return (u->isFlying() && CUNYAIModule::currentMapInventory.getAirThreatField(u->getTilePosition()) > 0) || (!u->isFlying() && CUNYAIModule::currentMapInventory.getGroundThreatField(u->getTilePosition()) > 0);
//}

// Counts all units of one type in existance and owned by enemies. Counts units under construction.
int CUNYAIModule::countUnits( const UnitType &type, const UnitInventory &ui )
{
    int count = 0;

    for ( auto & e : ui.unit_map_) {
        count += (e.second.type_ == type) + (e.second.type_ != type && e.second.type_ == UnitTypes::Zerg_Egg && e.second.build_type_ == type) * (1 + e.second.build_type_.isTwoUnitsInOneEgg()) ; // better without if-conditions.
    }

    return count;
}

bool CUNYAIModule::containsUnit(const UnitType &type, const UnitInventory &ui) {

    for (auto & e : ui.unit_map_) {
        if(e.second.type_ == type) return true; 
    }
    return false;
}


// Counts all units of one type in existance and owned by enemies. 
int CUNYAIModule::countSuccessorUnits(const UnitType &type, const UnitInventory &ui)
{
    int count = 0;

    for (auto & e : ui.unit_map_) {
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
        count += (unit->getType() == type) + (unit->getType() != type && unit->getType() == UnitTypes::Zerg_Egg && unit->getBuildType() == type) * (1 + unit->getBuildType().isTwoUnitsInOneEgg()); // better without if-conditions.
    }

    return count;
}

// Overload. Counts all units in a set of one type in the reservation system. Does not reserve larva units. 
const int CUNYAIModule::countUnits( const UnitType &type, const Reservation &res )
{
    int count = 0;
    for (auto const it : res.getReservedBuildings() ) {
        if( it.second == type ) count++;
    }

    for (auto const it : res.getReservedUnits()) {
        if (it.second == type) count++;
    }

    return count;
}

// Counts all units of one type in existance and owned by me. Counts units under construction.
int CUNYAIModule::countUnits(const UnitType &type, bool reservations_included)
{
    int count = 0;
    if (reservations_included) {
        count = countUnits(type, CUNYAIModule::my_reservation);
    }

    auto c_iter = find(CUNYAIModule::friendly_player_model.unit_type_.begin(), CUNYAIModule::friendly_player_model.unit_type_.end(), type);
    if (c_iter == CUNYAIModule::friendly_player_model.unit_type_.end()) {
        return count;
    }
    else {
        int distance = std::distance(CUNYAIModule::friendly_player_model.unit_type_.begin(), c_iter);
        return CUNYAIModule::friendly_player_model.unit_count_[distance] + count;
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
int CUNYAIModule::countUnitsDoing(const UnitType &type, const UnitCommandType &u_command_type, const UnitInventory &ui)
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
int CUNYAIModule::countUnitsInProgress(const UnitType &type, const UnitInventory &ui)
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
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == upType.whatUpgrades() && u.second.phase_ == StoredUnit::Phase::None) count++;
    }
    return count;
}

int CUNYAIModule::countUnitsBenifitingFrom(const UpgradeType &upType) {
    int count = 0;
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (std::find(upType.whatUses().begin(), upType.whatUses().end(), u.second.type_) != upType.whatUses().end()) count++;
    }
    return count;
}

int CUNYAIModule::countUnitsAvailableToPerform(const TechType &techType) {
    int count = 0;
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == techType.whatResearches() && u.second.phase_ == StoredUnit::Phase::None) count++;
    }
    return count;
}

int CUNYAIModule::countUnitsAvailable(const UnitType &uType) {
    int count = 0;
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == uType && u.second.phase_ == StoredUnit::Phase::None) count++;
    }
    return count;
}

// evaluates the value of a stock of buildings, in terms of pythagorian distance of min & gas & supply. Assumes building is zerg and therefore, a drone was spent on it.
int CUNYAIModule::Stock_Buildings( const UnitType &building, const UnitInventory &ui ) {
    int cost = StoredUnit(building).stock_value_;
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

int CUNYAIModule::Stock_Units( const UnitType &unit_type, const UnitInventory &ui) {
    int total_stock = 0;

    for ( auto & u : ui.unit_map_ ) {
        if ( u.second.type_ == unit_type ) {  // if you impose valid_pos here many of YOUR OWN UNITS will not be counted.
            total_stock += u.second.current_stock_value_;
        }
    }

    return total_stock;
}

// evaluates the value of a stock of combat units, for all unit types in a unit inventory. Does not count eggs.
//int CUNYAIModule::Stock_Combat_Units( const UnitInventory &ui ) {
//    int total_stock = 0;
//    for ( int i = 0; i != 173; i++ )
//    { // iterating through all enemy units we have available and CUNYAI "knows" about. 
//        if ( ((UnitType)i).airWeapon() != WeaponTypes::None || ((UnitType)i).groundWeapon() != WeaponTypes::None || ((UnitType)i).maxEnergy() > 0 ) {
//            total_stock += Stock_Units( ((UnitType)i), ui );
//        }
//    }
//    return total_stock;
//}

// Overload. evaluates the value of a stock of units, for all unit types in a unit inventory
int CUNYAIModule::Stock_Units_ShootUp( const UnitInventory &ui ) {
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
int CUNYAIModule::Stock_Units_ShootDown( const UnitInventory &ui ) {
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


int CUNYAIModule::getTargetableStocks(const Unit & u, const UnitInventory & enemy_loc)
{
    int targetable_e = 0;
    targetable_e = (u->getType().airWeapon() != WeaponTypes::None) * (enemy_loc.stock_fliers_ + enemy_loc.stock_air_fodder_ ) + (u->getType().groundWeapon() != WeaponTypes::None) * (enemy_loc.stock_ground_units_ + enemy_loc.stock_ground_fodder_);
    return targetable_e;
}

int CUNYAIModule::getThreateningStocks(const Unit & u, const UnitInventory & enemy_loc)
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

//Gets pointer to closest StoredUnit contained in UI to the Origin point. Checks range, check if in includedUnitType and not in ExcludedUnitType
StoredUnit * CUNYAIModule::getClosestStoredLinear(UnitInventory & ui, const Position & origin, const int & dist, const UnitType & includedUnitType, const UnitType & excludedUnitType){
    int min_dist = dist;
    double temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            if ((u->second.type_ == includedUnitType || includedUnitType == UnitTypes::AllUnits) && u->second.type_ != includedUnitType && u->second.valid_pos_) {
                temp_dist = CUNYAIModule::distanceToStoredCenter(origin, u->second);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(u->second);
                }
            }
        }
    }

    return return_unit;
}


//Gets pointer to closest StoredUnit contained in UI to the Origin point. Checks range, check if in includedUnitType and not in ExcludedUnitType
StoredUnit* CUNYAIModule::getClosestStoredByGround(UnitInventory &ui, const Position &origin, const int & dist, const UnitType & includedUnitType, const UnitType & excludedUnitType) {
    int min_dist = dist;
    double temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            if ((u->second.type_ == includedUnitType || includedUnitType == UnitTypes::AllUnits) && u->second.type_ != includedUnitType && u->second.valid_pos_) {
                temp_dist = CUNYAIModule::currentMapInventory.getDistanceBetween(CUNYAIModule::getStoredCenter(u->second), origin);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(u->second);
                }
            }
        }
    }

    return return_unit;
}

Stored_Resource * CUNYAIModule::getClosestStoredLinear(ResourceInventory & ri, const Position & origin, const int & dist, const UnitType & includedUnitType, const UnitType & excludedUnitType){
    int min_dist = dist;
    double temp_dist = 999999;
    Stored_Resource* returnResource = nullptr;

    if (!ri.ResourceInventory_.empty()) {
        for (auto & r = ri.ResourceInventory_.begin(); r != ri.ResourceInventory_.end() && !ri.ResourceInventory_.empty(); r++) {
            bool isMineral = r->second.type_.isMineralField() && includedUnitType.isMineralField();
            bool isGas = r->second.type_ == UnitTypes::Resource_Vespene_Geyser && includedUnitType == UnitTypes::Resource_Vespene_Geyser;
            bool isAll = includedUnitType == UnitTypes::AllUnits;
            if ((isMineral || isGas || isAll) && r->second.type_ != excludedUnitType && r->second.valid_pos_) {
                temp_dist = CUNYAIModule::distanceToStoredCenter(origin, r->second);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    returnResource = &(r->second);
                }
            }
        }
    }

    return returnResource;
}

Stored_Resource * CUNYAIModule::getClosestStoredByGround(ResourceInventory & ri, const Position & origin, const int & dist, const UnitType & includedUnitType, const UnitType & excludedUnitType) {
    int min_dist = dist;
    double temp_dist = 999999;
    Stored_Resource* returnResource = nullptr;

    if (!ri.ResourceInventory_.empty()) {
        for (auto & r = ri.ResourceInventory_.begin(); r != ri.ResourceInventory_.end() && !ri.ResourceInventory_.empty(); r++) {
            bool isMineral = r->second.type_.isMineralField() && includedUnitType.isMineralField();
            bool isGas = r->second.type_ == UnitTypes::Resource_Vespene_Geyser && includedUnitType == UnitTypes::Resource_Vespene_Geyser;
            bool isAll = includedUnitType == UnitTypes::AllUnits;
            if ((isMineral || isGas || isAll) && r->second.type_ != excludedUnitType && r->second.valid_pos_) {
                temp_dist = CUNYAIModule::currentMapInventory.getDistanceBetween(CUNYAIModule::getStoredCenter(r->second), origin);
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    returnResource = &(r->second);
                }
            }
        }
    }

    return returnResource;
}

StoredUnit* CUNYAIModule::getClosestGroundStored(UnitInventory &ui, const Position &origin) {

    int min_dist = 999999;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            temp_dist = CUNYAIModule::currentMapInventory.getDistanceBetween(u->second.pos_, origin); // can't be const because of this line.
            if (temp_dist <= min_dist && !u->second.is_flying_ && u->second.valid_pos_) {
                min_dist = temp_dist;
                return_unit = &(u->second);
            }
        }
    }

    return return_unit;
}

StoredUnit* CUNYAIModule::getClosestAirStored(UnitInventory &ui, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            if (u->second.is_flying_ && u->second.valid_pos_) {
                temp_dist = CUNYAIModule::currentMapInventory.getDistanceBetween(u->second.pos_, origin); // can't be const because of this line.
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(u->second);
                }
            }
        }
    }

    return return_unit;
}

StoredUnit* CUNYAIModule::getClosestAirStoredWithPriority(UnitInventory &ui, const Position &origin) {
    int min_dist = 999999;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
            if (u->second.is_flying_ && u->second.valid_pos_ && hasPriority(u->second.type_)) {
                temp_dist = CUNYAIModule::currentMapInventory.getDistanceBetween(u->second.pos_, origin); // can't be const because of this line.
                if (temp_dist <= min_dist) {
                    min_dist = temp_dist;
                    return_unit = &(u->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest unit to point in UnitInventory. Checks range. Careful about visiblity.
StoredUnit* CUNYAIModule::getClosestStoredBuilding(UnitInventory &ui, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

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

//Gets pointer to closest unit of a type to point in UnitInventory. Checks range. Careful about visiblity. Confirms unit is available
StoredUnit* CUNYAIModule::getClosestStoredAvailable(UnitInventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999) {
    int min_dist = dist;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (e->second.type_ == u_type && e->second.valid_pos_ && e->second.bwapi_unit_ && CUNYAIModule::spamGuard(e->second.bwapi_unit_)) {
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

//Gets pointer to closest attackable unit from unit in UnitInventory. Checks range. Careful about visiblity.  Can return nullptr.
StoredUnit* CUNYAIModule::getClosestAttackableStored(UnitInventory &ui, const Unit unit, const int &dist = 999999) {
    int min_dist = dist;
    bool can_attack;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = CUNYAIModule::checkCanFight(unit, e->second);
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

//Gets pointer to closest attackable unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestThreatOrTargetStored( UnitInventory &ui, const UnitType &u_type, const Position &origin, const int &dist = 999999 ) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;

    if ( !ui.unit_map_.empty() ) {
        for ( auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++ ) {
            can_attack = checkCanFight(u_type, e->second.type_) && e->second.bwapi_unit_;
            can_be_attacked_by = checkCanFight(e->second.type_, u_type);
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

//Gets pointer to closest threat or target unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestThreatOrTargetStored(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = checkCanFight(unit, e->second);
            can_be_attacked_by = checkCanFight(e->second, unit);

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

//Gets pointer to closest melee threat. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestMeleeThreatStored(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_be_attacked_by;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_be_attacked_by = checkCanFight(e->second, unit);

            if (can_be_attacked_by && !CUNYAIModule::isRanged(e->second.type_) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
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

//Gets pointer to closest attackable unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestThreatOrTargetExcluding(UnitInventory &ui, const UnitType ut, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    int temp_dist = 999999;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = checkCanFight(unit, e->second);
            can_be_attacked_by = checkCanFight(e->second, unit);

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

//Gets pointer to closest attackable unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestThreatOrTargetWithPriority(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by;
    double temp_dist = 999999;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = checkCanFight(unit, e->second);
            can_be_attacked_by = checkCanFight(e->second, unit);
            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && hasPriority(e->second)) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

StoredUnit* CUNYAIModule::getClosestThreatWithPriority(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_be_attacked_by;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_be_attacked_by = checkCanFight(e->second, unit);
            if (can_be_attacked_by && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && hasPriority(e->second)) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

StoredUnit* CUNYAIModule::getClosestTargetWithPriority(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = checkCanFight(unit, e->second);
            if (can_attack && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && hasPriority(e->second)) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestGroundWithPriority(UnitInventory &ui, const Position &pos, const int &dist) {
    int min_dist = dist;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = pos;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (!e->second.is_flying_ && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && hasPriority(e->second)) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest attackable unit to point within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestIndicatorOfArmy(UnitInventory &ui, const Position &pos, const int &dist) {
    int min_dist = dist;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = pos;

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if (!e->second.is_flying_ && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_ && hasPriority(e->second) && e->second.type_ != UnitTypes::Terran_Vulture_Spider_Mine && !e->second.type_.isWorker()) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

bool CUNYAIModule::hasPriority(StoredUnit e) {
    return e.type_ != UnitTypes::Zerg_Egg && e.type_ != UnitTypes::Zerg_Larva && e.type_ != UnitTypes::Zerg_Broodling && e.type_ != UnitTypes::Protoss_Interceptor;
}

//Gets pointer to closest threat to unit within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getClosestThreatStored(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_be_attacked_by;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_be_attacked_by = checkCanFight(e->second, unit);

            if (can_be_attacked_by && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                temp_dist = e->second.pos_.getDistance(origin);
                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Gets pointer to closest threat/target unit from home within UnitInventory. Checks range. Careful about visiblity.  Can return nullptr. Ignores Special Buildings and critters. Does not attract to cloaked.
StoredUnit* CUNYAIModule::getMostAdvancedThreatOrTargetStored(UnitInventory &ui, const Unit &unit, const int &dist) {
    int min_dist = dist;
    bool can_attack, can_be_attacked_by, we_are_a_flyer;
    double temp_dist = INT_MAX;
    StoredUnit* return_unit = nullptr;
    Position origin = unit->getPosition();
    we_are_a_flyer = unit->getType().isFlyer();

    if (!ui.unit_map_.empty()) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            can_attack = checkCanFight(unit, e->second);
            can_be_attacked_by = checkCanFight(e->second, unit);
            if ((can_attack || can_be_attacked_by) && !e->second.type_.isSpecialBuilding() && !e->second.type_.isCritter() && e->second.valid_pos_) {
                if (we_are_a_flyer) {
                    temp_dist = unit->getDistance(e->second.pos_);
                }
                else {
                    temp_dist = CUNYAIModule::currentMapInventory.getRadialDistanceOutFromHome(e->second.pos_);
                }

                if (temp_dist <= min_dist) {
                    min_dist = static_cast<int>(temp_dist);
                    return_unit = &(e->second);
                }
            }
        }
    }

    return return_unit;
}

//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
UnitInventory CUNYAIModule::getThreateningUnitInventoryInRadius( const UnitInventory &ui, const Position &origin, const int &dist, const bool &air_attack ) {
    UnitInventory ui_out;
    if (air_attack) {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if ((*e).second.pos_.getDistance(origin) <= dist && e->second.valid_pos_ && checkCanFight(e->second.type_, UnitTypes::Zerg_Overlord)) {
                ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
            }
        }
    }
    else {
        for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
            if ((*e).second.pos_.getDistance(origin) <= dist && e->second.valid_pos_ && checkCanFight(e->second.type_, UnitTypes::Zerg_Drone)) {
                ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
            }
        }
    }
    return ui_out;
}


//Searches an enemy inventory for units within a range. Returns enemy inventory meeting that critera. Can return nullptr.
UnitInventory CUNYAIModule::getUnitInventoryInRadius(const UnitInventory & ui, const Position & origin, const int & dist, const UnitType & includedUnitType, const UnitType & excludedUnitType) {
    UnitInventory ui_out;
    for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
        if (CUNYAIModule::distanceToStoredCenter(origin, e->second) <= dist && e->second.valid_pos_ && (e->second.type_ == includedUnitType || includedUnitType == UnitTypes::AllUnits) && e->second.type_ != excludedUnitType) {
            ui_out.addStoredUnit(e->second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an resource inventory for units within an area. Returns resource inventory meeting that critera. Can return nullptr.
ResourceInventory CUNYAIModule::getResourceInventoryInRadius(const ResourceInventory &ri, const Position &origin, const int &dist, const UnitType & includedUnitType, const UnitType & excludedUnitType) {
    ResourceInventory ri_out;
	for (auto & r = ri.ResourceInventory_.begin(); r != ri.ResourceInventory_.end() && !ri.ResourceInventory_.empty(); r++) {
        bool isMineral = r->second.type_.isMineralField() && includedUnitType.isMineralField();
        bool isGas = r->second.type_ == UnitTypes::Resource_Vespene_Geyser && includedUnitType == UnitTypes::Resource_Vespene_Geyser;
        bool isAll = includedUnitType == UnitTypes::AllUnits;
		if ((isMineral || isGas || isAll) && r->second.type_ != excludedUnitType && CUNYAIModule::distanceToStoredCenter(origin, r->second) <= dist) {
			ri_out.addStored_Resource(r->second); // if we take any distance and they are in inventory.
		}
	}
    ri_out.updateMines();
	return ri_out;
}

//Searches an resource inventory for units within an area. Returns resource inventory meeting that critera. Can return nullptr.
ResourceInventory CUNYAIModule::getResourceInventoryInArea(const ResourceInventory &ri, const Position &origin) {
	ResourceInventory ri_out;
	auto area = BWEM::Map::Instance().GetArea(TilePosition(origin));
	if (area) {
		int area_id = area->Id();
		for (auto & r = ri.ResourceInventory_.begin(); r != ri.ResourceInventory_.end() && !ri.ResourceInventory_.empty(); r++) {
			if (r->second.areaID_ == area_id) {
				ri_out.addStored_Resource((*r).second); // if we take any distance and they are in inventory.
			}
		}
	}
    ri_out.updateMines();
	return ri_out;
}

//Searches a research inventory for those resources that are both <350 away from a presumed base location and in the same area.
ResourceInventory CUNYAIModule::getResourceInventoryAtBase(const ResourceInventory &ri, const Position &origin) {
    ResourceInventory ri_area = getResourceInventoryInArea(ri, origin);
    ResourceInventory ri_distance = getResourceInventoryInRadius(ri, origin, 350);
    ResourceInventory ri_out;

    for (auto r_outer : ri_area.ResourceInventory_) {
        for (auto r_inner : ri_distance.ResourceInventory_) {
            if (r_outer.second.pos_ == r_inner.second.pos_) {
                ri_out.addStored_Resource(r_outer.second);
            }
        }
    }
    ri_out.updateMines();
    return ri_out;
}

//Searches an enemy inventory for units within a range. Returns units that are not in weapon range but are in inventory. Can return nullptr.
UnitInventory CUNYAIModule::getUnitsOutOfReach(const UnitInventory &ui, const Unit &target) {
    UnitInventory ui_out;
    for (auto & u = ui.unit_map_.begin(); u != ui.unit_map_.end() && !ui.unit_map_.empty(); u++) {
        if (u->second.valid_pos_ && ( !(*u).second.bwapi_unit_->canMove() && !(*u).second.bwapi_unit_->isInWeaponRange(target) ) ) {
            ui_out.addStoredUnit((*u).second); // if we take any distance and they are in inventory.
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Can return nullptr.
UnitInventory CUNYAIModule::getUnitInventoryInArea(const UnitInventory &ui, const Position &origin) {
    UnitInventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                if (e->second.areaID_ == area_id) {
                    ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
                }
            }
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Can return nullptr.
UnitInventory CUNYAIModule::getUnitInventoryInArea(const UnitInventory &ui, const int AreaID) {
    UnitInventory ui_out;

    for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
        if (e->second.areaID_ == AreaID) {
            ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
        }
    }

    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Neighborhood. Can return nullptr.
UnitInventory CUNYAIModule::getUnitInventoryInNeighborhood(const UnitInventory &ui, const Position &origin) {
    UnitInventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                for (auto & a : area->AccessibleNeighbours()) {
                    if (e->second.areaID_ == area_id || e->second.areaID_ == a->Id()) {
                        ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
                    }
                }
            }
        }
    }
    return ui_out;
}

//Searches an enemy inventory for units within a BWAPI Area. Returns enemy inventory meeting that critera. Can return nullptr.
UnitInventory CUNYAIModule::getUnitInventoryInArea(const UnitInventory &ui, const UnitType ut, const Position &origin) {
    UnitInventory ui_out;
    TilePosition tp = TilePosition(origin);
    if (tp.isValid()) {
        auto area = BWEM::Map::Instance().GetArea(tp);
        if (area) {
            int area_id = area->Id();
            for (auto & e = ui.unit_map_.begin(); e != ui.unit_map_.end() && !ui.unit_map_.empty(); e++) {
                if (e->second.areaID_ == area_id && e->second.type_ == ut) {
                    ui_out.addStoredUnit((*e).second); // if we take any distance and they are in inventory.
                }
            }
        }
    }
    return ui_out;
}

//Searches an inventory for units of within a range. Returns TRUE if the area is occupied. 
bool CUNYAIModule::checkOccupiedArea( const UnitInventory &ui, const Position &origin ) {
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
bool CUNYAIModule::checkOccupiedNeighborhood(const UnitInventory &ui, const Position &origin) {
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
bool CUNYAIModule::checkOccupiedArea(const UnitInventory &ui, const UnitType type, const Position &origin) {
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

    ready_to_move = unitSpamCheckDuration(unit) == 0;

    return ready_to_move; // we must wait at least 5 frames before issuing them a new command regardless.

}

int CUNYAIModule::unitSpamCheckDuration(const Unit &unit) {
    int cd_frames = 0;

    UnitCommandType u_command = unit->getLastCommand().getType();
    if (u_command == UnitCommandTypes::Attack_Unit || u_command == UnitCommandTypes::Attack_Move) {
        UnitType u_type = unit->getType();
        //cd_frames = Broodwar->getLatencyFrames();
        if (u_type == UnitTypes::Zerg_Drone) {
            cd_frames = 2;
        }
        else if (u_type == UnitTypes::Zerg_Zergling) {
            cd_frames = 5;
        }
        else if (u_type == UnitTypes::Zerg_Hydralisk) {
            cd_frames = 7;
        }
        else if (u_type == UnitTypes::Zerg_Lurker) {
            cd_frames = 2; // Lurkers take 125 frames to prepare to strike after burrowing.
        }
        else if (u_type == UnitTypes::Zerg_Mutalisk || u_type == UnitTypes::Zerg_Guardian) {
            cd_frames = 1;
        }
        else if (u_type == UnitTypes::Zerg_Ultralisk) {
            cd_frames = 15;
        }
        //wait_for_cooldown = unit->getGroundWeaponCooldown() > 0 || unit->getAirWeaponCooldown() > 0;
        if (u_type == UnitTypes::Zerg_Devourer) {
            cd_frames = 12; // this is an INSANE cooldown.
        }
    }

    if (u_command == UnitCommandTypes::Burrow) {
        cd_frames = 20; // LurkerLocal103 is not relevant.
    }
    if (u_command == UnitCommandTypes::Unburrow) {
        cd_frames = 9;
    }

    if (u_command == UnitCommandTypes::Morph || u_command == UnitCommandTypes::Build) {
        cd_frames = 24;
    }

    if (u_command == UnitCommandTypes::Move) {
        cd_frames = 9;
    }

    if (u_command == UnitCommandTypes::Gather) {
        cd_frames = 24;
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
    int duration_since_last_command = Broodwar->getFrameCount() - unit->getLastCommandFrame();
    int required_wait_time = cd_frames + Broodwar->getLatencyFrames();


    return max(required_wait_time - duration_since_last_command, 0);
}

Position CUNYAIModule::getUnitCenter(Unit unit)
{
    return Position(unit->getPosition().x + unit->getType().dimensionLeft(), unit->getPosition().y + unit->getType().dimensionUp());
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

    return getExactRange(u_type, owner);
}

int CUNYAIModule::getExactRange(const UnitType u_type, const Player owner) {

    int base_range = max(u_type.groundWeapon().maxRange(), u_type.airWeapon().maxRange());

    if (u_type == UnitTypes::Zerg_Hydralisk && owner->getUpgradeLevel(UpgradeTypes::Grooved_Spines) > 0) {
        base_range += convertTileDistanceToPixelDistance(1);
    }
    else if (u_type == UnitTypes::Protoss_Dragoon && owner->getUpgradeLevel(UpgradeTypes::Singularity_Charge) > 0) {
        base_range += convertTileDistanceToPixelDistance(2);
    }
    else if (u_type == UnitTypes::Protoss_Reaver) {
        base_range += convertTileDistanceToPixelDistance(8);
    }
    else if (u_type == UnitTypes::Protoss_Carrier) {
        base_range += convertTileDistanceToPixelDistance(12); // deploy range is 8, but attack range is 12.
    }
    else if (u_type == UnitTypes::Terran_Marine && owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) {
        base_range += convertTileDistanceToPixelDistance(1);
    }
    else if (u_type == UnitTypes::Terran_Goliath && owner->getUpgradeLevel(UpgradeTypes::Charon_Boosters) > 0) {
        base_range += convertTileDistanceToPixelDistance(3);
    }
    else if ( u_type == UnitTypes::Terran_Bunker ) {
        base_range = UnitTypes::Terran_Marine.groundWeapon().maxRange() + convertTileDistanceToPixelDistance(1) + (owner->getUpgradeLevel(UpgradeTypes::U_238_Shells) > 0) *  convertTileDistanceToPixelDistance(1);
    }

    return base_range;
}

bool CUNYAIModule::isRanged(const UnitType u_type) {
    return CUNYAIModule::getExactRange(u_type) > 64;
}


int CUNYAIModule::getFunctionalRange(const Unit u) {
    return max(getExactRange(u), convertTileDistanceToPixelDistance(1) );
}

//How far can the unit move in one MAFAP sim (120 frames)? Currently too large.
int CUNYAIModule::getChargableDistance(const Unit & u)
{
    int size_array[] = { u->getType().dimensionDown(), u->getType().dimensionUp(), u->getType().dimensionLeft(), u->getType().dimensionRight() };
    return (u->getType() != UnitTypes::Zerg_Lurker) * static_cast<int>(CUNYAIModule::getProperSpeed(u) * FAP_SIM_DURATION) + CUNYAIModule::getFunctionalRange(u) + *std::max_element( size_array, size_array + 4 ); //lurkers have a proper speed of 0. 96 frames is length of MAfap sim.

}


////finds nearest choke or best location within 100 minitiles.
//Position CUNYAIModule::getNearestChoke( const Position &initial, const Position &final, const MapInventory &inv ) {
//    WalkPosition e_position = WalkPosition( final );
//    WalkPosition wk_postion = WalkPosition( initial );
//    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );
//
//    int max_observed = CUNYAIModule::currentMapInventory.map_veins_[wk_postion.x][wk_postion.y];
//    Position nearest_choke; 
//
//    for ( auto i = 0; i < 100; ++i ) {
//        for ( int x = -1; x <= 1; ++x ) {
//            for ( int y = -1; y <= 1; ++y ) {
//
//                int testing_x = wk_postion.x + x;
//                int testing_y = wk_postion.y + y;
//
//                if ( !(x == 0, y == 0) &&
//                    testing_x < map_dim.x &&
//                    testing_y < map_dim.y &&
//                    testing_x > 0 &&
//                    testing_y > 0 ) { // check for being within reference space.
//
//                    int temp = CUNYAIModule::currentMapInventory.map_veins_[testing_x][testing_y];
//
//                    if ( temp >= max_observed ) {
//                        max_observed = temp;
//                        nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
//                        wk_postion = WalkPosition( nearest_choke ); //search from there.
//                        if ( max_observed > 175 ) {
//                            return nearest_choke;
//                            break;
//                        }
//                    }
//                }
//            }
//        }
//    }
//
//    //another attempt
//    //int x_dist_to_e = e_position.x - wk_postion.x;
//    //int y_dist_to_e = e_position.y - wk_postion.y;
//
//    //int dx = x_dist_to_e > 0 ? 1 : -1;
//    //int dy = y_dist_to_e > 0 ? 1 : -1;
//
//    //for ( auto i = 0; i < 50; ++i ) {
//    //    for ( int x = 0; x <= 1; ++x ) {
//    //        for ( int y = 0; y <= 1; ++y ) {
//
//    //            int testing_x = wk_postion.x + x * dx;
//    //            int testing_y = wk_postion.y + y * dy;
//
//    //            if ( !(x == 0, y == 0) &&
//    //                testing_x < map_dim.x &&
//    //                testing_y < map_dim.y &&
//    //                testing_x > 0 &&
//    //                testing_y > 0 ) { // check for being within reference space.
//
//    //                int temp = inv.map_veins_[testing_x][testing_y];
//
//    //                if ( temp > max_observed ) {
//    //                    max_observed = temp;
//    //                    nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
//    //                    wk_postion = WalkPosition( nearest_choke ); //search from there.
//    //                    if ( max_observed > 275 ) {
//    //                        return nearest_choke;
//    //                        break;
//    //                    }
//    //                }
//    //                else if ( y_dist_to_e == 0 || abs( x_dist_to_e / y_dist_to_e ) > 1 ) {
//    //                    dx = x_dist_to_e > 0 ? 1 : -1;
//    //                }
//    //                else {
//    //                    dy = y_dist_to_e > 0 ? 1 : -1;
//    //                }
//    //            }
//    //        }
//    //    }
//    //}
//     //another attempt
//    //int x_dist_to_e, y_dist_to_e, dx, dy, x_inc, y_inc;
//
//    //dx = x_dist_to_e > 0 ? 1 : -1;
//    //dy = y_dist_to_e > 0 ? 1 : -1;
//
//    //for ( auto i = 0; i < 50; ++i ) {
//
//    //    x_dist_to_e = e_position.x - wk_postion.x;
//    //    y_dist_to_e = e_position.y - wk_postion.y;
//
//    //    int testing_x = wk_postion.x + dx;
//    //    int testing_y = wk_postion.y + dy;
//
//    //    if ( testing_x < map_dim.x &&
//
//    //        testing_y < map_dim.y &&
//    //        testing_x > 0 &&
//    //        testing_y > 0 ) { // check for being within reference space.
//
//    //        int temp = inv.map_veins_[testing_x][testing_y];
//
//    //        if ( temp > max_observed ) {
//    //            max_observed = temp;
//    //            nearest_choke = Position( testing_x, testing_y ); //this is our best guess of a choke.
//    //            wk_postion = WalkPosition( nearest_choke ); //search from there.
//    //            if ( max_observed > 275 ) {
//    //                return nearest_choke;
//    //                break;
//    //            }
//    //        }
//    //        else if ( abs(y_dist_to_e / x_dist_to_e) < 1 ) {
//    //            dx += x_dist_to_e > 0 ? 1 : -1;
//    //        }
//    //        else {
//    //            dy += y_dist_to_e > 0 ? 1 : -1;
//    //        }
//    //    }
//    //}
//
//    return nearest_choke;
//}

// checks if a location is safe and doesn't block minerals.
bool CUNYAIModule::checkSafeBuildLoc(const Position pos) {
    auto area = BWEM::Map::Instance().GetArea(TilePosition(pos));
    auto area_home = BWEM::Map::Instance().GetArea(TilePosition(CUNYAIModule::currentMapInventory.getFrontLineBase()));
    bool it_is_home_ = false;
    UnitInventory e_neighborhood = getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, pos);
    UnitInventory friend_loc = getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, pos);
    StoredUnit* e_closest = getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, UnitTypes::Zerg_Drone, pos, 9999999);

    int radial_distance_to_closest_enemy = 0;
    int radial_distance_to_build_position = 0;
    bool enemy_has_not_penetrated = true;
    bool have_to_save = false;

    e_neighborhood.updateUnitInventorySummary();
    friend_loc.updateUnitInventorySummary();
    
    if (e_neighborhood.building_count_ > 0) return false; // don't build where they have buildings.

    if (!checkSuperiorFAPForecast(friend_loc, e_neighborhood) && e_closest) { // if they could overrun us if they organized and we did not.
        radial_distance_to_closest_enemy = CUNYAIModule::currentMapInventory.getRadialDistanceOutFromHome(e_closest->pos_);
        radial_distance_to_build_position = CUNYAIModule::currentMapInventory.getRadialDistanceOutFromHome(pos);
        enemy_has_not_penetrated = radial_distance_to_closest_enemy > radial_distance_to_build_position;
        if (area && area_home) {
            it_is_home_ = (area == area_home);
        }
        have_to_save = CUNYAIModule::land_inventory.countLocalMinPatches() <= 12 || radial_distance_to_build_position < 500 || CUNYAIModule::basemanager.getBaseCount() == 1;
    }


    return it_is_home_ || enemy_has_not_penetrated || have_to_save;
}


bool CUNYAIModule::checkSafeMineLoc(const Position pos, const UnitInventory &ui, const MapInventory &inv) {

    bool desperate_for_minerals = CUNYAIModule::land_inventory.countLocalMinPatches() < 6;
    bool safe_mine = checkOccupiedArea(ui, pos);
    return  safe_mine || desperate_for_minerals;
}

//Checks if enemy air units represent a potential problem. Note: does not check if they HAVE air units. Now defunct, replaced by build FAP evaluations
//bool CUNYAIModule::checkWeakAgainstAir(const UnitInventory &ui, const UnitInventory &ei) {
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


//bool CUNYAIModule::checkSuperiorFAPForecast(const UnitInventory &ui, const UnitInventory &ei) {
//    return  //((ui.stock_fighting_total_ - ui.moving_average_fap_stock_) * ei.stock_fighting_total_ < (ei.stock_fighting_total_ - ei.moving_average_fap_stock_) * ui.stock_fighting_total_ && ui.squadAliveinFuture(24)) || // Proportional win. fixed division by crossmultiplying. Added squadalive in future so the bot is more reasonable in combat situations.
//        //(ui.moving_average_fap_stock_ - ui.future_fap_stock_) < (ei.moving_average_fap_stock_ - ei.future_fap_stock_) || //Win by damage.
//        ui.moving_average_fap_stock_ > ei.moving_average_fap_stock_; //Antipcipated victory.
//}

bool CUNYAIModule::checkSuperiorFAPForecast(const UnitInventory &ui, const UnitInventory &ei, const bool equality_is_win) {
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
            //bool escaping = (u.second.phase_ == StoredUnit::Phase::Retreating && getProperSpeed(u.second.type_) > ei.max_speed_ && pow(u.second.velocity_x_,2) + pow(u.second.velocity_y_,2) > pow(ei.max_speed_,2) );
            //total_dying_ui += (u.second.stock_value_ - (u.second.type_ == UnitTypes::Terran_Bunker * 2 * StoredUnit(UnitTypes::Terran_Marine).stock_value_)) * u.second.unitDeadInFuture() * may_survive_and_fight * CUNYAIModule::canContributeToFight(u.second.type_, ei); // remember, FAP ignores non-fighting units. Bunkers leave about 100 minerals worth of stuff behind them.
            total_dying_ui += u.second.future_fap_value_ * !u.second.isSuicideUnit() * CUNYAIModule::canContributeToFight(u.second.type_, ei); // remember, FAP ignores non-fighting units. Bunkers leave about 100 minerals worth of stuff behind them.
            //total_dying_ui -= (u.second.type_ == UnitTypes::Terran_Bunker * 2 * StoredUnit(UnitTypes::Terran_Marine).stock_value_);
            total_surviving_ui += u.second.future_fap_value_ * CUNYAIModule::isFightingUnit(u.second);
            total_surviving_ui_up += u.second.future_fap_value_ * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_up_ * !u.second.isSuicideUnit();
            total_surviving_ui_down += u.second.future_fap_value_ * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_down_ * !u.second.isSuicideUnit();
        }
    }

    for (auto e : ei.unit_map_) {
        if (!e.first->isBeingConstructed()) { // don't count constructing units.
            //bool escaping = (e.second.order_ == Orders::Move && getProperSpeed(e.second.type_) > ui.max_speed_);
            total_dying_ei += e.second.future_fap_value_ * !e.second.isSuicideUnit() * CUNYAIModule::canContributeToFight(e.second.type_, ui); // remember, FAP ignores non-fighting units. Bunkers leave about 100 minerals worth of stuff behind them.
            //total_dying_ei -= (e.second.type_ == UnitTypes::Terran_Bunker * 2 * StoredUnit(UnitTypes::Terran_Marine).stock_value_);
            total_surviving_ei += e.second.future_fap_value_ * CUNYAIModule::isFightingUnit(e.second);
            total_surviving_ei_up += e.second.future_fap_value_ * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_up_ * !e.second.isSuicideUnit();
            total_surviving_ei_down += e.second.future_fap_value_ * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_down_ * !e.second.isSuicideUnit();
        }
    }

    // Calculate if the surviving side can destroy the fodder:
    //if (total_surviving_ei_up > 0) total_dying_ui += ui.stock_air_fodder_;
    //if (total_surviving_ei_down > 0) total_dying_ui += ui.stock_ground_fodder_;
    if (total_surviving_ui_up > 0) total_dying_ei += ei.stock_air_fodder_;
    if (total_surviving_ui_down > 0) total_dying_ei += ei.stock_ground_fodder_;

    //if (equality_is_win)
    //    return total_dying_ui <= total_dying_ei;
    //else
    //    return total_dying_ui < total_dying_ei;

    return  total_surviving_ei < total_surviving_ui /*|| total_dying_ei > total_dying_ui*/;

    //((ui.stock_fighting_total_ - ui.moving_average_fap_stock_) <= (ei.stock_fighting_total_ - ei.moving_average_fap_stock_)) || // If my losses are smaller than theirs..
    //(ui.moving_average_fap_stock_ - ui.future_fap_stock_) < (ei.moving_average_fap_stock_ - ei.future_fap_stock_) || //Win by damage.
    //(ei.moving_average_fap_stock_ == 0 && ui.moving_average_fap_stock_ > 0) || // or the enemy will get wiped out.
    //(total_surviving_ui > total_surviving_ei) ||
    //(total_dying_ui < total_dying_ei); //|| // If my losses are smaller than theirs..
    //(local && total_dying_ui == 0); // || // there are no losses.
    //ui.moving_average_fap_stock_ > ei.moving_average_fap_stock_; //Antipcipated victory.
}


int CUNYAIModule::getFAPSurvivalForecast(const UnitInventory & ui, const UnitInventory & ei, const int duration, const bool fodder)
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
        bool fighting_may_save = u.second.phase_ != StoredUnit::Phase::Retreating && u.second.type_ != UnitTypes::Zerg_Scourge && u.second.type_ != UnitTypes::Zerg_Infested_Terran; // Retreating units are sunk costs, they cannot inherently be saved.
        total_surviving_ui += u.second.stock_value_ * !u.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(u.second) * fighting_may_save;
        total_surviving_ui_up += u.second.stock_value_ * !u.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_up_ * fighting_may_save;
        total_surviving_ui_down += u.second.stock_value_ * !u.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(u.second) * u.second.shoots_down_ * fighting_may_save;
    }
    for (auto e : ei.unit_map_) {
        total_dying_ei += e.second.stock_value_ * e.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei += e.second.stock_value_ * !e.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(e.second);
        total_surviving_ei_up += e.second.stock_value_ * !e.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_up_;
        total_surviving_ei_down += e.second.stock_value_ * !e.second.unitDeadInFuture(duration) * CUNYAIModule::isFightingUnit(e.second) * e.second.shoots_down_;
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

bool CUNYAIModule::updateUnitPhase(const Unit &u, const StoredUnit::Phase phase) {

    auto found_item = CUNYAIModule::friendly_player_model.units_.getStoredUnit(u);
    if (found_item) {
        found_item->phase_ = phase;
        found_item->updateStoredUnit(u);
        return true;
    }
    return false;
}

bool CUNYAIModule::updateUnitBuildIntent(const Unit &u, const UnitType &intended_build_type, const TilePosition &intended_build_tile) {
    auto found_item = CUNYAIModule::friendly_player_model.units_.getStoredUnit(u);
    if (found_item) {
        found_item->stopMine();
        found_item->phase_ = StoredUnit::Prebuilding;
        found_item->intended_build_type_ = intended_build_type;
        found_item->intended_build_tile_ = intended_build_tile;
        found_item->updateStoredUnit(u);
        return true;
    }
    return false;
}

bool CUNYAIModule::checkDangerousArea(const UnitType ut, const Position pos) {
    UnitInventory ei_temp;
    ei_temp = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, pos);
    ei_temp.updateUnitInventorySummary();

    if (CUNYAIModule::isInPotentialDanger(ut, ei_temp)) return false;
    return true;
}

bool CUNYAIModule::checkDangerousArea(const UnitType ut, const int AreaID) {
    UnitInventory ei_temp;
    ei_temp = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, AreaID);
    ei_temp.updateUnitInventorySummary();

    if (CUNYAIModule::isInPotentialDanger(ut, ei_temp)) return false;
    return true;
}

string CUNYAIModule::safeString(string input)
{
    input.erase(std::remove_if(input.begin(), input.end(), [](char c) { return !isalpha(c); }), input.end());

    return input;
}

int CUNYAIModule::convertTileDistanceToPixelDistance(int numberOfTiles) {
    return numberOfTiles * 32; // Tiles are 32x32 pixels, so the route across them diagonally is STILL 32 pixels because grids.;
}

int CUNYAIModule::convertPixelDistanceToTileDistance(int numberOfPixels) {
    return numberOfPixels / 32; // Tiles are 32x32 pixels, so the route across them diagonally is STILL 32 pixels because grids.;
}


