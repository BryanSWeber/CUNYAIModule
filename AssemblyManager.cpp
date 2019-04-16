#pragma once
#include "Source\CUNYAIModule.h"
#include "Source\Map_Inventory.h"
#include "Source\AssemblyManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\PlayerModelManager.h" // needed for cartidges.
#include "Source\BWEM\include\bwem.h"
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
int AssemblyManager::last_frame_of_larva_morph_command = 0;
int AssemblyManager::last_frame_of_hydra_morph_command = 0;
int AssemblyManager::last_frame_of_muta_morph_command = 0;
std::map<UnitType, int> AssemblyManager::assembly_cycle_ = Player_Model::combat_unit_cartridge_;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations. Now updates the unit within the Unit_Inventory directly.
bool AssemblyManager::Check_N_Build(const UnitType &building, const Unit &unit, const bool &extra_critera)
{
    if (CUNYAIModule::checkDesirable(unit, building, extra_critera) ) {
        Position unit_pos = unit->getPosition();
        bool unit_can_build_intended_target = unit->canBuild(building);
        bool unit_can_morph_intended_target = unit->canMorph(building);
        //Check simple upgrade into lair/hive.
        Unit_Inventory local_area = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, unit_pos, 250);
        bool hatch_nearby = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery, local_area) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
            CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;
        if (unit_can_morph_intended_target && CUNYAIModule::checkSafeBuildLoc( unit_pos, CUNYAIModule::current_map_inventory, CUNYAIModule::enemy_player_model.units_, CUNYAIModule::friendly_player_model.units_, CUNYAIModule::land_inventory) && (unit->getType().isBuilding() || hatch_nearby ) ){ // morphing hatcheries into lairs & hives or spires into greater spires.
                if (unit->morph(building)) {
                    CUNYAIModule::buildorder.announceBuildingAttempt(building); // Takes no time, no need for the reserve system.
                    Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);
                    return true;
                }
        }
        else if (unit->canBuild(building) && building != UnitTypes::Zerg_Creep_Colony && building != UnitTypes::Zerg_Extractor && building != UnitTypes::Zerg_Hatchery)
        {
            TilePosition buildPosition = getBuildablePosition(unit->getTilePosition(), building, 12);
            if (CUNYAIModule::my_reservation.addReserveSystem(buildPosition, building) && hatch_nearby && unit->build(building, buildPosition) ) {
                CUNYAIModule::buildorder.announceBuildingAttempt(building);
                Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                morphing_unit.phase_ = "Building";
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if (CUNYAIModule::buildorder.checkBuilding_Desired(building)) {
                CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Freeze here please!...", building.c_str(), buildPosition.x, buildPosition.y);
                //buildorder.updateRemainingBuildOrder(building); // skips the building.
            }
        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Creep_Colony) { // creep colony loop specifically.

            Unitset base_core = unit->getUnitsInRadius(1, IsBuilding && IsResourceDepot && IsCompleted); // don't want undefined crash.
            TilePosition central_base = TilePositions::Origin;
            TilePosition final_creep_colony_spot = TilePositions::Origin;


            //get all the bases that might need a new creep colony.
            for (const auto &u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
                if ( u.second.type_ == UnitTypes::Zerg_Hatchery ) {
                    base_core.insert(u.second.bwapi_unit_);
                }
                else if (u.second.type_ == UnitTypes::Zerg_Lair) {
                    base_core.insert(u.second.bwapi_unit_);
                }
                else if (u.second.type_ == UnitTypes::Zerg_Hive) {
                    base_core.insert(u.second.bwapi_unit_);
                }
            }

            bool intended_spore = false;
            // If you need a spore, any old place will do for now.
            if (CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) > 0 && CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && CUNYAIModule::enemy_player_model.units_.stock_fliers_ > 0 ) {
                Unit_Inventory hacheries = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Hatchery, unit_pos, 500);
                Unit_Inventory lairs = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Lair, unit_pos, 500);
                Unit_Inventory hives = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, UnitTypes::Zerg_Hive, unit_pos, 500);
                hacheries = hacheries + lairs + hives;
                Stored_Unit *close_hatch = CUNYAIModule::getClosestStored(hacheries, unit_pos, 500);
                if (close_hatch) {
                    central_base = TilePosition(close_hatch->pos_);
                }
                CUNYAIModule::DiagnosticText("Anticipating a spore");
                intended_spore = true;
            }
            // Otherwise, let's find a place for sunkens. They should be at the base closest to the enemy, and should not blook off any paths. Alternatively, the base could be under threat.
            else if (CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_.size() != 0 && CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(unit_pos) > 0) { // if we have identified the enemy's base, build at the spot closest to them.
                if (central_base == TilePositions::Origin) {
                    int old_dist = 9999999;

                    for (auto base = base_core.begin(); base != base_core.end(); ++base) { // loop over every base.

                        TilePosition central_base_new = TilePosition((*base)->getPosition());
                        if (!BWAPI::Broodwar->hasCreep(central_base_new)) // Skip bases that don't have the creep yet for a sunken
                            continue;
                        int new_dist =CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy((*base)->getPosition()); // see how far it is from the enemy.
                        //CUNYAIModule::DiagnosticText("Dist from enemy is: %d", new_dist);

                        Unit_Inventory e_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::enemy_player_model.units_, Position(central_base_new),CUNYAIModule::current_map_inventory.my_portion_of_the_map_);
                        Unit_Inventory friend_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, Position(central_base_new),CUNYAIModule::current_map_inventory.my_portion_of_the_map_);
                        bool serious_problem = false;

                        if (CUNYAIModule::getClosestThreatOrTargetStored(e_loc, UnitTypes::Zerg_Drone, (*base)->getPosition(), CUNYAIModule::current_map_inventory.my_portion_of_the_map_)) { // if they outnumber us here...
                            serious_problem = (e_loc.moving_average_fap_stock_ > friend_loc.moving_average_fap_stock_);
                        }

                        if ((new_dist <= old_dist || serious_problem) && CUNYAIModule::checkSafeBuildLoc(Position(central_base_new), CUNYAIModule::current_map_inventory, CUNYAIModule::enemy_player_model.units_, CUNYAIModule::friendly_player_model.units_, CUNYAIModule::land_inventory)) {  // then let's build at that base.
                            central_base = central_base_new;
                            old_dist = new_dist;
                            if (serious_problem) {
                                break;
                            }
                        }
                    }
                } //confirm we have identified a base around which to build.
                int chosen_base_distance = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(Position(central_base)); // Now let us build around that base.
                for (int x = -10; x <= 10; ++x) {
                    for (int y = -10; y <= 10; ++y) {
                        int centralize_x = central_base.x + x;
                        int centralize_y = central_base.y + y;
                        bool within_map = centralize_x < Broodwar->mapWidth() &&
                            centralize_y < Broodwar->mapHeight() &&
                            centralize_x > 0 &&
                            centralize_y > 0;

                        TilePosition test_loc = TilePosition(centralize_x, centralize_y);
                        if (!BWAPI::Broodwar->hasCreep(test_loc)) //Only choose spots that have enough creep for the tumor
                            continue;

                        bool not_blocking_minerals = CUNYAIModule::getResourceInventoryInRadius(CUNYAIModule::land_inventory, Position(test_loc), 96).resource_inventory_.empty() || intended_spore;

                        if (!(x == 0 && y == 0) &&
                            within_map &&
                            not_blocking_minerals &&
                            Broodwar->canBuildHere(test_loc, UnitTypes::Zerg_Creep_Colony, unit, false) &&
                            CUNYAIModule::current_map_inventory.map_veins_[WalkPosition(test_loc).x][WalkPosition(test_loc).y] > UnitTypes::Zerg_Creep_Colony.tileWidth() * 4 && // don't wall off please. Wide berth around other buildings.
                            CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(Position(test_loc)) <= chosen_base_distance) // Count all points further from home than we are.
                        {
                            final_creep_colony_spot = test_loc;
                            chosen_base_distance = CUNYAIModule::current_map_inventory.getRadialDistanceOutFromEnemy(Position(test_loc));
                        }
                    }
                }
            }
            TilePosition buildPosition = getBuildablePosition(final_creep_colony_spot, building, 4);
            if (unit->build(building, buildPosition) && CUNYAIModule::my_reservation.addReserveSystem(buildPosition, building)) {
                CUNYAIModule::buildorder.announceBuildingAttempt(building);
                Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                morphing_unit.phase_ = "Building";
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if (CUNYAIModule::buildorder.checkBuilding_Desired(building)) {
                CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                //buildorder.updateRemainingBuildOrder(building); // skips the building.
                //buildorder.clearRemainingBuildOrder();
            }
        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Extractor) {

            Stored_Resource* closest_gas = CUNYAIModule::getClosestGroundStored(CUNYAIModule::land_inventory, UnitTypes::Resource_Vespene_Geyser, unit_pos);
            if (closest_gas && closest_gas->occupied_natural_ && closest_gas->bwapi_unit_ ) {
                //TilePosition buildPosition = closest_gas->bwapi_unit_->getTilePosition();
                //TilePosition buildPosition = CUNYAIModule::getBuildablePosition(TilePosition(closest_gas->pos_), building, 5);  // Not viable for extractors
                TilePosition buildPosition = Broodwar->getBuildLocation(building, TilePosition(closest_gas->pos_), 5);
                if ( BWAPI::Broodwar->isVisible(buildPosition) && CUNYAIModule::my_reservation.addReserveSystem(buildPosition, building) && unit->build(building, buildPosition)) {
                    CUNYAIModule::buildorder.announceBuildingAttempt(building);
                    Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);
                    return true;
                } //extractors must have buildings nearby or we shouldn't build them.
                //else if ( !BWAPI::Broodwar->isVisible(buildPosition) && unit->move(Position(buildPosition)) && my_reservation.addReserveSystem(building, buildPosition)) {
                //    CUNYAIModule::buildorder.announceBuildingAttempt(building);
                //    return true;
                //} //extractors must have buildings nearby or we shouldn't build them.
                else if ( BWAPI::Broodwar->isVisible(buildPosition) && CUNYAIModule::buildorder.checkBuilding_Desired(building)) {
                    CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                    //buildorder.updateRemainingBuildOrder(building); // skips the building.
                    //buildorder.clearRemainingBuildOrder();
                }
            }

        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Hatchery) {

            if (unit_can_build_intended_target && CUNYAIModule::checkSafeBuildLoc(unit_pos, CUNYAIModule::current_map_inventory, CUNYAIModule::enemy_player_model.units_, CUNYAIModule::friendly_player_model.units_, CUNYAIModule::land_inventory) ) {
                TilePosition buildPosition = Broodwar->getBuildLocation(building, unit->getTilePosition(), 64);

                local_area = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, Position(buildPosition), 250);
                hatch_nearby = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery, local_area) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
                    CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
                    CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;

                if ( hatch_nearby && CUNYAIModule::my_reservation.addReserveSystem(buildPosition, building) && unit->build(building, buildPosition)) {
                    CUNYAIModule::buildorder.announceBuildingAttempt(building);
                    Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);

                    return true;
                }
                else if (CUNYAIModule::buildorder.checkBuilding_Desired(building)) {
                    CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                    //buildorder.updateRemainingBuildOrder(building); // skips the building.
                    //buildorder.clearRemainingBuildOrder();
                }
            }
        }
    }
    return false;
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.  Updates CUNYAIModule::friendly_player_model.units_.
bool AssemblyManager::Check_N_Grow(const UnitType &unittype, const Unit &larva, const bool &extra_critera)
{

    if (CUNYAIModule::checkDesirable(larva, unittype, extra_critera))
    {
        if (larva->morph(unittype)) {
            Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(larva)->second;
            morphing_unit.phase_ = "Morphing";
            morphing_unit.updateStoredUnit(larva);
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
                auto friendly_area = CUNYAIModule::friendly_player_model.units_.getInventoryAtArea(expo_areaID);
                auto enemy_area = CUNYAIModule::enemy_player_model.units_.getInventoryAtArea(expo_areaID);

                bool safe_expo = CUNYAIModule::checkSafeBuildLoc(Position(p), inv, enemy_area, friendly_area, CUNYAIModule::land_inventory);

                bool occupied_expo = CUNYAIModule::getClosestStored(friendly_area, UnitTypes::Zerg_Hatchery, Position(p), 1000) ||
                                     CUNYAIModule::getClosestStored(friendly_area, UnitTypes::Zerg_Lair, Position(p), 1000) ||
                                     CUNYAIModule::getClosestStored(friendly_area, UnitTypes::Zerg_Hive, Position(p), 1000);

                bool path_available = !BWEM::Map::Instance().GetPath(unit->getPosition(), Position(p)).empty();
                if ( score_temp > expo_score && safe_expo && !occupied_expo && path_available) {
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
            //clear all obstructions, if any.
            clearBuildingObstuctions(CUNYAIModule::friendly_player_model.units_, inv, unit);

            if (Broodwar->isExplored(inv.next_expo_) && unit->build(UnitTypes::Zerg_Hatchery, inv.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(inv.next_expo_, UnitTypes::Zerg_Hatchery)) {
                CUNYAIModule::DiagnosticText("Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y);
                Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                morphing_unit.phase_ = "Expoing";
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if (!Broodwar->isExplored(inv.next_expo_) && CUNYAIModule::my_reservation.addReserveSystem(inv.next_expo_, UnitTypes::Zerg_Hatchery)) {
                unit->move(Position(inv.next_expo_));
                Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
                morphing_unit.phase_ = "Expoing";
                morphing_unit.updateStoredUnit(unit);
                CUNYAIModule::DiagnosticText("Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x, inv.next_expo_.y);
                return true;
            }
            //}
        }
    } // closure affordablity.

    return false;
}

//Creates a new building with DRONE. Does not create units that morph from other buildings: Lairs, Hives, Greater Spires, or sunken/spores.
bool AssemblyManager::Building_Begin(const Unit &drone, const Unit_Inventory &e_inv) {
    // will send it to do the LAST thing on this list that it can build.
    bool buildings_started = false;
    bool any_macro_problems = CUNYAIModule::current_map_inventory.min_workers_ > CUNYAIModule::current_map_inventory.min_fields_ * 1.75 || CUNYAIModule::current_map_inventory.gas_workers_ > 2 * CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) || CUNYAIModule::current_map_inventory.min_fields_ < CUNYAIModule::current_map_inventory.hatches_ * 5 || CUNYAIModule::current_map_inventory.workers_distance_mining_ > 0.0625 * CUNYAIModule::current_map_inventory.min_workers_; // 1/16 workers LD mining is too much.
    bool expansion_meaningful = CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) < 85 && (any_macro_problems);
    bool the_only_macro_hatch_case = (CUNYAIModule::larva_starved && !expansion_meaningful && !CUNYAIModule::econ_starved);
    bool upgrade_bool = (CUNYAIModule::tech_starved || (CUNYAIModule::Count_Units(UnitTypes::Zerg_Larva) == 0 && !CUNYAIModule::army_starved));
    bool lurker_tech_progressed = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect);
    bool one_tech_per_base = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) < CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery);
    bool can_upgrade_colonies = (CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0) ||
        (CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0); // There is a building complete that will allow either creep colony upgrade.
    bool enemy_mostly_ground = e_inv.stock_ground_units_ > e_inv.stock_fighting_total_ * 0.75;
    bool enemy_lacks_AA = e_inv.stock_shoots_up_ < 0.25 * e_inv.stock_fighting_total_;
    bool nearby_enemy = CUNYAIModule::checkOccupiedArea(CUNYAIModule::enemy_player_model.units_, drone->getPosition(), CUNYAIModule::current_map_inventory.my_portion_of_the_map_);
    bool drone_death = false;
    int number_of_evos_wanted =
        static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Carapace) > TechManager::returnTechRank(UpgradeTypes::None)) +
            static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Melee_Attacks) > TechManager::returnTechRank(UpgradeTypes::None)) +
                static_cast<int>(TechManager::returnTechRank(UpgradeTypes::Zerg_Missile_Attacks) > TechManager::returnTechRank(UpgradeTypes::None));
    int count_tech_buildings = CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Ultralisk_Cavern);
    bool have_idle_evos = false;
    for (auto evo : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (evo.second.type_ == UnitTypes::Zerg_Evolution_Chamber && evo.second.build_type_ && evo.second.phase_ != "Upgrading") have_idle_evos = true;
    }

    Unit_Inventory e_loc;
    Unit_Inventory u_loc;

    if (nearby_enemy) {
        e_loc = CUNYAIModule::getUnitInventoryInRadius(e_inv, drone->getPosition(), CUNYAIModule::current_map_inventory.my_portion_of_the_map_);
        u_loc = CUNYAIModule::getUnitInventoryInRadius(CUNYAIModule::friendly_player_model.units_, drone->getPosition(), CUNYAIModule::current_map_inventory.my_portion_of_the_map_);
        drone_death = u_loc.unit_map_.find(drone) != u_loc.unit_map_.end() && !Stored_Unit::unitAliveinFuture(u_loc.unit_map_.at(drone), 48); // This may be the only case with a 48 frame death clock (2 seconds).
    }

    // Trust the build order. If there is a build order and it wants a building, build it!
    if (!CUNYAIModule::buildorder.isEmptyBuildOrder()) {
        UnitType next_in_build_order = CUNYAIModule::buildorder.building_gene_.front().getUnit();
        if (next_in_build_order == UnitTypes::Zerg_Hatchery) buildings_started = Expo(drone, false, CUNYAIModule::current_map_inventory);
        else buildings_started = Check_N_Build(next_in_build_order, drone, false);
    }

    //Macro-related Buildings.
    if (!buildings_started) buildings_started = Expo(drone, (expansion_meaningful || CUNYAIModule::larva_starved || CUNYAIModule::econ_starved) && !the_only_macro_hatch_case, CUNYAIModule::current_map_inventory);
    //buildings_started = expansion_meaningful; // stop if you need an expo!

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hatchery, drone, the_only_macro_hatch_case); // only macrohatch if you are short on larvae and can afford to spend.


    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Extractor, drone,
        (CUNYAIModule::current_map_inventory.gas_workers_ >= 2 * (CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor)) && CUNYAIModule::gas_starved) &&
        Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.  Note that gas_workers count may be off. Sometimes units are in the gas geyser.

    //Combat Buildings
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone, (!CUNYAIModule::checkSuperiorFAPForecast2(u_loc, e_loc) || drone_death || CUNYAIModule::army_starved) && nearby_enemy && // under attack. ? And?
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Creep_Colony) * 50 + 50 <= CUNYAIModule::my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
        can_upgrade_colonies &&
        (CUNYAIModule::larva_starved || CUNYAIModule::supply_starved || CUNYAIModule::gas_starved) && // Only throw down a sunken if you have no larva floating around, or need the supply, or can't spare the gas.
        CUNYAIModule::current_map_inventory.hatches_ > 1 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Sunken_Colony) + CUNYAIModule::Count_Units(UnitTypes::Zerg_Spore_Colony) < max((CUNYAIModule::current_map_inventory.hatches_ * (CUNYAIModule::current_map_inventory.hatches_ + 1)) / 2, 6)); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.

    //First Building needed!
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) == 0 && CUNYAIModule::friendly_player_model.units_.resource_depot_count_ > 0);

    //Consider an organized build plan.
    if (CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && CUNYAIModule::enemy_player_model.units_.flyer_count_ > 0) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not).
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

    if (returnUnitRank(UnitTypes::Zerg_Ultralisk) == 0) {
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
        (!have_idle_evos || CUNYAIModule::Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0) &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) > count_tech_buildings);

    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
        CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Lair, CUNYAIModule::friendly_player_model.units_) > 0 &&
        (CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Hydralisk_Den, CUNYAIModule::friendly_player_model.units_) > 0 || CUNYAIModule::Count_SuccessorUnits(UnitTypes::Zerg_Spire, CUNYAIModule::friendly_player_model.units_) > 0) && // need spire or hydra to tech beyond lair please.
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

        Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(drone)->second;
        morphing_unit.updateStoredUnit(drone);

    return buildings_started;
};

