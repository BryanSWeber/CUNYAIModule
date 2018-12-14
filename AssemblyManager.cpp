#pragma once
#include "Source\CUNYAIModule.h"
#include "Source\Map_Inventory.h"
#include "Source\AssemblyManager.h"
#include "Source\Unit_Inventory.h"
#include <iterator>
#include <numeric>
#include <fstream>


using namespace BWAPI;
using namespace Filter;
using namespace std;

extern FAP::FastAPproximation<Stored_Unit*> buildfap;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations. Now updates the unit within the Unit_Inventory directly.
bool CUNYAIModule::Check_N_Build(const UnitType &building, const Unit &unit, const bool &extra_critera)
{
    if (checkDesirable(unit, building, extra_critera) ) {
        Position unit_pos = unit->getPosition();
        bool unit_can_build_intended_target = unit->canBuild(building);
        bool unit_can_morph_intended_target = unit->canMorph(building);
        //Check simple upgrade into lair/hive.
        Unit_Inventory local_area = getUnitInventoryInRadius( friendly_player_model.units_, unit_pos, 250);
        bool hatch_nearby = Count_Units(UnitTypes::Zerg_Hatchery, local_area) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
            Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
            Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;
        if (unit_can_morph_intended_target && checkSafeBuildLoc( unit_pos, current_map_inventory, enemy_player_model.units_, friendly_player_model.units_, land_inventory) && (unit->getType().isBuilding() || hatch_nearby ) ){ // morphing hatcheries into lairs & hives or spires into greater spires. 
                if (unit->morph(building)) {
                    buildorder.announceBuildingAttempt(building); // Takes no time, no need for the reserve system.
                    Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);
                    return true;
                }
        } 
        else if (unit->canBuild(building) && building != UnitTypes::Zerg_Creep_Colony && building != UnitTypes::Zerg_Extractor && building != UnitTypes::Zerg_Hatchery)
        {
            TilePosition buildPosition = CUNYAIModule::getBuildablePosition(unit->getTilePosition(), building, 12);
            if (my_reservation.addReserveSystem(buildPosition, building) && hatch_nearby && unit->build(building, buildPosition) ) {
                buildorder.announceBuildingAttempt(building); 
                Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                morphing_unit.phase_ = "Building";
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if (buildorder.checkBuilding_Desired(building)) {
                CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Freeze here please!...", building.c_str(), buildPosition.x, buildPosition.y);
                //buildorder.updateRemainingBuildOrder(building); // skips the building.
            }
        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Creep_Colony) { // creep colony loop specifically.

            Unitset base_core = unit->getUnitsInRadius(1, IsBuilding && IsResourceDepot && IsCompleted); // don't want undefined crash.
            TilePosition central_base = TilePositions::Origin;
            TilePosition final_creep_colony_spot = TilePositions::Origin;


            //get all the bases that might need a new creep colony.
            for (const auto &u : friendly_player_model.units_.unit_inventory_) {
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

            // If you need a spore, any old place will do for now.
            if (Count_Units(UnitTypes::Zerg_Evolution_Chamber) > 0 && friendly_player_model.u_relatively_weak_against_air_ && enemy_player_model.units_.stock_fliers_ > 0 ) {
                Unit_Inventory hacheries = getUnitInventoryInRadius(friendly_player_model.units_, UnitTypes::Zerg_Hatchery, unit_pos, 500);
                Unit_Inventory lairs = getUnitInventoryInRadius(friendly_player_model.units_, UnitTypes::Zerg_Lair, unit_pos, 500);
                Unit_Inventory hives = getUnitInventoryInRadius(friendly_player_model.units_, UnitTypes::Zerg_Hive, unit_pos, 500);
                hacheries = hacheries + lairs + hives;
                Stored_Unit *close_hatch = getClosestStored(hacheries, unit_pos, 500);
                if (close_hatch) {
                    central_base = TilePosition(close_hatch->pos_);
                }
                CUNYAIModule::DiagnosticText("Anticipating a spore");
            }
            // Otherwise, let's find a place for sunkens. They should be at the base closest to the enemy, and should not blook off any paths. Alternatively, the base could be under threat.
            else if (current_map_inventory.map_out_from_enemy_ground_.size() != 0 && current_map_inventory.getRadialDistanceOutFromEnemy(unit_pos) > 0) { // if we have identified the enemy's base, build at the spot closest to them.
                if (central_base == TilePositions::Origin) {
                    int old_dist = 9999999;

                    for (auto base = base_core.begin(); base != base_core.end(); ++base) { // loop over every base.
                        
                        TilePosition central_base_new = TilePosition((*base)->getPosition());
                        if (!BWAPI::Broodwar->hasCreep(central_base_new)) // Skip bases that don't have the creep yet for a sunken
                            continue;
                        int new_dist = current_map_inventory.getRadialDistanceOutFromEnemy((*base)->getPosition()); // see how far it is from the enemy.
                        //CUNYAIModule::DiagnosticText("Dist from enemy is: %d", new_dist);

                        Unit_Inventory e_loc = getUnitInventoryInRadius(enemy_player_model.units_, Position(central_base_new), 750);
                        Unit_Inventory friend_loc = getUnitInventoryInRadius(friendly_player_model.units_, Position(central_base_new), 750);
                        bool serious_problem = false;

                        if (getClosestThreatOrTargetStored(e_loc, UnitTypes::Zerg_Drone, (*base)->getPosition(), 750)) { // if they outnumber us here...
                            serious_problem = (e_loc.moving_average_fap_stock_ > friend_loc.moving_average_fap_stock_);
                        }

                        if ((new_dist <= old_dist || serious_problem) && checkSafeBuildLoc(Position(central_base_new), current_map_inventory, enemy_player_model.units_, friendly_player_model.units_, land_inventory)) {  // then let's build at that base.
                            central_base = central_base_new;
                            old_dist = new_dist;
                            if (serious_problem) {
                                break;
                            }
                        }
                    }
                } //confirm we have identified a base around which to build.
                int chosen_base_distance = current_map_inventory.getRadialDistanceOutFromEnemy(Position(central_base)); // Now let us build around that base.
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
                        
                        bool not_blocking_minerals = getResourceInventoryInRadius(land_inventory, Position(test_loc), 96).resource_inventory_.empty();
                        
                        if (!(x == 0 && y == 0) &&
                            within_map &&
                            not_blocking_minerals &&
                            Broodwar->canBuildHere(test_loc, UnitTypes::Zerg_Creep_Colony, unit, false) &&
                            current_map_inventory.map_veins_[WalkPosition(test_loc).x][WalkPosition(test_loc).y] > UnitTypes::Zerg_Creep_Colony.tileWidth() * 4 && // don't wall off please. Wide berth around other buildings.
                            current_map_inventory.getRadialDistanceOutFromEnemy(Position(test_loc)) <= chosen_base_distance) // Count all points further from home than we are.
                        {
                            final_creep_colony_spot = test_loc;
                            chosen_base_distance = current_map_inventory.getRadialDistanceOutFromEnemy(Position(test_loc));
                        }
                    }
                }
            }
            TilePosition buildPosition = CUNYAIModule::getBuildablePosition(final_creep_colony_spot, building, 4);
            if (unit->build(building, buildPosition) && my_reservation.addReserveSystem(buildPosition, building)) {
                buildorder.announceBuildingAttempt(building);
                Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                morphing_unit.phase_ = "Building";
                morphing_unit.updateStoredUnit(unit);
                return true;
            }
            else if (buildorder.checkBuilding_Desired(building)) {
                CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                //buildorder.updateRemainingBuildOrder(building); // skips the building.
                //buildorder.clearRemainingBuildOrder();
            }
        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Extractor) {

            Stored_Resource* closest_gas = CUNYAIModule::getClosestGroundStored(land_inventory, UnitTypes::Resource_Vespene_Geyser, current_map_inventory, unit_pos);
            if (closest_gas && closest_gas->occupied_natural_ && closest_gas->bwapi_unit_ ) {
                //TilePosition buildPosition = closest_gas->bwapi_unit_->getTilePosition();
                //TilePosition buildPosition = CUNYAIModule::getBuildablePosition(TilePosition(closest_gas->pos_), building, 5);  // Not viable for extractors
                TilePosition buildPosition = Broodwar->getBuildLocation(building, TilePosition(closest_gas->pos_), 5);
                if ( BWAPI::Broodwar->isVisible(buildPosition) && my_reservation.addReserveSystem(buildPosition, building) && unit->build(building, buildPosition)) {
                    buildorder.announceBuildingAttempt(building);
                    Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);
                    return true;
                } //extractors must have buildings nearby or we shouldn't build them.
                //else if ( !BWAPI::Broodwar->isVisible(buildPosition) && unit->move(Position(buildPosition)) && my_reservation.addReserveSystem(building, buildPosition)) {
                //    buildorder.announceBuildingAttempt(building);
                //    return true;
                //} //extractors must have buildings nearby or we shouldn't build them.
                else if ( BWAPI::Broodwar->isVisible(buildPosition) && buildorder.checkBuilding_Desired(building)) {
                    CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                    //buildorder.updateRemainingBuildOrder(building); // skips the building.
                    //buildorder.clearRemainingBuildOrder();
                }
            }

        }
        else if (unit_can_build_intended_target && building == UnitTypes::Zerg_Hatchery) {

            if (unit_can_build_intended_target && checkSafeBuildLoc(unit_pos, current_map_inventory, enemy_player_model.units_, friendly_player_model.units_, land_inventory) ) {
                TilePosition buildPosition = Broodwar->getBuildLocation(building, unit->getTilePosition(), 64);

                local_area = getUnitInventoryInRadius(friendly_player_model.units_, Position(buildPosition), 250);
                hatch_nearby = Count_Units(UnitTypes::Zerg_Hatchery, local_area) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
                    Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
                    Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;

                if ( hatch_nearby && my_reservation.addReserveSystem(buildPosition, building) && unit->build(building, buildPosition)) {
                    buildorder.announceBuildingAttempt(building);
                    Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
                    morphing_unit.phase_ = "Building";
                    morphing_unit.updateStoredUnit(unit);

                    return true;
                }
                else if (buildorder.checkBuilding_Desired(building)) {
                    CUNYAIModule::DiagnosticText("I can't put a %s at (%d, %d) for you. Clear the build order...", building.c_str(), buildPosition.x, buildPosition.y);
                    //buildorder.updateRemainingBuildOrder(building); // skips the building.
                    //buildorder.clearRemainingBuildOrder();
                }
            }
        }
    }
    return false;
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera. Updates friendly_player_model.units_.
bool CUNYAIModule::Check_N_Upgrade(const UpgradeType &ups, const Unit &unit, const bool &extra_critera)
{
    bool upgrade_in_cartridges = friendly_player_model.upgrade_cartridge_.find(ups) != friendly_player_model.upgrade_cartridge_.end();
    if ( unit->canUpgrade(ups) && my_reservation.checkAffordablePurchase(ups) && upgrade_in_cartridges && (buildorder.checkUpgrade_Desired(ups) || (extra_critera && buildorder.isEmptyBuildOrder())) ) {
        if (unit->upgrade(ups)) {
            buildorder.updateRemainingBuildOrder(ups);
            Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
            morphing_unit.phase_ = "Upgrading";
            morphing_unit.updateStoredUnit(unit);
            CUNYAIModule::DiagnosticText("Upgrading %s.", ups.c_str());
            return true;
        }
    }
    return false;
}

