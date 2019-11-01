#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "Source\MobilityManager.h"
#include "Source\CombatManager.h"
#include "Source/Diagnostics.h"
#include <bwem.h>

bool CombatManager::ready_to_fight = !CUNYAIModule::army_starved ||
CUNYAIModule::enemy_player_model.units_.unit_map_.empty() ||
CUNYAIModule::enemy_player_model.spending_model_.getlnYusing(CUNYAIModule::friendly_player_model.spending_model_.alpha_army, CUNYAIModule::friendly_player_model.spending_model_.alpha_tech) < CUNYAIModule::friendly_player_model.spending_model_.getlnY() ||
(CUNYAIModule::enemy_player_model.estimated_unseen_army_ + CUNYAIModule::enemy_player_model.estimated_unseen_tech_ > CUNYAIModule::enemy_player_model.estimated_resources_per_frame_ * 24 * 60 && CUNYAIModule::enemy_player_model.spending_model_.army_stock < CUNYAIModule::friendly_player_model.spending_model_.army_stock); // or we haven't scouted for an approximate minute. 

Unit_Inventory CombatManager::scout_squad_;
Unit_Inventory CombatManager::liabilities_squad_;

bool CombatManager::grandStrategyScript(const Unit & u) {

    bool task_assigned = false;

    auto found_item = CUNYAIModule::getStoredUnit(CUNYAIModule::friendly_player_model.units_, u);
    bool found_and_detecting = found_item->phase_ == Stored_Unit::Phase::Detecting;

    if (isScout(u)) {
        if (u->isBlind() || found_and_detecting) removeScout(u);
    }

    if (found_and_detecting && u->isIdle()) return CUNYAIModule::updateUnitPhase(u, Stored_Unit::Phase::None);

    if (CUNYAIModule::spamGuard(u)) {
        if (!task_assigned && u->getType().canMove() && (u->isUnderStorm() || u->isIrradiated() || u->isUnderDisruptionWeb()) && Mobility(u).Scatter_Logic())
            task_assigned = true;
        if (!task_assigned && (u->canAttack() || u->getType() == UnitTypes::Zerg_Lurker) && combatScript(u))
            task_assigned = true;
        if (!task_assigned && u->getType().canMove() && (u->getType() == UnitTypes::Zerg_Overlord || u->getType() == UnitTypes::Zerg_Zergling) && !u->isBlind() && scoutScript(u))
            task_assigned = true;
        if (!task_assigned && !u->getType().isWorker() && (u->canMove() || (u->getType() == UnitTypes::Zerg_Lurker && u->isBurrowed())) && u->getType() != UnitTypes::Zerg_Overlord && pathingScript(u))
            task_assigned = true;
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
        int u_areaID = BWEM::Map::Instance().GetNearestArea(u->getTilePosition())->Id();
        Mobility mobility = Mobility(u);
        int search_radius = max({ CUNYAIModule::enemy_player_model.units_.max_range_, CUNYAIModule::enemy_player_model.casualties_.max_range_, CUNYAIModule::friendly_player_model.units_.max_range_, 192 }) + mobility.getDistanceMetric(); // minimum range is 5 tiles, roughly 1 hydra, so we notice enemies BEFORE we get shot.
        Stored_Unit* e_closest = CUNYAIModule::getClosestThreatOrTargetExcluding(CUNYAIModule::enemy_player_model.units_, UnitTypes::Zerg_Larva, u, search_radius); // maximum sight distance of 352, siege tanks in siege mode are about 382
        Stored_Unit* my_unit = CUNYAIModule::getStoredUnit(CUNYAIModule::friendly_player_model.units_, u);
        bool unit_building = unit_building = my_unit->phase_ == Stored_Unit::Phase::Building || my_unit->phase_ == Stored_Unit::Phase::Prebuilding;

        if (e_closest && !unit_building) { // if there are bad guys, fight. Builders do not fight.
            int distance_to_foe = static_cast<int>(e_closest->pos_.getDistance(u->getPosition()));
            //int chargable_distance_self = CUNYAIModule::getChargableDistance(u);
            //int chargable_distance_enemy = CUNYAIModule::getChargableDistance(e_closest->bwapi_unit_);
            //int chargable_distance_max = max(chargable_distance_self, chargable_distance_enemy); // how far can you get before he shoots?

            Unit_Inventory friend_loc;
            Unit_Inventory enemy_loc;
            Resource_Inventory resource_loc = CUNYAIModule::getResourceInventoryInRadius(CUNYAIModule::land_inventory, e_closest->pos_, search_radius);

            //Unit_Inventory enemy_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, e_closest->pos_, search_radius);
            Unit_Inventory enemy_loc_around_self = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), search_radius);
            enemy_loc = (/*enemy_loc_around_target +*/ enemy_loc_around_self);

            //Unit_Inventory friend_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest->pos_, search_radius);
            Unit_Inventory friend_loc_around_me = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), search_radius);
            friend_loc = (/*friend_loc_around_target +*/ friend_loc_around_me);

            enemy_loc.updateUnitInventorySummary();
            friend_loc.updateUnitInventorySummary();

            // Bools needed before the switch.
            //bool unit_death_in_moments = Stored_Unit::unitDeadInFuture(CUNYAIModule::friendly_player_model.units_.unit_map_.at(u), 6);
            bool fight_looks_good = CUNYAIModule::checkSuperiorFAPForecast(friend_loc, enemy_loc);
            bool prepping_attack = friend_loc.count_of_each_phase_.at(Stored_Unit::Phase::PathingOut) > CUNYAIModule::Count_Units(UnitTypes::Zerg_Overlord, friend_loc) && friend_loc.count_of_each_phase_.at(Stored_Unit::Phase::Attacking) == 0 && distance_to_foe > enemy_loc.max_range_ + 32; // overlords path out and may prevent attacking.
            bool unit_will_survive = !Stored_Unit::unitDeadInFuture(*CUNYAIModule::friendly_player_model.units_.getStoredUnit(u), 6); // Worker is expected to live.
            bool worker_time_and_place = false;
            bool standard_fight_reasons = fight_looks_good || (friend_loc.stock_ground_fodder_ > 0 && unit_will_survive) || !CUNYAIModule::isInDanger(u->getType(), enemy_loc);
            Unit_Inventory expanded_friend_loc;
            if (e_closest->type_.isWorker()) {
                expanded_friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest->pos_, search_radius) + friend_loc; // this is critical for worker only fights, where the number of combatants determines if a new one is needed.
                expanded_friend_loc.updateUnitInventorySummary();
            }

            if (CUNYAIModule::canContributeToFight(u->getType(), enemy_loc)) {
                auto overstacked_units = CUNYAIModule::getClosestStored(u, friend_loc, u->getType(), u->getType().width());

                //Some unit types are special and behave differently.
                switch (u->getType())
                {
                case UnitTypes::Protoss_Probe:
                case UnitTypes::Terran_SCV:
                case UnitTypes::Zerg_Drone: // Workers are very unique.
                    if (isPulledWorkersFight(expanded_friend_loc, enemy_loc)) { // if this is a worker battle, eg stone , a mean worker scout, or a static defense rush.
                        if (expanded_friend_loc.count_of_each_phase_.at(Stored_Unit::Phase::Attacking) < ((enemy_loc.worker_count_ > 0)*(enemy_loc.worker_count_ + 1) + (enemy_loc.building_count_ > 0)*(enemy_loc.building_count_ + 3)) && CUNYAIModule::friendly_player_model.units_.count_of_each_phase_.at(Stored_Unit::Phase::MiningMin) > expanded_friend_loc.count_of_each_phase_.at(Stored_Unit::Phase::Attacking)) {
                            if (!resource_loc.resource_inventory_.empty()) { // Do you need to join in?
                                return mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White); 
                            }
                            else{
                                break; // exit this section and retreat.
                            }
                        }
                        else {
                            return false; // Too many workers are fighting, so let us have you continue your task.
                        }
                    }
                    else { // this fight is a regular fight.
                        if (!resource_loc.resource_inventory_.empty() && (standard_fight_reasons || (CUNYAIModule::current_map_inventory.hatches_ == 1 && friend_loc.building_count_ > 0))) {
                            return mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                        }
                    }
                    break;
                case UnitTypes::Zerg_Lurker: // Lurkesr are siege units and should be moved sparingly.
                    if ((fight_looks_good || isWorkerFight(friend_loc, enemy_loc)) && prepping_attack && CUNYAIModule::isInDanger(u->getType(), enemy_loc)) {
                        if (overstacked_units) { // we don't want lurkers literally on top of each other.
                            return mobility.surround(e_closest->pos_);
                        }
                        else {
                            return mobility.adjust_lurker_burrow(u->getPosition()); //attacking here exactly should burrow it.
                        }
                    }
                    else if (standard_fight_reasons || enemy_loc.detector_count_ > 0) {
                        return mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                    // Most simple combat units behave like this:
                case UnitTypes::Zerg_Scourge: // Suicide Units
                case UnitTypes::Zerg_Infested_Terran: 
                    if (my_unit->phase_ != Stored_Unit::Phase::Attacking && overstacked_units) {
                        return mobility.Scatter_Logic(overstacked_units->pos_);
                    }
                    else if (my_unit->phase_ != Stored_Unit::Phase::Attacking && (fight_looks_good || isWorkerFight(friend_loc, enemy_loc)) && prepping_attack && CUNYAIModule::isInDanger(u->getType(), enemy_loc)) {
                        return mobility.surround(e_closest->pos_);
                    }
                    else if (standard_fight_reasons || my_unit->phase_ == Stored_Unit::Phase::Attacking) {
                        return mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                default:
                    if ((fight_looks_good || isWorkerFight(friend_loc, enemy_loc)) && prepping_attack && CUNYAIModule::isInDanger(u->getType(), enemy_loc)) {
                        return mobility.surround(e_closest->pos_);
                    }
                    else if (standard_fight_reasons) {
                        return mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                }
            }


            Diagnostics::drawCircle(e_closest->pos_, CUNYAIModule::current_map_inventory.screen_position_, CUNYAIModule::enemy_player_model.units_.max_range_, Colors::Red);
            Diagnostics::drawCircle(e_closest->pos_, CUNYAIModule::current_map_inventory.screen_position_, search_radius, Colors::Green);
            
            if (CUNYAIModule::isInDanger(u->getType(), enemy_loc)) {
                return mobility.Retreat_Logic();
            }
            else {
                return mobility.surround(e_closest->pos_);
            }
        }
    }
    return false;
}