// Cannot use for extractors, they are "too close" to the geyser.
TilePosition AssemblyManager::getBuildablePosition( const TilePosition target_pos, const UnitType build_type, const int tile_grid_size ) {

    TilePosition canidate_return_position = TilePosition (0,0);
    int widest_dim_in_minitiles = 4 * max(build_type.tileHeight(), build_type.tileWidth());
    int width = Broodwar->mapWidth();
    int height = Broodwar->mapHeight();
    for (int x = -tile_grid_size; x <= tile_grid_size; ++x) {
        for (int y = -tile_grid_size; y <= tile_grid_size; ++y) {
            int centralize_x = target_pos.x + x;
            int centralize_y = target_pos.y + y;
            if (!(x == 0 && y == 0) &&
                centralize_x < width &&
                centralize_y < height &&
                centralize_x > 0 &&
                centralize_y > 0 &&
                Broodwar->canBuildHere(TilePosition(centralize_x, centralize_y), build_type) &&
                CUNYAIModule::current_map_inventory.map_veins_[WalkPosition(TilePosition(centralize_x, centralize_y)).x][WalkPosition(TilePosition(centralize_x, centralize_y)).y] > widest_dim_in_minitiles // don't wall off please. Wide berth around blue veins.
            ) {
                canidate_return_position = TilePosition(centralize_x, centralize_y);
                break;
            }
        }
    }

    return canidate_return_position;
}

