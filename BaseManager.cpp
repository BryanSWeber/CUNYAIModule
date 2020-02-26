#include <BWAPI.h>
#include "Source/CUNYAIModule.h"
#include "Source/Diagnostics.h"
#include "Source/AssemblyManager.h"
#include "Source/MobilityManager.h"

Base::Base() {
    //gas_taken_ = false;
    //gas_tolerable_ = false;
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
    //gas_taken_ = false;
    //gas_tolerable_ = false;
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

int BaseManager::getInactiveBaseCount(const int minimum_workers)
{
    int inactive_bases = 0;

    // if it has less than Y patches or less than X gatherers, it's inactive.
    if (!baseMap_.empty()) {
        for (auto base : baseMap_) {
            if (base.second.mineral_gatherers_ + base.second.gas_gatherers_ + base.second.returners_ < minimum_workers && base.second.gas_geysers_ * 3 + base.second.mineral_patches_ * 2 > minimum_workers)
                inactive_bases++;
        }
    }

    // if the unit is morphing into a hatchery for the first time, it's an inactive base.
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.bwapi_unit_ && u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery))
            if ((u.second.bwapi_unit_->getBuildType() == UnitTypes::Zerg_Hatchery && u.second.bwapi_unit_->isMorphing())) { 
                inactive_bases++;
            }
    }

    return inactive_bases;
}

int BaseManager::getLoadedBaseCount(const int maximum_workers)
{
    int loaded_bases = 0;

    // if it has less than Y patches or less than X gatherers, it's inactive.
    if (!baseMap_.empty()) {
        for (auto base : baseMap_) {
            if (base.second.mineral_gatherers_ + base.second.gas_gatherers_ + base.second.returners_ > maximum_workers && base.second.gas_geysers_ * 3 + base.second.mineral_patches_ * 2 > maximum_workers)
                loaded_bases++;
        }
    }

    return loaded_bases;
}

