#pragma once
#include "Source\CUNYAIModule.h"
#include "Source\MapInventory.h"
#include "Source\AssemblyManager.h"
#include "Source\UnitInventory.h"
#include "Source\MobilityManager.h"
#include "Source\Diagnostics.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\PlayerModelManager.h" // needed for cartidges.
#include "Source\BWEB\BWEB.h"
#include "Source\Build.h"
#include <bwem.h>
#include <iterator>
#include <numeric>
#include <fstream>


using namespace BWAPI;
using namespace Filter;
using namespace std;

UnitInventory AssemblyManager::larvaBank_;
UnitInventory AssemblyManager::hydraBank_;
UnitInventory AssemblyManager::mutaBank_;
UnitInventory AssemblyManager::builderBank_;
UnitInventory AssemblyManager::creepColonyBank_;
UnitInventory AssemblyManager::productionFacilityBank_;

bool AssemblyManager::subgoalEcon_ = false;
bool AssemblyManager::subgoalArmy_ = false;

std::map<UnitType, int> AssemblyManager::assemblyCycle_ = PlayerModel::getCombatUnitCartridge();

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations. Now updates the unit within the UnitInventory directly.
bool AssemblyManager::Check_N_Build(const UnitType &building, const Unit &unit, const bool &extra_critera, const TilePosition &tp)
{
    Position unitCenter = CUNYAIModule::getUnitCenter(unit);

    map<int, TilePosition> viable_placements = {};
    int max_travel_distance = INT_MAX; // No more max travel distance.

    if (!CUNYAIModule::checkWilling(building, extra_critera)) // If you're willing to build it let's begin the calculations for it.
        return false;

    //morphing hatcheries into lairs & hives, spires into greater spires, creep colonies into sunkens or spores
    if (unit->getType().isBuilding() && CUNYAIModule::checkWillingAndAble(unit, building, extra_critera) && unit->morph(building)) {
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->updateRemainingBuildOrder(building); // Remove from reserve systems.
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->announceBuildingAttempt(building);
            return CUNYAIModule::updateUnitBuildIntent(unit, building, unit->getTilePosition());
    }
    else if (canMakeCUNY(building, false, unit) && building == UnitTypes::Zerg_Creep_Colony) { // creep colony loop specifically. Checks willing and able within loop.

        //Do the nearest wall if it is for ground.
        map<int, TilePosition> wall_spots = addClosestWall(building, unit->getTilePosition());
        if (!wall_spots.empty())
            viable_placements.insert(wall_spots.begin(), wall_spots.end());
        if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, max_travel_distance))
            return true;

        //simply attempt the nearest station if the previous did not find.
        map<int, TilePosition> station_spots = addClosestStation(building, unit->getTilePosition());
        if (!station_spots.empty())
            viable_placements.insert(station_spots.begin(), station_spots.end());
        if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, max_travel_distance))
            return true;

    }
    else if (canMakeCUNY(building, false, unit) && building == UnitTypes::Zerg_Extractor) {
        Stored_Resource* closest_gas = CUNYAIModule::getClosestStoredByGround(CUNYAIModule::land_inventory, unitCenter, 9999999, UnitTypes::Resource_Vespene_Geyser);
        if (closest_gas && closest_gas->occupied_resource_ && closest_gas->bwapi_unit_){
            TilePosition tile = Broodwar->getBuildLocation(building, TilePosition(closest_gas->pos_), 5);
            if (CUNYAIModule::checkWillingAndAble(unit, building, extra_critera) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) {  // does not require an isplacable check because it won't pass such a check. It's on top of another object, the geyser.
                CUNYAIModule::CUNYAIModule::learnedPlan.inspectCurrentBuild().announceBuildingAttempt(building);
                unit->stop();
                return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
            } //extractors must have buildings nearby or we shouldn't build them.
        }
    }
    else if (canMakeCUNY(building, false, unit) && building == UnitTypes::Zerg_Hatchery) {

        //int base_walk = 0;
        //auto cpp = BWEM::Map::Instance().GetPath(BWEB::Map::getMainPosition(), BWEB::Map::getNaturalPosition(), &base_walk);

        //walls are catagorically better than macro hatches.
        map<int, TilePosition> wall_spots = addClosestWall(building, unit->getTilePosition());
        if (!wall_spots.empty() && CUNYAIModule::basemanager.getBaseCount() >= 2) // don't build at the wall if you have 1 base.
            viable_placements.insert(wall_spots.begin(), wall_spots.end());
        if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, 96))
            return true;

        map<int, TilePosition> block_spots = addClosestBlockWithSizeOrLargerWithinWall(building, unit->getTilePosition());
        if (!block_spots.empty())
            viable_placements.insert(block_spots.begin(), block_spots.end());
        if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, 96))
            return true;

    }
    else if (canMakeCUNY(building, false, unit) && !building.whatBuilds().first.isBuilding() ) { // We do not want to have drones reserving hives or greater spire locations anywhere. Hatcheries are specially built above.
            // We want walls first before anywhere else.
            map<int, TilePosition> wall_spots = addClosestWall(building, unit->getTilePosition());
            if (!wall_spots.empty())
                viable_placements.insert(wall_spots.begin(), wall_spots.end());
            if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, max_travel_distance))
                return true;

            // Then try a block,
            map<int, TilePosition> block_spots = addClosestBlockWithSizeOrLargerWithinWall(building, unit->getTilePosition());
            if (!block_spots.empty())
                viable_placements.insert(block_spots.begin(), block_spots.end());
            if (buildAtNearestPlacement(building, viable_placements, unit, extra_critera, max_travel_distance))
                return true;
    }

    return false;
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.  Updates CUNYAIModule::friendly_player_model.units_.
bool AssemblyManager::Check_N_Grow(const UnitType &unittype, const Unit &larva, const bool &extra_critera)
{

    if (CUNYAIModule::checkWillingAndAble(larva, unittype, extra_critera))
    {
        if (larva->morph(unittype)) {
            CUNYAIModule::updateUnitPhase(larva, StoredUnit::Phase::Morphing);
            return true;
        }
    }

    return false;
}

//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations. Now Updates Friendly inventory when command is sent.
bool AssemblyManager::Expo(const Unit &unit, const bool &extra_critera, MapInventory &inv) {

        TilePosition base_expo = TilePositions::Origin; // if we find no replacement position, we will know this null postion is never a good build canidate.

        Mobility drone_pathing_options = Mobility(unit);

        int worker_areaID = BWEM::Map::Instance().GetNearestArea(unit->getTilePosition())->Id();
        BWEB::Path newPath;

        bool safe_worker = CUNYAIModule::enemy_player_model.units_.getCombatInventoryAtArea(worker_areaID).unit_map_.empty();
        bool safe_path_available_or_needed, can_afford_with_travel = false;

        // Let's build at the safest close canidate position.
        if (safe_worker) {
            safe_path_available_or_needed = drone_pathing_options.checkSafeEscapePath(Position(getExpoPosition())) || CUNYAIModule::basemanager.getBaseCount() < 2;
            int travel_distance = min(static_cast<int>(getUnbuiltSpaceGroundDistance(unit->getPosition(), Position(getExpoPosition()))), getMaxTravelDistance());
            can_afford_with_travel = CUNYAIModule::checkWillingAndAble(unit, Broodwar->self()->getRace().getResourceDepot(), extra_critera, travel_distance); // cap travel distance for expo reservation funds.
            if (safe_path_available_or_needed && can_afford_with_travel)
                base_expo = getExpoPosition();
        }
        else {
            return false;  // If there's nothing, give up.
        }

        // If we found a viable base.
        if (base_expo != TilePositions::Origin) {
            if (CUNYAIModule::my_reservation.addReserveSystem(base_expo, Broodwar->self()->getRace().getResourceDepot())) {
                CUNYAIModule::CUNYAIModule::learnedPlan.inspectCurrentBuild().announceBuildingAttempt(Broodwar->self()->getRace().getResourceDepot());
                return CUNYAIModule::updateUnitBuildIntent(unit, Broodwar->self()->getRace().getResourceDepot(), base_expo);
            }
        }
    return false;
}

