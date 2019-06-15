#pragma once
#include "Source\CUNYAIModule.h"
#include "Source\Map_Inventory.h"
#include "Source\AssemblyManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\PlayerModelManager.h" // needed for cartidges.
#include "Source\BWEB\BWEB.h"
#include <bwem.h>
#include <iterator>
#include <numeric>
#include <fstream>


using namespace BWAPI;
using namespace Filter;
using namespace std;

Unit_Inventory AssemblyManager::larva_bank_;
Unit_Inventory AssemblyManager::hydra_bank_;
Unit_Inventory AssemblyManager::muta_bank_;
Unit_Inventory AssemblyManager::builder_bank_;
Unit_Inventory AssemblyManager::creep_colony_bank_;
Unit_Inventory AssemblyManager::production_facility_bank_;

int AssemblyManager::last_frame_of_larva_morph_command = 0;
int AssemblyManager::last_frame_of_hydra_morph_command = 0;
int AssemblyManager::last_frame_of_muta_morph_command = 0;
int AssemblyManager::last_frame_of_creep_command = 0;
bool AssemblyManager::have_idle_evos_ = false;
bool AssemblyManager::have_idle_spires_ = false;

std::map<UnitType, int> AssemblyManager::assembly_cycle_ = Player_Model::combat_unit_cartridge_;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations. Now updates the unit within the Unit_Inventory directly.
bool AssemblyManager::Check_N_Build(const UnitType &building, const Unit &unit, const bool &extra_critera)
{
    if (CUNYAIModule::checkDesirable(unit, building, extra_critera) ) {
        Position unit_pos = unit->getPosition();
        bool unit_can_morph_intended_target = unit->canMorph(building);
        //Check simple upgrade into lair/hive.
        Unit_Inventory local_area = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, unit_pos);
        bool hatch_nearby = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery, local_area) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;

        //morphing hatcheries into lairs & hives, spires into greater spires, creep colonies into sunkens or spores
        if (unit->getType().isBuilding() && unit->morph(building)) {
            CUNYAIModule::buildorder.announceBuildingAttempt(building); // Takes no time, no need for the reserve system.
            return CUNYAIModule::updateUnitPhase(unit, Stored_Unit::Phase::Building);
        }
        else if (canMakeCUNY(building, false, unit) && building == UnitTypes::Zerg_Creep_Colony) { // creep colony loop specifically.
            auto danger_hatch = CUNYAIModule::getClosestStored(production_facility_bank_, CUNYAIModule::current_map_inventory.enemy_base_ground_, 9999999);
            auto build_tile = CUNYAIModule::enemy_player_model.units_.unit_map_.empty() ? TilePosition(danger_hatch->pos_) : unit->getTilePosition();

            if (!CUNYAIModule::friendly_player_model.u_have_active_air_problem_) {
                auto closest_wall = BWEB::Walls::getClosestWall(build_tile);
                if (closest_wall) {
                    for (auto &tile : closest_wall->getDefenses()) {
                        if (isPlaceableCUNY(building,tile) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) { // bug is here, need to build and reserve at the same time.
                            unit->stop();
                            CUNYAIModule::buildorder.announceBuildingAttempt(building);
                            return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                        }
                    }
                }
            }

            // simply attempt this if the previous did not find.
            auto closest_station = BWEB::Stations::getClosestStation(build_tile);
            if (closest_station && (closest_station->getDefenseCount() > 1 || CUNYAIModule::friendly_player_model.u_have_active_air_problem_) ) {
                for (auto &tile : closest_station->getDefenseLocations()) {
                    if (isPlaceableCUNY(building, tile) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) { // bug is here, need to build and reserve at the same time.
                        unit->stop();
                        CUNYAIModule::buildorder.announceBuildingAttempt(building);
                        return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                    }
                }
            }

            map<int, set<TilePosition>> viable_placements;
            //Otherwise, use blocks.
            for (auto block : BWEB::Blocks::getBlocks()) {
                set<TilePosition> placements;
                if (building.tileSize() == TilePosition{ 2,2 })
                    placements = block.getSmallTiles();
                if (!placements.empty()) {
                    viable_placements.insert({ unit->getTilePosition().getDistance(block.getTilePosition()), placements });
                }
            }
            for (auto good_block : viable_placements) { // should automatically search by distance.
                for (auto &tile : good_block.second) {
                    if (isPlaceableCUNY(building, tile) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) { // bug is here, need to build and reserve at the same time.
                        CUNYAIModule::buildorder.announceBuildingAttempt(building);
                        unit->stop();
                        return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                    }
                }
            }

        }
        else if (canMakeCUNY(building, false, unit) && building == UnitTypes::Zerg_Extractor) {
            Stored_Resource* closest_gas = CUNYAIModule::getClosestGroundStored(CUNYAIModule::land_inventory, UnitTypes::Resource_Vespene_Geyser, unit_pos);
            if (closest_gas && closest_gas->occupied_resource_ && closest_gas->bwapi_unit_ ) {
                //TilePosition buildPosition = closest_gas->bwapi_unit_->getTilePosition();
                //TilePosition buildPosition = CUNYAIModule::getBuildablePosition(TilePosition(closest_gas->pos_), building, 5);  // Not viable for extractors
                TilePosition tile = Broodwar->getBuildLocation(building, TilePosition(closest_gas->pos_), 5);
                if (CUNYAIModule::my_reservation.addReserveSystem(tile, building)) {  // does not require an isplacable check because it won't pass such a check. It's on top of another object, the geyser.
                    CUNYAIModule::buildorder.announceBuildingAttempt(building);
                    unit->stop();
                    return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                } //extractors must have buildings nearby or we shouldn't build them.

                else if ( BWAPI::Broodwar->isVisible(tile) ) {
                    CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), tile.x, tile.y);
                }
            }
        }
        else if (canMakeCUNY(building,false, unit)) {
            if (CUNYAIModule::checkSafeBuildLoc(unit_pos) ) {
                
                map<int,set<TilePosition>> viable_placements;

                //check walls first
                auto closest_wall = BWEB::Walls::getClosestWall(TilePosition(CUNYAIModule::current_map_inventory.enemy_base_ground_));
                if (closest_wall) {
                    set<TilePosition> placements;
                    if (building.tileSize() == TilePosition{ 2,2 })
                        placements = closest_wall->getSmallTiles();
                    else if (building.tileSize() == TilePosition{ 3 , 2 })
                        placements = closest_wall->getMediumTiles();
                    else if (building.tileSize() == TilePosition{ 4 , 3 })
                        placements = closest_wall->getLargeTiles();
                    if (!placements.empty()) {
                        for (auto &tile : placements) {
                            if (isPlaceableCUNY(building, tile) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) { 
                                    CUNYAIModule::buildorder.announceBuildingAttempt(building);
                                    return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                            }
                        }
                    }
                }

                //Otherwise, use blocks.
                for (auto block : BWEB::Blocks::getBlocks()) {
                    set<TilePosition> placements;
                    if (building.tileSize() == TilePosition{2,2})
                        placements = block.getSmallTiles();
                    else if (building.tileSize() == TilePosition{ 3 , 2 })
                        placements = block.getMediumTiles();
                    else if (building.tileSize() == TilePosition{ 4 , 3 })
                        placements = block.getLargeTiles();
                    if (!placements.empty()) {
                        viable_placements.insert({ unit->getTilePosition().getDistance(block.getTilePosition()), placements });
                    }
                }

                for (auto good_block : viable_placements) { // should automatically search by distance.
                    for (auto &tile : good_block.second) {
                        if (isPlaceableCUNY(building, tile) && CUNYAIModule::my_reservation.addReserveSystem(tile, building)) {
                            CUNYAIModule::buildorder.announceBuildingAttempt(building);
                            unit->stop();
                            return CUNYAIModule::updateUnitBuildIntent(unit, building, tile);
                        }
                    }
                }
            }
        }
    }

    if (CUNYAIModule::buildorder.checkBuilding_Desired(building)) {
        //CUNYAIModule::DiagnosticText("I can't place a %s for you. Freeze here please!...", building.c_str());
        //buildorder.updateRemainingBuildOrder(building); // skips the building.
    }

    return false;
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.  Updates CUNYAIModule::friendly_player_model.units_.
bool AssemblyManager::Check_N_Grow(const UnitType &unittype, const Unit &larva, const bool &extra_critera)
{

    if (CUNYAIModule::checkDesirable(larva, unittype, extra_critera))
    {
        if (larva->morph(unittype)) {
            CUNYAIModule::updateUnitPhase(larva, Stored_Unit::Phase::Morphing);
            return true;
        }
    }

    return false;
}