//Checks if a research can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Updates friendly_player_model.units_.
bool CUNYAIModule::Check_N_Research(const TechType &tech, const Unit &unit, const bool &extra_critera)
{
    bool research_in_cartridges = friendly_player_model.tech_cartridge_.find(tech) != friendly_player_model.tech_cartridge_.end();
    if (unit->canResearch(tech) && my_reservation.checkAffordablePurchase(tech) && research_in_cartridges && (buildorder.checkResearch_Desired(tech) || (extra_critera && buildorder.isEmptyBuildOrder()))) {
        if (unit->research(tech)) {
            buildorder.updateRemainingBuildOrder(tech);
            Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(unit)->second;
            morphing_unit.phase_ = "Researching";
            morphing_unit.updateStoredUnit(unit);
            CUNYAIModule::DiagnosticText("Researching %s.", tech.c_str());
            return true;
        }
    }
    return false;
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.  Updates friendly_player_model.units_.
bool CUNYAIModule::Check_N_Grow(const UnitType &unittype, const Unit &larva, const bool &extra_critera)
{

    if (checkDesirable(larva, unittype, extra_critera))
    {
        if (larva->morph(unittype)) {
            Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(larva)->second;
            morphing_unit.phase_ = "Morphing";
            morphing_unit.updateStoredUnit(larva);
            return true;
        }
    }

    return false;
}

//Creates a new unit. Reflects upon enemy units in enemy_set. Could be improved in terms of overall logic. Now needs to be split into hydra, muta morphs and larva morphs. Now updates the unit_inventory.
bool CUNYAIModule::Reactive_Build(const Unit &larva, const Map_Inventory &inv, Unit_Inventory &ui, const Unit_Inventory &ei)
{
    // Am I bulding anything?
    bool is_building = false;

    //Am I sending this command to a larva or a hydra?
    UnitType u_type = larva->getType();
    bool is_larva = u_type == UnitTypes::Zerg_Larva;
    bool is_hydra = u_type == UnitTypes::Zerg_Hydralisk;
    bool is_muta = u_type == UnitTypes::Zerg_Mutalisk;

    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?

    //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
    //bool early_game = Count_Units( UnitTypes::Zerg_Spawning_Pool, ui ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 && inv.min_workers_ + inv.gas_workers_ <= 9;
    bool wasting_larva_soon = true;

    if (is_larva && larva->getHatchery()) {
        wasting_larva_soon = larva->getHatchery()->getRemainingTrainTime() < 5 + Broodwar->getLatencyFrames() && larva->getHatchery()->getLarva().size() == 3 && inv.min_fields_ > 8; // no longer will spam units when I need a hatchery.
    }

    bool enough_drones = (Count_Units(UnitTypes::Zerg_Drone) > inv.min_fields_ * 2 + Count_Units(UnitTypes::Zerg_Extractor) * 3 + 1) || Count_Units(UnitTypes::Zerg_Drone) >= 85;
    bool drone_conditional = econ_starved || tech_starved; // Econ does not detract from technology growth. (only minerals, gas is needed for tech). Always be droning.
    bool one_tech_per_base = Count_Units(UnitTypes::Zerg_Hydralisk_Den) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + Count_Units(UnitTypes::Zerg_Spire) || Count_Units(UnitTypes::Zerg_Greater_Spire) + Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) < inv.hatches_;
    bool would_force_spire = Count_Units(UnitTypes::Zerg_Spire) == 0 &&
        !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) &&
        !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) &&
        buildorder.isEmptyBuildOrder();

    bool would_force_lurkers = Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0 &&
        !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) &&
        !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) &&
        buildorder.isEmptyBuildOrder();

    bool enemy_mostly_ground = ei.stock_ground_units_ >= ei.stock_fighting_total_ * 0.75;
    bool enemy_lacks_AA = ei.stock_shoots_up_ <= 0.25 * ei.stock_shoots_down_;

    //bool marginal_benifit_from_air = ui.stock_fliers_ / (ei.stock_shoots_up_ + 1);
    //bool marginal_benifit_from_ground = ui.stock_shoots_down_ / (ei.stock_ground_units_ + 1);  //these aren't quite right 

    //int remaining_cost_to_get_lurkers = Stored_Unit(UnitTypes::Zerg_Spawning_Pool).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Spawning_Pool, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Hydralisk_Den).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Hydralisk_Den, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Lair).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Lair, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Hive).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Hive, ui) +
    //    TechTypes::Lurker_Aspect.mineralPrice() + 1.25 * TechTypes::Lurker_Aspect.gasPrice() - Stock_Tech(TechTypes::Lurker_Aspect);


    //int remaining_cost_to_get_mutas = Stored_Unit(UnitTypes::Zerg_Spawning_Pool).stock_value_ - Stock_Units(UnitTypes::Zerg_Spawning_Pool, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Spire).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Spire, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Lair).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Lair, ui) +
    //    Stored_Unit(UnitTypes::Zerg_Hive).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Hive, ui);


    //if (Inventory::getMapValue(inv.enemy_base_ground_, inv.map_out_from_home_) == 0) { e_relatively_weak_against_air = true; u_relatively_weak_against_air = true; } // If this is an island situation...Untested.

    // Do required build first.
    if (!buildorder.isEmptyBuildOrder() && buildorder.building_gene_.front().getUnit() != UnitTypes::None) {
        UnitType next_in_build_order = buildorder.building_gene_.front().getUnit();
        if (larva->canBuild(next_in_build_order) ) is_building = Check_N_Grow(next_in_build_order, larva, true);
    }

    // catchall ground units, in case you have a BO that needs to be done.  Should be redundant with above code?
    if (!buildorder.isEmptyBuildOrder()) {
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, false);
        if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Devourer, larva, false);
        if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Guardian, larva, false);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, false);
        if (is_hydra && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Lurker, larva, false);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, false);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, false);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Drone, larva, false);
    }

    //Supply blocked protection 
    if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Overlord, larva, supply_starved );
    // Eco building.
    if (is_larva && !is_building) is_building = Check_N_Grow(larva->getType().getRace().getWorker(), larva, (drone_conditional || wasting_larva_soon) && !enough_drones );

    if (is_larva && !is_building && buildorder.checkBuilding_Desired(UnitTypes::Zerg_Lurker) && Count_Units(UnitTypes::Zerg_Hydralisk) == 0) {
        buildorder.addBuildOrderElement(UnitTypes::Zerg_Hydralisk); // force in an evo chamber if they have Air.
        CUNYAIModule::DiagnosticText("Reactionary Hydralisk. Must have lost one.");
        return is_building = true;
    }

    //Army build/replenish.  Cycle through military units available.
    if (friendly_player_model.u_relatively_weak_against_air_ && ei.stock_fliers_ > 0 ) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not). 
        if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Devourer, larva, (army_starved || wasting_larva_soon) && Count_Units(UnitTypes::Zerg_Greater_Spire) > 0);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Scourge, larva, (army_starved || wasting_larva_soon)  && Count_Units(UnitTypes::Zerg_Spire) > 0 && Count_Units(UnitTypes::Zerg_Scourge) <= 6); // hard cap on scourges, they build 2 at a time.
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, (army_starved || wasting_larva_soon)  && Count_Units(UnitTypes::Zerg_Spire) > 0);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon)  && Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0);
            //Evo chamber is required tech for spore colony
        if (Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0 && buildorder.isEmptyBuildOrder()) {
            buildorder.addBuildOrderElement(UnitTypes::Zerg_Evolution_Chamber); // force in an evo chamber if they have Air.
            CUNYAIModule::DiagnosticText("Reactionary Evo Chamber");
            return is_building = true;
        } // hydralisk den is required tech for hydras, a ground to air/ ground to ground unit.
            //else if (Count_Units(UnitTypes::Zerg_Hydralisk_Den) == 0 && buildorder.isEmptyBuildOrder()) {
            //    buildorder.addBuildOrderElement(UnitTypes::Zerg_Hydralisk_Den);
            //    CUNYAIModule::DiagnosticText("Reactionary Hydra Den");
            //    return is_building = true;
            //}// spire requires LAIR. Spire allows mutalisks and scourge.   Greater spire allows devorers, but I do not have code to updrade to greater spire ATM.
            //else if (Count_Units(UnitTypes::Zerg_Lair) - Count_Units_In_Progress(UnitTypes::Zerg_Lair) > 0 && one_tech_per_base && Count_Units(UnitTypes::Zerg_Spire) == 0 && buildorder.isEmptyBuildOrder()) {
            //    buildorder.addBuildOrderElement(UnitTypes::Zerg_Spire);
            //    CUNYAIModule::DiagnosticText("Reactionary Spire");
            //    return is_building = true;
            //}

    }
    else if ( !friendly_player_model.e_relatively_weak_against_air_) {
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, (army_starved || wasting_larva_soon)  && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) > 0); // catchall ground units.

        bool lings_only = Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 && Count_Units(UnitTypes::Zerg_Hydralisk_Den) == 0 && Count_Units(UnitTypes::Zerg_Lair) == 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0;
        bool hydras_only = Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0 && Count_Units(UnitTypes::Zerg_Lair) == 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0;
        bool saving_for_lurkers = Count_Units(UnitTypes::Zerg_Lair) > 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0;
        bool lurkers_incoming = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) || Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0;
        bool ultralisks_ready = Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) > 0 && Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0;

        if (lings_only) {
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon)  && Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0);
        }
        else if (hydras_only) {
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon)  && my_reservation.checkExcessIsGreaterThan(UnitTypes::Zerg_Lair) );
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon)  && my_reservation.getExcessMineral() > UnitTypes::Zerg_Lair.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (saving_for_lurkers) {
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon)  && my_reservation.checkExcessIsGreaterThan(TechTypes::Lurker_Aspect));
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon)  && my_reservation.getExcessMineral() > TechTypes::Lurker_Aspect.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (lurkers_incoming) {
            if (is_hydra && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Lurker, larva, army_starved  && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0);
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon)  && my_reservation.checkExcessIsGreaterThan(UnitTypes::Zerg_Lurker) || Count_Units(UnitTypes::Zerg_Hydralisk) == 0);
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon)  && my_reservation.getExcessMineral() > UnitTypes::Zerg_Hydralisk.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (ultralisks_ready) {
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, (army_starved || wasting_larva_soon) );
            if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon)  && my_reservation.getExcessMineral() > UnitTypes::Zerg_Ultralisk.mineralPrice() && my_reservation.getExcessMineral() - my_reservation.getExcessGas() > UnitTypes::Zerg_Zergling.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }

        //morph mutas into something more robust if you have the option, but we will not morph additional mutas otherwise.
        if (friendly_player_model.u_relatively_weak_against_air_) {
            if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Devourer, larva, (army_starved || wasting_larva_soon) && Count_Units(UnitTypes::Zerg_Greater_Spire) > 0);
        }
        else {
            if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Guardian, larva, (army_starved || wasting_larva_soon) && Count_Units(UnitTypes::Zerg_Greater_Spire) > 0);
        }

    }
    else if (friendly_player_model.e_relatively_weak_against_air_ ) {
        if (is_muta && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Guardian, larva, (army_starved || wasting_larva_soon) && Count_Units(UnitTypes::Zerg_Greater_Spire) > 0);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, (army_starved || wasting_larva_soon) && Count_Units(UnitTypes::Zerg_Spire) > 0);
        if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && (Count_Units(UnitTypes::Zerg_Spire) == 0 || (my_reservation.getExcessMineral() > UnitTypes::Zerg_Mutalisk.mineralPrice() && my_reservation.getExcessMineral() - my_reservation.getExcessGas() > UnitTypes::Zerg_Zergling.mineralPrice()) ) ); // if you are floating minerals relative to gas, feel free to buy some lings.

        //if (is_larva && !is_building) {
        //    //Evo chamber is required tech for spore colony
        //    if (Count_Units(UnitTypes::Zerg_Lair) == 0 && Count_Units(UnitTypes::Zerg_Extractor) > 0 && one_tech_per_base && buildorder.isEmptyBuildOrder()) {
        //        buildorder.addBuildOrderElement(UnitTypes::Zerg_Lair); // force in a hydralisk den if they have Air.
        //        CUNYAIModule::DiagnosticText("Reactionary Lair");
        //        return is_building = true;
        //    } // spire requires LAIR. Spire allows mutalisks and scourge.   Greater spire allows devorers, but I do not have code to updrade to greater spire ATM.
        //    else if (Count_Units(UnitTypes::Zerg_Lair) - Count_Units_In_Progress(UnitTypes::Zerg_Lair) > 0 && Count_Units(UnitTypes::Zerg_Extractor) > 0 && one_tech_per_base && Count_Units(UnitTypes::Zerg_Spire) == 0 && buildorder.isEmptyBuildOrder()) {
        //        buildorder.addBuildOrderElement(UnitTypes::Zerg_Spire);
        //        CUNYAIModule::DiagnosticText("Reactionary Spire");
        //        return is_building = true;
        //    }
        //}
    }

    //if ((would_force_lurkers || would_force_spire) && Count_Units(UnitTypes::Zerg_Lair, ui) == 0 && one_tech_per_base && Count_Units(UnitTypes::Zerg_Extractor, ui) > 0 ) {
    //    buildorder..addBuildOrderElement(UnitTypes::Zerg_Lair); // force lair if you need it and are in a position for it.
    //    CUNYAIModule::DiagnosticText("Reactionary Lair, there's tech I want.");
    //    return is_building > 0;
    //} 

    //if (is_larva && !is_building) {
    //    if (u_relatively_weak_against_air && would_force_spire && buildorder.isEmptyBuildOrder() && Count_Units(UnitTypes::Zerg_Lair) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0 && one_tech_per_base) {
    //        buildorder.addBuildOrderElement(UnitTypes::Zerg_Spire); // force in a Spire if they have no AA. Note that there is no one-base muta build on TL. So let's keep this restriction of 1 tech per base.
    //        CUNYAIModule::DiagnosticText("Reactionary Spire");
    //        return is_building = true;
    //    }
    //    else if (enemy_mostly_ground && would_force_lurkers && buildorder.isEmptyBuildOrder() && Count_Units(UnitTypes::Zerg_Lair) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0) {
    //        buildorder.addBuildOrderElement(TechTypes::Lurker_Aspect); // force in a hydralisk den if they have Air.
    //        CUNYAIModule::DiagnosticText("Reactionary Lurker Upgrade");
    //        return is_building = true;
    //    }
    //}

    Stored_Unit& morphing_unit = ui.unit_inventory_.find(larva)->second;
    morphing_unit.updateStoredUnit(larva);

    return is_building;
}


