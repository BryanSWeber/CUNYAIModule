#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\UnitInventory.h"
#include "Source\MobilityManager.h"
#include "Source\CombatManager.h"
#include "Source\CombatSimulator.h"
#include "Source\UnitInventory.h"
#include "Source\Diagnostics.h"
#include "Source/FAP/FAP/include/FAP.hpp"
#include <bwem.h>


UnitInventory CombatManager::scout_squad_;
UnitInventory CombatManager::liabilities_squad_;

bool CombatManager::grandStrategyScript(const Unit & u) {

    bool task_assigned = false;

    auto found_item = CUNYAIModule::friendly_player_model.units_.getStoredUnit(u);
    bool found_and_detecting = found_item->phase_ == StoredUnit::Phase::Detecting;
    bool found_and_doing_nothing = found_item->phase_ == StoredUnit::Phase::None;
    bool found_and_morphing = found_item->phase_ == StoredUnit::Phase::Morphing;
    bool found_and_going_home = found_item->phase_ == StoredUnit::Phase::PathingHome;
    bool found_and_going_out = found_item->phase_ == StoredUnit::Phase::PathingOut;

    if (isScout(u)) {
        if (u->isBlind() || found_and_detecting) removeScout(u);
    }

    if ( (found_and_morphing || found_and_going_home || found_and_detecting) && u->isIdle()) return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);

    if (CUNYAIModule::spamGuard(u)) {
        task_assigned = !task_assigned && u->getType().canMove() && (u->isUnderStorm() || u->isIrradiated() || u->isUnderDisruptionWeb()) && Mobility(u).Scatter_Logic();
        task_assigned = !task_assigned && u->getType().canMove() && u->getType() == UnitTypes::Zerg_Overlord && !u->isBlind() && scoutScript(u);
        task_assigned = !task_assigned && (u->canAttack() || u->getType() == UnitTypes::Zerg_Lurker) && combatScript(u);
        task_assigned = !task_assigned && !u->getType().isWorker() && u->getType() != UnitTypes::Zerg_Overlord && (u->canMove() || (u->getType() == UnitTypes::Zerg_Lurker && u->isBurrowed())) && pathingScript(u);
        task_assigned = !task_assigned && u->getType() == UnitTypes::Zerg_Overlord && (found_and_doing_nothing || found_and_morphing || found_and_going_home) && liabilitiesScript(u);
    }
    

    if (task_assigned && u->getType() == Broodwar->self()->getRace().getWorker()) {
        stopMine(u);
    }

    return false;
}