//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations. Now Updates Friendly inventory when command is sent.
bool AssemblyManager::Expo(const Unit &unit, const bool &extra_critera, Map_Inventory &inv) {
    if (CUNYAIModule::checkDesirable(unit, UnitTypes::Zerg_Hatchery, extra_critera)) {

        int expo_score = -99999999;

        inv.getExpoPositions(); // update the possible expo positions.
        inv.setNextExpo(TilePositions::Origin); // if we find no replacement position, we will know this null postion is never a good build canidate.
        
        //bool safe_worker = CUNYAIModule::enemy_player_model.units_.unit_inventory_.empty() ||
        //    CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, UnitTypes::Zerg_Drone, unit->getPosition(), 500) == nullptr ||
        //    CUNYAIModule::getClosestThreatOrTargetStored(CUNYAIModule::enemy_player_model.units_, UnitTypes::Zerg_Drone, unit->getPosition(), 500)->type_.isWorker();

        int worker_areaID = BWEM::Map::Instance().GetNearestArea(unit->getTilePosition())->Id();

        bool safe_worker = CUNYAIModule::enemy_player_model.units_.getInventoryAtArea(worker_areaID).unit_map_.empty();

        // Let's build at the safest close canidate position.
        if (safe_worker) {
            for (auto &p : inv.expo_positions_) {
                int score_temp = static_cast<int>(std::sqrt(inv.getRadialDistanceOutFromEnemy(Position(p))) - std::sqrt(inv.getRadialDistanceOutFromHome(Position(p)))); // closer is better, further from enemy is better.
                int expo_areaID = BWEM::Map::Instance().GetNearestArea(TilePosition(p))->Id();
                bool safe_expo = CUNYAIModule::checkSafeBuildLoc(Position(p));
                //bool occupied_expo = false;
                //auto nearby_resource = CUNYAIModule::getClosestStored(CUNYAIModule::land_inventory, Position(p), 1000);
                //if (nearby_resource && nearby_resource->occupied_resource_)
                //    occupied_expo = true;


                bool path_available = !BWEM::Map::Instance().GetPath(unit->getPosition(), Position(p)).empty();

                if (!isOccupiedBuildLocation(Broodwar->self()->getRace().getResourceDepot(), p) && score_temp > expo_score && safe_expo && path_available) {
                    expo_score = score_temp;
                    inv.setNextExpo(p);
                    //CUNYAIModule::DiagnosticText("Found an expo at ( %d , %d )", inv.next_expo_.x, inv.next_expo_.y);
                }
            }
        }
        else {
            return false;  // If there's nothing, give up.
        }

        // If we found -something-
        if (inv.next_expo_ && inv.next_expo_ != TilePositions::Origin) {
            if (CUNYAIModule::my_reservation.addReserveSystem(inv.next_expo_, Broodwar->self()->getRace().getResourceDepot())) {
                CUNYAIModule::buildorder.announceBuildingAttempt(Broodwar->self()->getRace().getResourceDepot());
                return CUNYAIModule::updateUnitBuildIntent(unit, Broodwar->self()->getRace().getResourceDepot(), inv.next_expo_);
            }
        }
    } // closure affordablity.

    return false;
}

