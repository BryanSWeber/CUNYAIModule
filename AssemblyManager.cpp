#pragma once
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\AssemblyManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Checks if a building can be built, and passes additional boolean criteria.  If all critera are passed, then it builds the building and announces this to the building gene manager. It may now allow morphing, eg, lair, hive and lurkers, but this has not yet been tested.  It now has an extensive creep colony script that prefers centralized locations.
bool MeatAIModule::Check_N_Build( const UnitType &building, const Unit &unit, const Unit_Inventory &ui, const bool &extra_critera )
{
    if ( Broodwar->self()->minerals() >= building.mineralPrice() &&
        Broodwar->self()->gas() >= building.gasPrice() &&
        (buildorder.checkBuilding_Desired( building ) || (extra_critera && buildorder.checkEmptyBuildOrder() && !buildorder.active_builders_) ) ) {
        if ( unit->canBuild( building ) &&
            building != UnitTypes::Zerg_Creep_Colony && building != UnitTypes::Zerg_Extractor )
        {
            TilePosition buildPosition = Broodwar->getBuildLocation( building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony );
            if ( unit->build( building, buildPosition ) ) {
                buildorder.setBuilding_Complete( building );
            }
        }
        else if ( unit->canBuild( building ) &&
            building == UnitTypes::Zerg_Creep_Colony ) { // creep colony loop specifically.
            
            Unitset base_core = unit->getUnitsInRadius( 1, IsBuilding && IsResourceDepot && IsCompleted ); // don't want undefined crash online 44.

            for ( const auto &u : ui.unit_inventory_ ) {
                if ( u.second.type_ == UnitTypes::Zerg_Hatchery ) {
                    base_core.insert( u.second.bwapi_unit_ );
                }
            }

			TilePosition middle = TilePosition( Broodwar->mapWidth() / 2, Broodwar->mapHeight() / 2 );
			TilePosition central_base = TilePosition(0, 0);

            int furth_x_dist = 0;
            int furth_y_dist = 0;

			if (Count_Units(UnitTypes::Zerg_Evolution_Chamber, friendly_inventory) > 0 && enemy_inventory.stock_fliers_ > friendly_inventory.stock_shoots_up_){
				Unit_Inventory hacheries = getUnitInventoryInRadius(ui, UnitTypes::Zerg_Hatchery, unit->getPosition(), 500);
				Stored_Unit *close_hatch = getClosestStored(hacheries, unit->getPosition(), 500);
				if ( close_hatch ){
					central_base = TilePosition(close_hatch->pos_);
				}
			} else if (!base_core.empty()) {

				for (auto base = base_core.begin(); base != base_core.end(); ++base) {

					TilePosition central_base_new = TilePosition((*base)->getPosition());

					int x_dist = (int)pow(middle.x - central_base_new.x, 2);
					int y_dist = (int)pow(middle.y - central_base_new.y, 2);

					int new_dist = (int)sqrt((double)x_dist + (double)y_dist);

					int furth_x_dist = (int)pow(middle.x - central_base.x, 2);
					int furth_y_dist = (int)pow(middle.y - central_base.y, 2);

					int old_dist = (int)sqrt((double)furth_x_dist + (double)furth_y_dist);

					if (new_dist <= old_dist) {
						central_base = central_base_new;
					}
				}
			}
			double theta = atan2(middle.y - central_base.y, middle.x - central_base.x);
            int adj_dx = (int)(cos( theta ) * 4); // move n tiles closer to the center of the map.
            int adj_dy = (int)(sin( theta ) * 4);

			TilePosition buildPosition = Broodwar->getBuildLocation(building, { central_base.x + adj_dx, central_base.y + adj_dy }, 5);
            if ( unit->build( building, buildPosition ) ) {
                buildorder.setBuilding_Complete( building );
            }
        }
		else if (unit->canBuild(building) &&
			building == UnitTypes::Zerg_Extractor ){
			TilePosition buildPosition = Broodwar->getBuildLocation(building, unit->getTilePosition(), 64, building == UnitTypes::Zerg_Creep_Colony);
			if ( getUnitInventoryInRadius(friendly_inventory, Position(buildPosition), 256).getMeanBuildingLocation() != Position(0, 0) && unit->build(building, buildPosition) ) {
				buildorder.setBuilding_Complete(building);
			} //extractors must have buildings nearby or we shouldn't build them.
		}
        if ( unit->canMorph( building ) )
        {
            if ( unit->morph( building ) ) {
                buildorder.setBuilding_Complete( building );
				return true;
			}
		}
    }
	return false;
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Upgrade( const UpgradeType &ups, const Unit &unit, const bool &extra_critera )
{
    if ( unit->canUpgrade( ups ) &&
        Broodwar->self()->minerals() >= ups.mineralPrice() &&
        Broodwar->self()->gas() >= ups.gasPrice() &&
        (buildorder.checkUpgrade_Desired( ups ) || ( extra_critera && buildorder.checkEmptyBuildOrder() ) ) ) {
        unit->upgrade( ups );
        buildorder.updateRemainingBuildOrder( ups );
        Broodwar->sendText( "Upgrading %s. Let's hope it finishes!", ups.c_str() );
    }
}

//Checks if a unit can be built from a larva, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera.
void MeatAIModule::Check_N_Grow( const UnitType &unittype, const Unit &larva, const bool &extra_critera )
{
    if ( larva->canMorph( unittype ) &&
        Broodwar->self()->minerals() >= unittype.mineralPrice() &&
        Broodwar->self()->gas() >= unittype.gasPrice() &&
        ( buildorder.checkBuilding_Desired( unittype ) || (extra_critera && (buildorder.checkEmptyBuildOrder() || unittype == UnitTypes::Zerg_Drone || unittype == UnitTypes::Zerg_Overlord) ) ) )
    {
        larva->morph( unittype );

        buildorder.setBuilding_Complete( unittype ); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.

        if ( unittype.isTwoUnitsInOneEgg() ) {
            buildorder.setBuilding_Complete( unittype ); // Shouldn't be a problem if unit isn't in buildorder. Makes it negative, build order preference checks for >0.
        } 
    }
}

//Creates a new unit. Reflects (poorly) upon enemy units in enemy_set. Incomplete.
void MeatAIModule::Reactive_Build( const Unit &larva, const Inventory &inv, const Unit_Inventory &ui, const Unit_Inventory &ei )
{

    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?

    //Supply blocked protection 
    Check_N_Grow( UnitTypes::Zerg_Overlord, larva, supply_starved );

    //Army build/replenish.  Cycle through military units available.
    if ( army_starved ) {
        if ( ei.stock_fliers_ > ui.stock_shoots_up_ ) { // Mutas generally sucks against air unless properly massed and manuvered (which mine are not)
            Check_N_Grow( UnitTypes::Zerg_Scourge, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, ui ) > 0 && Count_Units( UnitTypes::Zerg_Scourge, ui ) < 5 ); // hard cap on scourges, they build 2 at a time. 
            //Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, ui ) > 0 );
            Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, ui ) > 0 );

            if ( Count_Units( UnitTypes::Zerg_Hydralisk_Den, ui ) == 0 && buildorder.checkEmptyBuildOrder() ) {
                buildorder.building_gene_.push_back( Build_Order_Object( UnitTypes::Zerg_Hydralisk_Den ) ); // force in a hydralisk den if they have Air.
            }

        }
        else if ( ei.stock_high_ground_ > ui.stock_fliers_ ) { // if we have to go through a choke, this is all we want. Save for them.
            Check_N_Grow( UnitTypes::Zerg_Ultralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, ui ) > 0 );
            Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, ui ) > 0 );
        }
		else if (ei.stock_total_ - ei.stock_shoots_up_ > 0.25 * ei.stock_total_) {
            Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, ui ) > 0 );
        }
		else if (ei.stock_total_ - ei.stock_shoots_down_ > 0.75 * ei.stock_total_) {
            Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, ui ) > 0 );
		} 

            Check_N_Grow( UnitTypes::Zerg_Ultralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, ui ) > 0 ); // catchall ground units.
            Check_N_Grow( UnitTypes::Zerg_Mutalisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Spire, ui ) > 0 );
            Check_N_Grow( UnitTypes::Zerg_Hydralisk, larva, army_starved && Count_Units( UnitTypes::Zerg_Hydralisk_Den, ui ) > 0 );
			Check_N_Grow(UnitTypes::Zerg_Zergling, larva, army_starved && Count_Units(UnitTypes::Zerg_Spawning_Pool, ui) > 0);

    }


    //Econ Build/replenish loop. Will build workers if I have no spawning pool, or if there is a worker shortage.
    bool early_game = Count_Units( UnitTypes::Zerg_Spawning_Pool, ui ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) == 0 && inv.min_workers_ + inv.gas_workers_ <= 9;
    bool drone_conditional = (econ_starved || early_game); // or it is early game and you have nothing to build. // if you're eco starved

    Check_N_Grow( larva->getType().getRace().getWorker(), larva, drone_conditional ); 
}

