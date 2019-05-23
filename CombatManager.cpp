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
        Stored_Unit* e_closest = CUNYAIModule::getClosestThreatStored(CUNYAIModule::enemy_player_model.units_, u, 400); // maximum sight distance of 352, siege tanks in siege mode are about 382
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
                search_radius = max(chargable_distance_max + 64, CUNYAIModule::enemy_player_model.units_.max_range_ + 64); // expanded radius because of units intermittently suiciding against static D.

                Unit_Inventory friend_loc;
                Unit_Inventory enemy_loc;

                Unit_Inventory enemy_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory enemy_loc_around_self = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                enemy_loc = (enemy_loc_around_target + enemy_loc_around_self);

                Unit_Inventory friend_loc_around_target = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory friend_loc_around_me = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                friend_loc = (friend_loc_around_target + friend_loc_around_me);

                bool unit_death_in_moments = Stored_Unit::unitDeadInFuture(CUNYAIModule::friendly_player_model.units_.unit_map_.at(u), 6);
                bool they_take_a_fap_beating = CUNYAIModule::checkSuperiorFAPForecast2(friend_loc, enemy_loc);

                bool foe_within_radius = distance_to_foe < search_radius;
                bool potentially_targetable = distance_to_foe < CUNYAIModule::enemy_player_model.units_.max_range_ + 64;
                if (e_closest->valid_pos_ && foe_within_radius) {  // Must have a valid postion on record to attack.
                    if (they_take_a_fap_beating /*&& u_type != UnitTypes::Zerg_Overlord*/) {
                        mobility.Tactical_Logic(*e_closest, enemy_loc, friend_loc, search_radius, Colors::White);
                    }
                    else {
                        mobility.Retreat_Logic();
                    }

                    if constexpr (DRAWING_MODE) {
                        if (draw_retreat_circle) {
                            Broodwar->drawCircleMap(e_closest->pos_, CUNYAIModule::enemy_player_model.units_.max_range_, Colors::Red);
                            Broodwar->drawCircleMap(e_closest->pos_, search_radius, Colors::Green);
                        }
                    }
                    return true; // this unit is finished.
                }
            } // close local examination.
        }

        // If there was no enemy to attack didn't trigger, try to approach.
        long_term_walking = mobility.BWEM_Movement(); // if this process didn't work, then you need to do your default walking. The distance is too short or there are enemies in your area. Or you're a flyer.
        return long_term_walking;
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
    return false;
}

bool CombatManager::addScout(const Unit & u)
{
    return false;
}

