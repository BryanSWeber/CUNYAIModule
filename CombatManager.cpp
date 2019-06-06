#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\Unit_Inventory.h"
#include "Source\MobilityManager.h"
#include "CombatManager.h"
#include <bwem.h>

bool CombatManager::combatScript(const Unit & u)
{
    if (CUNYAIModule::spamGuard(u))
    {
        int u_areaID = BWEM::Map::Instance().GetNearestArea(u->getTilePosition())->Id();
        Mobility mobility = Mobility(u);
        Stored_Unit* e_closest = CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, u, 400); // maximum sight distance of 352, siege tanks in siege mode are about 382
        if (!e_closest) e_closest = CUNYAIModule::getClosestAttackableStored(CUNYAIModule::enemy_player_model.units_, u, 400);

        bool draw_retreat_circle = false;
        int search_radius = 0;
        bool long_term_walking = true;


        if (e_closest) { // if there are bad guys, fight
            int e_areaID = BWEM::Map::Instance().GetNearestArea(TilePosition(e_closest->pos_))->Id();
            if (e_areaID == u_areaID || CUNYAIModule::getProperRange(u) > 64) { // if you are fighting.
                int distance_to_foe = static_cast<int>(e_closest->pos_.getDistance(u->getPosition()));
                int chargable_distance_self = CUNYAIModule::getChargableDistance(u, CUNYAIModule::enemy_player_model.units_);
                int chargable_distance_enemy = CUNYAIModule::getChargableDistance(e_closest->bwapi_unit_, CUNYAIModule::friendly_player_model.units_);
                int chargable_distance_max = max(chargable_distance_self, chargable_distance_enemy); // how far can you get before he shoots?
                search_radius = min(max({ chargable_distance_max + 64, CUNYAIModule::enemy_player_model.units_.max_range_ + 64 }), 400); // expanded radius because of units intermittently suiciding against static D.

                Unit_Inventory friend_loc;
                Unit_Inventory enemy_loc;

                Unit_Inventory enemy_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory enemy_loc_around_self = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                enemy_loc = (enemy_loc_around_target + enemy_loc_around_self);

                Unit_Inventory friend_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory friend_loc_around_me = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                friend_loc = (friend_loc_around_target + friend_loc_around_me);

                bool unit_death_in_moments = Stored_Unit::unitDeadInFuture(CUNYAIModule::friendly_player_model.units_.unit_map_.at(u), 6);
                bool they_take_a_fap_beating = CUNYAIModule::checkSuperiorFAPForecast(friend_loc, enemy_loc);
                auto found_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(u);

                if ( they_take_a_fap_beating || friend_loc.stock_ground_fodder_ > 0 ) {
                    mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                    return true;
                }
                else {
                    if constexpr (DRAWING_MODE) {
                        Broodwar->drawCircleMap(e_closest->pos_, CUNYAIModule::enemy_player_model.units_.max_range_, Colors::Red);
                        Broodwar->drawCircleMap(e_closest->pos_, search_radius, Colors::Green);
                    }
                    return mobility.Retreat_Logic();
                }
            } // close local examination.
        }

        if (!u->getType().isWorker() && u->canMove()) {
            // If there was no enemy to attack didn't trigger, try to approach.
            bool ready_to_fight = !CUNYAIModule::army_starved || CUNYAIModule::enemy_player_model.units_.unit_map_.empty() || CUNYAIModule::enemy_player_model.spending_model_.getlnY() < CUNYAIModule::friendly_player_model.spending_model_.getlnY();
            if (ready_to_fight) {
                return mobility.BWEM_Movement(1); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
            }
            else {
                return mobility.BWEM_Movement(-1); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
            }
        }
    }
    return false;
}

bool CombatManager::scoutScript(const Unit & u)
{
    if (CUNYAIModule::spamGuard(u)) {
        Mobility mobility = Mobility(u);
        Stored_Unit* e_closest = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, u, 400); // maximum sight distance of 352, siege tanks in siege mode are about 382

        if (!e_closest) { // if there are no bad guys nearby, feel free to explore outwards.
            bool ready_to_fight = !CUNYAIModule::army_starved || CUNYAIModule::enemy_player_model.units_.unit_map_.empty() || CUNYAIModule::enemy_player_model.spending_model_.getlnY() < CUNYAIModule::friendly_player_model.spending_model_.getlnY();
            if (ready_to_fight) {
                return mobility.BWEM_Movement(1); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
            }
            else {
                return mobility.BWEM_Movement(-1); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
            }
        }
        else {
            return mobility.Retreat_Logic();
        }
    }

    return false;
}

bool CombatManager::addAntiAir(const Unit & u)
{
    anti_air_squad_.addStored_Unit(u);
    anti_air_squad_.updateUnitInventorySummary();
}

bool CombatManager::addAntiGround(const Unit & u)
{
    anti_ground_squad_.addStored_Unit(u);
    anti_ground_squad_.updateUnitInventorySummary();
}

bool CombatManager::addUniversal(const Unit & u)
{
    universal_squad_.addStored_Unit(u);
    universal_squad_.updateUnitInventorySummary();
}

bool CombatManager::addLiablitity(const Unit & u)
{
    liabilities_squad_.addStored_Unit(u);
    liabilities_squad_.updateUnitInventorySummary();
}

bool CombatManager::addScout(const Unit & u)
{
    scout_squad_.addStored_Unit(u);
    scout_squad_.updateUnitInventorySummary();
}