//Creates a new building with DRONE. Does not create units that morph from other buildings: Lairs, Hives, Greater Spires, or sunken/spores.
bool AssemblyManager::buildBuilding(const Unit &drone) {
    // will send it to do the LAST thing on this list that it can build.
    bool buildings_started = false;
    bool any_macro_problems = CUNYAIModule::friendly_player_model.spending_model_.evalEconPossible() || CUNYAIModule::workermanager.workers_distance_mining_ > 0.0625 * CUNYAIModule::workermanager.min_workers_; // 1/16 workers LD mining is too much.
    bool the_only_macro_hatch_case = (CUNYAIModule::larva_starved && !any_macro_problems && !CUNYAIModule::econ_starved);
    bool upgrade_bool = (CUNYAIModule::tech_starved || (CUNYAIModule::Count_Units(UnitTypes::Zerg_Larva) == 0 && !CUNYAIModule::army_starved));
    bool lurker_tech_progressed = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect);
    bool one_tech_per_base = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) < CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery);
    bool can_upgrade_colonies = (CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0) ||
        (CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0); // There is a building complete that will allow either creep colony upgrade.
    bool enemy_mostly_ground = CUNYAIModule::enemy_player_model.units_.stock_ground_units_ > CUNYAIModule::enemy_player_model.units_.stock_fighting_total_ * 0.75;
    bool nearby_enemy = CUNYAIModule::checkOccupiedNeighborhood(CUNYAIModule::enemy_player_model.units_, drone->getPosition());
    bool drone_death = false;
    int number_of_evos_wanted =
        static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Carapace) > TechManager::returnTechRank(UpgradeTypes::None)) +
            static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Melee_Attacks) > TechManager::returnTechRank(UpgradeTypes::None)) +
                static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Missile_Attacks) > TechManager::returnTechRank(UpgradeTypes::None));
    int number_of_spires_wanted =
        static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Flyer_Carapace) > TechManager::returnTechRank(UpgradeTypes::None)) +
            static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Flyer_Attacks) > TechManager::returnTechRank(UpgradeTypes::None));
    int count_of_spire_decendents = CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire);
    int count_tech_buildings = CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern);

    bool fight_without_reinforcements = CUNYAIModule::army_starved && (CUNYAIModule::larva_starved || CUNYAIModule::supply_starved || CUNYAIModule::gas_starved);


    Unit_Inventory e_loc;
    Unit_Inventory u_loc;

    //if (nearby_enemy) {
        e_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, drone->getPosition());
        u_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::friendly_player_model.units_, drone->getPosition()); 
        e_loc.updateUnitInventorySummary();
        u_loc.updateUnitInventorySummary();
        drone_death = u_loc.unit_map_.find(drone) != u_loc.unit_map_.end() && Stored_Unit::unitDeadInFuture(u_loc.unit_map_.at(drone), 1);
    //}

    // Trust the build order. If there is a build order and it wants a building, build it!
    if (!CUNYAIModule::buildorder.isEmptyBuildOrder()) {
        UnitType next_in_build_order = CUNYAIModule::buildorder.building_gene_.front().getUnit();
        if (!next_in_build_order.isBuilding()) return false; 
        if (next_in_build_order == UnitTypes::Zerg_Hatchery) buildings_started = Expo(drone, false, CUNYAIModule::current_map_inventory);
        else buildings_started = Check_N_Build(next_in_build_order, drone, false);
    }

    ////Combat Buildings are now done on assignUnitAssembly

    //Macro-related Buildings.
    if (!buildings_started) buildings_started = Expo(drone, (any_macro_problems || CUNYAIModule::larva_starved || CUNYAIModule::econ_starved) && !the_only_macro_hatch_case && CUNYAIModule::checkSuperiorFAPForecast(u_loc, e_loc), CUNYAIModule::current_map_inventory);
    //buildings_started = expansion_meaningful; // stop if you need an expo!

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hatchery, drone, the_only_macro_hatch_case); // only macrohatch if you are short on larvae and can afford to spend.

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Extractor, drone,
        !CUNYAIModule::workermanager.excess_gas_capacity_ && CUNYAIModule::gas_starved &&
        CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Extractor) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.  Note that gas_workers count may be off. Sometimes units are in the gas geyser.


    //First Building needed!
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) == 0 && CUNYAIModule::friendly_player_model.units_.resource_depot_count_ > 0);

    //Consider an organized build plan.
    if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && CUNYAIModule::enemy_player_model.units_.flyer_count_ > 0) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not).
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, upgrade_bool &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            CUNYAIModule::current_map_inventory.hatches_ > 1);
    }

    //Muta or lurker for main body of units.
    if (max(returnUnitRank(UnitTypes::Zerg_Lurker), returnUnitRank(UnitTypes::Zerg_Hydralisk)) >= max({ returnUnitRank(UnitTypes::Zerg_Mutalisk), returnUnitRank(UnitTypes::Zerg_Scourge), returnUnitRank(UnitTypes::Zerg_Zergling) })) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not).
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, upgrade_bool && one_tech_per_base &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) == 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 2);
    } else {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool && one_tech_per_base &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) == 0 && CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) == 0 &&
            CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_)> 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 2);
    }
    
    //For your capstone tech:
    if (returnUnitRank(UnitTypes::Zerg_Guardian) == 0 || returnUnitRank(UnitTypes::Zerg_Devourer) == 0 || returnUnitRank(UnitTypes::Zerg_Ultralisk) == 0) {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
            CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
            (CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Hydralisk_Den, CUNYAIModule::friendly_player_model.units_) > 0 || CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) > 0) && // need spire or hydra to tech beyond lair please.
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.
    } 
    else if (returnUnitRank(UnitTypes::Zerg_Ultralisk) == 0) {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Ultralisk_Cavern, drone, upgrade_bool && one_tech_per_base &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) >= 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3);
    } else if (returnUnitRank(UnitTypes::Zerg_Guardian) == 0 || returnUnitRank(UnitTypes::Zerg_Devourer) == 0 || returnUnitRank(UnitTypes::Zerg_Mutalisk) == 0) {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool && one_tech_per_base &&
            CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) == 0 &&
            CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3);
    }


    // Always:
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) < number_of_evos_wanted &&
        Broodwar->self()->gas() > 100 * CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) &&
        Broodwar->self()->minerals() > 100 * CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) &&
        CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        (!have_idle_evos_ || CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0) &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) > count_tech_buildings);

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool &&
        count_of_spire_decendents < number_of_spires_wanted &&
        Broodwar->self()->gas() > 100 * count_of_spire_decendents &&
        Broodwar->self()->minerals() > 100 * count_of_spire_decendents &&
        CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        (!have_idle_spires_ || count_of_spire_decendents == 0) &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) > count_tech_buildings);

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
        CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        (CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Hydralisk_Den, CUNYAIModule::friendly_player_model.units_) > 0 || CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) > 0) && // need spire or hydra to tech beyond lair please.
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

        Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(drone)->second;
        morphing_unit.updateStoredUnit(drone); // don't give him a phase.


        //std::map<UnitType, int> local_map;
        //int sustainable_tech = min( CUNYAIModule::current_map_inventory.hatches_ , CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) );

        //switch (sustainable_tech) {
        //case 0:
        //    local_map = { { UnitTypes::Zerg_Hatchery, 2 },{ UnitTypes::Zerg_Extractor, 1 }, { UnitTypes::Zerg_Spawning_Pool, 1 } };
        //case 1:
        //    local_map = { { UnitTypes::Zerg_Lair, 1 }, { UnitTypes::Zerg_Hatchery, 1 }, { UnitTypes::Zerg_Extractor, 2 }, { UnitTypes::Zerg_Spawning_Pool, 1 } };
        //case 2:
        //    local_map = { { UnitTypes::Zerg_Lair, 1 }, { UnitTypes::Zerg_Hatchery, 4 }, { UnitTypes::Zerg_Extractor, 3 }, { UnitTypes::Zerg_Spawning_Pool, 1 } };
        //case 3:
        //    local_map = { { UnitTypes::Zerg_Hive, 1 }, { UnitTypes::Zerg_Hatchery, 6 }, { UnitTypes::Zerg_Extractor, 4 }, { UnitTypes::Zerg_Spawning_Pool, 1 }, { UnitTypes::Zerg_Queens_Nest, 1 } };
        //default:
        //    local_map = { { UnitTypes::Zerg_Hive, 1 }, { UnitTypes::Zerg_Hatchery, 8 }, { UnitTypes::Zerg_Extractor, 6 }, { UnitTypes::Zerg_Spawning_Pool, 1 }, { UnitTypes::Zerg_Queens_Nest, 1 } };
        //}
        //local_map.merge(core_buildings_);
        //local_map.swap(core_buildings_); // should put all elements into intended buildings with overwrite.

        //switch (returnOptimalUnit(CUNYAIModule::friendly_player_model.combat_unit_cartridge_, CUNYAIModule::friendly_player_model.researches_)) {
        //
        //
        //}


    return buildings_started;
};

// clears all blocking units in an area with size of UT starting at buildtile tile excluding EXCEPTION_UNIT. 
void AssemblyManager::clearBuildingObstuctions(const UnitType &ut, const TilePosition &tile ,const Unit &exception_unit ) {
    Unit_Inventory obstructions = Unit_Inventory( Broodwar->getUnitsInRectangle(Position(tile), Position(tile) + Position(ut.width(), ut.height())) );
    for (auto u = obstructions.unit_map_.begin(); u != obstructions.unit_map_.end() && !obstructions.unit_map_.empty(); u++) {
        if (u->second.bwapi_unit_ && u->second.bwapi_unit_ != exception_unit ) {
            u->second.bwapi_unit_->move({ Position(tile).x + (rand() % 200 - 100) * max(ut.tileWidth() + 1, ut.tileHeight() + 1) * 32, Position(tile).y + (rand() % 200 - 100) * max(ut.tileWidth() + 1, ut.tileHeight() + 1) * 32 });
        }
    }
}

bool AssemblyManager::isPlaceableCUNY(const UnitType &type, const TilePosition &location)
{
    if (isOccupiedBuildLocation(type, location))
        return false;
    // Modifies BWEB's isPlaceable()
    // Placeable is valid if buildable and not overlapping neutrals
    // Note: Must check neutrals due to the terrain below them technically being buildable
        const bool creepCheck = type.requiresCreep();
    for (auto x = location.x; x < location.x + type.tileWidth(); x++) {

        if (creepCheck) {
            TilePosition tile(x, location.y + 2);
            if (!Broodwar->hasCreep(tile) || !Broodwar->hasCreep(location))
                return false;
        }

        for (auto y = location.y; y < location.y + type.tileHeight(); y++) {
            TilePosition tile(x, y);
            if (!tile.isValid()
                || !Broodwar->isBuildable(tile)
                || !Broodwar->isWalkable(WalkPosition(tile))
                || (type.isResourceDepot() && !Broodwar->canBuildHere(tile, type)))
                return false;
        }
    }

    return true;

}
bool AssemblyManager::isOccupiedBuildLocation(const UnitType &type, const TilePosition &location) {
    auto units_in_area = Broodwar->getUnitsInRectangle(Position(location), Position(location) + Position(type.width(), type.height()));
    if (!units_in_area.empty()) {
        for (auto u : units_in_area) {
            if (!u->getType().canMove())
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


bool AssemblyManager::Reactive_BuildFAP(const Unit &morph_canidate) {

    //Am I sending this command to a larva or a hydra?
    UnitType u_type = morph_canidate->getType();
    bool is_larva = u_type == UnitTypes::Zerg_Larva;
    bool is_hydra = u_type == UnitTypes::Zerg_Hydralisk;
    bool is_muta = u_type == UnitTypes::Zerg_Mutalisk;
    bool is_building = false;

    if (CUNYAIModule::buildorder.checkBuilding_Desired(UnitTypes::Zerg_Lurker) && CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk) == 0) {
        CUNYAIModule::buildorder.retryBuildOrderElement(UnitTypes::Zerg_Hydralisk); // force in an hydra if
        CUNYAIModule::DiagnosticText("Reactionary Hydralisk. Must have lost one.");
        return true;
    }

    //Let us simulate some combat.
    if (!CUNYAIModule::buildorder.isEmptyBuildOrder() || CUNYAIModule::army_starved || (CUNYAIModule::tech_starved && Broodwar->self()->gas() > 300 && Broodwar->self()->minerals() > 300)) {
        is_building = AssemblyManager::buildOptimalCombatUnit(morph_canidate, assembly_cycle_);
    }

    return is_building;

}

bool AssemblyManager::buildStaticDefence(const Unit &morph_canidate) {

    if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Spore_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Sunken_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    bool must_make_spore = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Spore_Colony, true);
    bool must_make_sunken = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Sunken_Colony, true);

    if (CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && must_make_spore) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (!CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && must_make_sunken) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    return false;
}