//Creates a new building with DRONE. Does not create units that morph from other buildings: Lairs, Hives, Greater Spires, or sunken/spores.
bool CUNYAIModule::Building_Begin(const Unit &drone, const Map_Inventory &inv, const Unit_Inventory &e_inv) {
    // will send it to do the LAST thing on this list that it can build.
    bool buildings_started = false;
    bool any_macro_problems = current_map_inventory.min_workers_ > current_map_inventory.min_fields_ * 1.75 || current_map_inventory.gas_workers_ > 2 * Count_Units(UnitTypes::Zerg_Extractor) || current_map_inventory.min_fields_ < current_map_inventory.hatches_ * 5 || current_map_inventory.workers_distance_mining_ > 0.0625 * current_map_inventory.min_workers_; // 1/16 workers LD mining is too much.
    bool expansion_meaningful = Count_Units(UnitTypes::Zerg_Drone) < 85 && (any_macro_problems);
    bool larva_starved = Count_Units(UnitTypes::Zerg_Larva) <= Count_Units(UnitTypes::Zerg_Hatchery);
    bool the_only_macro_hatch_case = (larva_starved && !expansion_meaningful && !econ_starved);
    bool upgrade_bool = (tech_starved || (Count_Units(UnitTypes::Zerg_Larva) == 0 && !army_starved));
    bool lurker_tech_progressed = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect);
    bool one_tech_per_base = Count_Units(UnitTypes::Zerg_Hydralisk_Den) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + Count_Units(UnitTypes::Zerg_Spire) + Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) < Count_Units(UnitTypes::Zerg_Hatchery) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery);
    bool can_upgrade_colonies = (Count_Units(UnitTypes::Zerg_Spawning_Pool) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0) ||
        (Count_Units(UnitTypes::Zerg_Evolution_Chamber) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0); // There is a building complete that will allow either creep colony upgrade.
    bool enemy_mostly_ground = e_inv.stock_ground_units_ > e_inv.stock_fighting_total_ * 0.75;
    bool enemy_lacks_AA = e_inv.stock_shoots_up_ < 0.25 * e_inv.stock_fighting_total_;
    bool nearby_enemy = checkOccupiedArea(enemy_player_model.units_,drone->getPosition(), current_map_inventory.my_portion_of_the_map_);

    Unit_Inventory e_loc;
    Unit_Inventory u_loc;

    if (nearby_enemy) {
        e_loc = getUnitInventoryInRadius(e_inv, drone->getPosition(), current_map_inventory.my_portion_of_the_map_);
        u_loc = getUnitInventoryInRadius(friendly_player_model.units_, drone->getPosition(), current_map_inventory.my_portion_of_the_map_);
    }

    // Trust the build order. If there is a build order and it wants a building, build it!
    if (!buildorder.isEmptyBuildOrder()) {
        UnitType next_in_build_order = buildorder.building_gene_.front().getUnit();
        if (next_in_build_order == UnitTypes::Zerg_Hatchery) buildings_started = Expo(drone, false, current_map_inventory);
        else buildings_started = Check_N_Build(next_in_build_order, drone, false);
    }

    //Macro-related Buildings.
    if( !buildings_started ) buildings_started = Expo(drone, (expansion_meaningful || larva_starved || econ_starved) && !the_only_macro_hatch_case, current_map_inventory);
    //buildings_started = expansion_meaningful; // stop if you need an expo!

    if( !buildings_started ) buildings_started = Check_N_Build(UnitTypes::Zerg_Hatchery, drone, the_only_macro_hatch_case); // only macrohatch if you are short on larvae and can afford to spend.

    
    if( !buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Extractor, drone,
        (current_map_inventory.gas_workers_ >= 2 * (Count_Units(UnitTypes::Zerg_Extractor) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor)) && gas_starved) &&
        Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.  Note that gas_workers count may be off. Sometimes units are in the gas geyser.

    //Combat Buildings
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone, (army_starved || e_loc.moving_average_fap_stock_ >= u_loc.moving_average_fap_stock_ || e_loc.stock_fighting_total_ > u_loc.stock_fighting_total_) &&  // army starved or under attack. ? And?
        Count_Units(UnitTypes::Zerg_Creep_Colony) * 50 + 50 <= my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
        can_upgrade_colonies &&
        (nearby_enemy || larva_starved || supply_starved) && // Only throw down a sunken if you have no larva floating around, or need the supply.
        current_map_inventory.hatches_ > 1 &&
        Count_Units(UnitTypes::Zerg_Sunken_Colony) + Count_Units(UnitTypes::Zerg_Spore_Colony) < max( (current_map_inventory.hatches_ * (current_map_inventory.hatches_ + 1)) / 2, 6) ); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.


   
    //First Building needed!
    if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, Count_Units(UnitTypes::Zerg_Spawning_Pool) == 0 && friendly_player_model.units_.resource_depot_count_ > 0 );

    //Consider an organized build plan.
    if (friendly_player_model.u_relatively_weak_against_air_ && e_inv.stock_fliers_ > 0) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not). 
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_)  > 0 &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            current_map_inventory.hatches_ > 1);
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, upgrade_bool && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            Count_Units(UnitTypes::Zerg_Hydralisk_Den) == 0 &&
            current_map_inventory.hatches_ > 1);
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool && one_tech_per_base &&
            Count_SuccessorUnits(UnitTypes::Zerg_Spire, friendly_player_model.units_) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            current_map_inventory.hatches_ > 1);
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            (Count_SuccessorUnits(UnitTypes::Zerg_Spire, friendly_player_model.units_) > 0 || !checkInCartridge(UnitTypes::Zerg_Spire)) &&
            current_map_inventory.hatches_ > 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.
    }
    else if (!friendly_player_model.e_relatively_weak_against_air_) {

        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, upgrade_bool && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            Count_Units(UnitTypes::Zerg_Hydralisk_Den) == 0 &&
            current_map_inventory.hatches_ > 1);

        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            current_map_inventory.hatches_ > 1);

        // >2 bases
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 1 &&
            Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Upgrade, Broodwar->self()->getUnits()) == 1 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Morph, Broodwar->self()->getUnits()) == 0 && //costly, slow.
            Count_Units(UnitTypes::Zerg_Spawning_Pool) > 0 &&
            current_map_inventory.hatches_ > 2);

        // >3 bases
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            (Count_SuccessorUnits(UnitTypes::Zerg_Spire, friendly_player_model.units_) > 0 || !checkInCartridge(UnitTypes::Zerg_Spire)) &&
            current_map_inventory.hatches_ > 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Ultralisk_Cavern, drone, upgrade_bool && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) == 0 &&
            Count_Units(UnitTypes::Zerg_Hive) >= 0 &&
            current_map_inventory.hatches_ > 3);
    }
    else if (friendly_player_model.e_relatively_weak_against_air_) {
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Spire, drone, upgrade_bool && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Spire) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_)> 0 &&
            current_map_inventory.hatches_ > 1);
        // >3 bases
        if (!buildings_started) buildings_started = Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, upgrade_bool &&
            Count_Units(UnitTypes::Zerg_Queens_Nest) == 0 &&
            Count_SuccessorUnits(UnitTypes::Zerg_Lair, friendly_player_model.units_) > 0 &&
            (Count_SuccessorUnits(UnitTypes::Zerg_Spire, friendly_player_model.units_) > 0 || !checkInCartridge(UnitTypes::Zerg_Spire)) &&
            current_map_inventory.hatches_ > 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.
    }

        Stored_Unit& morphing_unit = friendly_player_model.units_.unit_inventory_.find(drone)->second;
        morphing_unit.updateStoredUnit(drone);

    return buildings_started;
};