//Creates a new building with DRONE. Does not create units that morph from other buildings: Lairs, Hives, Greater Spires, or sunken/spores.
bool AssemblyManager::buildBuilding(const Unit &drone) {
    // will send it to do the LAST thing on this list that it can build.

    bool buildings_started = false; // We will go through each possible building in order, and if this is TRUE we've done something.
    bool canDumpLings = CUNYAIModule::workermanager.getMinWorkers() / 5 >= CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Hatchery, CUNYAIModule::friendly_player_model.units_); //True if you have enough income to pump lings from all hatcheries continually.
    bool distance_mining = CUNYAIModule::workermanager.getDistanceWorkers() + CUNYAIModule::workermanager.getOverstackedWorkers() > 0; // 1/16 workers LD mining is too much.
    bool macro_hatch_timings = (CUNYAIModule::basemanager.getBaseCount() == 3 && CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Hatchery, CUNYAIModule::friendly_player_model.units_) <= 5) || 
                               (CUNYAIModule::basemanager.getBaseCount() == 4 && CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Hatchery, CUNYAIModule::friendly_player_model.units_) <= 7);
    bool one_tech_per_base = CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + CUNYAIModule::countUnits(UnitTypes::Zerg_Spire) + CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire) + CUNYAIModule::countUnits(UnitTypes::Zerg_Ultralisk_Cavern) < CUNYAIModule::basemanager.getBaseCount();
    bool prefer_hydra_den_over_spire = max(returnUnitRank(UnitTypes::Zerg_Lurker), returnUnitRank(UnitTypes::Zerg_Hydralisk)) >= max({ returnUnitRank(UnitTypes::Zerg_Mutalisk), returnUnitRank(UnitTypes::Zerg_Scourge), returnUnitRank(UnitTypes::Zerg_Zergling) }) ||
        CUNYAIModule::enemy_player_model.units_.detector_count_ + CUNYAIModule::enemy_player_model.casualties_.detector_count_ == 0;
    int number_of_evos_wanted =
        static_cast<int>(TechManager::returnUpgradeRank(UpgradeTypes::Zerg_Carapace) > TechManager::returnUpgradeRank(UpgradeTypes::None)) +
        static_cast<int>(TechManager::returnUpgradeRank(UpgradeTypes::Zerg_Melee_Attacks) > TechManager::returnUpgradeRank(UpgradeTypes::None)) +
        static_cast<int>(TechManager::returnUpgradeRank(UpgradeTypes::Zerg_Missile_Attacks) > TechManager::returnUpgradeRank(UpgradeTypes::None));
    int number_of_spires_wanted =
        static_cast<int>(TechManager::returnUpgradeRank(UpgradeTypes::Zerg_Flyer_Carapace) > TechManager::returnUpgradeRank(UpgradeTypes::None)) +
        static_cast<int>(TechManager::returnUpgradeRank(UpgradeTypes::Zerg_Flyer_Attacks) > TechManager::returnUpgradeRank(UpgradeTypes::None));
    int count_of_spire_decendents = CUNYAIModule::countUnits(UnitTypes::Zerg_Spire, true) + CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire);
    int count_tech_buildings = CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber, true) + CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den, true) + CUNYAIModule::countUnits(UnitTypes::Zerg_Spire, true) + CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire, true) + CUNYAIModule::countUnits(UnitTypes::Zerg_Ultralisk_Cavern, true);

    UnitInventory e_loc;
    UnitInventory u_loc;

    // Trust the build order. If there is a build order and it wants a building, build it!
    if (!CUNYAIModule::learnedPlan.inspectCurrentBuild().isEmptyBuildOrder()) {
        UnitType next_in_build_order = CUNYAIModule::learnedPlan.inspectCurrentBuild().getNext().getUnit();
        if (!next_in_build_order.isBuilding()) return false;
        if (next_in_build_order == UnitTypes::Zerg_Hatchery) buildings_started = Expo(drone, false, CUNYAIModule::currentMapInventory);
        else buildings_started = Check_N_Build(next_in_build_order, drone, false);
    }

    ////Combat Buildings are now done on assignUnitAssembly

    //Macro-related Buildings.
    bool bases_are_active = CUNYAIModule::basemanager.getInactiveBaseCount(3) + CUNYAIModule::my_reservation.isBuildingInReserveSystem(Broodwar->self()->getRace().getResourceDepot()) < 1;
    bool less_bases_than_enemy = CUNYAIModule::basemanager.getBaseCount() < 2 + CUNYAIModule::countUnits(CUNYAIModule::enemy_player_model.getPlayer()->getRace().getResourceDepot(), CUNYAIModule::enemy_player_model.units_);
    if (!buildings_started) buildings_started = Expo(drone, canDumpLings &&
                                                            (less_bases_than_enemy || (distance_mining || CUNYAIModule::econ_starved || !checkSlackLarvae() || CUNYAIModule::basemanager.getLoadedBaseCount(8) > 1) && !macro_hatch_timings), CUNYAIModule::currentMapInventory);
    //buildings_started = expansion_meaningful; // stop if you need an expo!

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hatchery, drone, canDumpLings && (!checkSlackLarvae() || macro_hatch_timings) ); // only macrohatch if you are short on larvae and floating a lot.

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Extractor, drone,
        !CUNYAIModule::workermanager.checkExcessGasCapacity() && CUNYAIModule::gas_starved &&
        CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Extractor) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.  Note that gas_workers count may be off. Sometimes units are in the gas geyser.


    //First Building needed!
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool, true) == 0 && CUNYAIModule::friendly_player_model.units_.resource_depot_count_ > 0);

    //Consider an organized build plan.
    if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && (CUNYAIModule::enemy_player_model.units_.flyer_count_ > 0 || CUNYAIModule::enemy_player_model.getEstimatedUnseenFliers() > 0)) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not).
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, CUNYAIModule::tech_starved &&
            CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber, true) == 0 &&
            CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool, true) > 0 &&
            CUNYAIModule::basemanager.getBaseCount() > 1);
    }

    //Muta or lurker for main body of units. Short-circuit for lurkers if they have no detection. Build both if hive is present.
    if (prefer_hydra_den_over_spire || CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) > 0) { 
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, CUNYAIModule::tech_starved && one_tech_per_base &&
            CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den, true) == 0 &&
            CUNYAIModule::basemanager.getBaseCount() >= 2);
    }

    if (!prefer_hydra_den_over_spire || CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) > 0){
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, CUNYAIModule::tech_starved && one_tech_per_base &&
            CUNYAIModule::countUnits(UnitTypes::Zerg_Spire, true) == 0 && CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire, true) == 0 &&
            CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
            CUNYAIModule::basemanager.getBaseCount() >= 2);
    }

    //For getting to hive: See techmanager for hive, it will trigger after queens nest is built!
    if (returnUnitRank(UnitTypes::Zerg_Guardian) == 0 || returnUnitRank(UnitTypes::Zerg_Devourer) == 0 || returnUnitRank(UnitTypes::Zerg_Ultralisk) == 0) {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, CUNYAIModule::tech_starved &&
            CUNYAIModule::countUnits(UnitTypes::Zerg_Queens_Nest, true) == 0 &&
            CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
            (CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den) > 0 || CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) > 0) && // need spire or hydra to tech beyond lair please.
            CUNYAIModule::basemanager.getBaseCount() >= 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.
    }

    //For your capstone tech:
    for (auto r : { 0,1,2,3,4,5 }) {
        if (returnUnitRank(UnitTypes::Zerg_Ultralisk) == r) {
            if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Ultralisk_Cavern, drone, CUNYAIModule::tech_starved && one_tech_per_base &&
                CUNYAIModule::countUnits(UnitTypes::Zerg_Ultralisk_Cavern, true) == 0 &&
                CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) >= 0 &&
                CUNYAIModule::basemanager.getBaseCount() >= 3);
        }
        if (returnUnitRank(UnitTypes::Zerg_Guardian) == r || returnUnitRank(UnitTypes::Zerg_Devourer) == r || returnUnitRank(UnitTypes::Zerg_Mutalisk) == r) {
            if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, CUNYAIModule::tech_starved && one_tech_per_base &&
                CUNYAIModule::countUnits(UnitTypes::Zerg_Spire, true) == 0 &&
                CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) == 0 &&
                CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
                CUNYAIModule::basemanager.getBaseCount() >= 3);
        }
    }

    // Always:
    bool upgrade_worth_melee = CUNYAIModule::friendly_player_model.units_.ground_melee_count_ > (100 + 100 * 1.25) / StoredUnit(UnitTypes::Zerg_Zergling).stock_value_ && CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool) > 0; // first upgrade is +1
    bool upgrade_worth_ranged = CUNYAIModule::friendly_player_model.units_.ground_range_count_ > (100 + 100 * 1.25) / StoredUnit(UnitTypes::Zerg_Hydralisk).stock_value_ && CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den) > 0; // first upgrade is +1

    // For extra upgrades:
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, CUNYAIModule::tech_starved &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber, true) < number_of_evos_wanted &&
        CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        CUNYAIModule::countUnitsAvailable(UnitTypes::Zerg_Evolution_Chamber) == 0 &&
        (upgrade_worth_melee || upgrade_worth_ranged) &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Extractor) > count_tech_buildings &&
        count_tech_buildings >= 1 &&
        CUNYAIModule::basemanager.getBaseCount() > count_tech_buildings);

    //if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool &&
    //    count_of_spire_decendents < number_of_spires_wanted &&
    //    Broodwar->self()->gas() > 100 * count_of_spire_decendents &&
    //    Broodwar->self()->minerals() > 100 * count_of_spire_decendents &&
    //    CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
    //    (!have_idle_spires_ && count_of_spire_decendents >= 1) &&
    //    CUNYAIModule::countUnits(UnitTypes::Zerg_Spawning_Pool) > 0 &&
    //    CUNYAIModule::basemanager.getBaseCount() > count_tech_buildings);

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, CUNYAIModule::tech_starved &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Queens_Nest, true) == 0 &&
        CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Evolution_Chamber, true) > 0 &&
        (CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk_Den) > 0 || CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) > 0) && // need spire or hydra to tech beyond lair please.
        CUNYAIModule::basemanager.getBaseCount() >= 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

    StoredUnit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(drone)->second;
    morphing_unit.updateStoredUnit(drone); // don't give him a phase.

    if (buildings_started) {
        Diagnostics::DiagnosticWrite("Looks like we wanted to build something. Here's the general inputs I was thinking about:");
        Diagnostics::DiagnosticWrite("Distance Mining: %s", distance_mining ? "TRUE" : "FALSE");
        Diagnostics::DiagnosticWrite("Tech Starved: %s", CUNYAIModule::tech_starved ? "TRUE" : "FALSE");
        Diagnostics::DiagnosticWrite("Hydras Preferred: %s", prefer_hydra_den_over_spire ? "TRUE" : "FALSE");
        Diagnostics::DiagnosticWrite("MaxEvos (for ups): %d", number_of_evos_wanted);
        Diagnostics::DiagnosticWrite("MaxSpires (for ups): %d", number_of_spires_wanted);
        Diagnostics::DiagnosticWrite("CountTech buildings: %d, Extractors: %d", count_tech_buildings, CUNYAIModule::countUnits(UnitTypes::Zerg_Extractor));
    }

    return buildings_started;
};

// clears all blocking units in an area with size of UT starting at buildtile tile excluding EXCEPTION_UNIT. 
void AssemblyManager::clearBuildingObstuctions(const UnitType &ut, const TilePosition &tile, const Unit &exception_unit) {
    UnitInventory obstructions = UnitInventory(Broodwar->getUnitsInRectangle(Position(tile), Position(tile) + Position(ut.width(), ut.height())));
    for (auto u = obstructions.unit_map_.begin(); u != obstructions.unit_map_.end() && !obstructions.unit_map_.empty(); u++) {
        if (u->second.bwapi_unit_ && u->second.bwapi_unit_ != exception_unit) {
            u->second.bwapi_unit_->move({ Position(tile).x + (rand() % 200 - 100) * max(ut.tileWidth() + 1, ut.tileHeight() + 1) * 32, Position(tile).y + (rand() % 200 - 100) * max(ut.tileWidth() + 1, ut.tileHeight() + 1) * 32 });
        }
    }
}

