#pragma once
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\AssemblyManager.h"
#include <iterator>

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations.
bool MeatAIModule::Check_N_Build(const UnitType &building, const Unit &unit, const Unit_Inventory &ui, const bool &extra_critera)
{
    if (my_reservation.checkAffordablePurchase(building) && (buildorder.checkBuilding_Desired(building) || (extra_critera && buildorder.checkEmptyBuildOrder()))) {

        //Check simple upgrade into lair/hive.
        Unit_Inventory local_area = getUnitInventoryInRadius( ui, unit->getPosition(), 250);
        bool hatch_nearby = Count_Units(UnitTypes::Zerg_Hatchery, local_area) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
            Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
            Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;
        if (unit->canMorph(building) && checkSafeBuildLoc( unit->getPosition(), inventory, enemy_inventory, friendly_inventory, neutral_inventory) &&
            (unit->getType().isBuilding() || hatch_nearby ) ){
                if (unit->morph(building)) {
                    buildorder.announceBuildingAttempt(building); // Takes no time, no need for the reserve system.
                    return true;
                }
        }

        if (unit->canBuild(building) && building != UnitTypes::Zerg_Creep_Colony && building != UnitTypes::Zerg_Extractor && building != UnitTypes::Zerg_Hatchery)
        {
            TilePosition buildPosition = MeatAIModule::getBuildablePosition(unit->getTilePosition(), building, 12);
            if (unit->build(building, buildPosition) && my_reservation.addReserveSystem(building, buildPosition) && hatch_nearby) {
                buildorder.announceBuildingAttempt(building);
                return true;
            }
            else if (buildorder.checkBuilding_Desired(building)) {
                Broodwar->sendText("I can't put a %s at (%d, %d) for you. Skip it and go on?...", building.c_str(), buildPosition.x, buildPosition.y);
                buildorder.updateRemainingBuildOrder(building); // skips the building.
            }
        }
        else if (unit->canBuild(building) && building == UnitTypes::Zerg_Creep_Colony) { // creep colony loop specifically.

            Unitset base_core = unit->getUnitsInRadius(1, IsBuilding && IsResourceDepot && IsCompleted); // don't want undefined crash.
            TilePosition central_base = TilePosition(0, 0);
            TilePosition final_creep_colony_spot = TilePosition(0, 0);

            for (const auto &u : ui.unit_inventory_) {
                if (u.second.type_ == UnitTypes::Zerg_Hatchery) {
                    base_core.insert(u.second.bwapi_unit_);
                }
                else if (u.second.type_ == UnitTypes::Zerg_Lair) {
                    base_core.insert(u.second.bwapi_unit_);
                }
                else if (u.second.type_ == UnitTypes::Zerg_Hive) {
                    base_core.insert(u.second.bwapi_unit_);
                }
            }

            if (Count_Units(UnitTypes::Zerg_Evolution_Chamber, inventory) > 0 && enemy_inventory.stock_fliers_ > 0.75 * friendly_inventory.stock_shoots_up_) {
                Unit_Inventory hacheries = getUnitInventoryInRadius(ui, UnitTypes::Zerg_Hatchery, unit->getPosition(), 500);
                Stored_Unit *close_hatch = getClosestStored(hacheries, unit->getPosition(), 500);
                if (close_hatch) {
                    central_base = TilePosition(close_hatch->pos_);
                }
            }

            if (inventory.map_veins_out_from_enemy_.size() != 0 && inventory.getRadialDistanceOutFromEnemy(unit->getPosition()) > 0) { // if we have identified the enemy's base, build at the spot closest to them.
                if (central_base == TilePosition(0, 0)) {
                    int old_dist = 9999999;

                    for (auto base = base_core.begin(); base != base_core.end(); ++base) {
                        TilePosition central_base_new = TilePosition((*base)->getPosition());
                        int new_dist = inventory.getRadialDistanceOutFromEnemy((*base)->getPosition());
                        Unit_Inventory e_loc = getUnitInventoryInRadius(enemy_inventory, Position(central_base_new), 750);
                        Unit_Inventory e_too_close = getUnitInventoryInRadius(enemy_inventory, Position(central_base_new), 250);
                        Unit_Inventory friend_loc = getUnitInventoryInRadius(friendly_inventory, Position(central_base_new), 750);
                        int closest_enemy = 0;
                        int closest_hatch = 0;
                        bool enemy_nearby = false;
                        if (getClosestThreatOrTargetStored(e_loc, UnitTypes::Zerg_Drone, (*base)->getPosition(), 750)) {
                            enemy_nearby = e_loc.stock_ground_units_ > friend_loc.stock_ground_units_;
                        }

                        if ( (new_dist <= old_dist || enemy_nearby) && checkSafeBuildLoc(Position(central_base_new), inventory, enemy_inventory, friendly_inventory, neutral_inventory) ) {
                            central_base = central_base_new;
                            old_dist = new_dist;
                            if (enemy_nearby) { 
                                break; 
                            }
                        }
                    }
                } //confirm we have identified a base around which to build.

                int chosen_base_distance = inventory.getRadialDistanceOutFromEnemy(Position(central_base));
                for (int x = -5; x <= 5; ++x) {
                    for (int y = -5; y <= 5; ++y) {
                        double centralize_x = central_base.x + x;
                        double centralize_y = central_base.y + y;
                        if (!(x == 0 && y == 0) &&
                            centralize_x < Broodwar->mapWidth() &&
                            centralize_y < Broodwar->mapHeight() &&
                            centralize_x > 0 &&
                            centralize_y > 0 &&
                            getResourceInventoryInRadius(neutral_inventory, Position(TilePosition((int)centralize_x, (int)centralize_y)), 96).resource_inventory_.empty() &&
                            Broodwar->canBuildHere(TilePosition((int)centralize_x, (int)centralize_y), UnitTypes::Zerg_Creep_Colony, unit, false) &&
                            inventory.map_veins_[WalkPosition(TilePosition((int)centralize_x, (int)centralize_y)).x][WalkPosition(TilePosition((int)centralize_x, (int)centralize_y)).y] > 20 && // don't wall off please. Wide berth around blue veins.
                            inventory.getRadialDistanceOutFromEnemy(Position(TilePosition((int)centralize_x, (int)centralize_y))) <= chosen_base_distance) // Count all points further from home than we are.
                        {
                            final_creep_colony_spot = TilePosition((int)centralize_x, (int)centralize_y);
                            chosen_base_distance = inventory.getRadialDistanceOutFromEnemy(Position(TilePosition((int)centralize_x, (int)centralize_y)));
                        }
                    }
                }
            }

            if (final_creep_colony_spot == TilePosition(0, 0)) {// if we have NOT identified the enemy's base, build at the spot furthest from our center..
                if (central_base == TilePosition(0, 0)) {
                    int old_dist = 0;

                    for (auto base = base_core.begin(); base != base_core.end(); ++base) {

                        TilePosition central_base_new = TilePosition((*base)->getPosition());
                        int new_dist = inventory.getRadialDistanceOutFromHome((*base)->getPosition());

                        if (new_dist >= old_dist) {
                            central_base = central_base_new;
                            old_dist = new_dist;
                        }
                    }
                } //confirm we have identified a base around which to build.

                int chosen_base_distance = inventory.getRadialDistanceOutFromHome(Position(central_base));
                for (int x = -5; x <= 5; ++x) {
                    for (int y = -5; y <= 5; ++y) {
                        double centralize_x = central_base.x + x;
                        double centralize_y = central_base.y + y;
                        if (!(x == 0 && y == 0) &&
                            centralize_x < Broodwar->mapWidth() &&
                            centralize_y < Broodwar->mapHeight() &&
                            centralize_x > 0 &&
                            centralize_y > 0 &&
                            getResourceInventoryInRadius(neutral_inventory, Position(TilePosition((int)centralize_x, (int)centralize_y)), 96).resource_inventory_.empty() &&
                            Broodwar->canBuildHere(TilePosition((int)centralize_x, (int)centralize_y), UnitTypes::Zerg_Creep_Colony, unit, false) &&
                            inventory.map_veins_[WalkPosition(TilePosition((int)centralize_x, (int)centralize_y)).x][WalkPosition(TilePosition((int)centralize_x, (int)centralize_y)).y] > 20 && // don't wall off please. wide berth around blue veins
                            inventory.getRadialDistanceOutFromHome(Position(TilePosition((int)centralize_x, (int)centralize_y))) >= chosen_base_distance) // Count all points further from home than we are.
                        {
                            final_creep_colony_spot = TilePosition((int)centralize_x, (int)centralize_y);
                            chosen_base_distance = inventory.getRadialDistanceOutFromHome(Position(TilePosition((int)centralize_x, (int)centralize_y)));
                        }
                    }
                }
            }

            TilePosition buildPosition = Broodwar->getBuildLocation(building, final_creep_colony_spot, 4);
            if (unit->build(building, buildPosition) && my_reservation.addReserveSystem(building, buildPosition)) {
                buildorder.announceBuildingAttempt(building);
                return true;
            }
            else if (buildorder.checkBuilding_Desired(building)) {
                Broodwar->sendText("I can't put a %s at (%d, %d) for you. Skip it and go on?...", building.c_str(), buildPosition.x, buildPosition.y);
                buildorder.updateRemainingBuildOrder(building); // skips the building.
            }
        }
        else if (unit->canBuild(building) && building == UnitTypes::Zerg_Extractor) {
            Stored_Resource* closest_gas = MeatAIModule::getClosestGroundStored(neutral_inventory, UnitTypes::Resource_Vespene_Geyser, inventory, unit->getPosition());
            if (closest_gas && closest_gas->occupied_natural_ && closest_gas->bwapi_unit_ ) {
                TilePosition buildPosition = closest_gas->bwapi_unit_->getTilePosition();
                //TilePosition buildPosition = MeatAIModule::getBuildablePosition(TilePosition(closest_gas->pos_), building, 5);
                //TilePosition buildPosition = Broodwar->getBuildLocation(building, TilePosition(closest_gas->pos_), 64) ;
                if ( BWAPI::Broodwar->isVisible(buildPosition) && unit->build(building, buildPosition) && my_reservation.addReserveSystem(building, buildPosition)) {
                    buildorder.announceBuildingAttempt(building);
                    return true;
                } //extractors must have buildings nearby or we shouldn't build them.
                //else if ( !BWAPI::Broodwar->isVisible(buildPosition) && unit->move(Position(buildPosition)) && my_reservation.addReserveSystem(building, buildPosition)) {
                //    buildorder.announceBuildingAttempt(building);
                //    return true;
                //} //extractors must have buildings nearby or we shouldn't build them.
                else if ( BWAPI::Broodwar->isVisible(buildPosition) && buildorder.checkBuilding_Desired(building)) {
                    Broodwar->sendText("I can't put a %s at (%d, %d) for you. Skip it and go on?...", building.c_str(), buildPosition.x, buildPosition.y);
                    buildorder.updateRemainingBuildOrder(building); // skips the building.
                }
            }
        }
        else if (unit->canBuild(building) && building == UnitTypes::Zerg_Hatchery) {
            if (unit->canBuild(building) && checkSafeBuildLoc(unit->getPosition(), inventory, enemy_inventory, friendly_inventory, neutral_inventory) ) {
                TilePosition buildPosition = Broodwar->getBuildLocation(building, unit->getTilePosition(), 64);

                local_area = getUnitInventoryInRadius(ui, Position(buildPosition), 250);
                hatch_nearby = Count_Units(UnitTypes::Zerg_Hatchery, local_area) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
                    Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
                    Count_Units(UnitTypes::Zerg_Hive, local_area) > 0;

                if (unit->build(building, buildPosition) && my_reservation.addReserveSystem(building, buildPosition) && hatch_nearby) {
                    buildorder.announceBuildingAttempt(building);
                    return true;
                }
                else if (buildorder.checkBuilding_Desired(building)) {
                    Broodwar->sendText("I can't put a %s at (%d, %d) for you. Skip it and go on?...", building.c_str(), buildPosition.x, buildPosition.y);
                    buildorder.updateRemainingBuildOrder(building); // skips the building.
                }
            }
        }
    }
    return false;
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
bool MeatAIModule::Check_N_Upgrade(const UpgradeType &ups, const Unit &unit, const bool &extra_critera)
{
    if (unit->canUpgrade(ups) && my_reservation.checkAffordablePurchase(ups) && (buildorder.checkUpgrade_Desired(ups) || (extra_critera && buildorder.checkEmptyBuildOrder()))) {
        if (unit->upgrade(ups)) {
            buildorder.updateRemainingBuildOrder(ups);
            Broodwar->sendText("Upgrading %s.", ups.c_str());
            return true;
        }
    }
    return false;
}

bool MeatAIModule::Check_N_Research(const TechType &tech, const Unit &unit, const bool &extra_critera)
{
    if (unit->canResearch(tech) && my_reservation.checkAffordablePurchase(tech) && (buildorder.checkResearch_Desired(tech) || (extra_critera && buildorder.checkEmptyBuildOrder()))) {
        if (unit->research(tech)) {
            buildorder.updateRemainingBuildOrder(tech);
            Broodwar->sendText("Researching %s.", tech.c_str());
            return true;
        }
    }
    return false;
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
bool MeatAIModule::Check_N_Grow(const UnitType &unittype, const Unit &larva, const bool &extra_critera)
{
    if (larva->canMorph(unittype) && my_reservation.checkAffordablePurchase(unittype) && (buildorder.checkBuilding_Desired(unittype) || (extra_critera && buildorder.checkEmptyBuildOrder())))
    {
        larva->morph(unittype);
        buildorder.updateRemainingBuildOrder(unittype); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.

        if (unittype.isTwoUnitsInOneEgg()) {
            buildorder.updateRemainingBuildOrder(unittype); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.
        }
        return true;
    }

    return false;
}

//Creates a new unit. Reflects (poorly) upon enemy units in enemy_set. Incomplete.
bool MeatAIModule::Reactive_Build(const Unit &larva, const Inventory &inv, const Unit_Inventory &ui, const Unit_Inventory &ei)
{
    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?
    int is_building = 0;

    //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
    //bool early_game = Count_Units( UnitTypes::Zerg_Spawning_Pool, ui ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 && inv.min_workers_ + inv.gas_workers_ <= 9;
    bool wasting_larva_soon = true;

    if (larva->getHatchery()) {
        wasting_larva_soon = larva->getHatchery()->getRemainingTrainTime() < 5 && larva->getHatchery()->getLarva().size() == 3 && inv.min_fields_ > 8; // no longer will spam units when I need a hatchery.
    }

    bool enough_drones = (Count_Units(UnitTypes::Zerg_Drone, inv) > inv.min_fields_ * 2 + Count_Units(UnitTypes::Zerg_Extractor, inv) * 3 + 1) || Count_Units(UnitTypes::Zerg_Drone, inv) >= 85;
    bool drone_conditional = econ_starved || tech_starved; // Econ does not detract from technology growth. (only minerals, gas is needed for tech). Always be droning.
    bool one_tech_per_base = Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + Count_Units(UnitTypes::Zerg_Spire, inv) + Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) < inv.hatches_;

    bool would_force_spire = Count_Units(UnitTypes::Zerg_Spire, inv) == 0 &&
        !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) &&
        !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) &&
        buildorder.checkEmptyBuildOrder();

    bool would_force_lurkers = Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) > 0 &&
        !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) &&
        !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) &&
        buildorder.checkEmptyBuildOrder();

    bool enemy_mostly_ground = ei.stock_ground_units_ >= ei.stock_total_ * 0.75;
    bool enemy_lacks_AA = ei.stock_shoots_up_ <= 0.25 * ei.stock_shoots_down_;

    //bool marginal_benifit_from_air = ui.stock_fliers_ / (ei.stock_shoots_up_ + 1);
    //bool marginal_benifit_from_ground = ui.stock_shoots_down_ / (ei.stock_ground_units_ + 1);  //these aren't quite right 

    int remaining_cost_to_get_lurkers = Stored_Unit(UnitTypes::Zerg_Spawning_Pool).stock_value_ - Stock_Units(UnitTypes::Zerg_Spawning_Pool, ui) +
        Stored_Unit(UnitTypes::Zerg_Hydralisk_Den).stock_value_ - Stock_Units(UnitTypes::Zerg_Hydralisk_Den, ui) +
        Stored_Unit(UnitTypes::Zerg_Lair).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Lair, ui) +
        Stored_Unit(UnitTypes::Zerg_Hive).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Hive, ui) +
        TechTypes::Lurker_Aspect.mineralPrice() + 1.25 * TechTypes::Lurker_Aspect.gasPrice() - Stock_Tech(TechTypes::Lurker_Aspect);

    int remaining_cost_to_get_mutas = Stored_Unit(UnitTypes::Zerg_Spawning_Pool).stock_value_ - Stock_Units(UnitTypes::Zerg_Spawning_Pool, ui) +
        Stored_Unit(UnitTypes::Zerg_Spire).stock_value_ - Stock_Units(UnitTypes::Zerg_Spire, ui) +
        Stored_Unit(UnitTypes::Zerg_Lair).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Lair, ui) +
        Stored_Unit(UnitTypes::Zerg_Hive).stock_value_ - Stock_Buildings(UnitTypes::Zerg_Hive, ui);

    //bool u_relatively_weak_against_air = ei.stock_fliers_ / (double)(ui.stock_shoots_up_ + 1) > ei.stock_ground_units_ / (double)(ui.stock_shoots_down_ + 1); // div by zero concern.
    bool u_relatively_weak_against_air = - ei.stock_fliers_ / (double)pow((ui.stock_shoots_up_ + 1),2) < - ei.stock_ground_units_ / (double)pow((ui.stock_shoots_down_ + 1),2); // div by zero concern. Derivative of the above equation and inverted (ie. which will decrease my weakness faster?)
    //bool e_relatively_weak_against_air = ui.stock_fliers_ / (double)(ei.stock_shoots_up_ + 1) > ui.stock_ground_units_ / (double)(ei.stock_shoots_down_ + 1); // div by zero concern.  
    bool e_relatively_weak_against_air = 1 / (double)(ei.stock_shoots_up_ + 1) > 1 / (double)(ei.stock_shoots_down_ + 1); // div by zero concern. Derivative of the above equation.

    // Do required build first.
    if (!buildorder.checkEmptyBuildOrder() && buildorder.building_gene_.front().getUnit() != UnitTypes::None) {
        is_building += Check_N_Grow(buildorder.building_gene_.front().getUnit(), larva, true);
    }

    //Supply blocked protection 
    is_building += Check_N_Grow(UnitTypes::Zerg_Overlord, larva, supply_starved && is_building == 0);
    // Eco building.
    is_building += Check_N_Grow(larva->getType().getRace().getWorker(), larva, (drone_conditional || wasting_larva_soon) && !enough_drones);

    //Army build/replenish.  Cycle through military units available.
    if (ei.stock_fliers_ > ui.stock_shoots_up_ || u_relatively_weak_against_air ) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not). 
        is_building += Check_N_Grow(UnitTypes::Zerg_Scourge, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spire, inv) > 0 && Count_Units(UnitTypes::Zerg_Scourge, inv) < 5); // hard cap on scourges, they build 2 at a time. May (still) overbuild within a single frame.
        is_building += Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spire, inv) > 0);
        is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) > 0);

        if (Count_Units(UnitTypes::Zerg_Evolution_Chamber, inv) == 0 && buildorder.checkEmptyBuildOrder()) {
            buildorder.building_gene_.push_back(Build_Order_Object(UnitTypes::Zerg_Evolution_Chamber)); // force in a hydralisk den if they have Air.
            Broodwar->sendText("Reactionary Evo Chamber");
            return is_building > 0;
        }
        else if (Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) == 0 && buildorder.checkEmptyBuildOrder()) {
            buildorder.building_gene_.push_back(Build_Order_Object(UnitTypes::Zerg_Hydralisk_Den));
            Broodwar->sendText("Reactionary Hydra Den");
            return is_building > 0;
        }
        else if (Count_Units(UnitTypes::Zerg_Lair, inv) - Count_Units_In_Progress(UnitTypes::Zerg_Lair, inv) > 0 && one_tech_per_base && Count_Units(UnitTypes::Zerg_Spire, inv) == 0 && buildorder.checkEmptyBuildOrder()) {
            buildorder.building_gene_.push_back(Build_Order_Object(UnitTypes::Zerg_Spire));
            Broodwar->sendText("Reactionary Spire");
            return is_building > 0;
        }

    }
    else if ( !e_relatively_weak_against_air) {
        is_building += Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) > 0); // catchall ground units.

        bool lings_only = Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0 && Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv ) == 0 && Count_Units(UnitTypes::Zerg_Lair, inv) == 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0;
        bool hydras_only = Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) > 0 && Count_Units(UnitTypes::Zerg_Lair, inv) == 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0;
        bool saving_for_lurkers = Count_Units(UnitTypes::Zerg_Lair, inv) > 0 && !Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) && !Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0;
        bool lurkers_incoming = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) || Broodwar->self()->isResearching(TechTypes::Lurker_Aspect) && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0;
        bool ultralisks_ready = Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) > 0 && Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0;

        if (lings_only) {
            is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0);
        }
        else if (hydras_only) {
            is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.checkExcessIsGreaterThan(UnitTypes::Zerg_Lair) );
            is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.getExcessMineral() > UnitTypes::Zerg_Lair.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (saving_for_lurkers) {
            is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.checkExcessIsGreaterThan(TechTypes::Lurker_Aspect));
            is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.getExcessMineral() > TechTypes::Lurker_Aspect.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (lurkers_incoming) {
            is_building += Check_N_Grow(UnitTypes::Zerg_Lurker, larva, army_starved && is_building == 0 && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0);
            is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.checkExcessIsGreaterThan(UnitTypes::Zerg_Lurker) || Count_Units(UnitTypes::Zerg_Hydralisk, inv) == 0);
            is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.getExcessMineral() > UnitTypes::Zerg_Lurker.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
        else if (ultralisks_ready) {
            is_building += Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0);
            is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && my_reservation.getExcessMineral() % 200 >  UnitTypes::Zerg_Zergling.mineralPrice()); // if you are floating minerals relative to gas, feel free to buy some lings.
        }
    }
    else if ( e_relatively_weak_against_air ) {
        is_building += Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spire, inv) > 0);
        is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spire, inv) > 0 && my_reservation.getExcessMineral() - my_reservation.getExcessGas() > 50); // if you are floating minerals relative to gas, feel free to buy some lings.
    }

    is_building += Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) > 0 && !u_relatively_weak_against_air); // catchall units.
    is_building += Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spire, inv) > 0);
    is_building += Check_N_Grow(UnitTypes::Zerg_Lurker, larva, army_starved && is_building == 0 && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0 && !u_relatively_weak_against_air);
    is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) > 0 );
    is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, (army_starved || wasting_larva_soon) && is_building == 0 && Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0 );


    is_building += Check_N_Grow(UnitTypes::Zerg_Ultralisk, larva, false); // catchall ground units, in case you have a BO that needs to be done.
    is_building += Check_N_Grow(UnitTypes::Zerg_Mutalisk, larva, false);
    is_building += Check_N_Grow(UnitTypes::Zerg_Lurker, larva, false);
    is_building += Check_N_Grow(UnitTypes::Zerg_Hydralisk, larva, false);
    is_building += Check_N_Grow(UnitTypes::Zerg_Zergling, larva, false);
    is_building += Check_N_Grow(UnitTypes::Zerg_Drone, larva, false);

    //if ((would_force_lurkers || would_force_spire) && Count_Units(UnitTypes::Zerg_Lair, ui) == 0 && one_tech_per_base && Count_Units(UnitTypes::Zerg_Extractor, ui) > 0 ) {
    //    buildorder.building_gene_.push_back(Build_Order_Object(UnitTypes::Zerg_Lair)); // force lair if you need it and are in a position for it.
    //    Broodwar->sendText("Reactionary Lair, there's tech I want.");
    //    return is_building > 0;
    //} 

    if (u_relatively_weak_against_air && would_force_spire && buildorder.checkEmptyBuildOrder() && Count_Units(UnitTypes::Zerg_Lair, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0 && one_tech_per_base) {
        buildorder.building_gene_.push_back(Build_Order_Object(UnitTypes::Zerg_Spire)); // force in a Spire if they have no AA. Note that there is no one-base muta build on TL. So let's keep this restriction of 1 tech per base.
        Broodwar->sendText("Reactionary Spire");
        return is_building > 0;
    }
    else if (enemy_mostly_ground && would_force_lurkers && buildorder.checkEmptyBuildOrder() && Count_Units(UnitTypes::Zerg_Lair, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0) {
        buildorder.building_gene_.push_back(Build_Order_Object(TechTypes::Lurker_Aspect)); // force in a hydralisk den if they have Air.
        Broodwar->sendText("Reactionary Lurker Upgrade");
        return is_building > 0;
    }
    return is_building > 0;
}

//Creates a new building with DRONE. Incomplete.
bool MeatAIModule::Building_Begin(const Unit &drone, const Inventory &inv, const Unit_Inventory &e_inv, const Unit_Inventory &u_inv) {
    // will send it to do the LAST thing on this list that it can build.
    int buildings_started = 0;
    bool expansion_meaningful = (Count_Units(UnitTypes::Zerg_Drone, inv) < 85 && (inventory.min_workers_ >= inventory.min_fields_ * 2 || inventory.gas_workers_ >= 2 * Count_Units(UnitTypes::Zerg_Extractor, inv))) || inventory.min_fields_ < 8;
    bool larva_starved = Count_Units(UnitTypes::Zerg_Larva, inv) <= Count_Units(UnitTypes::Zerg_Hatchery, inv);
    bool upgrade_bool = (tech_starved || (Count_Units(UnitTypes::Zerg_Larva, inv) == 0 && !army_starved));
    bool lurker_tech_progressed = Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect);
    bool one_tech_per_base = Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) /*+ Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect) + Broodwar->self()->isResearching(TechTypes::Lurker_Aspect)*/ + Count_Units(UnitTypes::Zerg_Spire, inv) + Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) < Count_Units(UnitTypes::Zerg_Hatchery, inv) - Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, inv);
    bool can_upgrade_colonies = (Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0) ||
        (Count_Units(UnitTypes::Zerg_Evolution_Chamber, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber) > 0); // There is a building complete that will allow either creep colony upgrade.
    bool enemy_mostly_ground = e_inv.stock_ground_units_ > e_inv.stock_total_ * 0.75;
    bool enemy_lacks_AA = e_inv.stock_shoots_up_ < 0.25 * e_inv.stock_total_;
    bool nearby_enemy = checkOccupiedArea(enemy_inventory,drone->getPosition(), 2500);
    Unit_Inventory e_loc;
    Unit_Inventory u_loc;

    if (nearby_enemy) {
        e_loc = getUnitInventoryInRadius(e_inv, drone->getPosition(), inv.my_portion_of_the_map_);
        u_loc = getUnitInventoryInRadius(u_inv, drone->getPosition(), inv.my_portion_of_the_map_);
    }


    int invest_in_lurkers = Stock_Units(UnitTypes::Zerg_Spawning_Pool, u_inv) +
        Stock_Units(UnitTypes::Zerg_Hydralisk_Den, u_inv) +
        Stock_Buildings(UnitTypes::Zerg_Lair, u_inv) +
        Stock_Buildings(UnitTypes::Zerg_Hive, u_inv) +
        Stock_Tech(TechTypes::Lurker_Aspect);

    int invest_in_mutas = Stock_Units(UnitTypes::Zerg_Spawning_Pool, u_inv) +
        Stock_Units(UnitTypes::Zerg_Spire, u_inv) +
        Stock_Buildings(UnitTypes::Zerg_Lair, u_inv) +
        Stock_Buildings(UnitTypes::Zerg_Hive, u_inv);

    //Combat Buildings
    buildings_started += Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone, friendly_inventory, (army_starved || e_loc.stock_total_ > u_loc.stock_total_) &&  // army starved or under attack. ? And?
        Count_Units(UnitTypes::Zerg_Creep_Colony, inv) * 50 + 50 <= my_reservation.getExcessMineral() && // Only build a creep colony if we can afford to upgrade the ones we have.
        can_upgrade_colonies &&
        buildings_started == 0 &&
        ((larva_starved || supply_starved) && nearby_enemy) && // Only throw down a sunken if you have no larva floating around, or need the supply.
        inv.hatches_ > 1 &&
        Count_Units(UnitTypes::Zerg_Sunken_Colony, inv) + Count_Units(UnitTypes::Zerg_Spore_Colony, inv) < max((inv.hatches_ * (inv.hatches_ + 1)) / 2, 6)); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.

    //Macro-related Buildings.
    buildings_started += Check_N_Build(UnitTypes::Zerg_Extractor, drone, friendly_inventory, buildings_started == 0 && 
        (inv.gas_workers_ >= 2 * (Count_Units(UnitTypes::Zerg_Extractor, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor)) && gas_starved) &&
        Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.  Note that gas_workers count is off. Sometimes units are in the gas geyser.

    buildings_started += Expo(drone, buildings_started == 0 && (!army_starved || e_inv.stock_total_< friendly_inventory.stock_total_ ) && (expansion_meaningful || larva_starved || econ_starved), inventory);
    buildings_started += Check_N_Build(UnitTypes::Zerg_Hatchery, drone, friendly_inventory, buildings_started == 0 && larva_starved && inv.min_workers_ + inv.gas_workers_ > inv.hatches_ * 5 ); // only macrohatch if you are short on larvae and can afford to spend.

    //Tech Buildings
    buildings_started += Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, friendly_inventory, !econ_starved && buildings_started == 0 &&
        Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) == 0);

        // > 1 base.
        buildings_started += Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, friendly_inventory, upgrade_bool && buildings_started == 0 && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0 &&
            Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) == 0 &&
            inv.hatches_ > 1);

        buildings_started += Check_N_Build(UnitTypes::Zerg_Spire, drone, friendly_inventory, upgrade_bool && buildings_started == 0 && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Spire, inv) == 0 &&
            Count_Units(UnitTypes::Zerg_Lair, inv) > 0 &&
            inv.hatches_ > 1);

        buildings_started += Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, friendly_inventory, upgrade_bool && buildings_started == 0 &&
            Count_Units(UnitTypes::Zerg_Evolution_Chamber, inv) == 0 &&
            Count_Units(UnitTypes::Zerg_Lair, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0 &&
            Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0 &&
            inv.hatches_ > 1);
        // >2 bases
        buildings_started += Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, friendly_inventory, upgrade_bool && buildings_started == 0 &&
            Count_Units(UnitTypes::Zerg_Evolution_Chamber, inv) == 1 &&
            Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Upgrade, Broodwar->self()->getUnits()) == 1 &&
            Count_Units(UnitTypes::Zerg_Lair, inv) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) > 0 &&
            Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Morph, Broodwar->self()->getUnits()) == 0 && //costly, slow.
            Count_Units(UnitTypes::Zerg_Spawning_Pool, inv) > 0 &&
            inv.hatches_ > 2);

        // >3 bases
        buildings_started += Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, friendly_inventory, upgrade_bool && buildings_started == 0 &&
            Count_Units(UnitTypes::Zerg_Queens_Nest, inv) == 0 &&
            Count_Units(UnitTypes::Zerg_Lair, inv) > 0 &&
            Count_Units(UnitTypes::Zerg_Spire, inv) > 0 &&
            inv.hatches_ > 3); // no less than 3 bases for hive please. // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

        buildings_started += Check_N_Build(UnitTypes::Zerg_Ultralisk_Cavern, drone, friendly_inventory, upgrade_bool && buildings_started == 0 && one_tech_per_base &&
            Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) == 0 &&
            Count_Units(UnitTypes::Zerg_Hive, inv) >= 0 &&
            inv.hatches_ > 3);

    return buildings_started > 0;
};

