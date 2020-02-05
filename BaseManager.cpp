#include <BWAPI.h>
#include "Source/CUNYAIModule.h"
#include "Source/Diagnostics.h"
#include "Source/AssemblyManager.h"
#include "Source/MobilityManager.h"

Base::Base() {
    gas_taken_ = false;
    gas_tolerable_ = false;
    spore_count_ = 0;
    sunken_count_ = 0;
    creep_count_ = 0;
    gas_gatherers_ = 0;
    mineral_gatherers_ = 0;
    returners_ = 0;
    overlords_ = 0;
    distance_to_ground_ = 0;
    distance_to_air_ = 0;
    emergency_spore_ = false;
    emergency_sunken_ = false;
};

Base::Base(const Unit & u)
{
    gas_taken_ = false;
    gas_tolerable_ = false;
    spore_count_ = 0;
    sunken_count_ = 0;
    creep_count_ = 0;
    gas_gatherers_ = 0;
    mineral_gatherers_ = 0;
    returners_ = 0;
    overlords_ = 0;
    distance_to_ground_ = 0;
    distance_to_air_ = 0;
    emergency_spore_ = false;
    emergency_sunken_ = false;
    unit_ = u;
};


map<Position, Base> BaseManager::getBases()
{
    return baseMap_;
}

int BaseManager::getBaseCount()
{
    if (!baseMap_.empty())
        return baseMap_.size();
    return 0;
}

void BaseManager::updateBases()
{
    baseMap_.clear();
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.bwapi_unit_ && u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery))
            if (!(u.second.bwapi_unit_->getBuildType() == UnitTypes::Zerg_Hatchery && u.second.bwapi_unit_->isMorphing())) { // if the unit is morphing into a hatchery for the first time, don't count it as a base.
                for (auto expo : CUNYAIModule::current_map_inventory.expo_tilepositions_) {
                    if (u.second.bwapi_unit_->getTilePosition() == expo)
                        baseMap_.insert({ u.second.pos_, Base(u.first) });
                }
            }
    }

    if (baseMap_.empty()) {
        return;
    }

    Unit_Inventory alarming_enemy_ground = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::current_map_inventory.enemy_base_ground_);
    Unit_Inventory alarming_enemy_air = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::current_map_inventory.enemy_base_air_);

    alarming_enemy_ground.updateUnitInventorySummary();
    alarming_enemy_air.updateUnitInventorySummary();

    set<int> distance_to_alarming_ground;
    set<int> distance_to_alarming_air;

    for (auto & b : baseMap_) {
        b.second.distance_to_ground_ = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(b.first);
        distance_to_alarming_ground.insert(b.second.distance_to_ground_);
        b.second.distance_to_air_ = static_cast<int>(b.first.getDistance(CUNYAIModule::current_map_inventory.enemy_base_air_));
        distance_to_alarming_air.insert(b.second.distance_to_air_);
    }

    for (auto & b : baseMap_) {
        auto e_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, b.first);
        auto u_loc = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, b.first);
        e_loc.updateUnitInventorySummary();
        u_loc.updateUnitInventorySummary();

        Mobility base_mobility = Mobility(b.second.unit_);
        bool they_are_moving_out_ground = false;
        bool they_are_moving_out_air = false;

        if (CUNYAIModule::getClosestGroundStored(alarming_enemy_ground, b.first))
            they_are_moving_out_ground = alarming_enemy_ground.building_count_ == 0 && base_mobility.checkEnemyApproachingUs(*CUNYAIModule::getClosestGroundStored(alarming_enemy_ground, b.first)) || CUNYAIModule::getClosestGroundStored(alarming_enemy_ground, b.first)->pos_.getApproxDistance(b.first) < 500;
        if(CUNYAIModule::getClosestAirStored(alarming_enemy_air, b.first))
            they_are_moving_out_air = alarming_enemy_air.building_count_ == 0 && base_mobility.checkEnemyApproachingUs(*CUNYAIModule::getClosestAirStored(alarming_enemy_air, b.first)) || CUNYAIModule::getClosestAirStored(alarming_enemy_air, b.first)->pos_.getApproxDistance(b.first) < 500;

        bool too_close_by_ground = true;
        if (distance_to_alarming_ground.size() >= 2) {
            std::set<int>::reverse_iterator ground_iter = distance_to_alarming_ground.rbegin();
            too_close_by_ground = b.second.distance_to_ground_ < *std::next(ground_iter) && b.second.distance_to_ground_ < CUNYAIModule::enemy_player_model.units_.max_speed_ * (UnitTypes::Zerg_Creep_Colony.buildTime() + UnitTypes::Zerg_Sunken_Colony.buildTime() + 128);
        }

        bool too_close_by_air = true;
        if (distance_to_alarming_air.size() >= 2) {
            std::set<int>::reverse_iterator air_iter = distance_to_alarming_air.rbegin();
            too_close_by_air = b.second.distance_to_air_ <= *std::next(air_iter) && b.second.distance_to_ground_ < CUNYAIModule::enemy_player_model.units_.max_speed_ * (UnitTypes::Zerg_Creep_Colony.buildTime() + UnitTypes::Zerg_Spore_Colony.buildTime() + 128);
        }

        b.second.sunken_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Sunken_Colony, u_loc);
        b.second.spore_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Spore_Colony, u_loc);
        b.second.creep_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony, u_loc);
        b.second.overlords_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Overlord, u_loc);

        b.second.mineral_gatherers_ = u_loc.count_of_each_phase_.at(Stored_Unit::Phase::MiningMin);
        b.second.gas_gatherers_ = u_loc.count_of_each_phase_.at(Stored_Unit::Phase::MiningGas);
        b.second.returners_ = u_loc.count_of_each_phase_.at(Stored_Unit::Phase::Returning);

        bool can_upgrade_spore = CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0; // There is a building complete that will allow either creep colony upgrade.
        bool can_upgrade_sunken = (CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0);
        bool can_upgrade_colonies = can_upgrade_spore || can_upgrade_sunken;
        bool getting_hit_ground = (e_loc.worker_count_ > 1 || e_loc.building_count_ > 0 || e_loc.stock_ground_units_ > 0);
        bool getting_hit_air = (e_loc.stock_fliers_ > 0);
        bool on_one_base = baseMap_.size() - CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Hatchery) <= 1;
        b.second.emergency_sunken_ = CUNYAIModule::assemblymanager.canMakeCUNY(UnitTypes::Zerg_Creep_Colony, false) && (too_close_by_ground && (getting_hit_ground || they_are_moving_out_ground)) && can_upgrade_sunken;
        b.second.emergency_spore_ = CUNYAIModule::assemblymanager.canMakeCUNY(UnitTypes::Zerg_Creep_Colony, false) && (too_close_by_air && (getting_hit_air || they_are_moving_out_air)) && can_upgrade_spore;
        
        if (b.second.emergency_sunken_) {
            Stored_Unit * drone = CUNYAIModule::getClosestStored(u_loc, Broodwar->self()->getRace().getWorker(), b.first, 999999);
            if (drone && drone->bwapi_unit_ && CUNYAIModule::spamGuard(drone->bwapi_unit_)) {
                CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone->bwapi_unit_, CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony) * 50 <= CUNYAIModule::my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
                    b.second.sunken_count_ + b.second.creep_count_ < 6, b.second.unit_->getTilePosition()); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
            }
        }
        if (b.second.emergency_spore_) {
            Stored_Unit * drone = CUNYAIModule::getClosestStored(u_loc, Broodwar->self()->getRace().getWorker(), b.first, 999999);
            if (drone && drone->bwapi_unit_ && CUNYAIModule::spamGuard(drone->bwapi_unit_)) {
                CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone->bwapi_unit_, CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony) * 50 <= CUNYAIModule::my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
                    b.second.sunken_count_ + b.second.creep_count_ < 6, b.second.unit_->getTilePosition()); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
            }
        }

        Unit_Inventory creep_colonies = CUNYAIModule::getUnitInventoryInArea(u_loc, UnitTypes::Zerg_Creep_Colony, b.first);
        for (auto u : creep_colonies.unit_map_) {
            if (u.first) AssemblyManager::buildStaticDefence(u.first, b.second.emergency_spore_, b.second.emergency_sunken_);
        }
    }

    displayBaseData();
}