bool AssemblyManager::isPlaceableCUNY(const UnitType &type, const TilePosition &location)
{
    //Need to consider placement of Geysers.

    // Modifies BWEB's isPlaceable()
    if (isOccupiedBuildLocation(type, location, true)) // Added true here to reject building on squares that are occupied by enemies.
        return false;

    const auto creepCheck = type.requiresCreep() ? true : false;

    if (creepCheck) {
        for (auto x = location.x; x < location.x + type.tileWidth() ; x++) {
            for (auto y = location.y; y < location.y + type.tileHeight(); y++) { //0,0 is top left.
                const TilePosition creepTile(x,y);
                if (!Broodwar->hasCreep(creepTile))
                    return false;
            }
        }
    }

    const auto psiCheck = type.requiresPsi() ? true : false;

    if (psiCheck) {
        for (auto x = location.x; x < location.x + type.tileWidth(); x++) {
            for (auto y = location.y; y < location.y + type.tileHeight(); y++) { //0,0 is top left.
                const TilePosition psiTile(x, y);
                if (!Broodwar->hasPower(psiTile))
                    return false;
            }
        }
    }

    //if (type.isResourceDepot() && !Broodwar->canBuildHere(location, type)) //Don't need to check if resource depots can be built here anymore, we have BWEB to determine that.
    //    return false;
    
//    for (auto x = location.x; x < location.x + type.tileWidth(); x++) {
//        for (auto y = location.y; y < location.y + type.tileHeight(); y++) {
//            const TilePosition tile(x, y);
//            if (!tile.isValid()
//                || !Broodwar->isBuildable(tile) 
//                || BWEB::Map::isUsed(tile) != UnitTypes::None //Removed so that units (other than buildings) no longer cause this to return FALSE.
//                || !Broodwar->isWalkable(WalkPosition(tile)))
//                return false;
//        }
//    }
    return true;
}

bool AssemblyManager::isOccupiedBuildLocation(const UnitType &type, const TilePosition &location, bool checkEnemy) {
    auto units_in_area = Broodwar->getUnitsInRectangle(Position(location), Position(location) + Position(type.width(), type.height()));
    if (!units_in_area.empty()) {
        for (auto u : units_in_area) {
            if (!u->getType().canMove() || (u->getPlayer()->isEnemy(Broodwar->self()) && checkEnemy))
                return true;
        }
    }
    return false;
}

bool AssemblyManager::isFullyVisibleBuildLocation(const UnitType &type, const TilePosition &location) {
    for (auto x = location.x; x < location.x + type.tileWidth(); x++) {
        for (auto y = location.y; y < location.y + type.tileHeight(); y++) {
            TilePosition tile(x, y);
            if (!BWAPI::Broodwar->isVisible(location))
                return false;
        }
    }
    return true;
}


bool AssemblyManager::buildCombatUnit(const Unit &morph_canidate) {

    //Am I sending this command to a larva or a hydra?
    UnitType u_type = morph_canidate->getType();
    bool is_larva = u_type == UnitTypes::Zerg_Larva;
    bool is_hydra = u_type == UnitTypes::Zerg_Hydralisk;
    bool is_muta = u_type == UnitTypes::Zerg_Mutalisk;
    bool is_building = false;

    if (CUNYAIModule::learnedPlan.inspectCurrentBuild().checkIfNextInBuild(UnitTypes::Zerg_Lurker) && CUNYAIModule::countUnits(UnitTypes::Zerg_Hydralisk) + CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Hydralisk) == 0) {
        CUNYAIModule::learnedPlan.modifyCurrentBuild()->retryBuildOrderElement(UnitTypes::Zerg_Hydralisk); // force in an hydra if
        Diagnostics::DiagnosticWrite("Reactionary Hydralisk. Must have lost one.");
        return true;
    }

    //Let us utilize the combat sim
    if (!CUNYAIModule::learnedPlan.inspectCurrentBuild().isEmptyBuildOrder() || subgoalArmy_ || (CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Zergling) && is_larva) || (CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Lurker) && is_hydra) || (CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Guardian) && is_muta)) {
        is_building = AssemblyManager::reserveOptimalCombatUnit(morph_canidate, assemblyCycle_);
    }

    return is_building;

}

bool AssemblyManager::buildStaticDefence(const Unit &morph_canidate, const bool & force_spore = false, const bool & force_sunken = false) {

    if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Spore_Colony) || force_spore) {
        if (morph_canidate->morph(UnitTypes::Zerg_Spore_Colony)) {
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->updateRemainingBuildOrder(UnitTypes::Zerg_Spore_Colony);
            return true;
        }
        return false;
    }
    else if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Sunken_Colony) || force_sunken) {
        if (morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony)) {
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->updateRemainingBuildOrder(UnitTypes::Zerg_Sunken_Colony);
                return true;
        }
        return false;
    }
    return false;
}

//contains a filter to discard unbuildable sorts of units, then finds the best unit via a series of BuildFAP sim, then builds it. Passes by copy so I can mutilate the values.
bool AssemblyManager::reserveOptimalCombatUnit(const Unit &morph_canidate, map<UnitType, int> combat_types) {
    bool building_optimal_unit = false;
    int best_sim_score = INT_MIN;
    UnitType build_type = UnitTypes::None;

    // Check if unit is even feasible and that the unit does not demand something that we have already reserved for another product.
    auto potential_type = combat_types.begin();
    while (potential_type != combat_types.end()) {
        if (CUNYAIModule::checkWilling(potential_type->first, true) && CUNYAIModule::my_reservation.canReserveWithExcessResource(potential_type->first))
            potential_type++;
        else combat_types.erase(potential_type++);
    }


    if (combat_types.empty()) return false;
    //else if (combat_types.size() == 1) build_type = combat_types.begin()->first;
    else build_type = refineOptimalUnit(combat_types, CUNYAIModule::friendly_player_model.researches_);


    //A catch for prerequisite build units.
    bool morph_into_prerequisite_hydra = CUNYAIModule::checkWilling(UnitTypes::Zerg_Hydralisk, true) && build_type == UnitTypes::Zerg_Lurker && morph_canidate->getType() == UnitTypes::Zerg_Larva;
    bool morph_into_prerequisite_muta = CUNYAIModule::checkWilling(UnitTypes::Zerg_Mutalisk, true) && (build_type == UnitTypes::Zerg_Guardian || build_type == UnitTypes::Zerg_Devourer) && morph_canidate->getType() == UnitTypes::Zerg_Larva;
    if (morph_into_prerequisite_hydra) building_optimal_unit = CUNYAIModule::my_reservation.addReserveSystem(morph_canidate, UnitTypes::Zerg_Hydralisk);
    else if (morph_into_prerequisite_muta) building_optimal_unit = CUNYAIModule::my_reservation.addReserveSystem(morph_canidate, UnitTypes::Zerg_Mutalisk);

    // Build it.
    if (!building_optimal_unit) building_optimal_unit = CUNYAIModule::my_reservation.addReserveSystem(morph_canidate, build_type); // catchall ground units, in case you have a BO that needs to be done.
    if (building_optimal_unit) {
        Diagnostics::DiagnosticWrite("We are choosing to build a: %s", build_type.c_str());
        Diagnostics::DiagnosticWrite("Out of the subset:");
        for (auto s : combat_types) {
            Diagnostics::DiagnosticWrite("%s, score: %d", s.first.c_str(), s.second);
        }
        return true;
    }
    return false;
}

void AssemblyManager::weightUnitSim(const bool & condition, const UnitType & unit, const double & weight)
{
    if (condition)
        if (assemblyCycle_.find(unit) != assemblyCycle_.end())
            assemblyCycle_[unit] += weight;
}

void AssemblyManager::applyWeightsFor(const UnitType & unit)
{
    switch (unit) {
    case UnitTypes::Zerg_Zergling:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Zealot, CUNYAIModule::enemy_player_model.units_) > 3, unit, -50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Zealot, CUNYAIModule::enemy_player_model.units_) > 6, unit, -50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Vulture, CUNYAIModule::enemy_player_model.units_) > 0, unit, -25);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Firebat, CUNYAIModule::enemy_player_model.units_) > 1, unit, -50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Siege_Mode, CUNYAIModule::enemy_player_model.units_) > 4, unit, -25);
        break;
    case UnitTypes::Zerg_Hydralisk:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Goliath, CUNYAIModule::enemy_player_model.units_) > 4, unit, 25);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Siege_Mode, CUNYAIModule::enemy_player_model.units_) + 
                      CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Tank_Mode, CUNYAIModule::enemy_player_model.units_) > 4, unit, -25);
        weightUnitSim(CUNYAIModule::enemy_player_model.getPlayer()->getRace() == Races::Protoss, unit, 50);
        break;
    case UnitTypes::Zerg_Lurker:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Marine, CUNYAIModule::enemy_player_model.units_) +
                      CUNYAIModule::countUnits(UnitTypes::Terran_Medic, CUNYAIModule::enemy_player_model.units_) * 4 +
                      CUNYAIModule::countUnits(UnitTypes::Terran_Firebat, CUNYAIModule::enemy_player_model.units_) * 2 > 5, unit, 75);
        weightUnitSim(CUNYAIModule::enemy_player_model.getPlayer()->getRace() == Races::Protoss, unit, 50);
        break;
    case UnitTypes::Zerg_Mutalisk:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Vulture, CUNYAIModule::enemy_player_model.units_) > 0, unit, 50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Siege_Mode, CUNYAIModule::enemy_player_model.units_) +
                      CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Tank_Mode, CUNYAIModule::enemy_player_model.units_) > 1, unit, 50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Shuttle, CUNYAIModule::enemy_player_model.units_) +
                      CUNYAIModule::countUnits(UnitTypes::Protoss_Reaver, CUNYAIModule::enemy_player_model.units_) > 0, unit, 50);
        break;
    case UnitTypes::Zerg_Devourer:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Carrier, CUNYAIModule::enemy_player_model.units_) > 0, unit, 75);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Photon_Cannon, CUNYAIModule::enemy_player_model.units_) > 5, unit, 75);
        break;
    case UnitTypes::Zerg_Guardian:
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Siege_Tank_Siege_Mode, CUNYAIModule::enemy_player_model.units_) > 0, unit, 50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Terran_Goliath, CUNYAIModule::enemy_player_model.units_) > 0, unit, 50);
        weightUnitSim(CUNYAIModule::countUnits(UnitTypes::Protoss_Photon_Cannon, CUNYAIModule::enemy_player_model.units_) > 5, unit, 75);
        break;
    }
    //Consider all units you have legal ability to build, but weight them as such):
}