// clears all blocking units in the area excluding EXCEPTION_UNIT.  Purges all the worker relations for the scattered units.
void AssemblyManager::clearBuildingObstuctions(const Unit_Inventory &ui, Map_Inventory &inv,const Unit &exception_unit ) {
    Unit_Inventory obstructions = CUNYAIModule::getUnitInventoryInRadius(ui, Position(inv.next_expo_), 3 * 32);
    for (auto u = obstructions.unit_map_.begin(); u != obstructions.unit_map_.end() && !obstructions.unit_map_.empty(); u++) {
        if (u->second.bwapi_unit_ && u->second.bwapi_unit_ != exception_unit ) {
            CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u->second.bwapi_unit_, CUNYAIModule::land_inventory, inv, CUNYAIModule::my_reservation);
            CUNYAIModule::friendly_player_model.units_.purgeWorkerRelationsNoStop(u->second.bwapi_unit_, CUNYAIModule::land_inventory, inv, CUNYAIModule::my_reservation);
            u->second.bwapi_unit_->move({ Position(inv.next_expo_).x + (rand() % 200 - 100) * 4 * 32, Position(inv.next_expo_).y + (rand() % 200 - 100) * 4 * 32 });
        }
    }
}

bool AssemblyManager::Reactive_BuildFAP(const Unit &morph_canidate, const Map_Inventory &inv, const Unit_Inventory &ui, const Unit_Inventory &ei) {

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
    if (!CUNYAIModule::buildorder.isEmptyBuildOrder() || CUNYAIModule::army_starved || (!CUNYAIModule::econ_starved && Broodwar->self()->gas() > 300 && Broodwar->self()->minerals() > 300)) {
        is_building = AssemblyManager::buildOptimalUnit(morph_canidate, assembly_cycle_);
    }

    return is_building;

}