//contains a filter to discard unbuildable sorts of units, then finds the best unit via a series of BuildFAP sim, then builds it. Passes by copy so I can mutilate the values.
bool AssemblyManager::buildOptimalCombatUnit(const Unit &morph_canidate, map<UnitType, int> combat_types) {
    bool building_optimal_unit = false;
    int best_sim_score = INT_MIN;
    UnitType build_type = UnitTypes::None;

    // drop all units types I cannot assemble at this time.
    auto pt_type = combat_types.begin();
    while (pt_type != combat_types.end()) {
        bool can_make_or_already_is = morph_canidate->getType() == pt_type->first || CUNYAIModule::checkDesirable( morph_canidate, pt_type->first, true);
        bool is_larva = morph_canidate->getType() == UnitTypes::Zerg_Larva;
        bool can_morph_into_prerequisite_hydra = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Hydralisk, true) && CUNYAIModule::checkDesirable(UnitTypes::Zerg_Lurker , true) && pt_type->first == UnitTypes::Zerg_Lurker;
        bool can_morph_into_prerequisite_muta = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Mutalisk, true) && ( (CUNYAIModule::checkDesirable(UnitTypes::Zerg_Guardian, true) && (pt_type->first == UnitTypes::Zerg_Guardian) || (CUNYAIModule::checkDesirable(UnitTypes::Zerg_Guardian, true) && pt_type->first == UnitTypes::Zerg_Devourer)));


        if (can_make_or_already_is || (is_larva && can_morph_into_prerequisite_hydra) || (is_larva && can_morph_into_prerequisite_muta)) {
            //CUNYAIModule::DiagnosticText("Considering morphing a %s", pt_type->first.c_str());
            pt_type++;
        }
        else {
            combat_types.erase(pt_type++);
        }
    }


    //Heuristic classes for building. They are pretty simple combat results from a simulation.
    bool it_needs_to_shoot_up = false;
    bool it_needs_to_shoot_down = false;
    bool it_needs_to_fly = false;
    bool too_many_scourge = false;
    bool required = false;

    // Check if unit is even feasible, or the unit already IS that type, or is needed for that type.
    auto potential_type = combat_types.begin();
    while (potential_type != combat_types.end() ) {
        bool can_morph_into_prerequisite_hydra = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Hydralisk, true) && CUNYAIModule::checkDesirable(UnitTypes::Zerg_Lurker, true) && potential_type->first == UnitTypes::Zerg_Lurker;
        bool can_morph_into_prerequisite_muta = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Mutalisk, true) && ( (CUNYAIModule::checkDesirable(UnitTypes::Zerg_Guardian, true) && potential_type->first == UnitTypes::Zerg_Guardian) || (CUNYAIModule::checkDesirable(UnitTypes::Zerg_Guardian, true) && potential_type->first == UnitTypes::Zerg_Devourer));
        if (CUNYAIModule::checkDesirable(morph_canidate, potential_type->first, true) || morph_canidate->getType() == potential_type->first || can_morph_into_prerequisite_hydra || can_morph_into_prerequisite_muta) potential_type++;
        else combat_types.erase(potential_type++);
    }

    // Check if we have a member of each of the classes.
    bool up_shooting_class = false;
    bool down_shooting_class = false;
    bool flying_class = false;

    for (auto &potential_type : combat_types) {
        if (potential_type.first.airWeapon() != WeaponTypes::None)  up_shooting_class = true;
        if (potential_type.first.groundWeapon() != WeaponTypes::None )  down_shooting_class = true;
        if (potential_type.first.isFlyer() && CUNYAIModule::friendly_player_model.e_has_air_vunerability_)  flying_class = true;
    }

    // Identify desirable unit classes prior to simulation.
    for (auto &potential_type : combat_types) {
        if (potential_type.first.airWeapon() != WeaponTypes::None && CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && up_shooting_class)  it_needs_to_shoot_up = true; // can't build things that shoot up if you don't have the gas or larva.
        if (potential_type.first.groundWeapon() != WeaponTypes::None && !CUNYAIModule::friendly_player_model.u_have_active_air_problem_ && down_shooting_class)  it_needs_to_shoot_down = true;
        if (potential_type.first.isFlyer() && CUNYAIModule::friendly_player_model.e_has_air_vunerability_ && flying_class) it_needs_to_fly = true;
        if (potential_type.first == UnitTypes::Zerg_Scourge && CUNYAIModule::enemy_player_model.units_.flyer_count_ <= CUNYAIModule::Count_Units(UnitTypes::Zerg_Scourge) )  too_many_scourge = true;
    }

    // remove undesireables.
    auto potential_type2 = combat_types.begin();
    while (potential_type2 != combat_types.end()) {
        if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, potential_type2->first)) potential_type2++;
        else if (potential_type2->first == UnitTypes::Zerg_Scourge && too_many_scourge)  combat_types.erase(potential_type2++);
        else if (potential_type2->first.groundWeapon() == WeaponTypes::None && it_needs_to_shoot_down) combat_types.erase(potential_type2++);
        else if (potential_type2->first.airWeapon() == WeaponTypes::None && it_needs_to_shoot_up) combat_types.erase(potential_type2++);
        else if (!potential_type2->first.isFlyer() && it_needs_to_fly) combat_types.erase(potential_type2++);

        else potential_type2++;
    }

    if (combat_types.empty()) return false;
    //else if (combat_types.size() == 1) build_type = combat_types.begin()->first;
    else build_type = returnOptimalUnit(combat_types, CUNYAIModule::friendly_player_model.researches_);


    //A catch for prerequisite build units.
    bool morph_into_prerequisite_hydra = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Hydralisk, true) && build_type == UnitTypes::Zerg_Lurker && morph_canidate->getType() == UnitTypes::Zerg_Larva;
    bool morph_into_prerequisite_muta = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Mutalisk, true) && (build_type == UnitTypes::Zerg_Guardian || build_type == UnitTypes::Zerg_Devourer) && morph_canidate->getType() == UnitTypes::Zerg_Larva;
    if (morph_into_prerequisite_hydra) building_optimal_unit = Check_N_Grow(UnitTypes::Zerg_Hydralisk, morph_canidate, true);
    else if (morph_into_prerequisite_muta) building_optimal_unit = Check_N_Grow(UnitTypes::Zerg_Mutalisk, morph_canidate, true);

    // Build it.
    if (!building_optimal_unit && morph_canidate->getType() != build_type) building_optimal_unit = Check_N_Grow(build_type, morph_canidate, true); // catchall ground units, in case you have a BO that needs to be done.
    if (building_optimal_unit || morph_canidate->getType() == build_type) {
        //if (Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Best sim score is: %d, building %s", best_sim_score, build_type.c_str());
        return true;
    }
    return false;
}

//Simply returns the unittype that is the "best" of a BuildFAP sim.
UnitType AssemblyManager::returnOptimalUnit(const map<UnitType, int> combat_types, const Research_Inventory &ri) {
    int best_sim_score = INT_MIN; // Optimal unit must be better than the others.  Using UnitTypes::None leads to intermittent freezes when the options are collectively bad.
    UnitType build_type = UnitTypes::None;

    for (auto &potential_type : combat_types) {
        if (potential_type.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_type.second;
            build_type = potential_type.first;
            //CUNYAIModule::DiagnosticText("Found a Best_sim_score of %d, for %s", best_sim_score, build_type.c_str());
        }
        else if (potential_type.second == best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            //build_type =
            if(build_type.airWeapon() != WeaponTypes::None && build_type.groundWeapon() != WeaponTypes::None) continue; // if the current unit is "flexible" with regard to air and ground units, then keep it and continue to consider the next unit.
            else if(potential_type.first.airWeapon() != WeaponTypes::None && potential_type.first.groundWeapon() != WeaponTypes::None) build_type = potential_type.first; // if the tying unit is "flexible", then let's use that one.
            //CUNYAIModule::DiagnosticText("Found a tie, favoring the flexible unit %d, for %s", best_sim_score, build_type.c_str());
        }
    }

    return build_type;

}