// Cannot use for extractors, they are "too close" to the geyser.
TilePosition CUNYAIModule::getBuildablePosition( const TilePosition target_pos, const UnitType build_type, const int tile_grid_size ) {

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
                current_map_inventory.map_veins_[WalkPosition(TilePosition(centralize_x, centralize_y)).x][WalkPosition(TilePosition(centralize_x, centralize_y)).y] > widest_dim_in_minitiles // don't wall off please. Wide berth around blue veins.
            ) {
                canidate_return_position = TilePosition(centralize_x, centralize_y);
                break;
            }
        }
    }

    return canidate_return_position;
}

// clears all blocking units in the area excluding EXCEPTION_UNIT.  Purges all the worker relations for the scattered units.
void CUNYAIModule::clearBuildingObstuctions(const Unit_Inventory &ui, Map_Inventory &inv,const Unit &exception_unit ) {
    Unit_Inventory obstructions = CUNYAIModule::getUnitInventoryInRadius(ui, Position(inv.next_expo_), 3 * 32);
    for (auto u = obstructions.unit_inventory_.begin(); u != obstructions.unit_inventory_.end() && !obstructions.unit_inventory_.empty(); u++) {
        if (u->second.bwapi_unit_ && u->second.bwapi_unit_ != exception_unit ) {
            friendly_player_model.units_.purgeWorkerRelationsNoStop(u->second.bwapi_unit_, land_inventory, inv, my_reservation);
            friendly_player_model.units_.purgeWorkerRelationsNoStop(u->second.bwapi_unit_, land_inventory, inv, my_reservation);
            u->second.bwapi_unit_->move({ Position(inv.next_expo_).x + (rand() % 200 - 100) * 4 * 32, Position(inv.next_expo_).y + (rand() % 200 - 100) * 4 * 32 });
        }
    }
}