TilePosition MeatAIModule::getBuildablePosition(TilePosition target_pos, UnitType build_type, int tile_grid_size ) {

    TilePosition canidate_return_position = TilePosition (0,0);
    int widest_dim_in_minitiles = 0.25 * max(build_type.height(), build_type.width()) + 8;
    for (int x = -tile_grid_size; x <= tile_grid_size; ++x) {
        for (int y = -tile_grid_size; y <= tile_grid_size; ++y) {
            int centralize_x = target_pos.x + x;
            int centralize_y = target_pos.y + y;
            if (!(x == 0 && y == 0) &&
                centralize_x < Broodwar->mapWidth() &&
                centralize_y < Broodwar->mapHeight() &&
                centralize_x > 0 &&
                centralize_y > 0 &&
                Broodwar->canBuildHere(TilePosition(centralize_x, centralize_y), build_type) &&
                inventory.map_veins_[WalkPosition(TilePosition(centralize_x, centralize_y)).x][WalkPosition(TilePosition(centralize_x, centralize_y)).y] > widest_dim_in_minitiles // don't wall off please. Wide berth around blue veins.
            ) {
                canidate_return_position = TilePosition(centralize_x, centralize_y);
                break;
            }
        }
    }

    return canidate_return_position;
}

//bool operator==( const Build_Order_Object &lhs, const Build_Order_Object & rhs )
//{
//    bool research = lhs.getResearch().getID() != TechTypes::None.getID();
//    bool unit = lhs.getUnit() != UnitTypes::None;
//    bool upgrade = lhs.getUpgrade() != UpgradeTypes::None;
//
//}
//
//bool operator!=( const Build_Order_Object &lhs, const Build_Order_Object & rhs )
//{
//    return !operator==( lhs.getResearch(), rhs.getResearch() );
//}
//
//bool operator==( const Building_Gene &lhs, const Building_Gene &rhs ) {
//    return lhs.building_gene_ == rhs.building_gene_;
//};
//bool operator!=( const Building_Gene &lhs, const Building_Gene &rhs ) {
//    return !operator==( lhs.building_gene_ , rhs.building_gene_ );
//};

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
    if (ut.isBuilding()) {
        last_build_order = ut;
        Broodwar->sendText("Building a %s", last_build_order.c_str());
    }
}