//Creates a new building with DRONE. Incomplete.
bool MeatAIModule::Building_Begin( const Unit &drone, const Inventory &inv, const Unit_Inventory &e_inv ) {
    // will send it to do the LAST thing on this list that it can build.
	int buildings_started = 0;
    //Gas Buildings
	buildings_started += Check_N_Build(UnitTypes::Zerg_Extractor, drone, friendly_inventory, (inv.gas_workers_ > 3 * Count_Units(UnitTypes::Zerg_Extractor, friendly_inventory) || gas_starved) && 
		Count_Units_Doing(UnitTypes::Zerg_Extractor, UnitCommandTypes::Build, Broodwar->self()->getUnits()) == 0);  // wait till you have a spawning pool to start gathering gas. If your gas is full (or nearly full) get another extractor.

    //Expo loop, whenever not army starved. 
	buildings_started += Expo(drone, !army_starved , inv);
	buildings_started += Check_N_Build(UnitTypes::Zerg_Hatchery, drone, friendly_inventory, Count_Units(UnitTypes::Zerg_Larva, friendly_inventory) <= Count_Units(UnitTypes::Zerg_Hatchery, friendly_inventory) && Broodwar->self()->minerals() > 300); // only macrohatch if you are short on larvae and being a moron.

    //Basic Buildings
	buildings_started += Check_N_Build(UnitTypes::Zerg_Spawning_Pool, drone, friendly_inventory, !econ_starved &&
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) == 0 );

    //Tech Buildings
	buildings_started += Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) == 0 && 
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 );

	buildings_started += Check_N_Build(UnitTypes::Zerg_Evolution_Chamber, drone, friendly_inventory, tech_starved &&
		Count_Units(UnitTypes::Zerg_Evolution_Chamber, friendly_inventory) == 1 && 
		Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Upgrade, Broodwar->self()->getUnits() ) == 0 && 
		Count_Units_Doing(UnitTypes::Zerg_Evolution_Chamber, UnitCommandTypes::Build, Broodwar->self()->getUnits()) == 0 && //costly, slow.
		Count_Units(UnitTypes::Zerg_Spawning_Pool, friendly_inventory) > 0);

	buildings_started += Check_N_Build(UnitTypes::Zerg_Hydralisk_Den, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 &&
        Count_Units( UnitTypes::Zerg_Hydralisk_Den, friendly_inventory ) == 0 );

	buildings_started += Check_N_Build(UnitTypes::Zerg_Spire, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) >= 0 );

	buildings_started += Check_N_Build(UnitTypes::Zerg_Queens_Nest, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Queens_Nest, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) > 0 &&
        Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) > 0 );  // Spires are expensive and it will probably skip them unless it is floating a lot of gas.

	buildings_started += Check_N_Build(UnitTypes::Zerg_Ultralisk_Cavern, drone, friendly_inventory, tech_starved &&
        Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) == 0 &&
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) >= 0 );

	bool upgradable_creep_colonies = (Count_Units(UnitTypes::Zerg_Spawning_Pool, friendly_inventory) > 0 && Count_Units(UnitTypes::Zerg_Spawning_Pool, friendly_inventory) > Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool)) || // you can upgrade them SOMEHOW.
		(Count_Units(UnitTypes::Zerg_Evolution_Chamber, friendly_inventory) > 0 && Count_Units(UnitTypes::Zerg_Evolution_Chamber, friendly_inventory) > Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Evolution_Chamber)); // And there is a building complete that will allow either creep colony upgrade.

    //Combat Buildings
	buildings_started += Check_N_Build(UnitTypes::Zerg_Creep_Colony, drone, friendly_inventory, army_starved &&  // army starved.
        Count_Units( UnitTypes::Zerg_Creep_Colony, friendly_inventory ) == 0 && // no creep colonies waiting to upgrade
		upgradable_creep_colonies &&
        (inv.hatches_ * (inv.hatches_ + 1 )) / 2 > Count_Units( UnitTypes::Zerg_Sunken_Colony, friendly_inventory ) ); // and you're not flooded with sunkens. Spores could be ok if you need AA.  as long as you have sum(hatches+hatches-1+hatches-2...)>sunkens.
        //hatches >= 2 ); // and don't build them if you're on one base.

	return buildings_started > 0;

};