//Simply returns the unittype that is the "best" of a BuildFAP sim.
int AssemblyManager::returnUnitRank(const UnitType &ut) {
    int postion_in_line = 0;
    multimap<int, UnitType> sorted_list;
    for (auto it : assembly_cycle_) {
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

//Updates the assembly cycle to consider the value of each unit.
void AssemblyManager::updateOptimalCombatUnit() {
    bool building_optimal_unit = false;
    Position comparision_spot = Unit_Inventory::positionBuildFap(true);// all compared units should begin in the exact same position.

    FAP::FastAPproximation<Stored_Unit*> buildFAP; // attempting to integrate FAP into building decisions.
    CUNYAIModule::friendly_player_model.units_.addToBuildFAP(buildFAP, true, CUNYAIModule::friendly_player_model.researches_);
    CUNYAIModule::enemy_player_model.units_.addToBuildFAP(buildFAP, false, CUNYAIModule::enemy_player_model.researches_);
    //add friendly units under consideration to FAP in loop, resetting each time.
    for (auto &potential_type : assembly_cycle_) {
        //if (CUNYAIModule::checkDesirable(potential_type.first, true) || assembly_cycle_[potential_type.first] != 0 || potential_type.first == UnitTypes::None) { // while this runs faster, it will potentially get biased towards lings and hydras and other lower-cost units?
            Stored_Unit su = Stored_Unit(potential_type.first);
            Unit_Inventory friendly_units_under_consideration; // new every time.
            auto buildFAP_copy = buildFAP;
            friendly_units_under_consideration.addStored_Unit(su); //add unit we are interested in to the inventory:
            if (potential_type.first.isTwoUnitsInOneEgg()) friendly_units_under_consideration.addStored_Unit(su); // do it twice if you're making 2.
            friendly_units_under_consideration.addToFAPatPos(buildFAP_copy, comparision_spot, true, CUNYAIModule::friendly_player_model.researches_);
            buildFAP_copy.simulate(FAP_SIM_DURATION); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
            int score = CUNYAIModule::getFAPScore(buildFAP_copy, true) - CUNYAIModule::getFAPScore(buildFAP_copy, false);
            if (assembly_cycle_.find(potential_type.first) == assembly_cycle_.end()) assembly_cycle_[potential_type.first] = score;
            else assembly_cycle_[potential_type.first] = static_cast<int>( (23.0 * assembly_cycle_[potential_type.first] + score) / 24); //moving average over 24 simulations, 1 seconds.
    //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("have a sim score of %d, for %s", assembly_cycle_.find(potential_type.first)->second, assembly_cycle_.find(potential_type.first)->first.c_str());
    }

    have_idle_evos_ = false;
    have_idle_spires_ = false;
    for (auto upgrader : CUNYAIModule::friendly_player_model.units_.unit_map_) { // should only run this 1x per frame.
        if (upgrader.second.type_ == UnitTypes::Zerg_Evolution_Chamber /*&& upgrader.second.build_type_*/ && upgrader.second.phase_ == Stored_Unit::None) have_idle_evos_ = true;
        if (upgrader.second.type_ == UnitTypes::Zerg_Spire /*&& upgrader.second.build_type_*/ && upgrader.second.phase_ == Stored_Unit::None) have_idle_spires_ = true;
        if (upgrader.second.type_ == UnitTypes::Zerg_Greater_Spire /*&& upgrader.second.build_type_*/ && upgrader.second.phase_ == Stored_Unit::None) have_idle_spires_ = true;
    }
}


//Returns true if (players) units would do more damage if they only shot up. Player is self (if true) or to the enemy (if false). 
bool AssemblyManager::testActiveAirProblem(const Research_Inventory &ri, const bool &test_for_self_weakness) {

    int benifit_of_shooting_air_targets = 0;
    int benifit_of_shooting_ground_targets = 0;

    UnitType build_type = UnitTypes::None;
    Position comparision_spot = Unit_Inventory::positionBuildFap(true);// all compared units should begin in the exact same position.
                                                       //add friendly units under consideration to FAP in loop, resetting each time.

    FAP::FastAPproximation<Stored_Unit*> buildFAP; // attempting to integrate FAP into building decisions.

    Unit_Inventory potentially_weak_team;
    Research_Inventory potentially_weak_team_researches;
    Unit_Inventory team_creating_problems;
    Research_Inventory team_creating_problems_researches;

    if (test_for_self_weakness) {
        potentially_weak_team = CUNYAIModule::friendly_player_model.units_;
        potentially_weak_team_researches = CUNYAIModule::friendly_player_model.researches_;
        team_creating_problems = CUNYAIModule::enemy_player_model.units_;
        team_creating_problems_researches = CUNYAIModule::enemy_player_model.researches_;

    }
    else {
        potentially_weak_team = CUNYAIModule::enemy_player_model.units_;
        potentially_weak_team_researches = CUNYAIModule::enemy_player_model.researches_;
        team_creating_problems = CUNYAIModule::friendly_player_model.units_;
        team_creating_problems_researches = CUNYAIModule::friendly_player_model.researches_;
    }

    potentially_weak_team.addToBuildFAP(buildFAP, true, potentially_weak_team_researches);
    team_creating_problems.addToBuildFAP(buildFAP, false, team_creating_problems_researches);

    auto buildfap_temp = buildFAP; // restore the buildfap temp.

    if (team_creating_problems.flyer_count_ > 0) {
        // test sunkens
        buildfap_temp.clear();
        buildfap_temp = buildFAP; // restore the buildfap temp.
        Stored_Unit su = Stored_Unit(UnitTypes::Zerg_Sunken_Colony);
        // enemy units do not change.
        Unit_Inventory friendly_units_under_consideration; // new every time.
        for (int i = 0; i < 5; ++i) {
            friendly_units_under_consideration.addStored_Unit(su); //add unit we are interested in to the inventory:
        }
        friendly_units_under_consideration.addToFAPatPos(buildfap_temp, comparision_spot, true, ri);
        buildfap_temp.simulate(FAP_SIM_DURATION); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
        benifit_of_shooting_ground_targets = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false);
        buildfap_temp.clear();
        //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());


        // test fake anti-air sunkens
        buildfap_temp.clear();
        buildfap_temp = buildFAP; // restore the buildfap temp.
        // enemy units do not change.
        Unit_Inventory friendly_units_under_consideration2; // new every time.
        for (int i = 0; i < 5; ++i) {
            friendly_units_under_consideration2.addStored_Unit(su); //add unit we are interested in to the inventory:
        }
        friendly_units_under_consideration2.addAntiAirToFAPatPos(buildfap_temp, comparision_spot, true, ri);
        buildfap_temp.simulate(FAP_SIM_DURATION); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
        benifit_of_shooting_air_targets = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false); //The spore colony is just a placeholder.
        buildfap_temp.clear();
        //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());
        return benifit_of_shooting_air_targets >= benifit_of_shooting_ground_targets;
    }

    return false;
}