bool AssemblyManager::checkNewUnitWithinMaximum(const UnitType &unit)
{
    return maxUnits_.find(unit) != maxUnits_.end() && maxUnits_[unit] > CUNYAIModule::countUnits(unit) + CUNYAIModule::countUnitsInProgress(unit) + CUNYAIModule::my_reservation.countInReserveSystem(unit);
}

map<int, TilePosition> AssemblyManager::addClosestWall(const UnitType &building, const TilePosition &tp)
{
    map<int, TilePosition> viable_placements = {};

    //check walls first
    auto closest_wall = BWEB::Walls::getClosestWall(tp);
    if (closest_wall) {
        set<TilePosition> placements;
        if (building == UnitTypes::Zerg_Creep_Colony)
            placements = closest_wall->getDefenses();
        else if (building.tileSize() == TilePosition{ 2,2 })
            placements = closest_wall->getSmallTiles();
        else if (building.tileSize() == TilePosition{ 3 , 2 })
            placements = closest_wall->getMediumTiles();
        else if (building.tileSize() == TilePosition{ 4 , 3 })
            placements = closest_wall->getLargeTiles();
        if (!placements.empty()) {
            for (auto &tile : placements) {
                int walkDist = getUnbuiltSpaceGroundDistance(Position(tp), Position(tile));
                if (walkDist > 0)
                    viable_placements.insert({ walkDist, tile });
            }
        }
    }
    return viable_placements;
}

map<int, TilePosition> AssemblyManager::addClosestBlockWithSizeOrLargerWithinWall(const UnitType & building, const TilePosition & tp)
{

    map<int, TilePosition> viable_placements = {};
    int baseToWallDist = 0;
    BWEB::Wall* wall = BWEB::Walls::getClosestWall(Broodwar->self()->getStartLocation());
    Position start_pos = Position(Broodwar->self()->getStartLocation());

    if (wall) {
        Position wall_pos = Position(wall->getCentroid());
        baseToWallDist = getBuildingRelatedGroundDistance(wall_pos, start_pos);
        if (baseToWallDist == 0) {
            Diagnostics::DiagnosticWrite("I can't figure out how far the main is from the wall.");
            return viable_placements;
        }
    }
    else
        baseToWallDist = INT_MAX;

    set<TilePosition> placements;
    set<TilePosition> backup_placements;
    // Get each block
    for (auto block : BWEB::Blocks::getBlocks()) {
        //For each block get all placements
        if (building.tileSize() == TilePosition{ 2,2 }) {
            placements.insert(block.getSmallTiles().begin(), block.getSmallTiles().end());
            backup_placements.insert(block.getLargeTiles().begin(), block.getLargeTiles().end()); // cannot insert because there is no proper operators, particularly (==) for these blocks. Need backup positions if a medium doe snot exist.
        }
        else if (building.tileSize() == TilePosition{ 3 , 2 }) { // allow to build medium tile at large blocks that are not walls. If you need a building, you *need* it.
            placements.insert(block.getMediumTiles().begin(), block.getMediumTiles().end());
            backup_placements.insert(block.getLargeTiles().begin(), block.getLargeTiles().end()); // cannot insert because there is no proper operators, particularly (==) for these blocks.
        }
        else if (building.tileSize() == TilePosition{ 4 , 3 })
            placements.insert(block.getLargeTiles().begin(), block.getLargeTiles().end());
    }

    // If there's a good placement, let's use it.
    if (!placements.empty()) {
        for (auto tile : placements) {
            int unitWalk = getUnbuiltSpaceGroundDistance(Position(tp), Position(tile));
            int plength = getBuildingRelatedGroundDistance(start_pos, Position(tile));
            if (plength == 0) {
                Diagnostics::DiagnosticWrite("I can't figure out how far the block is from the start postion.");
                return viable_placements;
            }
            if (unitWalk > 0 && plength < baseToWallDist)
                viable_placements.insert({ unitWalk, tile });
        }
    }
    // Otherwise, let's fall back on the backup placements
    if (!backup_placements.empty()) {
        for (auto tile : backup_placements) {
            int unitWalk = getUnbuiltSpaceGroundDistance(Position(tp), Position(tile));
            int plength = getBuildingRelatedGroundDistance(start_pos, Position(tile));
            if (plength == 0) {
                Diagnostics::DiagnosticWrite("I can't figure out how far the block is from the start postion.");
                return viable_placements;
            }
            if (unitWalk > 0 && plength < baseToWallDist)
                viable_placements.insert({ unitWalk, tile });
        }
    }

    return viable_placements;
}

//The distance from the station doesn't make much sense since they all are within 1 tile of the station.
map<int, TilePosition> AssemblyManager::addClosestStation(const UnitType & building, const TilePosition & tp)
{
    map<int, TilePosition> viable_placements = {};

    auto closest_station = BWEB::Stations::getClosestStation(tp);
    if (closest_station) {
        int i = 0;
        for (auto &tile : closest_station->getDefenseLocations()) {
            int walkDistance = getUnbuiltSpaceGroundDistance(Position(tp), Position(tile));
            if (walkDistance > 0)
                viable_placements.insert({ walkDistance, tile });
        }
    }

    return viable_placements;
}

bool AssemblyManager::buildAtNearestPlacement(const UnitType &building, map<int, TilePosition> placements, const Unit u, const bool extra_critera, const int cap_distance)
{
    int currentBestBuildDistance = INT_MAX;
    StoredUnit bestBuilder(u);
    UnitInventory canidateBuilders;
    TilePosition buildSpot = TilePositions::Origin;

    // find good workers
    for (auto worker : CUNYAIModule::friendly_player_model.units_.unit_map_) { // We need the closest worker and want to check for it.
        if (worker.first && worker.second.type_.isWorker() && CUNYAIModule::checkUnitTouchable(worker.first) && (worker.second.phase_ == StoredUnit::Phase::Returning || worker.second.phase_ == StoredUnit::Phase::None) && CUNYAIModule::workermanager.isEmptyWorker(worker.first)) {
            canidateBuilders.addStoredUnit(worker.first);
        }
    }
    // Find position closest to those workers.
    for (auto good_block = placements.begin(); good_block != placements.end(); good_block++) { // should automatically search by distance.
        for (auto canidateBuilder : canidateBuilders.unit_map_) {
            int min_plength;

            min_plength = min({ cap_distance,  getUnbuiltSpaceGroundDistance(canidateBuilder.first->getPosition(), Position(good_block->second)) }); //use the minimum of cap or the walking distance.

            if (min_plength < currentBestBuildDistance && CUNYAIModule::checkWillingAndAble(u, building, extra_critera, min_plength) && isPlaceableCUNY(building, good_block->second)) {
                currentBestBuildDistance = min_plength;
                bestBuilder = StoredUnit(canidateBuilder.first);
                buildSpot = good_block->second;
            }
        }
    }

    //Then build it:
    if (buildSpot != TilePositions::Origin && CUNYAIModule::my_reservation.addReserveSystem(buildSpot, building)) {
        CUNYAIModule::learnedPlan.inspectCurrentBuild().announceBuildingAttempt(building);
        bestBuilder.bwapi_unit_->stop();
        return CUNYAIModule::updateUnitBuildIntent(bestBuilder.bwapi_unit_, building, buildSpot);
    }
    //Diagnostics for failsafe
    if(CUNYAIModule::checkWillingAndAble(u, building, extra_critera))
        Diagnostics::DiagnosticWrite("I couldn't place a %s!", building.c_str());
    return false;
}

//Simply returns the unittype that is the "best" of a BuildFAP sim.
UnitType AssemblyManager::refineOptimalUnit(const map<UnitType, int> combat_types, const ResearchInventory &ri) {
    int best_sim_score = INT_MIN; // Optimal unit must be better than the others.  Using UnitTypes::None leads to intermittent freezes when the options are collectively bad.
    UnitType build_type = UnitTypes::None;

    for (auto &potential_type : combat_types) {
        if (potential_type.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_type.second;
            build_type = potential_type.first;
            //Diagnostics::DiagnosticWrite("Found a Best_sim_score of %d, for %s", best_sim_score, build_type.c_str());
        }
        else if (potential_type.second == best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            bool current_best_flexible = build_type.airWeapon() != WeaponTypes::None && build_type.groundWeapon() != WeaponTypes::None;
            bool new_best_flexible = potential_type.first.airWeapon() != WeaponTypes::None && potential_type.first.groundWeapon() != WeaponTypes::None;

            if (current_best_flexible && !new_best_flexible) continue; // if the current unit is "flexible" with regard to air and ground units, then keep it and continue to consider the next unit.
            else if (new_best_flexible && !current_best_flexible) build_type = potential_type.first; // if the tying unit is "flexible", then let's use that one.
            else if (current_best_flexible == new_best_flexible) build_type = build_type.buildTime() < potential_type.first.buildTime() ? build_type : potential_type.first; // If they both are poor choices or both are good choices, get the faster building one.
            //Diagnostics::DiagnosticWrite("Found a tie, favoring the flexible unit %d, for %s", best_sim_score, build_type.c_str());
        }
    }

    return build_type;

}