void BaseManager::displayBaseData()
{
    if (DIAGNOSTIC_MODE) {
        for (auto b : baseMap_) {
            Broodwar->drawTextMap(b.first + Position(5, -40), "Gasers: %d", b.second.gas_gatherers_);
            Broodwar->drawTextMap(b.first + Position(5, -30), "Miners: %d", b.second.mineral_gatherers_);
            Broodwar->drawTextMap(b.first + Position(5, -20), "Returners: %d", b.second.returners_);
            Broodwar->drawTextMap(b.first + Position(5, -10), "Sunkens: %d", b.second.sunken_count_);
            Broodwar->drawTextMap(b.first + Position(5, -0), "Spores: %d", b.second.spore_count_);
            Broodwar->drawTextMap(b.first + Position(5, 10), "Creeps: %d", b.second.creep_count_);
            Broodwar->drawTextMap(b.first + Position(5, 20), "Overlords: %d", b.second.overlords_);
            Broodwar->drawTextMap(b.first + Position(5, 30), "Ground Dist: %d", b.second.distance_to_ground_);
            Broodwar->drawTextMap(b.first + Position(5, 40), "Air Dist: %d", b.second.distance_to_air_);
        }
    }
}

Base BaseManager::getClosestBaseGround(const Position & pos)
{
    int minDist = INT_MAX;
    Position basePos = Positions::Origin;
    Base nullBase;
    for (auto & b : baseMap_) {
        int currentDist = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(b.first);
        if (currentDist < minDist) {
             minDist = currentDist;
             basePos = b.first;
        }
    }
    if (basePos != Positions::Origin)
        return baseMap_.at(basePos);
    else
        return nullBase;
}

Base BaseManager::getClosestBaseAir(const Position & pos)
{
    int minDist = INT_MAX;
    Position basePos = Positions::Origin;
    Base nullBase;
    for (auto & b : baseMap_) {
        int currentDist = static_cast<int>(b.first.getDistance(CUNYAIModule::current_map_inventory.enemy_base_air_));
        if (currentDist < minDist) {
            minDist = currentDist;
            basePos = b.first;
        }
    }
    if (basePos != Positions::Origin)
        return baseMap_.at(basePos);
    else
        return nullBase;
}