bool AssemblyManager::buildStaticDefence(const Unit &morph_canidate) {

    if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Spore_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Sunken_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    bool must_make_spore = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Spore_Colony, true);
    bool must_make_sunken = CUNYAIModule::checkDesirable(morph_canidate, UnitTypes::Zerg_Sunken_Colony, true);

    if (CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && must_make_spore) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (!CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && must_make_sunken) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    return false;
}

//contains a filter to discard unbuildable sorts of units, then finds the best unit via a series of BuildFAP sim, then builds it. Passes by copy so I can mutilate the values.
bool AssemblyManager::buildOptimalUnit(const Unit &morph_canidate, map<UnitType, int> combat_types) {
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
        if (potential_type.first.isFlyer() && CUNYAIModule::friendly_player_model.e_relatively_weak_against_air_)  flying_class = true;
    }

    // Identify desirable unit classes prior to simulation.
    for (auto &potential_type : combat_types) {
        if (potential_type.first.airWeapon() != WeaponTypes::None && CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && up_shooting_class)  it_needs_to_shoot_up = true; // can't build things that shoot up if you don't have the gas or larva.
        if (potential_type.first.groundWeapon() != WeaponTypes::None && !CUNYAIModule::friendly_player_model.u_relatively_weak_against_air_ && down_shooting_class)  it_needs_to_shoot_down = true;
        if (potential_type.first.isFlyer() && CUNYAIModule::friendly_player_model.e_relatively_weak_against_air_ && flying_class) it_needs_to_fly = true;
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
void AssemblyManager::updateOptimalUnit() {
    bool building_optimal_unit = false;
    //auto buildfap_temp = CUNYAIModule::buildfap; // contains everything we're looking for except for the mock units. Keep this copy around so we don't destroy the original.
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

            buildFAP_copy.simulate(24 * 3); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
            int score = CUNYAIModule::getFAPScore(buildFAP_copy, true) - CUNYAIModule::getFAPScore(buildFAP_copy, false);
            if (assembly_cycle_.find(potential_type.first) == assembly_cycle_.end()) assembly_cycle_[potential_type.first] = score;
            else assembly_cycle_[potential_type.first] = static_cast<int>(static_cast<double>(23.0 / 24.0 * assembly_cycle_[potential_type.first]) + static_cast<double>(1.0 / 24.0 * score)); //moving average over 24 simulations, 1 seconds.
        //}
    //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("have a sim score of %d, for %s", assembly_cycle_.find(potential_type.first)->second, assembly_cycle_.find(potential_type.first)->first.c_str());
    }

}