bool CombatManager::combatScript(const Unit & u)
{

    if (CUNYAIModule::spamGuard(u))
    {
        Mobility mobility = Mobility(u); // We intend to move the unit, we do it through this class.

        int search_radius = getSearchRadius(u);

        //if(!u->getType().isWorker())
        //    Diagnostics::drawCircle(u->getPosition(), Broodwar->getScreenPosition(), search_radius, Colors::Green);

        UnitInventory enemy_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), search_radius);
        UnitInventory friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), search_radius);
        friend_loc.updateUnitInventorySummary();
        enemy_loc.updateUnitInventorySummary();

        StoredUnit* e_closest_demanding_response = CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, u, search_radius); // maximum sight distance of 352, siege tanks in siege mode are about 382
        StoredUnit* e_closest_threat = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, u, search_radius); // maximum sight distance of 352, siege tanks in siege mode are about 382
        StoredUnit* my_unit = CUNYAIModule::friendly_player_model.units_.getStoredUnit(u);

        // Building units (workers) should not be pulled into combat manouvers.
        if (my_unit->phase_ == StoredUnit::Phase::Building || my_unit->phase_ == StoredUnit::Phase::Prebuilding) 
            return false;

        // if you are in the extra wide buffer and collecting forces, surround/spread. Should lead to smoother entry/exit.
        if (isCollectingForces(friend_loc) && !my_unit->type_.isWorker() && CUNYAIModule::isInPotentialDanger(u->getType(), enemy_loc) && CUNYAIModule::currentMapInventory.isInExtraWideBufferField(u->getTilePosition()) > 0 /*&& CUNYAIModule::currentMapInventory.getBufferField(u->getTilePosition()) == 0*/ && (my_unit->phase_ == StoredUnit::Phase::PathingOut || my_unit->phase_ == StoredUnit::Phase::Surrounding || my_unit->phase_ == StoredUnit::Phase::Retreating))
            return mobility.surroundLogic();

        //If there are potential targets, must fight.
        if (e_closest_demanding_response) {

            //Overwrite local inventories, once we decide we need to fight, we need to make our decision based on our target and what is immediately available around the target. This way, all of our units have the SAME potential enemies and same potential friends.
            UnitInventory enemy_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, e_closest_demanding_response->pos_, search_radius);
            UnitInventory friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest_demanding_response->pos_, search_radius);

            friend_loc.updateUnitInventorySummary();
            enemy_loc.updateUnitInventorySummary();

            //Diagnostics::drawCircle(e_closest_demanding_response->pos_, Broodwar->getScreenPosition(), CUNYAIModule::enemy_player_model.units_.max_range_, Colors::Red);
            //Diagnostics::drawCircle(e_closest_demanding_response->pos_, Broodwar->getScreenPosition(), search_radius, Colors::Green);

            //If we can fight, our unit type will determine our behavior.
            if (CUNYAIModule::canContributeToFight(u->getType(), enemy_loc)) {

                ResourceInventory resource_loc = CUNYAIModule::getResourceInventoryInRadius(CUNYAIModule::land_inventory, e_closest_demanding_response->pos_, max(enemy_loc.max_range_ground_, 256));
                UnitInventory trigger_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest_demanding_response->pos_, max(enemy_loc.max_range_ground_, 200));
                trigger_loc.updateUnitInventorySummary();
                //resource_loc.updateResourceInventory();

                //bool unit_death_in_moments = StoredUnit::unitDeadInFuture(CUNYAIModule::friendly_player_model.units_.unit_map_.at(u), 6);
                bool fight_looks_good = CUNYAIModule::checkSuperiorFAPForecast(friend_loc, enemy_loc);
                //bool unit_will_survive = !StoredUnit::unitDeadInFuture(*CUNYAIModule::friendly_player_model.units_.getStoredUnit(u), 6); // Worker is expected to live.
                //bool worker_time_and_place = false;
                UnitInventory expanded_friend_loc = friend_loc;
                bool standard_fight_reasons = fight_looks_good || trigger_loc.building_count_ > 0 || !CUNYAIModule::isInPotentialDanger(u->getType(), enemy_loc);
                bool positionedForAttack = CUNYAIModule::currentMapInventory.isInExtraWideBufferField(TilePosition(u->getPosition())) > 0.0;
                if (e_closest_threat && e_closest_threat->type_.isWorker()) {
                    expanded_friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest_threat->pos_, search_radius) + friend_loc; // this is critical for worker only fights, where the number of combatants determines if a new one is needed.
                    expanded_friend_loc.updateUnitInventorySummary();
                }

                //Some unit types are special and behave differently.
                switch (u->getType()) {
                case UnitTypes::Protoss_Probe:
                case UnitTypes::Terran_SCV:
                case UnitTypes::Zerg_Drone: // Workers are very unique.
                    if ((checkNeedMoreWorkersToHold(expanded_friend_loc, enemy_loc) || my_unit->phase_ == StoredUnit::Phase::Attacking) && !resource_loc.ResourceInventory_.empty()) {
                        bool unit_dead_next_check = CUNYAIModule::friendly_player_model.units_.getStoredUnit(u)->unitDeadInFuture(14);
                        if (CUNYAIModule::basemanager.getBaseCount() > 1 && CUNYAIModule::friendly_player_model.units_.stock_shoots_down_ > 0 && unit_dead_next_check)
                            return mobility.Retreat_Logic();// exit this section and retreat if there is somewhere to go, someone will fight for you, and you are about to die.
                        else if (!unit_dead_next_check) // Do you need to join in? Don't join in if you will be dead the next time we check.
                            return mobility.Tactical_Logic(enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    // Too many workers are fighting, or you are not suitable for fighting, so continue your task. Otherwise, run.
                    if (my_unit->phase_ != StoredUnit::Phase::Attacking && my_unit->phase_ != StoredUnit::Phase::Retreating)
                        return false; //Do not issue combat commands to workers, they will stop working.
                    else {
                        return mobility.Retreat_Logic();// exit this section and retreat if there is somewhere to go, someone will fight for you, and you are about to die.
                    }
                    break; 
                case UnitTypes::Zerg_Lurker: // Lurkesr are siege units and should be moved sparingly.
                    if (isCollectingForces(friend_loc) && CUNYAIModule::isInPotentialDanger(u->getType(), enemy_loc) && positionedForAttack && enemy_loc.detector_count_ > 0 && (my_unit->phase_ == StoredUnit::Phase::PathingOut || my_unit->phase_ == StoredUnit::Phase::Surrounding || my_unit->phase_ == StoredUnit::Phase::Retreating)) {
                        mobility.prepareLurkerToAttack(u->getPosition()); //attacking here exactly should burrow it.
                        return true; // now the lurker should be burrowed.
                    }
                    else if (standard_fight_reasons || enemy_loc.detector_count_ == 0) {
                        return mobility.Tactical_Logic(enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                case UnitTypes::Zerg_Scourge: // Suicide Units
                case UnitTypes::Zerg_Infested_Terran:
                    if (standard_fight_reasons || my_unit->phase_ == StoredUnit::Phase::Attacking) {
                        return mobility.Tactical_Logic(enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                default: // Most simple combat units behave like this:
                    if (standard_fight_reasons) {
                        StoredUnit* e_closest_melee_threat = CUNYAIModule::getClosestMeleeThreatStored(enemy_loc, u, 450); // maximum sight distance of 352, siege tanks in siege mode are about 382
                        int distance_to_ground_threat = 0;
                        if (e_closest_melee_threat) {
                            distance_to_ground_threat = e_closest_melee_threat->pos_.getDistance(u->getPosition());
                            bool kiting_away = e_closest_melee_threat->bwapi_unit_ && !e_closest_melee_threat->type_.isBuilding() && CUNYAIModule::isRanged(u->getType()) && distance_to_ground_threat < 64;  // only kite if he's in range,
                            if (kiting_away)
                                return mobility.Retreat_Logic(); // if kiting, retreat.
                        }
                        return mobility.Tactical_Logic(enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                }
            }
            return mobility.Retreat_Logic();
        }

    }
    return false;
}

bool CombatManager::scoutScript(const Unit & u)
{
    if (scoutCount() < 2 || isScout(u)) { // if the scout squad is empty or this unit is in it.
        auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);
        if (found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && found_item->second.phase_ != StoredUnit::Phase::Detecting) {
            addScout(u);
            removeLiablity(u);
            Mobility mobility = Mobility(u);
            StoredUnit* e_closest = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, u, 400); // maximum sight distance of 352, siege tanks in siege mode are about 382
            if (!e_closest) { // if there are no bad guys nearby, feel free to explore outwards.
                return pathingScript(u);
            }
            else {
                return mobility.Retreat_Logic();
            }
        }
    }
    else if(CUNYAIModule::basemanager.getBaseCount() > 5 && CUNYAIModule::enemy_player_model.units_.building_count_ == 0) {
        Mobility mobility = Mobility(u);
            StoredUnit* closest = CUNYAIModule::getClosestStoredLinear(CUNYAIModule::friendly_player_model.units_, u->getPosition(), u->getType().sightRange() * 2, UnitTypes::AllUnits, UnitTypes::None);
            if (closest)
                return mobility.moveTo(u->getPosition(), u->getPosition() - mobility.approach(closest->pos_) + mobility.avoid_edges(), StoredUnit::Phase::PathingOut);
    }
    return false;
}

int CombatManager::scoutCount() {
    if(!scout_squad_.unit_map_.empty())
        return scout_squad_.unit_map_.size();
    return 0;
};

int CombatManager::scoutPosition(const Unit & u) {
    auto found_item = CUNYAIModule::combat_manager.scout_squad_.unit_map_.find(u);
    if (found_item != CUNYAIModule::combat_manager.scout_squad_.unit_map_.end())
        return distance(CUNYAIModule::combat_manager.scout_squad_.unit_map_.begin(), found_item);
    return 0;
}

// Protects a unit (primarily overlords) that is otherwise simply a liability.
bool CombatManager::liabilitiesScript(const Unit &u)
{
    removeScout(u);
    liabilities_squad_.addStoredUnit(u);
    StoredUnit* closestSpore = CUNYAIModule::getClosestStoredLinear(CUNYAIModule::friendly_player_model.units_, u->getPosition(), 9999, UnitTypes::Zerg_Spore_Colony, UnitTypes::None);
    if (closestSpore && u->getPosition().getDistance(closestSpore->pos_) < 32) // If they're there at the destination, they are doing nothing.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
    if (closestSpore && u->move(closestSpore->pos_)) // Otherwise, get them to safety.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::PathingHome);

    StoredUnit* closestSunken = CUNYAIModule::getClosestStoredLinear(CUNYAIModule::friendly_player_model.units_, u->getPosition(), 9999, UnitTypes::Zerg_Sunken_Colony, UnitTypes::None);
    if (closestSunken && u->getPosition().getDistance(closestSunken->pos_) < 32) // If they're there at the destination, they are doing nothing.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None); 
    if (closestSunken && u->move(closestSunken->pos_)) // Otherwise, get them to safety.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::PathingHome); 
    return false;
}

bool CombatManager::pathingScript(const Unit & u)
{
    Mobility mobility = Mobility(u);
    if ( getMacroCombatReadiness() || isScout(u)) {
        return mobility.BWEM_Movement(true); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
    }
    else {
        return mobility.BWEM_Movement(false); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
    }

    return false;
}

bool CombatManager::checkNeedMoreWorkersToHold(const UnitInventory &friendly, const UnitInventory &enemy)
{
    bool check_enough_mining = CUNYAIModule::friendly_player_model.units_.count_of_each_phase_.at(StoredUnit::Phase::MiningMin) + CUNYAIModule::friendly_player_model.units_.count_of_each_phase_.at(StoredUnit::Phase::Returning) + CUNYAIModule::friendly_player_model.units_.count_of_each_phase_.at(StoredUnit::Phase::MiningGas) > friendly.count_of_each_phase_.at(StoredUnit::Phase::Attacking) + 1;
    bool bare_minimum_defenders = friendly.count_of_each_phase_.at(StoredUnit::Phase::Attacking) < (enemy.worker_count_ > 0) + (enemy.worker_count_ > 1) * enemy.worker_count_ + (enemy.building_count_ > 0)*(enemy.building_count_ + 3) + (enemy.ground_count_ > 0)*(enemy.ground_count_ + 3); // If there is 1 worker, bring 1, if 2+, bring workers+1, if buildings, buildings + 3, if troops, troops+3;
    return bare_minimum_defenders && check_enough_mining;
}

bool CombatManager::addAntiAir(const Unit & u)
{
    if (anti_air_squad_.addStoredUnit(u)) {
        anti_air_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addAntiGround(const Unit & u)
{
    if (anti_ground_squad_.addStoredUnit(u)) {
        anti_ground_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addUniversal(const Unit & u)
{
    if (universal_squad_.addStoredUnit(u)) {
        universal_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addLiablitity(const Unit & u)
{
    if (liabilities_squad_.addStoredUnit(u)) {
        liabilities_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addScout(const Unit & u)
{
    if (scout_squad_.addStoredUnit(u)) {
        scout_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

void CombatManager::removeScout(const Unit & u)
{
    scout_squad_.removeStoredUnit(u);
    scout_squad_.updateUnitInventorySummary();
}

bool CombatManager::isScout(const Unit & u)
{
    auto found_item = CUNYAIModule::combat_manager.scout_squad_.unit_map_.find(u);
    if (found_item != CUNYAIModule::combat_manager.scout_squad_.unit_map_.end()) return true;
    return false;
}

bool CombatManager::isLiability(const Unit & u)
{
    auto found_item = CUNYAIModule::combat_manager.liabilities_squad_.unit_map_.find(u);
    if (found_item != CUNYAIModule::combat_manager.liabilities_squad_.unit_map_.end()) return true;
    return false;
}

void CombatManager::removeLiablity(const Unit & u)
{
    liabilities_squad_.removeStoredUnit(u);
    liabilities_squad_.updateUnitInventorySummary();
}

bool CombatManager::isCollectingForces(const UnitInventory & ui)
{
    return ui.count_of_each_phase_.at(StoredUnit::Phase::PathingOut) > ui.unit_map_.size() / 5;
}

bool CombatManager::isWorkerFight(const UnitInventory & friendly, const UnitInventory & enemy)
{
    if (enemy.worker_count_ + enemy.building_count_ == static_cast<int>(enemy.unit_map_.size()))
        return true; // They're all workers
    else return false; 
}

int CombatManager::getSearchRadius(const Unit & u)
{
    int totalSearchRadius = 0;
    if (u->isFlying())
        totalSearchRadius += CUNYAIModule::enemy_player_model.units_.max_range_air_ + CUNYAIModule::convertTileDistanceToPixelDistance(2);
    else
        totalSearchRadius += CUNYAIModule::enemy_player_model.units_.max_range_ground_ + CUNYAIModule::convertTileDistanceToPixelDistance(2); //Units have size/mass. Largest shooting unit has size 2 tiles.

    totalSearchRadius = max({ CUNYAIModule::friendly_player_model.units_.max_range_, 192, totalSearchRadius });

    return totalSearchRadius;
}

void CombatManager::onFrame()
{
    // Update FAPS with units, runs sim, and reports issues.
    CombatSimulator mainCombatSim;
    mainCombatSim.addPlayersToSimulation();
    mainCombatSim.runSimulation();
    CUNYAIModule::friendly_player_model.units_.updateWithPredictedStatus(mainCombatSim);
    CUNYAIModule::enemy_player_model.units_.updateWithPredictedStatus(mainCombatSim, false);

    Diagnostics::drawAllFutureDeaths(CUNYAIModule::friendly_player_model.units_);
    Diagnostics::drawAllFutureDeaths(CUNYAIModule::enemy_player_model.units_);

}

bool CombatManager::getMacroCombatReadiness()
{
    bool ready_to_fight = !CUNYAIModule::army_starved ||
        CUNYAIModule::enemy_player_model.units_.unit_map_.empty() ||
        CUNYAIModule::enemy_player_model.spending_model_.getlnY() < CUNYAIModule::friendly_player_model.spending_model_.getlnY();
    return ready_to_fight;
}