//void setBuilding_Destroyed( UnitType ut ) {
//    --building_gene_[find( building_gene_.begin(), building_gene_.end(), u->getBuildType() )];
//}

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

bool Building_Gene::checkEmptyBuildOrder() {
    return building_gene_.empty();
}

//bool Building_Gene::checkExistsInBuild( UnitType unit ) {
//    return find( building_gene_.begin(), building_gene_.end(), Build_Order_Object( unit ) )!= building_gene_.end() ;
//}

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
    Build_Order_Object speed = Build_Order_Object(UpgradeTypes::Metabolic_Boost);
    Build_Order_Object ling = Build_Order_Object(UnitTypes::Zerg_Zergling);
    Build_Order_Object creep = Build_Order_Object(UnitTypes::Zerg_Creep_Colony);
    Build_Order_Object sunken = Build_Order_Object(UnitTypes::Zerg_Sunken_Colony);
    Build_Order_Object lair = Build_Order_Object(UnitTypes::Zerg_Lair);
    Build_Order_Object spire = Build_Order_Object(UnitTypes::Zerg_Spire);
    Build_Order_Object muta = Build_Order_Object(UnitTypes::Zerg_Mutalisk);
    Build_Order_Object hydra = Build_Order_Object(UnitTypes::Zerg_Hydralisk);
    Build_Order_Object lurker = Build_Order_Object(UnitTypes::Zerg_Lurker);
    Build_Order_Object hydra_den = Build_Order_Object(UnitTypes::Zerg_Hydralisk_Den);
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
        else if (build == "lair") {
            building_gene_.push_back(lair);
        }
        else if (build == "spire") {
            building_gene_.push_back(spire);
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
        else if (build == "grooved_spines") {
            building_gene_.push_back(grooved_spines);
        }
        else if (build == "muscular_augments") {
            building_gene_.push_back(muscular_augments);
        }
    }
}

void Building_Gene::clearRemainingBuildOrder() {
    building_gene_.clear();
};

Building_Gene::Building_Gene() {};

Building_Gene::Building_Gene(string s) { // unspecified items are unrestricted.

    getInitialBuildOrder(s);


} // for zerg 9 pool speed into 3 hatch muta, T: 3 hatch muta, P: 10 hatch into 3 hatch hydra bust.