bool CUNYAIModule::Reactive_BuildFAP(const Unit &morph_canidate, const Map_Inventory &inv, const Unit_Inventory &ui, const Unit_Inventory &ei) { 

    //Am I sending this command to a larva or a hydra?
    UnitType u_type = morph_canidate->getType();
    bool is_larva = u_type == UnitTypes::Zerg_Larva;
    bool is_hydra = u_type == UnitTypes::Zerg_Hydralisk;
    bool is_muta = u_type == UnitTypes::Zerg_Mutalisk;
    bool is_building = false;
    bool wasting_larva_soon = true;
    int best_sim_score = INT_MIN;

    if (buildorder.checkBuilding_Desired(UnitTypes::Zerg_Lurker) && Count_Units(UnitTypes::Zerg_Hydralisk) == 0) {
        buildorder.retryBuildOrderElement(UnitTypes::Zerg_Hydralisk); // force in an hydra if 
        CUNYAIModule::DiagnosticText("Reactionary Hydralisk. Must have lost one.");
        return is_building = true;
    }

    //Evo chamber is required tech for spore colony
    if (Count_Units(UnitTypes::Zerg_Evolution_Chamber) == 0 && !buildorder.checkBuilding_Desired(UnitTypes::Zerg_Evolution_Chamber) && friendly_player_model.u_relatively_weak_against_air_ && enemy_player_model.units_.stock_fliers_ > 0) {
        buildorder.retryBuildOrderElement(UnitTypes::Zerg_Evolution_Chamber); // force in an evo chamber if they have Air.
        CUNYAIModule::DiagnosticText("Reactionary Evo Chamber");
        return is_building = true;
    }


    if (is_larva && morph_canidate->getHatchery()) {
        wasting_larva_soon = morph_canidate->getHatchery()->getRemainingTrainTime() < 5 + Broodwar->getLatencyFrames() && morph_canidate->getHatchery()->getLarva().size() == 3 && inv.min_fields_ > 8; // no longer will spam units when I need a hatchery.
    }

    bool enough_drones = (Count_Units(UnitTypes::Zerg_Drone) > inv.min_fields_ * 2 + Count_Units(UnitTypes::Zerg_Extractor) * 3 + 1) || Count_Units(UnitTypes::Zerg_Drone) >= 85;
    bool drone_conditional = econ_starved || tech_starved; // Econ does not detract from technology growth. (only minerals, gas is needed for tech). Always be droning.

    //Supply blocked protection 
    if (is_larva && !is_building) is_building = Check_N_Grow(UnitTypes::Zerg_Overlord, morph_canidate, supply_starved);
    // Eco building.
    if (is_larva && !is_building) is_building = Check_N_Grow(morph_canidate->getType().getRace().getWorker(), morph_canidate, (drone_conditional || wasting_larva_soon) && !enough_drones);
    if (is_building) return is_building; // combat simulations are very costly.

    //Let us simulate some combat.
    is_building = CUNYAIModule::buildOptimalUnit(morph_canidate, friendly_player_model.combat_unit_cartridge_);


    return is_building;

}