void BaseManager::updateBases()
{
    int enemy_unit_count_ = 0;

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
        auto r_loc = CUNYAIModule::getResourceInventoryInArea(CUNYAIModule::land_inventory, b.first);

        r_loc.updateMines();
        e_loc.updateUnitInventorySummary();
        u_loc.updateUnitInventorySummary();

        enemy_unit_count_ += e_loc.ground_count_;

        if (enemy_unit_count_ >= 2 && !CUNYAIModule::buildorder.ever_clear_) {
            CUNYAIModule::buildorder.clearRemainingBuildOrder(false);
            Diagnostics::DiagnosticText("Clearing Build order since there are %d baddies nearby.", enemy_unit_count_);
        }


        Mobility base_mobility = Mobility(b.second.unit_);
        bool they_are_moving_out_ground = false;
        bool they_are_moving_out_air = false;

        if (CUNYAIModule::getClosestGroundStored(alarming_enemy_ground, b.first))
            they_are_moving_out_ground = alarming_enemy_ground.building_count_ == 0 || CUNYAIModule::getClosestGroundStored(alarming_enemy_ground, b.first)->pos_.getApproxDistance(b.first) < 500;
        if(CUNYAIModule::getClosestAirStored(alarming_enemy_air, b.first))
            they_are_moving_out_air = alarming_enemy_air.building_count_ == 0 || CUNYAIModule::getClosestAirStored(alarming_enemy_air, b.first)->pos_.getApproxDistance(b.first) < 500;

        bool too_close_by_ground = false;
        if (distance_to_alarming_ground.size() >= 2 || b.second.distance_to_ground_ < 640 ) {
            std::set<int>::reverse_iterator ground_iter = distance_to_alarming_ground.rbegin();
            too_close_by_ground = b.second.distance_to_ground_ <= *std::next(ground_iter) && !b.second.checkHasGroundBuffer(CUNYAIModule::current_map_inventory.enemy_base_ground_); // if it is exposed and does not have a ground buffer, build sunkens for it.
        }

        bool too_close_by_air = false;
        if (distance_to_alarming_air.size() >= 2) {
            std::set<int>::reverse_iterator air_iter = distance_to_alarming_air.rbegin();
            too_close_by_air = b.second.distance_to_air_ <= *std::next(air_iter);
        }

        b.second.sunken_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Sunken_Colony, u_loc);
        b.second.spore_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Spore_Colony, u_loc);
        b.second.creep_count_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony, u_loc);
        b.second.overlords_ = CUNYAIModule::countUnits(UnitTypes::Zerg_Overlord, u_loc);

        b.second.mineral_gatherers_ = u_loc.count_of_each_phase_.at(StoredUnit::Phase::MiningMin);
        b.second.gas_gatherers_ = u_loc.count_of_each_phase_.at(StoredUnit::Phase::MiningGas);
        b.second.returners_ = u_loc.count_of_each_phase_.at(StoredUnit::Phase::Returning);
        b.second.mineral_patches_ = r_loc.getLocalMinPatches();
        b.second.gas_geysers_ = r_loc.getLocalRefineries();

        bool can_upgrade_spore = CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0; // There is a building complete that will allow either creep colony upgrade.
        bool can_upgrade_sunken = (CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0);
        bool can_upgrade_colonies = can_upgrade_spore || can_upgrade_sunken;
        bool getting_hit_ground = (e_loc.worker_count_ > 1 || e_loc.building_count_ > 0 || e_loc.stock_ground_units_ > 0);
        bool getting_hit_air = (e_loc.stock_fliers_ > 0);
        bool on_one_base = baseMap_.size() - CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Hatchery) <= 1;
        b.second.emergency_sunken_ = CUNYAIModule::assemblymanager.canMakeCUNY(UnitTypes::Zerg_Creep_Colony, false) && (too_close_by_ground && (getting_hit_ground || they_are_moving_out_ground)) && can_upgrade_sunken && b.second.sunken_count_ <= max(alarming_enemy_ground.ground_count_,2);
        b.second.emergency_spore_ = CUNYAIModule::assemblymanager.canMakeCUNY(UnitTypes::Zerg_Creep_Colony, false) && (too_close_by_air && (getting_hit_air || they_are_moving_out_air)) && can_upgrade_spore && b.second.spore_count_ <= max(alarming_enemy_air.flyer_count_,2);
        
        if (b.second.emergency_sunken_ && Broodwar->getFrameCount() % 24 == 0) {
            StoredUnit * drone = CUNYAIModule::getClosestStoredAvailable(u_loc, Broodwar->self()->getRace().getWorker(), b.first, 999999);
            if (drone && drone->bwapi_unit_ && CUNYAIModule::spamGuard(drone->bwapi_unit_)) {
                CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone->bwapi_unit_, CUNYAIModule::countUnits(UnitTypes::Zerg_Creep_Colony) * 50 <= CUNYAIModule::my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
                    b.second.sunken_count_ + b.second.creep_count_ < 6, b.second.unit_->getTilePosition()); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
            }
        }
        if (b.second.emergency_spore_ && Broodwar->getFrameCount() % 24 == 0) {
            StoredUnit * drone = CUNYAIModule::getClosestStoredAvailable(u_loc, Broodwar->self()->getRace().getWorker(), b.first, 999999);
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
            Broodwar->drawTextMap(b.first + Position(5, -40), "Gasers: %d / %d", b.second.gas_gatherers_, b.second.gas_geysers_ * 3);
            Broodwar->drawTextMap(b.first + Position(5, -30), "Miners: %d / %d", b.second.mineral_gatherers_, b.second.mineral_patches_ * 2);
            Broodwar->drawTextMap(b.first + Position(5, -20), "Returners: %d", b.second.returners_);
            Broodwar->drawTextMap(b.first + Position(5, -10), "Sunkens: %d", b.second.sunken_count_);
            Broodwar->drawTextMap(b.first + Position(5, -0), "Spores: %d", b.second.spore_count_);
            Broodwar->drawTextMap(b.first + Position(5, 10), "Creeps: %d", b.second.creep_count_);
            Broodwar->drawTextMap(b.first + Position(5, 20), "Overlords: %d", b.second.overlords_);
            Broodwar->drawTextMap(b.first + Position(5, 30), "Ground Dist: %d", b.second.distance_to_ground_);
            Broodwar->drawTextMap(b.first + Position(5, 40), "Air Dist: %d", b.second.distance_to_air_);
            Broodwar->drawTextMap(b.first + Position(5, 60), "Sunkens: %s", b.second.emergency_sunken_ ? "TRUE":"FALSE" );
            Broodwar->drawTextMap(b.first + Position(5, 70), "Spores: %s", b.second.emergency_spore_ ? "TRUE" : "FALSE");
        }
    }
}

Base BaseManager::getClosestBaseGround(const Position & pos)
{
    int minDist = INT_MAX;
    Position basePos = Positions::Origin;
    Base nullBase;
    for (auto & b : baseMap_) {
        int currentDist = 0;
        auto cpp = BWEM::Map::Instance().GetPath(pos, b.first, &currentDist);
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
        int currentDist = pos.getDistance(Position(b.first));
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

bool Base::checkHasGroundBuffer(const Position& threat_pos)
{
    int plength = 0;
    bool unit_sent = false;
    auto cpp = BWEM::Map::Instance().GetPath(threat_pos, unit_->getPosition(), &plength);

    if (!cpp.empty()) { // if there's an actual path to follow...
        for (auto choke_point : cpp) {
            BWEM::Area area = *choke_point->GetAreas().first;
            BWEM::Area area2 = *choke_point->GetAreas().second;
            for (auto b : CUNYAIModule::basemanager.getBases()) { // if another base blocks the path.
                if (b.second.unit_ == this->unit_) continue; // The same base cannot block the path.
                int id = BWEM::Map::Instance().GetNearestArea(b.second.unit_->getTilePosition())->Id();
                if ((area.Id() == id) || (area2.Id() == id)) return true; // return true if it is being blocked.
            }
        }
        return false; // If there's a cpp and everything is unblocked, there is no ground buffer, and it's not safe.
    }

    if (plength == 0)
        return true; // there is no path, so it's safe!
    else
        return false;// They are literally on us right now.
}
