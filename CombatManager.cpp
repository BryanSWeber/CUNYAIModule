#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\UnitInventory.h"
#include "Source\MobilityManager.h"
#include "Source\CombatManager.h"
#include "Source/Diagnostics.h"
#include <bwem.h>

bool CombatManager::ready_to_fight = !CUNYAIModule::army_starved ||
CUNYAIModule::enemy_player_model.units_.unit_map_.empty() ||
CUNYAIModule::enemy_player_model.spending_model_.getlnYusing(CUNYAIModule::friendly_player_model.spending_model_.alpha_army, CUNYAIModule::friendly_player_model.spending_model_.alpha_tech) < CUNYAIModule::friendly_player_model.spending_model_.getlnY(); // or we haven't scouted for an approximate minute. 

UnitInventory CombatManager::scout_squad_;
UnitInventory CombatManager::liabilities_squad_;

bool CombatManager::grandStrategyScript(const Unit & u) {

    bool task_assigned = false;

    auto found_item = CUNYAIModule::getStoredUnit(CUNYAIModule::friendly_player_model.units_, u);
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
        if (!task_assigned && u->getType().canMove() && (u->isUnderStorm() || u->isIrradiated() || u->isUnderDisruptionWeb()) && Mobility(u).Scatter_Logic())
            task_assigned = true;
        if (!task_assigned && u->getType().canMove() && u->getType() == UnitTypes::Zerg_Overlord && !u->isBlind() && scoutScript(u))
            task_assigned = true;
        if (!task_assigned && (u->canAttack() || u->getType() == UnitTypes::Zerg_Lurker) && combatScript(u))
            task_assigned = true;
        if (!task_assigned && !u->getType().isWorker() && u->getType() != UnitTypes::Zerg_Overlord && (u->canMove() || (u->getType() == UnitTypes::Zerg_Lurker && u->isBurrowed())) && pathingScript(u))
            task_assigned = true;
        if (!task_assigned && u->getType() == UnitTypes::Zerg_Overlord && (found_and_doing_nothing || found_and_morphing || found_and_going_home) && liabilitiesScript(u))
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
        int u_safeAreaID = BWEM::Map::Instance().GetNearestArea(TilePosition(CUNYAIModule::currentMapInventory.safe_base_))->Id();

        Mobility mobility = Mobility(u);
        int search_radius = max({ u->isFlying() ? CUNYAIModule::enemy_player_model.units_.max_range_air_ : CUNYAIModule::enemy_player_model.units_.max_range_ground_, u->isFlying() ? CUNYAIModule::enemy_player_model.casualties_.max_range_air_ : CUNYAIModule::enemy_player_model.casualties_.max_range_ground_ , CUNYAIModule::friendly_player_model.units_.max_range_, 192 }) + mobility.getDistanceMetric(); // minimum range is 5 tiles, roughly 1 hydra, so we notice enemies BEFORE we get shot.
        StoredUnit* e_closest_threat = CUNYAIModule::getClosestThreatWithPriority(CUNYAIModule::enemy_player_model.units_, u, search_radius); // maximum sight distance of 352, siege tanks in siege mode are about 382
        StoredUnit* my_unit = CUNYAIModule::getStoredUnit(CUNYAIModule::friendly_player_model.units_, u);
        bool unit_building = (my_unit->phase_ == StoredUnit::Phase::Building) || (my_unit->phase_ == StoredUnit::Phase::Prebuilding);

        if (unit_building)
            return false;

        if (e_closest_threat) { // if there are bad guys, fight. Builders do not fight.
            //Collect information determinine what kind of fight it is, and if we want to fight.
            int distance_to_foe = static_cast<int>(e_closest_threat->pos_.getDistance(u->getPosition()));
            int distance_to_threat = 0;
            int distance_to_ground = 0;

            UnitInventory enemy_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), search_radius);
            enemy_loc.updateUnitInventorySummary();
            UnitInventory friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), search_radius);
            friend_loc.updateUnitInventorySummary();
            UnitInventory trigger_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest_threat->pos_, max(enemy_loc.max_range_ground_, 200) );
            trigger_loc.updateUnitInventorySummary();
            UnitInventory casualties_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.casualties_, e_closest_threat->pos_, max(enemy_loc.max_range_ground_, 256));
            casualties_loc.updateUnitInventorySummary();
            Resource_Inventory resource_loc = CUNYAIModule::getResourceInventoryInRadius(CUNYAIModule::land_inventory, e_closest_threat->pos_, max(enemy_loc.max_range_ground_, 256));
            //resource_loc.updateResourceInventory();                                                   

            StoredUnit* e_closest_ground = CUNYAIModule::getClosestGroundStored(enemy_loc, u->getPosition()); // maximum sight distance of 352, siege tanks in siege mode are about 382
            distance_to_threat = static_cast<int>(e_closest_threat->pos_.getDistance(u->getPosition()));

            if (e_closest_ground)
                distance_to_ground = static_cast<int>(e_closest_ground->pos_.getDistance(u->getPosition()));

            //bool unit_death_in_moments = StoredUnit::unitDeadInFuture(CUNYAIModule::friendly_player_model.units_.unit_map_.at(u), 6);
            bool fight_looks_good = CUNYAIModule::checkSuperiorFAPForecast(friend_loc, enemy_loc);
            bool unit_will_survive = !StoredUnit::unitDeadInFuture(*CUNYAIModule::friendly_player_model.units_.getStoredUnit(u), 6); // Worker is expected to live.
            bool worker_time_and_place = false;
            bool standard_fight_reasons = fight_looks_good || trigger_loc.building_count_ > 0 || !CUNYAIModule::isInDanger(u->getType(), enemy_loc);
            UnitInventory expanded_friend_loc;
            bool prepping_attack = (!mobility.isOnDifferentHill(*e_closest_threat) || u->isFlying()) && friend_loc.count_of_each_phase_.at(StoredUnit::Phase::PathingOut) > CUNYAIModule::countUnits(UnitTypes::Zerg_Overlord, friend_loc) && friend_loc.count_of_each_phase_.at(StoredUnit::Phase::Attacking) == 0 && distance_to_threat > (enemy_loc.max_range_air_ * u->isFlying() + enemy_loc.max_range_ground_ * !u->isFlying() + 32); // overlords path out and may prevent attacking.
            if (e_closest_threat->type_.isWorker()) {
                expanded_friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest_threat->pos_, search_radius) + friend_loc; // this is critical for worker only fights, where the number of combatants determines if a new one is needed.
                expanded_friend_loc.updateUnitInventorySummary();
            }

            //If we can fight, our unit type will determine our behavior.
            if (CUNYAIModule::canContributeToFight(u->getType(), enemy_loc)) {
                auto overstacked_units = CUNYAIModule::getClosestStored(u, friend_loc, u->getType(), u->getType().width());

                //Some unit types are special and behave differently.
                switch (u->getType())
                {
                case UnitTypes::Protoss_Probe:
                case UnitTypes::Terran_SCV:
                case UnitTypes::Zerg_Drone: // Workers are very unique.
                    if ((checkNeedMoreWorkersToHold(expanded_friend_loc, enemy_loc) || my_unit->phase_ == StoredUnit::Phase::Attacking) && !resource_loc.resource_inventory_.empty()) {
                        bool unit_dead_next_check = StoredUnit::unitDeadInFuture(*CUNYAIModule::friendly_player_model.units_.getStoredUnit(u), 14);
                        if (CUNYAIModule::basemanager.getBaseCount() > 1 && CUNYAIModule::friendly_player_model.units_.stock_shoots_down_ > 0 && unit_dead_next_check)
                            break; // exit this section and retreat if there is somewhere to go, someone will fight for you, and you are about to die.
                        else if (!unit_dead_next_check) // Do you need to join in? Don't join in if you will be dead the next time we check.
                            return mobility.Tactical_Logic(*e_closest_threat, enemy_loc, friend_loc, search_radius, Colors::White);
                        else
                            return false; // if there's no where to go and you are about to die.... keep mining.
                    }
                    else {
                        return false; // Too many workers are fighting, so let us have you continue your task.
                    }
                    break;
                case UnitTypes::Zerg_Lurker: // Lurkesr are siege units and should be moved sparingly.
                    if ((!standard_fight_reasons && !enemy_loc.detector_count_ == 0) && (my_unit->phase_ == StoredUnit::Phase::PathingOut || my_unit->phase_ == StoredUnit::Phase::Attacking) && prepping_attack && !my_unit->burrowed_) {
                        if (overstacked_units) { // we don't want lurkers literally on top of each other.
                            return mobility.surroundLogic(e_closest_threat->pos_);
                        }
                        else {
                            mobility.prepareLurkerToAttack(u->getPosition()); //attacking here exactly should burrow it.
                            return true; // now the lurker should be burrowed.
                        }
                    }
                    else if (standard_fight_reasons || enemy_loc.detector_count_ == 0) {
                        return mobility.Tactical_Logic(*e_closest_threat, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                case UnitTypes::Zerg_Scourge: // Suicide Units
                case UnitTypes::Zerg_Infested_Terran:
                    if ((my_unit->phase_ == StoredUnit::Phase::PathingOut || my_unit->phase_ == StoredUnit::Phase::Surrounding) && overstacked_units) {
                        return mobility.Scatter_Logic(overstacked_units->pos_);
                    }
                    else if (!standard_fight_reasons && my_unit->phase_ == StoredUnit::Phase::PathingOut && prepping_attack) {
                        return mobility.surroundLogic(e_closest_threat->pos_);
                    }
                    else if (standard_fight_reasons || my_unit->phase_ == StoredUnit::Phase::Attacking) {
                        return mobility.Tactical_Logic(*e_closest_threat, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                    // Most simple combat units behave like this:
                default:
                    if (!standard_fight_reasons && (my_unit->phase_ == StoredUnit::Phase::PathingOut || my_unit->phase_ == StoredUnit::Phase::Attacking) && prepping_attack) {
                        return mobility.surroundLogic(e_closest_threat->pos_);
                    }
                    else if (standard_fight_reasons) {
                        bool is_near_choke = false;
                        if(BWEB::Map::getClosestChokeTile(u->getPosition()).isValid())
                            is_near_choke = (BWEB::Map::getClosestChokeTile(u->getPosition()) + Position(16,16)).getDistance(u->getPosition()) < 64;
                        bool target_is_escaping = (e_closest_ground && mobility.checkGoingDifferentDirections(e_closest_ground->bwapi_unit_) && !mobility.checkEnemyApproachingUs(e_closest_ground->bwapi_unit_) && getEnemySpeed(e_closest_ground->bwapi_unit_) > 0);
                        bool surround_is_viable = (distance_to_ground > max(mobility.getDistanceMetric(), CUNYAIModule::getFunctionalRange(u)) / 2 && target_is_escaping && !u->isFlying() && !u->getType() != UnitTypes::Zerg_Lurker); // if they are far apart, they're moving different directions, and the enemy is actually moving away from us, surround him!
                        bool kiting_away = e_closest_threat->bwapi_unit_ && 64 > CUNYAIModule::getExactRange(e_closest_threat->bwapi_unit_) && CUNYAIModule::getExactRange(u) > 64 && distance_to_threat < 64;  // only kite if he's in range,
                        bool kiting_in = !u->isFlying() && is_near_choke && CUNYAIModule::getExactRange(u) > 64 && distance_to_threat > UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().minRange() && my_unit->phase_ == StoredUnit::Phase::Attacking;  // only kite if he's in range, and if you JUST finished an attack.
                        if ((kiting_in || surround_is_viable) && e_closest_ground)
                            return mobility.moveTo(u->getPosition(), u->getPosition() + mobility.getVectorToEnemyDestination(e_closest_ground->bwapi_unit_) + mobility.getVectorToBeyondEnemy(e_closest_ground->bwapi_unit_), StoredUnit::Phase::Surrounding);
                        if (kiting_away)
                            break; // if kiting, just exit and we will retreat.
                        else
                            return mobility.Tactical_Logic(*e_closest_threat, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    break;
                }
            }

            Diagnostics::drawCircle(e_closest_threat->pos_, CUNYAIModule::currentMapInventory.screen_position_, CUNYAIModule::enemy_player_model.units_.max_range_, Colors::Red);
            Diagnostics::drawCircle(e_closest_threat->pos_, CUNYAIModule::currentMapInventory.screen_position_, search_radius, Colors::Green);

            if (CUNYAIModule::isInDanger(u->getType(), enemy_loc) || u->getType().isWorker()) {
                return mobility.Retreat_Logic(*e_closest_threat);
            }
            else {
                return mobility.surroundLogic(e_closest_threat->pos_);
            }
        }

        //If there are no threats, let's smash stuff under these conditions.
        StoredUnit* e_closest_target = CUNYAIModule::getClosestAttackableStored(CUNYAIModule::enemy_player_model.units_, u, search_radius); // maximum sight distance of 352, siege tanks in siege mode are about 382
        if (e_closest_target) {
            UnitInventory enemy_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), search_radius);
            UnitInventory friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), search_radius);
            enemy_loc.updateUnitInventorySummary();
            friend_loc.updateUnitInventorySummary();

            bool kiting_in = !u->isFlying() && CUNYAIModule::getExactRange(u) > 64 && static_cast<int>(e_closest_target->pos_.getDistance(u->getPosition())) > UnitTypes::Terran_Siege_Tank_Siege_Mode.groundWeapon().minRange() && my_unit->phase_ == StoredUnit::Phase::Attacking;  // only kite if he's in range, and if you JUST finished an attack.
            if (kiting_in)
                return mobility.moveTo(u->getPosition(), u->getPosition() + mobility.getVectorToEnemyDestination(e_closest_target->bwapi_unit_) + mobility.getVectorToBeyondEnemy(e_closest_target->bwapi_unit_), StoredUnit::Phase::Surrounding);
            else
                return mobility.Tactical_Logic(*e_closest_target, enemy_loc, friend_loc, search_radius, Colors::White);
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
                return mobility.Retreat_Logic(*e_closest);
            }
        }
    }
    else if(CUNYAIModule::basemanager.getBaseCount() > 5 && CUNYAIModule::enemy_player_model.units_.building_count_ == 0) {
        Mobility mobility = Mobility(u);
        //Position explore_vector = mobility.getVectorTowardsField(CUNYAIModule::current_MapInventory.pf_explore_);
        //if(explore_vector != Positions::Origin)
        //    return mobility.moveTo(u->getPosition(), u->getPosition() + explore_vector, StoredUnit::Phase::PathingOut);
        //else {
            StoredUnit* closest = CUNYAIModule::getClosestStored(CUNYAIModule::friendly_player_model.units_, u->getPosition(), u->getType().sightRange() * 2);
            if (closest)
                return mobility.moveTo(u->getPosition(), u->getPosition() - mobility.approach(closest->pos_) + mobility.avoid_edges(), StoredUnit::Phase::PathingOut);
        //}
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
    StoredUnit* closestSpore = CUNYAIModule::getClosestStored(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Spore_Colony, u->getPosition(), 500);
    if (closestSpore && u->getPosition().getDistance(closestSpore->pos_) < 32) // If they're there at the destination, they are doing nothing.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None);
    if (closestSpore && u->move(closestSpore->pos_)) // Otherwise, get them to safety.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::PathingHome);

    StoredUnit* closestSunken = CUNYAIModule::getClosestStored(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Sunken_Colony, u->getPosition(), 9999);
    if (closestSunken && u->getPosition().getDistance(closestSunken->pos_) < 32) // If they're there at the destination, they are doing nothing.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::None); 
    if (closestSunken && u->move(closestSunken->pos_)) // Otherwise, get them to safety.
        return CUNYAIModule::updateUnitPhase(u, StoredUnit::Phase::PathingHome); 
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

bool CombatManager::isWorkerFight(const UnitInventory & friendly, const UnitInventory & enemy)
{
    if (enemy.worker_count_ + enemy.building_count_ == static_cast<int>(enemy.unit_map_.size()))
        return true; // They're all workers
    else return false; 
}

void CombatManager::updateReadiness()
{
    ready_to_fight = !CUNYAIModule::army_starved || CUNYAIModule::enemy_player_model.units_.unit_map_.empty() || CUNYAIModule::enemy_player_model.spending_model_.getlnYusing(CUNYAIModule::friendly_player_model.spending_model_.alpha_army, CUNYAIModule::friendly_player_model.spending_model_.alpha_tech) < CUNYAIModule::friendly_player_model.spending_model_.getlnY();
}