void Building_Gene::updateBuildingTimer( const Unit_Inventory &ui ) {
    int longest_project = 0;

    active_builders_ = false;

    for ( auto & u : ui.unit_inventory_ ) {

        if ( u.second.bwapi_unit_->getRemainingBuildTime() > longest_project && u.second.bwapi_unit_->getType().isBuilding() ) {
            longest_project = u.second.bwapi_unit_->getRemainingBuildTime();
        }

        if ( (u.second.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Build || u.second.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Move) && u.second.bwapi_unit_->getType() == UnitTypes::Zerg_Drone ) {  // drones morph into buildings with the BUILD command not the MORPH command.  No drones ever get the MOVE command unless deliberately scouting, they are otherwise only sent the gather command.
            active_builders_ = true;
        }

    }

    building_timer_ = longest_project;
}

void Building_Gene::updateRemainingBuildOrder( const Unit &u ) {
    if ( !building_gene_.empty() ) {
        if ( building_gene_.front().getUnit() == u->getType() ) {
            building_gene_.erase( building_gene_.begin() );
        }
    }
}

void Building_Gene::updateRemainingBuildOrder(const UnitType &ut) {
	if (!building_gene_.empty()) {
		if (building_gene_.front().getUnit() == ut ) {
			building_gene_.erase(building_gene_.begin());
		}
	}
}

void Building_Gene::updateRemainingBuildOrder( const UpgradeType &ups ) {
    if ( !building_gene_.empty() ) {
        if ( building_gene_.front().getUpgrade() == ups ) {
            building_gene_.erase( building_gene_.begin() );
        }
    }
}

void Building_Gene::setBuilding_Complete( UnitType ut ) {
    if ( ut.isBuilding() && ut.whatBuilds().first == UnitTypes::Zerg_Drone ) {
        last_build_order = ut;
        Broodwar->sendText( "Building a %s", last_build_order.c_str() );
        active_builders_ = true;
    }
}

//void setBuilding_Destroyed( UnitType ut ) {
//    --building_gene_[find( building_gene_.begin(), building_gene_.end(), u->getBuildType() )];
//}

bool Building_Gene::checkBuilding_Desired( UnitType ut ) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    if ( building_gene_.empty() ) {
        return false;
    }
    else {
        return building_gene_.front().getUnit() == ut && !active_builders_;
    }
}

bool Building_Gene::checkUpgrade_Desired( UpgradeType upgrade ) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return !building_gene_.empty() && building_gene_.front().getUpgrade() == upgrade && !active_builders_;
}

bool Building_Gene::checkEmptyBuildOrder() {
    return building_gene_.empty();
}