//Returns true if (players) units would do more damage if they flew. Player is self (if true) or to the enemy (if false). 
bool AssemblyManager::testPotentialAirVunerability(const Research_Inventory &ri, const bool &test_for_self_weakness) {
    Position comparision_spot = Unit_Inventory::positionBuildFap(true);// all compared units should begin in the exact same position.
                                                                       //add friendly units under consideration to FAP in loop, resetting each time.

    FAP::FastAPproximation<Stored_Unit*> buildFAP; // attempting to integrate FAP into building decisions.
    int value_of_flyers = 0;
    int value_of_ground = 0;
    Unit_Inventory potentially_weak_team;
    Research_Inventory potentially_weak_team_researches;
    Unit_Inventory team_creating_problems;
    Research_Inventory team_creating_problems_researches;

    if (test_for_self_weakness) {
        potentially_weak_team = CUNYAIModule::friendly_player_model.units_;
        potentially_weak_team_researches = CUNYAIModule::friendly_player_model.researches_;
        team_creating_problems = CUNYAIModule::enemy_player_model.units_;
        team_creating_problems_researches = CUNYAIModule::enemy_player_model.researches_;
    }
    else {
        potentially_weak_team = CUNYAIModule::enemy_player_model.units_;
        potentially_weak_team_researches = CUNYAIModule::enemy_player_model.researches_;
        team_creating_problems = CUNYAIModule::friendly_player_model.units_;
        team_creating_problems_researches = CUNYAIModule::friendly_player_model.researches_;
    }

    potentially_weak_team.addToBuildFAP(buildFAP, true, potentially_weak_team_researches);
    team_creating_problems.addToBuildFAP(buildFAP, false, team_creating_problems_researches);

    auto buildfap_temp = buildFAP; // restore the buildfap temp.

    // test ground hydras.
    buildfap_temp.clear();
    buildfap_temp = buildFAP; // restore the buildfap temp.
    Stored_Unit su = Stored_Unit(UnitTypes::Zerg_Hydralisk);
    // enemy units do not change.
    Unit_Inventory friendly_units_under_consideration; // new every time.
    for (int i = 0; i < 5; ++i) {
        friendly_units_under_consideration.addStored_Unit(su); //add unit we are interested in to the inventory:
    }
    friendly_units_under_consideration.addToFAPatPos(buildfap_temp, comparision_spot, true, ri);
    buildfap_temp.simulate(24 * 5); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
    value_of_ground = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false);
    buildfap_temp.clear();
    //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());


    // test flying hydras.
    buildfap_temp.clear();
    buildfap_temp = buildFAP; // restore the buildfap temp.
                              // enemy units do not change.
    Unit_Inventory friendly_units_under_consideration2; // new every time.
    for (int i = 0; i < 5; ++i) {
        friendly_units_under_consideration2.addStored_Unit(su); //add unit we are interested in to the inventory:
    }
    friendly_units_under_consideration2.addFlyingToFAPatPos(buildfap_temp, comparision_spot, true, ri);
    buildfap_temp.simulate(24 * 5); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
    value_of_flyers = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false);
    buildfap_temp.clear();
    //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());


    return value_of_flyers >= value_of_ground;
}

// Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void AssemblyManager::Print_Assembly_FAP_Cycle(const int &screen_x, const int &screen_y) {
    int another_sort_of_unit = 0;
    multimap<int, UnitType> sorted_list;
    for (auto it : assembly_cycle_) {
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

    larva_bank_.unit_map_.clear();
    hydra_bank_.unit_map_.clear();
    muta_bank_.unit_map_.clear();
    builder_bank_.unit_map_.clear();
    creep_colony_bank_.unit_map_.clear();
    production_facility_bank_.unit_map_.clear();

    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Larva) larva_bank_.addStored_Unit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Hydralisk) hydra_bank_.addStored_Unit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Mutalisk) muta_bank_.addStored_Unit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Creep_Colony) creep_colony_bank_.addStored_Unit(u.second);
        if (u.second.type_ == Broodwar->self()->getRace().getWorker()) builder_bank_.addStored_Unit(u.second);
        if (u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery)) production_facility_bank_.addStored_Unit(u.second);
    }

    larva_bank_.updateUnitInventorySummary();
    hydra_bank_.updateUnitInventorySummary();
    muta_bank_.updateUnitInventorySummary();
    builder_bank_.updateUnitInventorySummary();
    creep_colony_bank_.updateUnitInventorySummary();
    production_facility_bank_.updateUnitInventorySummary();

}

bool AssemblyManager::creepColonyInArea(const Position & pos) {
    bool creep_colony_nearby = false;
    Unit_Inventory units_loc = CUNYAIModule::getUnitInventoryInArea(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Creep_Colony, pos);
    creep_colony_nearby = !units_loc.unit_map_.empty();
    return creep_colony_nearby;
}