//Simply returns rank of the unittype.
int AssemblyManager::returnUnitRank(const UnitType &ut) {
    int postion_in_line = 0;
    multimap<int, UnitType> sorted_list;
    for (auto it : assemblyCycle_) {
        sorted_list.insert({ it.second, it.first });
    }

    for (auto unit_idea = sorted_list.rbegin(); unit_idea != sorted_list.rend(); ++unit_idea) {
        if (unit_idea->second == ut) {
            return postion_in_line;
        }
        else postion_in_line++;
    }
    return postion_in_line;
}

//Simply checks if the unit is the "best" (or tied with best) of a BuildFAP sim.
bool AssemblyManager::checkBestUnit(const UnitType &ut) {
    if (assemblyCycle_.find(ut) == assemblyCycle_.end()) {
        return false;
    }
    else {
        for (auto &potential_type : assemblyCycle_) {
            if (potential_type.second > assemblyCycle_.find(ut)->second)
                return false;
        }
        return true;
    }
}

//Updates the assembly cycle to consider the value of each unit. Discards units one might not want to build on a heuristic basis.
void AssemblyManager::updateOptimalCombatUnit() {
    bool building_optimal_unit = false;
    CombatSimulator buildSim;
    buildSim.addPlayersToMiniSimulation();

    //add friendly units under consideration to FAP in loop, resetting each time.
    for (auto &potential_type : assemblyCycle_) {
        StoredUnit su = StoredUnit(potential_type.first);
        auto buildSimCopy = buildSim;
        remainder_.getReservationCapacity(); //First, let us consider building our units.

        for (int i = 0; i <= remainder_.getWaveSize(potential_type.first); i++) {
            buildSimCopy.addExtraUnitToSimulation(su); //add unit we are interested in to the inventory:
            if (potential_type.first.isTwoUnitsInOneEgg()) buildSimCopy.addExtraUnitToSimulation(su); // do it twice if you're making 2.
        }
        // Imagine any leftover will be spent on "other units" that we can afford. How do we determine them? Try each and every one in my list.
        for (auto ut : assemblyCycle_) {
            for (int i = 0; i <= remainder_.getWaveSize(potential_type.first); i++) {
                buildSimCopy.addExtraUnitToSimulation(su); //add unit we are interested in to the inventory:
                if (potential_type.first.isTwoUnitsInOneEgg()) buildSimCopy.addExtraUnitToSimulation(su); // do it twice if you're making 2.
            }
        }

        buildSimCopy.runSimulation(); // a complete infinitely long simulation cannot be ran... medics & firebats vs air causes a lockup.

        int score = buildSimCopy.getFriendlyScore() - buildSimCopy.getEnemyScore(); //Which shows best gain over opponents?

        //Apply holistic weights.
        applyWeightsFor(potential_type.first);

        if (assemblyCycle_.find(potential_type.first) == assemblyCycle_.end()) assemblyCycle_[potential_type.first] = score;
        else assemblyCycle_[potential_type.first] = static_cast<int>((23.0 * assemblyCycle_[potential_type.first] + score) / 24.0); //moving average over 24 simulations, 1 seconds.
    }
}


bool AssemblyManager::testActiveAirDefenseBest(const bool testSelf) const {

    UnitType build_type = UnitTypes::None;
    CombatSimulator buildSim;
    buildSim.addPlayersToMiniSimulation();

    //if (team_creating_problems.flyer_count_ > 0) {
        //Test a baseline with Sunks
        CombatSimulator baselineWithSunks = buildSim;
        StoredUnit su = StoredUnit(UnitTypes::Zerg_Sunken_Colony);
        // enemy units do not change.
        for (int i = 0; i < 5; ++i) {
            baselineWithSunks.addExtraUnitToSimulation(su, testSelf); //add unit we are interested in to the inventory:
        }
        baselineWithSunks.runSimulation();
        int gainInBaseline = baselineWithSunks.getScoreGap(testSelf);

        // test fake anti-air sunkens
        CombatSimulator baselineWithSpores = buildSim;
        StoredUnit su = StoredUnit(UnitTypes::Zerg_Spore_Colony);
        // enemy units do not change.
        for (int i = 0; i < 5; ++i) {
            baselineWithSpores.addExtraUnitToSimulation(su, testSelf); //add unit we are interested in to the inventory:
        }
        int gainInBaseline = baselineWithSpores.getScoreGap();
        baselineWithSpores.runSimulation();

        return baselineWithSpores.getScoreGap(testSelf) >= baselineWithSunks.getScoreGap(testSelf);
    //}

}

bool AssemblyManager::testAirAttackBest(const bool testSelf) const {
    UnitType build_type = UnitTypes::None;
    CombatSimulator buildSim;
    buildSim.addPlayersToMiniSimulation();

    // This has two magic numbers that deserve discussion (68 and 33). The thought process is that hydras and mutalisks have unit values of 206.25 and 425 respectively. 
    // So we need to add equal values to the simulation to ensure that we do not bias it too strongly. The LCM is 14025 which can be equally made by 68 hydras or 33 mutas.

    //Test a baseline with hydras
    CombatSimulator baselineWithHydras = buildSim;
    StoredUnit su = StoredUnit(UnitTypes::Zerg_Hydralisk);
    for (int i = 0; i < 68; ++i) {
        baselineWithHydras.addExtraUnitToSimulation(su, testSelf); //add unit we are interested in to the inventory:
    }
    baselineWithHydras.runSimulation();
    int gainInBaseline = baselineWithHydras.getScoreGap(testSelf);

    // test mutalisks
    CombatSimulator baselineWithMutas = buildSim;
    StoredUnit su = StoredUnit(UnitTypes::Zerg_Mutalisk);
    // enemy units do not change.
    for (int i = 0; i < 33; ++i) {
        baselineWithMutas.addExtraUnitToSimulation(su, testSelf); //add unit we are interested in to the inventory:
    }
    int gainInBaseline = baselineWithMutas.getScoreGap();
    baselineWithMutas.runSimulation();

    return baselineWithMutas.getScoreGap(testSelf) >= baselineWithHydras.getScoreGap(testSelf);
}

// Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void AssemblyManager::Print_Assembly_FAP_Cycle(const int &screen_x, const int &screen_y) {
    int another_sort_of_unit = 0;
    multimap<int, UnitType> sorted_list;
    for (auto it : assemblyCycle_) {
        sorted_list.insert({ it.second, it.first });
    }

    for (auto unit_idea = sorted_list.rbegin(); unit_idea != sorted_list.rend(); ++unit_idea) {
        Broodwar->drawTextScreen(screen_x, screen_y, "UnitSimResults:");  //
        Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_unit * 10, "%s: %d", unit_idea->second.c_str(), unit_idea->first);
        another_sort_of_unit++;
    }
}

void AssemblyManager::updatePotentialBuilders()
{

    larvaBank_.unit_map_.clear();
    hydraBank_.unit_map_.clear();
    mutaBank_.unit_map_.clear();
    builderBank_.unit_map_.clear();
    creepColonyBank_.unit_map_.clear();
    productionFacilityBank_.unit_map_.clear();

    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Larva) larvaBank_.addStoredUnit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Hydralisk) hydraBank_.addStoredUnit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Mutalisk) mutaBank_.addStoredUnit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Creep_Colony) creepColonyBank_.addStoredUnit(u.second);
        if (u.second.type_ == Broodwar->self()->getRace().getWorker()) builderBank_.addStoredUnit(u.second);
        if (u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery)) productionFacilityBank_.addStoredUnit(u.second);
    }

    larvaBank_.updateUnitInventorySummary();
    hydraBank_.updateUnitInventorySummary();
    mutaBank_.updateUnitInventorySummary();
    builderBank_.updateUnitInventorySummary();
    creepColonyBank_.updateUnitInventorySummary();
    productionFacilityBank_.updateUnitInventorySummary();

}

bool AssemblyManager::creepColonyInArea(const Position & pos) {
    bool creep_colony_nearby = false;
    UnitInventory units_loc = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Creep_Colony, pos);
    creep_colony_nearby = !units_loc.unit_map_.empty();
    return creep_colony_nearby;
}