bool CUNYAIModule::buildStaticDefence(const Unit &morph_canidate) {

    if (checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Spore_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (checkFeasibleRequirement(morph_canidate, UnitTypes::Zerg_Sunken_Colony)) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    bool must_make_spore = checkDesirable(morph_canidate, UnitTypes::Zerg_Spore_Colony, true);
    bool must_make_sunken = checkDesirable(morph_canidate, UnitTypes::Zerg_Sunken_Colony, true);

    if (friendly_player_model.u_relatively_weak_against_air_ && must_make_spore) return morph_canidate->morph(UnitTypes::Zerg_Spore_Colony);
    else if (!friendly_player_model.u_relatively_weak_against_air_ && must_make_sunken) return morph_canidate->morph(UnitTypes::Zerg_Sunken_Colony);

    return false;
}

//contains a filter to discard unbuildable sorts of units, then finds the best unit via a series of BuildFAP sim, then builds it.
bool CUNYAIModule::buildOptimalUnit(const Unit &morph_canidate, map<UnitType, int> combat_types) {
    bool building_optimal_unit = false;
    int best_sim_score = INT_MIN;
    UnitType build_type = UnitTypes::None;

    // drop all units types I cannot assemble at this time.
    auto pt_type = combat_types.begin();
    while (pt_type != combat_types.end()) {
        bool can_make_or_already_is = morph_canidate->getType() == pt_type->first || checkDesirable( morph_canidate, pt_type->first, true);
        bool is_larva = morph_canidate->getType() == UnitTypes::Zerg_Larva;
        bool can_morph_into_prerequisite_hydra = checkDesirable(morph_canidate, UnitTypes::Zerg_Lurker, true) && pt_type->first == UnitTypes::Zerg_Lurker;
        bool can_morph_into_prerequisite_muta = checkDesirable(morph_canidate, UnitTypes::Zerg_Guardian, true) && (pt_type->first == UnitTypes::Zerg_Guardian || pt_type->first == UnitTypes::Zerg_Devourer); 


        if (can_make_or_already_is || (is_larva && can_morph_into_prerequisite_hydra) || (is_larva && can_morph_into_prerequisite_muta)) {
            //CUNYAIModule::DiagnosticText("Considering morphing a %s", pt_type->first.c_str());
            pt_type++;
        }
        else {
            combat_types.erase(pt_type++);
        }
    }


    //Heuristics for building. They are pretty simple combat results from a simulation.
    bool it_needs_to_shoot_up = false;
    bool it_needs_to_shoot_down = false;
    bool it_needs_to_fly = false;
    bool too_many_scourge = false;
    bool required = false;

    // determine undesirables in simulation.
    for (auto &potential_type : combat_types) {
        if (potential_type.first.airWeapon() != WeaponTypes::None && friendly_player_model.u_relatively_weak_against_air_)  it_needs_to_shoot_up = true;
        if (potential_type.first.groundWeapon() != WeaponTypes::None && !friendly_player_model.u_relatively_weak_against_air_)  it_needs_to_shoot_down = true;
        if (potential_type.first.isFlyer() && friendly_player_model.e_relatively_weak_against_air_)  it_needs_to_fly = true;
        if (potential_type.first == UnitTypes::Zerg_Scourge && enemy_player_model.units_.stock_fliers_ < Count_Units(UnitTypes::Zerg_Scourge) * Stored_Unit(UnitTypes::Zerg_Scourge).stock_value_ )  too_many_scourge = true;
        if (CUNYAIModule::checkFeasibleRequirement(morph_canidate, potential_type.first)) required = true;
    }
    // remove undesireables.
    auto potential_type = combat_types.begin();
    while (potential_type != combat_types.end()) {
        if (required) potential_type++;
        else if (potential_type->first.airWeapon() == WeaponTypes::None && it_needs_to_shoot_up) combat_types.erase(potential_type++);
        else if (potential_type->first.groundWeapon() == WeaponTypes::None && it_needs_to_shoot_down) combat_types.erase(potential_type++);
        else if (!potential_type->first.isFlyer() && it_needs_to_fly) combat_types.erase(potential_type++);
        else if (potential_type->first == UnitTypes::Zerg_Scourge && too_many_scourge)  combat_types.erase(potential_type++);

        else potential_type++;
    }

    if (combat_types.empty()) return false;
    else if (combat_types.size() == 1) build_type = combat_types.begin()->first;
    else build_type = returnOptimalUnit(combat_types, friendly_player_model.researches_);

    // Build it.
    if (!building_optimal_unit) building_optimal_unit = Check_N_Grow(build_type, morph_canidate, true) || morph_canidate->getType() == build_type; // catchall ground units, in case you have a BO that needs to be done.
    if (building_optimal_unit) {
        //if (Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Best sim score is: %d, building %s", best_sim_score, build_type.c_str());
        return true;
    }
    return false;
}

//Simply returns the unittype that is the "best" of a BuildFAP sim.
UnitType CUNYAIModule::returnOptimalUnit(map<UnitType, int> &combat_types, const Research_Inventory &ri) {
    bool building_optimal_unit = false;
    auto buildfap_temp = buildfap; // contains everything we're looking for except for the mock units. Keep this copy around so we don't destroy the original.
    int best_sim_score = INT_MIN;
    UnitType build_type = UnitTypes::None;
    Position comparision_spot = positionBuildFap(true);// all compared units should begin in the exact same position.

    //add friendly units under consideration to FAP in loop, resetting each time.
    for (auto &potential_type : combat_types) {
            buildfap_temp.clear();
            buildfap_temp = buildfap; // restore the buildfap temp.
            Stored_Unit su = Stored_Unit(potential_type.first);
            // enemy units do not change.
            Unit_Inventory friendly_units_under_consideration; // new every time.
            friendly_units_under_consideration.addStored_Unit(su); //add unit we are interested in to the inventory:
            if (potential_type.first.isTwoUnitsInOneEgg()) friendly_units_under_consideration.addStored_Unit(su); // do it twice if you're making 2.
            friendly_units_under_consideration.addToFAPatPos(buildfap_temp, comparision_spot, true, ri);
            buildfap_temp.simulate(24*20); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
            potential_type.second = getFAPScore(buildfap_temp, true) - getFAPScore(buildfap_temp, false);
            //if(Broodwar->getFrameCount() % 96 == 0) CUNYAIModule::DiagnosticText("Found a sim score of %d, for %s", combat_types.find(potential_type.first)->second, combat_types.find(potential_type.first)->first.c_str());
    }

    for (auto &potential_type : combat_types) {
        if (potential_type.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_type.second;
            build_type = potential_type.first;
            //CUNYAIModule::DiagnosticText("Found a Best_sim_score of %d, for %s", best_sim_score, build_type.c_str());
        }
        else if (potential_type.second == best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            //build_type = 
            if(build_type.airWeapon() != WeaponTypes::None && build_type.groundWeapon() != WeaponTypes::None) continue; // if the current unit is "flexible" with regard to air and ground units, then keep it and continue to consider the next unit.
            if(potential_type.first.airWeapon() != WeaponTypes::None && potential_type.first.groundWeapon() != WeaponTypes::None) build_type = potential_type.first; // if the tying unit is "flexible", then let's use that one.
            //CUNYAIModule::DiagnosticText("Found a tie, favoring the flexible unit %d, for %s", best_sim_score, build_type.c_str());
        }
    }

    return build_type;

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

bool CUNYAIModule::checkFeasibleRequirement(const Unit &unit, const UnitType &ut) {
    return Broodwar->canMake(ut, unit) && my_reservation.checkAffordablePurchase(ut) && checkInCartridge(ut) && buildorder.checkBuilding_Desired(ut);
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