bool AssemblyManager::assignUnitAssembly()
{
    Unit_Inventory overlord_larva;
    Unit_Inventory immediate_drone_larva;
    Unit_Inventory transfer_drone_larva;
    Unit_Inventory combat_creators;

    for (auto hatch : production_facility_bank_.unit_map_) {
        auto e_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::enemy_player_model.units_, hatch.first->getPosition());
        auto u_loc = CUNYAIModule::getUnitInventoryInNeighborhood(CUNYAIModule::friendly_player_model.units_, hatch.first->getPosition());
        e_loc.updateUnitInventorySummary();
        u_loc.updateUnitInventorySummary();
        if (!CUNYAIModule::checkSuperiorFAPForecast(u_loc, e_loc)) {
            CUNYAIModule::Diagnostic_Dot(hatch.second.pos_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Red);
            //CUNYAIModule::DiagnosticText("Danger, Will Robinson! (%d, %d)", hatch.second.pos_.x, hatch.second.pos_.y);

            bool can_upgrade_colonies = (CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0) ||
                (CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0); // There is a building complete that will allow either creep colony upgrade.

            Stored_Unit * drone = CUNYAIModule::getClosestStored(u_loc, Broodwar->self()->getRace().getWorker(), hatch.second.pos_, 999999);
            if (drone && drone->bwapi_unit_ && CUNYAIModule::spamGuard(drone->bwapi_unit_)) {
                Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone->bwapi_unit_,  CUNYAIModule::Count_Units(UnitTypes::Zerg_Creep_Colony) * 50 + 50 <= CUNYAIModule::my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
                    can_upgrade_colonies &&
                    CUNYAIModule::current_map_inventory.hatches_ > 1 &&
                    CUNYAIModule::Count_Units(UnitTypes::Zerg_Sunken_Colony) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spore_Colony) < max((CUNYAIModule::current_map_inventory.hatches_ * (CUNYAIModule::current_map_inventory.hatches_ + 1)) / 2, 6)); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
            }
        }
    }

    if (last_frame_of_creep_command < Broodwar->getFrameCount() - 12) {
        for (auto creep_colony : creep_colony_bank_.unit_map_) {
            buildStaticDefence(creep_colony.first); // checks globally but not bad, info is mostly already there.
        }
        last_frame_of_creep_command = Broodwar->getFrameCount();
    }
    //Assess the units and sort them into bins.
    if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto larva : larva_bank_.unit_map_) {
            if (!CUNYAIModule::checkUnitTouchable(larva.first)) continue;
            //Workers check
            // Is everywhere ok for workers?
            // Which larva should be morphed first?
            bool wasting_larva_soon = true;
            bool hatch_wants_drones = true;
            bool prep_for_transfer = true;

            if (larva.first->getHatchery()) {
                wasting_larva_soon = larva.first->getHatchery()->getRemainingTrainTime() < 5 + Broodwar->getLatencyFrames() && larva.first->getHatchery()->getLarva().size() == 3 && CUNYAIModule::land_inventory.getLocalMinPatches() > 8; // no longer will spam units when I need a hatchery.
                Resource_Inventory local_resources = CUNYAIModule::getResourceInventoryInArea(CUNYAIModule::land_inventory, larva.first->getHatchery()->getPosition());
                local_resources.updateMines();
                hatch_wants_drones = 2 * local_resources.getLocalMinPatches() + 3 * local_resources.getLocalRefineries() > local_resources.getLocalMiners() + local_resources.getLocalGasCollectors();
                prep_for_transfer = CUNYAIModule::Count_Units_In_Progress(Broodwar->self()->getRace().getResourceDepot()) > 0 || CUNYAIModule::my_reservation.checkTypeInReserveSystem(Broodwar->self()->getRace().getResourceDepot());
            }

            bool enough_drones_globally = (CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) > CUNYAIModule::land_inventory.getLocalMinPatches() * 2 + CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) * 3 + 1) || CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) >= 85;
            bool drone_conditional = (CUNYAIModule::econ_starved || CUNYAIModule::tech_starved); // Econ does not detract from technology growth. (only minerals, gas is needed for tech). Always be droning.

            bool drones_are_needed_here = (drone_conditional || wasting_larva_soon) && !enough_drones_globally && hatch_wants_drones;
            bool drones_are_needed_elsewhere = (drone_conditional || wasting_larva_soon) && !enough_drones_globally && !hatch_wants_drones && prep_for_transfer;
            bool found_noncombat_use = false;

            if (drones_are_needed_here || CUNYAIModule::checkFeasibleRequirement(larva.first,UnitTypes::Zerg_Drone)) {
                immediate_drone_larva.addStored_Unit(larva.second);
                found_noncombat_use = true;
            }
            if (drones_are_needed_elsewhere || CUNYAIModule::checkFeasibleRequirement(larva.first, UnitTypes::Zerg_Drone)) {
                transfer_drone_larva.addStored_Unit(larva.second);
                found_noncombat_use = true;
            }
            if (CUNYAIModule::supply_starved || CUNYAIModule::checkFeasibleRequirement(larva.first, UnitTypes::Zerg_Overlord)) {
                overlord_larva.addStored_Unit(larva.second);
                found_noncombat_use = true;
            }
            combat_creators.addStored_Unit(larva.second);// needs to be clear so we can consider building combat units whenever they are required.
        }
    }

    if (last_frame_of_hydra_morph_command < Broodwar->getFrameCount() - 12) {
        bool lurkers_permissable = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
        for (auto potential_lurker : hydra_bank_.unit_map_) {
            if (!CUNYAIModule::checkUnitTouchable(potential_lurker.first) && potential_lurker.second.phase_ != Stored_Unit::Attacking) continue;
            if (static_cast<bool>(potential_lurker.second.time_since_last_dmg_ > FAP_SIM_DURATION)) combat_creators.addStored_Unit(potential_lurker.second);
        }
    }

    if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12) {
        bool endgame_fliers_permissable = CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Greater_Spire) > 0;
        for (auto potential_endgame_flier : muta_bank_.unit_map_) {
            if (!CUNYAIModule::checkUnitTouchable(potential_endgame_flier.first) && potential_endgame_flier.second.phase_ != Stored_Unit::Attacking && potential_endgame_flier.second.phase_ != Stored_Unit::Retreating) continue;
            if (static_cast<bool>(potential_endgame_flier.second.time_since_last_dmg_ > FAP_SIM_DURATION) && endgame_fliers_permissable) combat_creators.addStored_Unit(potential_endgame_flier.second);
        }
    }

    //Build from the units as needed: priority queue should be as follows. Note some of these are mutually exclusive.
    //Unit_Inventory overlord_larva;
    //Unit_Inventory immediate_drone_larva;
    //Unit_Inventory transfer_drone_larva;
    //Unit_Inventory combat_creators;

    if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12) {
        for (auto o : overlord_larva.unit_map_) {
            if (Check_N_Grow(UnitTypes::Zerg_Overlord, o.first, true)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
        for (auto d : immediate_drone_larva.unit_map_) {
            if (Check_N_Grow(UnitTypes::Zerg_Drone, d.first, CUNYAIModule::econ_starved || CUNYAIModule::tech_starved)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
        for (auto d : transfer_drone_larva.unit_map_) {
            if (Check_N_Grow(UnitTypes::Zerg_Drone, d.first, CUNYAIModule::econ_starved || CUNYAIModule::tech_starved)) {
                last_frame_of_larva_morph_command = Broodwar->getFrameCount();
                return true;
            }
        }
    }

    for (auto c : combat_creators.unit_map_) {
        if (Reactive_BuildFAP(c.first)) {
            if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Hydralisk) last_frame_of_hydra_morph_command = Broodwar->getFrameCount();
            if (last_frame_of_hydra_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Mutalisk) last_frame_of_muta_morph_command = Broodwar->getFrameCount();
            if (last_frame_of_larva_morph_command < Broodwar->getFrameCount() - 12 && c.second.type_ == UnitTypes::Zerg_Larva) last_frame_of_larva_morph_command = Broodwar->getFrameCount();
            return true;
        }
    }

    return false;
}

void AssemblyManager::clearSimulationHistory()
{
    assembly_cycle_ = CUNYAIModule::friendly_player_model.combat_unit_cartridge_;
    for (auto unit : assembly_cycle_) {
        unit.second = 0;
    }
    assembly_cycle_.insert({ UnitTypes::None, 0 });
}

void AssemblyManager::getDefensiveWalls()
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

bool CUNYAIModule::checkInCartridge(const UnitType &ut) {
    return friendly_player_model.combat_unit_cartridge_.find(ut) != friendly_player_model.combat_unit_cartridge_.end() || friendly_player_model.building_cartridge_.find(ut) != friendly_player_model.building_cartridge_.end() || friendly_player_model.eco_unit_cartridge_.find(ut) != friendly_player_model.eco_unit_cartridge_.end();
}

bool CUNYAIModule::checkInCartridge(const UpgradeType &ut) {
    return friendly_player_model.upgrade_cartridge_.find(ut) != friendly_player_model.upgrade_cartridge_.end();
}

bool CUNYAIModule::checkInCartridge(const TechType &ut) {
    return friendly_player_model.tech_cartridge_.find(ut) != friendly_player_model.tech_cartridge_.end();
}

bool CUNYAIModule::checkDesirable(const Unit &unit, const UnitType &ut, const bool &extra_criteria) {
    return Broodwar->canMake(ut, unit) && my_reservation.checkAffordablePurchase(ut, 3 + 10 * ut == Broodwar->self()->getRace().getResourceDepot()) && checkInCartridge(ut) && (buildorder.checkBuilding_Desired(ut) || (extra_criteria && buildorder.isEmptyBuildOrder()));
}

bool CUNYAIModule::checkDesirable(const UpgradeType &ut, const bool &extra_criteria) {
    return Broodwar->canUpgrade(ut) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && (buildorder.checkUpgrade_Desired(ut) || (extra_criteria && buildorder.isEmptyBuildOrder()));
}

bool CUNYAIModule::checkDesirable(const Unit &unit, const UpgradeType &up, const bool &extra_criteria) {
    if (unit && up && up != UpgradeTypes::None) return unit->canUpgrade(up) && my_reservation.checkAffordablePurchase(up) && checkInCartridge(up) && (buildorder.checkUpgrade_Desired(up) || (extra_criteria && buildorder.isEmptyBuildOrder()));
    return false;
}

// checks if player wants the unit, in general.
bool CUNYAIModule::checkDesirable(const UnitType &ut, const bool &extra_criteria) {
    return Broodwar->canMake(ut) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && (buildorder.checkBuilding_Desired(ut) || (extra_criteria && buildorder.isEmptyBuildOrder()));
}

bool CUNYAIModule::checkFeasibleRequirement(const Unit &unit, const UnitType &ut) {
    return Broodwar->canMake(ut, unit) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && buildorder.checkBuilding_Desired(ut);
}

bool CUNYAIModule::checkFeasibleRequirement(const Unit &unit, const UpgradeType &up) {
    return Broodwar->canUpgrade(up, unit) && my_reservation.checkAffordablePurchase(up) && checkInCartridge(up) && buildorder.checkUpgrade_Desired(up);
}

void Building_Gene::updateRemainingBuildOrder(const Unit &u) {
    if (!building_gene_.empty()) {
        if (building_gene_.front().getUnit() == u->getType()) {
            building_gene_.erase(building_gene_.begin());
        }
    }
}

void Building_Gene::updateRemainingBuildOrder(const UnitType &ut) {
    if (!building_gene_.empty()) {
        if (building_gene_.front().getUnit() == ut) {
            building_gene_.erase(building_gene_.begin());
        }
    }
}

void Building_Gene::updateRemainingBuildOrder(const UpgradeType &ups) {
    if (!building_gene_.empty()) {
        if (building_gene_.front().getUpgrade() == ups) {
            building_gene_.erase(building_gene_.begin());
        }
    }
}

void Building_Gene::updateRemainingBuildOrder(const TechType &research) {
    if (!building_gene_.empty()) {
        if (building_gene_.front().getResearch() == research) {
            building_gene_.erase(building_gene_.begin());
        }
    }
}

void Building_Gene::announceBuildingAttempt(UnitType ut) {
    if ( ut.isBuilding() ) {
        last_build_order = ut;
        CUNYAIModule::DiagnosticText("Building a %s", last_build_order.c_str());
    }
}

bool Building_Gene::checkBuilding_Desired(UnitType ut) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return !building_gene_.empty() && building_gene_.front().getUnit() == ut;
}

bool Building_Gene::checkUpgrade_Desired(UpgradeType upgrade) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return !building_gene_.empty() && building_gene_.front().getUpgrade() == upgrade;
}