bool AssemblyManager::assignAssemblyRole()
{
    UnitInventory overlord_larva;
    UnitInventory immediate_drone_larva;
    UnitInventory transfer_drone_larva;
    UnitInventory combat_creators_larva;
    UnitInventory combat_creators_hydra;
    UnitInventory combat_creators_muta;

    UnitInventory alarming_enemy_ground = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::currentMapInventory.getEnemyBaseGround());
    UnitInventory alarming_enemy_air = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::enemy_player_model.units_, CUNYAIModule::currentMapInventory.getEnemyBaseAir());

    alarming_enemy_ground.updateUnitInventorySummary();
    alarming_enemy_air.updateUnitInventorySummary();

    subgoalArmy_ = CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::ArmyAlpha) * CUNYAIModule::friendly_player_model.spending_model_.evalArmyPossible() > CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::EconAlpha) * CUNYAIModule::friendly_player_model.spending_model_.evalEconPossible();
    subgoalEcon_ = CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::ArmyAlpha) * CUNYAIModule::friendly_player_model.spending_model_.evalArmyPossible() < CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::EconAlpha) * CUNYAIModule::friendly_player_model.spending_model_.evalEconPossible();; // they're complimentrary but I'd like them positively defined, negations can confuse.

    bool they_are_moving_out_ground = alarming_enemy_ground.building_count_ == 0;
    bool they_are_moving_out_air = alarming_enemy_air.building_count_ == 0;
    int distance_to_alarming_ground = INT_MAX;
    int distance_to_alarming_air = INT_MAX;

    // Identify desirable unit classes prior to simulation.
    setMaxUnit(UnitTypes::Zerg_Scourge, 2 * CUNYAIModule::enemy_player_model.units_.flyer_count_ + 4 * CUNYAIModule::countUnits(UnitTypes::Terran_Battlecruiser, CUNYAIModule::enemy_player_model.units_) + 4 * CUNYAIModule::countUnits(UnitTypes::Protoss_Carrier, CUNYAIModule::enemy_player_model.units_));


    for (auto hatch : productionFacilityBank_.unit_map_) {
        distance_to_alarming_ground = min(CUNYAIModule::currentMapInventory.getRadialDistanceOutFromEnemy(hatch.second.pos_), distance_to_alarming_ground);
        distance_to_alarming_air = min(static_cast<int>(hatch.second.pos_.getDistance(CUNYAIModule::currentMapInventory.getEnemyBaseAir())), distance_to_alarming_air);
    }

    // Creep colony logic is very similar to each hatch's decision to build a creep colony. If a creep colony would be built, we are probably also morphing existing creep colonies...  May want make a base manager. There is a logic error in here, since the creep colony may have been built because of a threat at a nearby base B, but the closest viable building location to B was actually closer to A than B.
    if (last_frame_of_creep_command < Broodwar->getFrameCount() - 12) {
        for (auto creep_colony : creepColonyBank_.unit_map_) {
            Base ground_base = CUNYAIModule::basemanager.getClosestBaseGround(creep_colony.second.pos_);
            Base air_base = CUNYAIModule::basemanager.getClosestBaseAir(creep_colony.second.pos_);
            bool force_air = air_base.emergency_spore_ && canMakeCUNY(UnitTypes::Zerg_Spore_Colony, true, creep_colony.first) && air_base.spore_count_ < 6;
            bool force_ground = ground_base.emergency_sunken_ && canMakeCUNY(UnitTypes::Zerg_Sunken_Colony, true, creep_colony.first) && ground_base.sunken_count_ < 6;
            buildStaticDefence(creep_colony.first, force_air, force_ground); // checks globally but not bad, info is mostly already there.
        }
        last_frame_of_creep_command = Broodwar->getFrameCount();
    }

    //Assess the units and sort them into bins.
    if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto larva : larvaBank_.unit_map_) {
            if (!CUNYAIModule::checkUnitTouchable(larva.first)) continue;
            //Workers check
            // Is everywhere ok for workers?
            // Which larva should be morphed first?
            bool wasting_larva_soon = true;
            bool hatch_wants_drones = true;
            bool prep_for_transfer = true;
            bool minerals_on_left = false;

            if (larva.first->getHatchery()) {
                wasting_larva_soon = larva.first->getHatchery()->getRemainingTrainTime() < 5 + Broodwar->getLatencyFrames() && larva.first->getHatchery()->getLarva().size() == 2 && CUNYAIModule::land_inventory.countLocalMinPatches() > 8; // no longer will spam units when I need a hatchery.
                Base b = CUNYAIModule::basemanager.getBase(larva.first->getHatchery()->getPosition());
                hatch_wants_drones = 2 * b.mineral_patches_ + 3 * b.gas_refinery_ > b.mineral_gatherers_ + b.gas_gatherers_;
                prep_for_transfer = CUNYAIModule::countUnitsInProgress(Broodwar->self()->getRace().getResourceDepot()) > 0;

                Position hatch_spot = larva.first->getHatchery()->getPosition();
                Position centroid = BWEB::Stations::getClosestStation(TilePosition(hatch_spot))->getResourceCentroid();
                Position left_of_base = hatch_spot - Position(124, 0); // left of base.
                minerals_on_left = checkSameDirection(centroid - hatch_spot, left_of_base - hatch_spot);
            }

            bool enough_drones_globally = (CUNYAIModule::countUnits(UnitTypes::Zerg_Drone) > CUNYAIModule::land_inventory.countLocalMinPatches() * 2 + CUNYAIModule::countUnits(UnitTypes::Zerg_Extractor) * 3 + 1) || !CUNYAIModule::assemblymanager.checkNewUnitWithinMaximum(UnitTypes::Zerg_Drone);

            bool drones_are_needed_here = (CUNYAIModule::econ_starved || wasting_larva_soon || (CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Drone) && subgoalEcon_)) && !enough_drones_globally && hatch_wants_drones;
            bool drones_are_needed_elsewhere = (CUNYAIModule::econ_starved || wasting_larva_soon || (CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Drone) && subgoalEcon_)) && !enough_drones_globally && !hatch_wants_drones && prep_for_transfer;
            bool create_supply_buffer = (wasting_larva_soon || CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Overlord)) && !checkSlackSupply();

            if (minerals_on_left && Broodwar->getFrameCount() % 96 == 0) {
                larva.first->stop(); // this will larva trick them to the left.
                larva.second.updateStoredUnit(larva.first);
            }

            if (drones_are_needed_here || CUNYAIModule::checkFeasibleRequirement(larva.first, UnitTypes::Zerg_Drone)) {
                immediate_drone_larva.addStoredUnit(larva.second);
            }
            if (drones_are_needed_elsewhere || CUNYAIModule::checkFeasibleRequirement(larva.first, UnitTypes::Zerg_Drone)) {
                transfer_drone_larva.addStoredUnit(larva.second);
            }
            if (CUNYAIModule::supply_starved || create_supply_buffer || CUNYAIModule::checkFeasibleRequirement(larva.first, UnitTypes::Zerg_Overlord)) {
                overlord_larva.addStoredUnit(larva.second);
            }
            combat_creators_larva.addStoredUnit(larva.second);// needs to be clear so we can consider building combat units whenever they are required.
        }
    }

    if (last_frame_of_hydra_morph_command < Broodwar->getFrameCount() - 12) {
        bool lurkers_permissable = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
        for (auto potential_lurker : hydraBank_.unit_map_) {
            bool bad_phase = (potential_lurker.second.phase_ == StoredUnit::Attacking || potential_lurker.second.phase_ == StoredUnit::Retreating || potential_lurker.second.phase_ == StoredUnit::Surrounding) /*&& potential_lurker.second.current_hp_ > 0.5 * (potential_lurker.second.type_.maxHitPoints() + potential_lurker.second.type_.maxShields())*/;
            if (!CUNYAIModule::checkUnitTouchable(potential_lurker.first) || bad_phase) continue;
            if (potential_lurker.second.time_since_last_dmg_ > FAP_SIM_DURATION) combat_creators_hydra.addStoredUnit(potential_lurker.second);
        }
    }

    if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12) {
        bool endgame_fliers_permissable = CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire) - CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Greater_Spire) > 0;
        for (auto potential_endgame_flier : mutaBank_.unit_map_) {
            bool bad_phase = (potential_endgame_flier.second.phase_ == StoredUnit::Attacking || potential_endgame_flier.second.phase_ == StoredUnit::Retreating || potential_endgame_flier.second.phase_ == StoredUnit::Surrounding) /*&& potential_endgame_flier.second.current_hp_ > 0.5 * (potential_endgame_flier.second.type_.maxHitPoints() + potential_endgame_flier.second.type_.maxShields())*/;
            if (!CUNYAIModule::checkUnitTouchable(potential_endgame_flier.first) || bad_phase) continue;
            if (potential_endgame_flier.second.time_since_last_dmg_ > FAP_SIM_DURATION && endgame_fliers_permissable) combat_creators_muta.addStoredUnit(potential_endgame_flier.second);
        }
    }

    //Build from the units as needed: priority queue should be as follows. Note some of these are mutually exclusive.
    //UnitInventory overlord_larva;
    //UnitInventory immediate_drone_larva;
    //UnitInventory transfer_drone_larva;
    //UnitInventory combat_creators;

    if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto o : overlord_larva.unit_map_) {
            if (CUNYAIModule::checkWilling(UnitTypes::Zerg_Overlord, true) && CUNYAIModule::my_reservation.addReserveSystem(o.first, UnitTypes::Zerg_Overlord)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
        for (auto d : immediate_drone_larva.unit_map_) {
            if (CUNYAIModule::checkWilling(UnitTypes::Zerg_Drone, true) && CUNYAIModule::my_reservation.addReserveSystem(d.first, UnitTypes::Zerg_Drone)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
        for (auto d : transfer_drone_larva.unit_map_) {
            if (CUNYAIModule::checkWilling(UnitTypes::Zerg_Drone, true) && CUNYAIModule::my_reservation.addReserveSystem(d.first, UnitTypes::Zerg_Drone)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
    }

    //We will fall through to this case if resources remain.
    if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto c : combat_creators_muta.unit_map_) {
            if (buildCombatUnit(c.first)) {
                //if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Mutalisk) 
                last_frame_of_muta_morph_command = Broodwar->getFrameCount();
                //if (last_frame_of_larva_morph_command == Broodwar->getFrameCount() || last_frame_of_muta_morph_command == Broodwar->getFrameCount() || last_frame_of_hydra_morph_command == Broodwar->getFrameCount())
                return true;
            }
        }
    }

    //We will fall through to this case if resources remain.
    if (last_frame_of_hydra_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto c : combat_creators_hydra.unit_map_) {
            if (buildCombatUnit(c.first)) {
               //if (last_frame_of_hydra_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Hydralisk) 
                    last_frame_of_hydra_morph_command = Broodwar->getFrameCount();
                //if (last_frame_of_larva_morph_command == Broodwar->getFrameCount() || last_frame_of_muta_morph_command == Broodwar->getFrameCount() || last_frame_of_hydra_morph_command == Broodwar->getFrameCount())
                    return true;
            }
        }
    }

    //We will fall through to this case if resources remain.
    if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto c : combat_creators_larva.unit_map_) {
            if (buildCombatUnit(c.first)) {
                //if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Larva) 
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                //if (last_frame_of_larva_morph_command == Broodwar->getFrameCount() || last_frame_of_muta_morph_command == Broodwar->getFrameCount() || last_frame_of_hydra_morph_command == Broodwar->getFrameCount())
                return true;
            }
        }
    }

    return false;
}

void AssemblyManager::morphReservedUnits()
{
    for (auto r : CUNYAIModule::my_reservation.getReservedUnits())
        r.first->morph(r.second);
}

void AssemblyManager::clearSimulationHistory()
{
    assemblyCycle_ = CUNYAIModule::friendly_player_model.getCombatUnitCartridge();
    for (auto unit : assemblyCycle_) {
        unit.second = 0;
    }
    assemblyCycle_.insert({ UnitTypes::None, 0 });
}