bool CombatManager::scoutScript(const Unit & u)
{

    if (scout_squad_.unit_map_.empty() || isScout(u)) { // if the scout squad is empty or this unit is in it.
        auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);
        if (found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end() && found_item->second.phase_ != Stored_Unit::Phase::Detecting) {
            scout_squad_.addStored_Unit(u);
            Mobility mobility = Mobility(u);
            Stored_Unit* e_closest = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, u, 400); // maximum sight distance of 352, siege tanks in siege mode are about 382
            if (!e_closest) { // if there are no bad guys nearby, feel free to explore outwards.
                pathingScript(u);
            }
            else {
                return mobility.Retreat_Logic();
            }
        }
    }
    else {

    }
    return false;
}

bool CombatManager::pathingScript(const Unit & u)
{
    Mobility mobility = Mobility(u);
    if (ready_to_fight || isScout(u)) {
        return mobility.BWEM_Movement(true); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
    }
    else {
        return mobility.BWEM_Movement(false); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
    }

    return false;
}

bool CombatManager::addAntiAir(const Unit & u)
{
    if (anti_air_squad_.addStored_Unit(u)) {
        anti_air_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addAntiGround(const Unit & u)
{
    if (anti_ground_squad_.addStored_Unit(u)) {
        anti_ground_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addUniversal(const Unit & u)
{
    if (universal_squad_.addStored_Unit(u)) {
        universal_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addLiablitity(const Unit & u)
{
    if (liabilities_squad_.addStored_Unit(u)) {
        liabilities_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

bool CombatManager::addScout(const Unit & u)
{
    if (scout_squad_.addStored_Unit(u)) {
        scout_squad_.updateUnitInventorySummary();
        return true;
    }
    return false;
}

void CombatManager::removeScout(const Unit & u)
{
    scout_squad_.removeStored_Unit(u);
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

bool CombatManager::isWorkerFight(const Unit_Inventory & friendly, const Unit_Inventory & enemy)
{
    if (enemy.worker_count_ == static_cast<int>(enemy.unit_map_.size()))
        return true; // They're all workers
    else return false; 
}

bool CombatManager::isPulledWorkersFight(const Unit_Inventory & friendly, const Unit_Inventory & enemy)
{
    if (enemy.worker_count_ + enemy.building_count_ == static_cast<int>(enemy.unit_map_.size()))
        return true; // The enemy has pulled the boys and is fighting amongst the buildings.
    else return false; 
}

void CombatManager::updateReadiness()
{
    ready_to_fight = !CUNYAIModule::army_starved || CUNYAIModule::enemy_player_model.units_.unit_map_.empty() || CUNYAIModule::enemy_player_model.spending_model_.getlnYusing(CUNYAIModule::friendly_player_model.spending_model_.alpha_army, CUNYAIModule::friendly_player_model.spending_model_.alpha_tech) < CUNYAIModule::friendly_player_model.spending_model_.getlnY();
}