bool Building_Gene::checkResearch_Desired(TechType research) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return !building_gene_.empty() && building_gene_.front().getResearch() == research;
}

bool Building_Gene::isEmptyBuildOrder() {
    return building_gene_.empty();
}

void Building_Gene::addBuildOrderElement(const UpgradeType & ups)
{
    building_gene_.push_back(Build_Order_Object(ups));
}

void Building_Gene::addBuildOrderElement(const TechType & research)
{
    building_gene_.push_back(Build_Order_Object(research));
}

void Building_Gene::addBuildOrderElement(const UnitType & ut)
{
    building_gene_.push_back(Build_Order_Object(ut));
}

void Building_Gene::retryBuildOrderElement(const UnitType & ut)
{
    building_gene_.insert(building_gene_.begin(), Build_Order_Object(ut));
}

void Building_Gene::getInitialBuildOrder(string s) {

	building_gene_.clear();

    initial_building_gene_ = s;

    std::stringstream ss(s);
    std::istream_iterator<std::string> begin(ss);
    std::istream_iterator<std::string> end;
    std::vector<std::string> build_string(begin, end);

    Build_Order_Object hatch = Build_Order_Object(UnitTypes::Zerg_Hatchery);
    Build_Order_Object extract = Build_Order_Object(UnitTypes::Zerg_Extractor);
    Build_Order_Object drone = Build_Order_Object(UnitTypes::Zerg_Drone);
    Build_Order_Object ovi = Build_Order_Object(UnitTypes::Zerg_Overlord);
    Build_Order_Object pool = Build_Order_Object(UnitTypes::Zerg_Spawning_Pool);
    Build_Order_Object evo = Build_Order_Object(UnitTypes::Zerg_Evolution_Chamber);
    Build_Order_Object speed = Build_Order_Object(UpgradeTypes::Metabolic_Boost);
    Build_Order_Object ling = Build_Order_Object(UnitTypes::Zerg_Zergling);
    Build_Order_Object creep = Build_Order_Object(UnitTypes::Zerg_Creep_Colony);
    Build_Order_Object sunken = Build_Order_Object(UnitTypes::Zerg_Sunken_Colony);
    Build_Order_Object spore = Build_Order_Object(UnitTypes::Zerg_Spore_Colony);
    Build_Order_Object lair = Build_Order_Object(UnitTypes::Zerg_Lair);
    Build_Order_Object hive = Build_Order_Object(UnitTypes::Zerg_Hive);
    Build_Order_Object spire = Build_Order_Object(UnitTypes::Zerg_Spire);
    Build_Order_Object greater_spire = Build_Order_Object(UnitTypes::Zerg_Greater_Spire);
    Build_Order_Object devourer = Build_Order_Object(UnitTypes::Zerg_Devourer);
    Build_Order_Object muta = Build_Order_Object(UnitTypes::Zerg_Mutalisk);
    Build_Order_Object hydra = Build_Order_Object(UnitTypes::Zerg_Hydralisk);
    Build_Order_Object lurker = Build_Order_Object(UnitTypes::Zerg_Lurker);
    Build_Order_Object hydra_den = Build_Order_Object(UnitTypes::Zerg_Hydralisk_Den);
    Build_Order_Object queens_nest = Build_Order_Object(UnitTypes::Zerg_Queens_Nest);
    Build_Order_Object lurker_tech = Build_Order_Object(TechTypes::Lurker_Aspect);
    Build_Order_Object grooved_spines = Build_Order_Object(UpgradeTypes::Grooved_Spines);
    Build_Order_Object muscular_augments = Build_Order_Object(UpgradeTypes::Muscular_Augments);

    for (auto &build : build_string) {
        if (build == "hatch") {
            building_gene_.push_back(hatch);
        }
        else if (build == "extract") {
            building_gene_.push_back(extract);
        }
        else if (build == "drone") {
            building_gene_.push_back(drone);
        }
        else if (build == "ovi") {
            building_gene_.push_back(ovi);
        }
        else if (build == "overlord") {
            building_gene_.push_back(ovi);
        }
        else if (build == "pool") {
            building_gene_.push_back(pool);
        }
        else if (build == "evo") {
            building_gene_.push_back(evo);
        }
        else if (build == "speed") {
            building_gene_.push_back(speed);
        }
        else if (build == "ling") {
            building_gene_.push_back(ling);
        }
        else if (build == "creep") {
            building_gene_.push_back(creep);
        }
        else if (build == "sunken") {
            building_gene_.push_back(sunken);
        }
        else if (build == "spore") {
            building_gene_.push_back(spore);
        }
        else if (build == "lair") {
            building_gene_.push_back(lair);
        }
        else if (build == "hive") {
            building_gene_.push_back(hive);
        }
        else if (build == "spire") {
            building_gene_.push_back(spire);
        }
        else if (build == "greater_spire") {
            building_gene_.push_back(greater_spire);
        }
        else if (build == "devourer") {
            building_gene_.push_back(devourer);
        }
        else if (build == "muta") {
            building_gene_.push_back(muta);
        }
        else if (build == "lurker_tech") {
            building_gene_.push_back(lurker_tech);
        }
        else if (build == "hydra") {
            building_gene_.push_back(hydra);
        }
        else if (build == "lurker") {
            building_gene_.push_back(lurker);
        }
        else if (build == "hydra_den") {
            building_gene_.push_back(hydra_den);
        }
        else if (build == "queens_nest") {
            building_gene_.push_back(queens_nest);
        }
        else if (build == "grooved_spines") {
            building_gene_.push_back(grooved_spines);
        }
        else if (build == "muscular_augments") {
            building_gene_.push_back(muscular_augments);
        }
    }
}

void Building_Gene::clearRemainingBuildOrder( const bool diagnostic) {
    if constexpr (ANALYSIS_MODE) {
        if (!building_gene_.empty() && diagnostic) {

            if (building_gene_.front().getUnit().supplyRequired() > Broodwar->self()->supplyTotal() - Broodwar->self()->supplyTotal()) {
                ofstream output; // Prints to brood war file while in the WRITE file.
                output.open(".\\bwapi-data\\write\\BuildOrderFailures.txt", ios_base::app);
                string print_value = "";

                //print_value += building_gene_.front().getResearch().c_str();
                print_value += building_gene_.front().getUnit().c_str();
                //print_value += building_gene_.front().getUpgrade().c_str();

                output << "Supply blocked: " << print_value << endl;
                output.close();
                Broodwar->sendText("A %s was canceled.", print_value);
            }
            else {
                ofstream output; // Prints to brood war file while in the WRITE file.
                output.open(".\\bwapi-data\\write\\BuildOrderFailures.txt", ios_base::app);
                string print_value = "";

                print_value += building_gene_.front().getResearch().c_str();
                print_value += building_gene_.front().getUnit().c_str();
                print_value += building_gene_.front().getUpgrade().c_str();

                output << "Couldn't build: " << print_value << endl;
                output.close();
                Broodwar->sendText("A %s was canceled.", print_value);
            }
        }
    }
    building_gene_.clear();
};

Building_Gene::Building_Gene() {};

Building_Gene::Building_Gene(string s) { // unspecified items are unrestricted.

    getInitialBuildOrder(s);

}