void AssemblyManager::planDefensiveWalls()
{
    //vector<UnitType> buildings = { UnitTypes::Zerg_Hatchery, UnitTypes::Zerg_Evolution_Chamber, UnitTypes::Zerg_Evolution_Chamber };
    //vector<UnitType> defenses(6, UnitTypes::Zerg_Sunken_Colony);
    //for (auto &area : BWEM::Map::Instance().Areas()) {
    //    // Only make walls at gas bases that aren't starting bases
    //    bool invalidBase = false;
    //    for (auto &base : area.Bases()) {
    //        if (base.Starting())
    //            invalidBase = true;
    //    }
    //    if (invalidBase)
    //        continue;

    //    const BWEM::ChokePoint * bestChoke = nullptr;
    //    double distBest = DBL_MAX;
    //    for (auto &cp : area.ChokePoints()) {
    //        auto dist = Position(cp->Center()).getDistance(BWEM::Map::Instance().Center());
    //        if (dist < distBest) {
    //            distBest = dist;
    //            bestChoke = cp;
    //        }
    //    }
    //    BWEB::Walls::createWall(buildings, &area, bestChoke, UnitTypes::None, defenses, true, false);
    //}
    BWEB::Walls::createZSimCity();
}

bool AssemblyManager::canMakeCUNY(const UnitType & type, const bool can_afford, const Unit & builder)
{
    // Error checking
    Broodwar->setLastError();
    if (!Broodwar->self())
        return Broodwar->setLastError(Errors::Unit_Not_Owned);

    // Check if the unit type is available (UMS game)
    if (!Broodwar->self()->isUnitAvailable(type))
        return Broodwar->setLastError(Errors::Access_Denied);

    // Get the required UnitType
    BWAPI::UnitType requiredType = type.whatBuilds().first;

    Player pSelf = Broodwar->self();
    if (builder != nullptr) // do checks if a builder is provided
    {
        // Check if the owner of the unit is you
        if (builder->getPlayer() != pSelf)
            return Broodwar->setLastError(Errors::Unit_Not_Owned);

        BWAPI::UnitType builderType = builder->getType();
        if (type == UnitTypes::Zerg_Nydus_Canal && builderType == UnitTypes::Zerg_Nydus_Canal)
        {
            if (!builder->isCompleted())
                return Broodwar->setLastError(Errors::Unit_Busy);

            if (builder->getNydusExit())
                return Broodwar->setLastError(Errors::Unknown);

            return true;
        }

        // Check if this unit can actually build the unit type
        if (requiredType == UnitTypes::Zerg_Larva && builderType.producesLarva())
        {
            if (builder->getLarva().size() == 0)
                return Broodwar->setLastError(Errors::Unit_Does_Not_Exist);
        }
        else if (builderType != requiredType)
        {
            return Broodwar->setLastError(Errors::Incompatible_UnitType);
        }

        // Carrier/Reaver space checking
        int max_amt;
        switch (builderType)
        {
        case UnitTypes::Enum::Protoss_Carrier:
        case UnitTypes::Enum::Hero_Gantrithor:
            // Get max interceptors
            max_amt = 4;
            if (pSelf->getUpgradeLevel(UpgradeTypes::Carrier_Capacity) > 0 || builderType == UnitTypes::Hero_Gantrithor)
                max_amt += 4;

            // Check if there is room
            if (builder->getInterceptorCount() + (int)builder->getTrainingQueue().size() >= max_amt)
                return Broodwar->setLastError(Errors::Insufficient_Space);
            break;
        case UnitTypes::Enum::Protoss_Reaver:
        case UnitTypes::Enum::Hero_Warbringer:
            // Get max scarabs
            max_amt = 5;
            if (pSelf->getUpgradeLevel(UpgradeTypes::Reaver_Capacity) > 0 || builderType == UnitTypes::Hero_Warbringer)
                max_amt += 5;

            // check if there is room
            if (builder->getScarabCount() + static_cast<int>(builder->getTrainingQueue().size()) >= max_amt)
                return Broodwar->setLastError(Errors::Insufficient_Space);
            break;
        }
    } // if builder != nullptr

    if (can_afford) {
        // Check if player has enough minerals
        if (pSelf->minerals() < type.mineralPrice())
            return Broodwar->setLastError(Errors::Insufficient_Minerals);

        // Check if player has enough gas
        if (pSelf->gas() < type.gasPrice())
            return Broodwar->setLastError(Errors::Insufficient_Gas);

        // Check if player has enough supplies
        BWAPI::Race typeRace = type.getRace();
        const int supplyRequired = type.supplyRequired() * (type.isTwoUnitsInOneEgg() ? 2 : 1);
        if (supplyRequired > 0 && pSelf->supplyTotal(typeRace) < pSelf->supplyUsed(typeRace) + supplyRequired - (requiredType.getRace() == typeRace ? requiredType.supplyRequired() : 0))
            return Broodwar->setLastError(Errors::Insufficient_Supply);
    }

    UnitType addon = UnitTypes::None;
    for (auto &it : type.requiredUnits())
    {
        if (it.first.isAddon())
            addon = it.first;

        if (!pSelf->hasUnitTypeRequirement(it.first, it.second))
            return Broodwar->setLastError(Errors::Insufficient_Tech);
    }

    if (type.requiredTech() != TechTypes::None && !pSelf->hasResearched(type.requiredTech()))
        return Broodwar->setLastError(Errors::Insufficient_Tech);

    if (builder &&
        addon != UnitTypes::None &&
        addon.whatBuilds().first == type.whatBuilds().first &&
        (!builder->getAddon() || builder->getAddon()->getType() != addon))
        return Broodwar->setLastError(Errors::Insufficient_Tech);

    return true;
}

bool AssemblyManager::checkSlackLarvae()
{
    return  CUNYAIModule::my_reservation.getExcessLarva() >= 2;
}

bool AssemblyManager::checkSlackMinerals()
{
    return  CUNYAIModule::my_reservation.getExcessMineral() > 50;
}

bool AssemblyManager::checkSlackGas()
{
    return  CUNYAIModule::my_reservation.getExcessGas() > 50;
}

bool AssemblyManager::checkSlackSupply()
{
    return  CUNYAIModule::my_reservation.getExcessSupply() + CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Overlord) * UnitTypes::Zerg_Overlord.supplyProvided() >= CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Hatchery) * getMaxSupply(); // Note max supply is 400 in this metric (ling = 1 supply each).
}

int AssemblyManager::getMaxGas()
{
    int max_gas = 0;
    for (auto u : CUNYAIModule::friendly_player_model.getCombatUnitCartridge()) {
        if (canMakeCUNY(u.first) && u.first) max_gas = max(max_gas, u.first.gasPrice());
    }
    for (auto u : CUNYAIModule::friendly_player_model.getBuildingCartridge()) {
        if (canMakeCUNY(u.first) && u.first) max_gas = max(max_gas, u.first.gasPrice());
    }
    return max_gas;
}

int AssemblyManager::getMaxSupply()
{
    int max_supply = 0;
    for (auto u : CUNYAIModule::friendly_player_model.getCombatUnitCartridge()) {
        if (canMakeCUNY(u.first) && u.first) max_supply = max(max_supply, u.first.supplyRequired());
    }
    for (auto u : CUNYAIModule::friendly_player_model.getBuildingCartridge()) {
        if (canMakeCUNY(u.first) && u.first) max_supply = max(max_supply, u.first.supplyRequired());
    }
    return max_supply;
}

TilePosition AssemblyManager::updateExpoPosition()
{
    int score_temp, expo_score = INT_MIN;
    TilePosition base_expo = TilePositions::Origin;
    Base mainBase = CUNYAIModule::basemanager.getClosestBaseGround(Position(Broodwar->self()->getStartLocation())); // This is a long way of specifying the base closest to the start position

    if (mainBase.unit_ && mainBase.unit_->exists()) {
        Mobility mobile = Mobility(mainBase.unit_);

        for (auto &p : CUNYAIModule::currentMapInventory.getExpoTilePositions()) {

            score_temp = CUNYAIModule::currentMapInventory.getExpoPositionScore(Position(p)); // closer is better, further from enemy is better.  The first base (the natural, sometimes the 3rd) simply must be the closest, distance is irrelevant.

            bool want_more_gas = CUNYAIModule::learnedPlan.inspectCurrentBuild().countTimesInBuildQueue(Broodwar->self()->getRace().getRefinery()) > CUNYAIModule::basemanager.getBaseGeyserCount(); // if you need/have 2 extractors, your first expansion must have gas.
            bool base_has_gas = CUNYAIModule::getResourceInventoryAtBase(CUNYAIModule::land_inventory, Position(p)).countLocalGeysers() > 0;

            bool meets_gas_requirements = (base_has_gas && want_more_gas) || !want_more_gas;

            if (isPlaceableCUNY(Broodwar->self()->getRace().getResourceDepot(), p) && score_temp > expo_score && mobile.checkSafeEscapePath(Position(p)) && meets_gas_requirements) {
                expo_score = score_temp;
                base_expo = p;
            }
        }
        expo_spot_ = base_expo; // update variable of interest.
        return expo_spot_;
    }
    else {  // if you've got no main base, we have ... trouble.
        if constexpr (RESIGN_MODE) {
            Broodwar->leaveGame();
        }
        expo_spot_ = base_expo;
        return base_expo;
    }
}

int AssemblyManager::getMaxTravelDistance()
{
    if (CUNYAIModule::basemanager.getBaseCount() == 0)
        return INT_MAX;
    if (CUNYAIModule::basemanager.getBaseCount() == 1)
        return getBuildingRelatedGroundDistance(BWEB::Map::getMainPosition(), BWEB::Map::getNaturalPosition()); // give them the distance to the natural and then some extra tiles.
    else
        return getBuildingRelatedGroundDistance(BWEB::Map::getMainPosition(), BWEM::Map::Instance().Center()) ; // give them the distance to the map center.
}

int AssemblyManager::getBuildingRelatedGroundDistance(const Position & A, const Position & B)
{
    int plength = INT_MIN;

    //Try CPP pathing.
        auto cpp = BWEM::Map::Instance().GetPath(A, B, &plength);
        return plength;
}