//Simply returns the unittype that is the "best" of a BuildFAP sim.
UnitType AssemblyManager::testAirWeakness(const Research_Inventory &ri) {
    bool building_optimal_unit = false;
    int best_sim_score = INT_MIN;
    UnitType build_type = UnitTypes::None;
    Position comparision_spot = Unit_Inventory::positionBuildFap(true);// all compared units should begin in the exact same position.
                                                       //add friendly units under consideration to FAP in loop, resetting each time.

    FAP::FastAPproximation<Stored_Unit*> buildFAP; // attempting to integrate FAP into building decisions.
    CUNYAIModule::friendly_player_model.units_.addToBuildFAP(buildFAP, true, CUNYAIModule::friendly_player_model.researches_);
    CUNYAIModule::enemy_player_model.units_.addToBuildFAP(buildFAP, false, CUNYAIModule::enemy_player_model.researches_);

    map<UnitType, int> air_test_1 = { { UnitTypes::Zerg_Sunken_Colony, INT_MIN } ,{ UnitTypes::Zerg_Spore_Colony, INT_MIN } };
    auto buildfap_temp = buildFAP; // restore the buildfap temp.

    if (CUNYAIModule::enemy_player_model.units_.flyer_count_ > 0) {
        // test sunkens
        buildfap_temp.clear();
        buildfap_temp = buildFAP; // restore the buildfap temp.
        Stored_Unit su = Stored_Unit(UnitTypes::Zerg_Sunken_Colony);
        // enemy units do not change.
        Unit_Inventory friendly_units_under_consideration; // new every time.
        friendly_units_under_consideration.addStored_Unit(su); //add unit we are interested in to the inventory:
        friendly_units_under_consideration.addToFAPatPos(buildfap_temp, comparision_spot, true, ri);
        buildfap_temp.simulate(24 * 5); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
        air_test_1.at(UnitTypes::Zerg_Sunken_Colony) = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false);
        buildfap_temp.clear();
        //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());


        // test fake anti-air sunkens
        buildfap_temp.clear();
        buildfap_temp = buildFAP; // restore the buildfap temp.
        // enemy units do not change.
        Unit_Inventory friendly_units_under_consideration2; // new every time.
        friendly_units_under_consideration2.addStored_Unit(su); //add unit we are interested in to the inventory:
        friendly_units_under_consideration2.addAntiAirToFAPatPos(buildfap_temp, comparision_spot, true, ri);
        buildfap_temp.simulate(24 * 5); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
        air_test_1.at(UnitTypes::Zerg_Spore_Colony) = CUNYAIModule::getFAPScore(buildfap_temp, true) - CUNYAIModule::getFAPScore(buildfap_temp, false); //The spore colony is just a placeholder.
        buildfap_temp.clear();
        //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());


        for (auto &potential_type : air_test_1) {
            if (potential_type.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
                best_sim_score = potential_type.second;
                build_type = potential_type.first;
                //CUNYAIModule::DiagnosticText("Found a Best_sim_score of %d, for %s", best_sim_score, build_type.c_str());
            }
        }

        return build_type;
    }

    else return UnitTypes::Zerg_Sunken_Colony;
}

// Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void AssemblyManager::Print_Assembly_FAP_Cycle(const int &screen_x, const int &screen_y) {
    int another_sort_of_unit = 0;
    multimap<int, UnitType> sorted_list;
    for (auto it : assembly_cycle_) {
        if(it.second > 0) sorted_list.insert({ it.second, it.first });
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

    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if (u.second.type_ == UnitTypes::Zerg_Larva) larva_bank_.addStored_Unit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Hydralisk) hydra_bank_.addStored_Unit(u.second);
        if (u.second.type_ == UnitTypes::Zerg_Mutalisk) muta_bank_.addStored_Unit(u.second);
        if (u.second.type_ == Broodwar->self()->getRace().getWorker()) builder_bank_.addStored_Unit(u.second);
    }

    larva_bank_.updateUnitInventorySummary();
    hydra_bank_.updateUnitInventorySummary();
    muta_bank_.updateUnitInventorySummary();
    builder_bank_.updateUnitInventorySummary();
}

bool AssemblyManager::assignUnitAssembly()
{
    Unit_Inventory overlord_larva;
    Unit_Inventory immediate_drone_larva;
    Unit_Inventory transfer_drone_larva;
    Unit_Inventory combat_creators;

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
                wasting_larva_soon = larva.first->getHatchery()->getRemainingTrainTime() < 5 + Broodwar->getLatencyFrames() && larva.first->getHatchery()->getLarva().size() == 3 && CUNYAIModule::current_map_inventory.min_fields_ > 8; // no longer will spam units when I need a hatchery.
                Resource_Inventory local_resources = CUNYAIModule::getResourceInventoryInRadius(CUNYAIModule::land_inventory, larva.first->getHatchery()->getPosition(), 250);
                local_resources.countViableMines();
                hatch_wants_drones = 2 * local_resources.local_mineral_patches_ + 3 * local_resources.local_refineries_ > local_resources.local_miners_ + local_resources.local_gas_collectors_;
                prep_for_transfer = CUNYAIModule::Count_Units_In_Progress(Broodwar->self()->getRace().getResourceDepot()) > 0 || CUNYAIModule::my_reservation.checkTypeInReserveSystem(Broodwar->self()->getRace().getResourceDepot());
            }

            bool enough_drones_globally = (CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) > CUNYAIModule::current_map_inventory.min_fields_ * 2 + CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) * 3 + 1) || CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone) >= 85;
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
            if (!CUNYAIModule::checkUnitTouchable(potential_lurker.first)) continue;
            if (!potential_lurker.first->isUnderAttack()) combat_creators.addStored_Unit(potential_lurker.second);
        }
    }

    if (last_frame_of_muta_morph_command < Broodwar->getFrameCount() - 12) {
        bool endgame_fliers_permissable = CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Greater_Spire) > 0;
        for (auto potential_endgame_flier : muta_bank_.unit_map_) {
            if (!CUNYAIModule::checkUnitTouchable(potential_endgame_flier.first)) continue;
            if (!potential_endgame_flier.first->isUnderAttack() && endgame_fliers_permissable) combat_creators.addStored_Unit(potential_endgame_flier.second);
        }
    }

    //Build from the units as needed: priority queue should be as follows. Note some of these are mutually exclusive.
    //Unit_Inventory overlord_larva;
    //Unit_Inventory immediate_drone_larva;
    //Unit_Inventory transfer_drone_larva;
    //Unit_Inventory combat_creators;

    for (auto o : overlord_larva.unit_map_) {
        if (Check_N_Grow(UnitTypes::Zerg_Overlord, o.first, true)) {
            last_frame_of_larva_morph_command = Broodwar->getFrameCount();
            return true;
        }
    }
    for (auto d : immediate_drone_larva.unit_map_) {
        if (Check_N_Grow(UnitTypes::Zerg_Drone, d.first, CUNYAIModule::econ_starved || CUNYAIModule::tech_starved )) {
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
    for (auto c : combat_creators.unit_map_) {
        if (Reactive_BuildFAP(c.first, CUNYAIModule::current_map_inventory, CUNYAIModule::friendly_player_model.units_, CUNYAIModule::enemy_player_model.units_)) {
            if (c.second.type_ == UnitTypes::Zerg_Hydralisk) last_frame_of_hydra_morph_command = Broodwar->getFrameCount();
            if (c.second.type_ == UnitTypes::Zerg_Mutalisk) last_frame_of_muta_morph_command = Broodwar->getFrameCount();
            if (c.second.type_ == UnitTypes::Zerg_Larva) last_frame_of_larva_morph_command = Broodwar->getFrameCount();
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
    return Broodwar->canMake(ut, unit) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && (buildorder.checkBuilding_Desired(ut) || (extra_criteria && buildorder.isEmptyBuildOrder()));
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
