#include "BaseManager.h"
#include "Source/Map_Inventory.h"
#include "Source/CUNYAIModule.h"


Base::Base(const Unit & u)
{
    gas_taken_ = false;
    gas_tolerable_ = true;
    spores_ = 0;
    sunkens_ = 0;
    gas_gatherers_ = 0;
    mineral_gatherers_ = 0;
}

void BaseManager::updateBases()
{
    baseMap_.clear();

    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery) && std::find(CUNYAIModule::current_map_inventory.expo_tilepositions_.begin(), CUNYAIModule::current_map_inventory.expo_tilepositions_.end(), TilePosition(u.second.pos_)) != CUNYAIModule::current_map_inventory.expo_tilepositions_.end())
            baseMap_.insert({ u.second.pos_, Base( u.first ) });
    }

    Unit_Inventory alarming_enemy_ground = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::current_map_inventory.enemy_base_ground_);
    Unit_Inventory alarming_enemy_air = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::current_map_inventory.enemy_base_air_);

    alarming_enemy_ground.updateUnitInventorySummary();
    alarming_enemy_air.updateUnitInventorySummary();

    bool they_are_moving_out_ground = alarming_enemy_ground.building_count_ == 0;
    bool they_are_moving_out_air = alarming_enemy_air.building_count_ == 0;
    int distance_to_alarming_ground = INT_MAX;
    int distance_to_alarming_air = INT_MAX;

    for (auto b : baseMap_) {
        distance_to_alarming_ground = min(CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(b.first), distance_to_alarming_ground);
        distance_to_alarming_air = min(static_cast<int>(b.first.getDistance(CUNYAIModule::current_map_inventory.enemy_base_air_)), distance_to_alarming_air);
    }

    for (auto b : baseMap_) {
        auto e_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, b.first);
        auto u_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::friendly_player_model.units_, b.first);
        e_loc.updateUnitInventorySummary();
        u_loc.updateUnitInventorySummary();
        bool this_is_the_closest_ground_base = distance_to_alarming_ground == CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(b.first);
        bool this_is_the_closest_air_base = distance_to_alarming_air == static_cast<int>(b.first.getDistance(CUNYAIModule::current_map_inventory.enemy_base_air_));
        b.second.sunken_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Sunken_Colony, u_loc);
        b.second.spore_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Spore_Colony, u_loc);
        b.second.creep_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony, u_loc);
        b.second.ground_weak_ = (this_is_the_closest_ground_base && !CUNYAIModule::checkMiniFAPForecast(u_loc, alarming_enemy_ground, true)) || (b.second.sunken_count_ == 0);
        b.second.air_weak_ = (this_is_the_closest_air_base && !CUNYAIModule::checkMiniFAPForecast(u_loc, alarming_enemy_air, true)) || (b.second.spore_count_ == 0);
    }
}