int AssemblyManager::getUnbuiltSpaceGroundDistance(const Position & A, const Position & B) {
    BWEB::Path newPath;
    //Try JPS pathing.
    newPath.createUnitPath(A, B);

    if (newPath.isReachable() && newPath.getDistance() > 0)
        return static_cast<int>(newPath.getDistance());
    else
        return 0;
}

TilePosition AssemblyManager::getExpoPosition()
{
    if (!expo_spot_ || expo_spot_ == TilePositions::Origin)
        return updateExpoPosition(); // being safe.
    else
        return expo_spot_;
}

void AssemblyManager::setMaxUnit(const UnitType & ut, const int max)
{
    if(maxUnits_.find(ut) == maxUnits_.end())
        return;
    maxUnits_.at(ut) = max;
}

bool CUNYAIModule::checkInCartridge(const UnitType &ut) {
    auto mapCombat = friendly_player_model.getCombatUnitCartridge();
    auto mapBuild = friendly_player_model.getBuildingCartridge();
    auto mapEco = friendly_player_model.getEcoUnitCartridge();
    return mapCombat.find(ut) != mapCombat.end() || mapBuild.find(ut) != mapBuild.end() || mapEco.find(ut) != mapEco.end();
}

bool CUNYAIModule::checkInCartridge(const UpgradeType &ut) {
    auto mapUps = friendly_player_model.getUpgradeCartridge();
    return mapUps.find(ut) != mapUps.end();
}

bool CUNYAIModule::checkInCartridge(const TechType &ut) {
    auto mapTechs = friendly_player_model.getTechCartridge();
    return mapTechs.find(ut) != mapTechs.end();
}

bool CUNYAIModule::checkOpenToBuild(const UnitType &ut, const bool &extra_criteria) {
    return checkInCartridge(ut) && AssemblyManager::checkNewUnitWithinMaximum(ut) && my_reservation.canReserveWithExcessResource(ut) && (learnedPlan.inspectCurrentBuild().checkIfNextInBuild(ut) || (extra_criteria && CUNYAIModule::learnedPlan.inspectCurrentBuild().isEmptyBuildOrder()));
}

bool CUNYAIModule::checkOpenToUpgrade(const UpgradeType &ut, const bool &extra_criteria) {
    return checkInCartridge(ut) && (learnedPlan.inspectCurrentBuild().checkIfNextInBuild(ut) || (extra_criteria && learnedPlan.inspectCurrentBuild().isEmptyBuildOrder()));
}

bool CUNYAIModule::checkWillingAndAble(const Unit &unit, const UnitType &ut, const bool &extra_criteria, const int &travel_distance) {
     return AssemblyManager::canMakeCUNY(ut, !ut.isBuilding(), unit) && my_reservation.checkAffordablePurchase(ut, travel_distance) && checkOpenToBuild(ut, extra_criteria);
}

//bool CUNYAIModule::checkWillingAndAble(const UnitType &ut, const bool &extra_criteria, const int &travel_distance) {
//    return AssemblyManager::canMakeCUNY(ut, !ut.isBuilding()) && my_reservation.checkAffordablePurchase(ut, travel_distance) && checkOpenToBuild(ut, extra_criteria);
//}

//bool CUNYAIModule::checkWillingAndAble(const UpgradeType &ut, const bool &extra_criteria) {
//    return Broodwar->canUpgrade(ut) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && (buildorder.checkUpgradeNextInBO(ut) || (extra_criteria && buildorder.isEmptyBuildOrder()));
//}

bool CUNYAIModule::checkWilling(const UnitType &ut, const bool &extra_criteria) {
     return AssemblyManager::canMakeCUNY(ut, false) && checkOpenToBuild(ut, extra_criteria);
}

bool CUNYAIModule::checkFeasibleRequirement(const Unit &unit, const UnitType &ut) {
    return Broodwar->canMake(ut, unit) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && learnedPlan.inspectCurrentBuild().checkIfNextInBuild(ut);
}

bool CUNYAIModule::checkFeasibleRequirement(const Unit &unit, const UpgradeType &up) {
    return Broodwar->canUpgrade(up, unit) && my_reservation.checkAffordablePurchase(up) && checkInCartridge(up) && learnedPlan.inspectCurrentBuild().checkIfNextInBuild(up);
}

bool CUNYAIModule::checkFeasibleRequirement(const UpgradeType &up) {
    return Broodwar->canUpgrade(up) && my_reservation.checkAffordablePurchase(up) && checkInCartridge(up) && learnedPlan.inspectCurrentBuild().checkIfNextInBuild(up);
}

//void Build::setInitialBuildOrder(string s) {
//
//    building_gene_.clear();
//
//    initial_building_gene_ = s;
//
//    std::stringstream ss(s);
//    std::istream_iterator<std::string> begin(ss);
//    std::istream_iterator<std::string> end;
//    std::vector<std::string> build_string(begin, end);
//
//    BuildOrderElement hatch = BuildOrderElement(UnitTypes::Zerg_Hatchery);
//    BuildOrderElement extract = BuildOrderElement(UnitTypes::Zerg_Extractor);
//    BuildOrderElement drone = BuildOrderElement(UnitTypes::Zerg_Drone);
//    BuildOrderElement ovi = BuildOrderElement(UnitTypes::Zerg_Overlord);
//    BuildOrderElement pool = BuildOrderElement(UnitTypes::Zerg_Spawning_Pool);
//    BuildOrderElement evo = BuildOrderElement(UnitTypes::Zerg_Evolution_Chamber);
//    BuildOrderElement speed = BuildOrderElement(UpgradeTypes::Metabolic_Boost);
//    BuildOrderElement ling = BuildOrderElement(UnitTypes::Zerg_Zergling);
//    BuildOrderElement creep = BuildOrderElement(UnitTypes::Zerg_Creep_Colony);
//    BuildOrderElement sunken = BuildOrderElement(UnitTypes::Zerg_Sunken_Colony);
//    BuildOrderElement spore = BuildOrderElement(UnitTypes::Zerg_Spore_Colony);
//    BuildOrderElement lair = BuildOrderElement(UnitTypes::Zerg_Lair);
//    BuildOrderElement hive = BuildOrderElement(UnitTypes::Zerg_Hive);
//    BuildOrderElement spire = BuildOrderElement(UnitTypes::Zerg_Spire);
//    BuildOrderElement greater_spire = BuildOrderElement(UnitTypes::Zerg_Greater_Spire);
//    BuildOrderElement devourer = BuildOrderElement(UnitTypes::Zerg_Devourer);
//    BuildOrderElement muta = BuildOrderElement(UnitTypes::Zerg_Mutalisk);
//    BuildOrderElement hydra = BuildOrderElement(UnitTypes::Zerg_Hydralisk);
//    BuildOrderElement lurker = BuildOrderElement(UnitTypes::Zerg_Lurker);
//    BuildOrderElement hydra_den = BuildOrderElement(UnitTypes::Zerg_Hydralisk_Den);
//    BuildOrderElement queens_nest = BuildOrderElement(UnitTypes::Zerg_Queens_Nest);
//    BuildOrderElement lurker_tech = BuildOrderElement(TechTypes::Lurker_Aspect);
//    BuildOrderElement grooved_spines = BuildOrderElement(UpgradeTypes::Grooved_Spines);
//    BuildOrderElement muscular_augments = BuildOrderElement(UpgradeTypes::Muscular_Augments);
//
//    for (auto &build : build_string) {
//        if (build == "hatch") {
//            building_gene_.push_back(hatch);
//        }
//        else if (build == "extract") {
//            building_gene_.push_back(extract);
//        }
//        else if (build == "drone") {
//            building_gene_.push_back(drone);
//        }
//        else if (build == "ovi") {
//            building_gene_.push_back(ovi);
//        }
//        else if (build == "overlord") {
//            building_gene_.push_back(ovi);
//        }
//        else if (build == "pool") {
//            building_gene_.push_back(pool);
//        }
//        else if (build == "evo") {
//            building_gene_.push_back(evo);
//        }
//        else if (build == "speed") {
//            building_gene_.push_back(speed);
//        }
//        else if (build == "ling") {
//            building_gene_.push_back(ling);
//        }
//        else if (build == "creep") {
//            building_gene_.push_back(creep);
//        }
//        else if (build == "sunken") {
//            building_gene_.push_back(sunken);
//        }
//        else if (build == "spore") {
//            building_gene_.push_back(spore);
//        }
//        else if (build == "lair") {
//            building_gene_.push_back(lair);
//        }
//        else if (build == "hive") {
//            building_gene_.push_back(hive);
//        }
//        else if (build == "spire") {
//            building_gene_.push_back(spire);
//        }
//        else if (build == "greater_spire") {
//            building_gene_.push_back(greater_spire);
//        }
//        else if (build == "devourer") {
//            building_gene_.push_back(devourer);
//        }
//        else if (build == "muta") {
//            building_gene_.push_back(muta);
//        }
//        else if (build == "lurker_tech") {
//            building_gene_.push_back(lurker_tech);
//        }
//        else if (build == "hydra") {
//            building_gene_.push_back(hydra);
//        }
//        else if (build == "lurker") {
//            building_gene_.push_back(lurker);
//        }
//        else if (build == "hydra_den") {
//            building_gene_.push_back(hydra_den);
//        }
//        else if (build == "queens_nest") {
//            building_gene_.push_back(queens_nest);
//        }
//        else if (build == "grooved_spines") {
//            building_gene_.push_back(grooved_spines);
//        }
//        else if (build == "muscular_augments") {
//            building_gene_.push_back(muscular_augments);
//        }
//        else if (build == "5pool") { //shortcuts.
//            building_gene_.push_back(drone);
//            building_gene_.push_back(pool);
//        }
//        else if (build == "7pool") {
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(pool);
//        }
//        else if (build == "9pool") {
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(pool);
//        }
//        else if (build == "12pool") {
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(ovi);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(drone);
//            building_gene_.push_back(pool);
//        }
//        else if (build == "12hatch") {
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(ovi);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(drone);
//        building_gene_.push_back(hatch);
//        }
//    }
//    getCumulativeResources();
//}



