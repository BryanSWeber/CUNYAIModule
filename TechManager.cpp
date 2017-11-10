#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\MeatAIModule.h"
# include "Source\InventoryManager.h"

using namespace BWAPI;


// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool MeatAIModule::Tech_Avail() {

    for ( auto & u : BWAPI::Broodwar->self()->getUnits() ) {

        if ( u->getType() == BWAPI::UnitTypes::Zerg_Drone ) {
            bool long_condition = (u->canBuild( BWAPI::UnitTypes::Zerg_Spawning_Pool ) && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Evolution_Chamber ) && Count_Units( BWAPI::UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Hydralisk_Den ) && Count_Units( BWAPI::UnitTypes::Zerg_Hydralisk_Den, friendly_inventory ) == 0)||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Spire ) && Count_Units( BWAPI::UnitTypes::Zerg_Spire, friendly_inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Queens_Nest ) && Count_Units( BWAPI::UnitTypes::Zerg_Queens_Nest, friendly_inventory ) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Spire, friendly_inventory ) > 0 ) || // I have hardcoded spire before queens nest.
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern ) && Count_Units( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) == 0);
            if ( long_condition ) {
                return true;
            }
        }
        else if ( (u->getType() == BWAPI::UnitTypes::Zerg_Hatchery || u->getType() == BWAPI::UnitTypes::Zerg_Lair || u->getType() == BWAPI::UnitTypes::Zerg_Hive) && !u->isUpgrading() && !u->isMorphing() ) {
            bool long_condition = (u->canMorph( BWAPI::UnitTypes::Zerg_Lair ) && Count_Units( BWAPI::UnitTypes::Zerg_Lair, friendly_inventory ) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Hive, friendly_inventory ) == 0) ||
                    (u->canMorph( BWAPI::UnitTypes::Zerg_Hive ) && Count_Units( BWAPI::UnitTypes::Zerg_Hive, friendly_inventory ) == 0);
            if ( long_condition ) {
                return true;
            }
        }
        else if ( u->getType().isBuilding() && !u->isUpgrading() && !u->isMorphing() ){ // check idle buildings for potential upgrades.
            for ( int i = 0; i != 13; i++ )
            { // iterating through the main upgrades we have available and MeatAI "knows" about. 
                int known_ups[13] = { 3, 4, 10, 11, 12, 25, 26, 27, 28, 29, 30, 52, 53 }; // Identifies zerg upgrades of that we have initialized at this time. See UpgradeType definition for references, listed below for conveinence.
                UpgradeType up_current = (UpgradeType) known_ups[i];
                UpgradeType::set building_up_set = u->getType().upgradesWhat(); // does this idle building make that upgrade?
                if ( building_up_set.find( up_current ) != building_up_set.end() ) {

					bool upgrade_incomplete = BWAPI::Broodwar->self()->getUpgradeLevel(up_current) < up_current.maxRepeats() && !BWAPI::Broodwar->self()->isUpgrading(up_current);

					bool hydra_upgrade = UpgradeTypes::Zerg_Missile_Attacks || UpgradeTypes::Grooved_Spines || UpgradeTypes::Muscular_Augments;
					bool ling_upgrade = UpgradeTypes::Zerg_Melee_Attacks || UpgradeTypes::Metabolic_Boost;

					bool upgrade_conditionals = ( hydra_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_inventory) > Stock_Units(UnitTypes::Zerg_Zergling, friendly_inventory)) ||
						(ling_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_inventory) < Stock_Units(UnitTypes::Zerg_Zergling, friendly_inventory));

					if (upgrade_incomplete && upgrade_conditionals) { // if it is not maxed, and nothing is upgrading it, then there must be some tech work we could do.
                        return true;
                    }
                }
            }
        } // if condition
    }// for every unit

    return false;
}
// Tells a building to begin the next tech on our list.
bool MeatAIModule::Tech_Begin(Unit building, const Unit_Inventory &ui) {
    int busy = 0;

    busy += Check_N_Upgrade( UpgradeTypes::Zerg_Carapace, building, tech_starved );

    busy += Check_N_Upgrade( UpgradeTypes::Metabolic_Boost, building, tech_starved && ( Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3 ) );
    busy += Check_N_Upgrade( UpgradeTypes::Zerg_Melee_Attacks, building, tech_starved && ( Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Missile_Attacks ) == 3 ) );

    busy += Check_N_Upgrade( UpgradeTypes::Zerg_Missile_Attacks, building, tech_starved && (Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3) );
    busy += Check_N_Upgrade( UpgradeTypes::Grooved_Spines, building, tech_starved && (Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3) );
    busy += Check_N_Upgrade( UpgradeTypes::Muscular_Augments, building, tech_starved && (Stock_Units( UnitTypes::Zerg_Hydralisk, friendly_inventory ) > Stock_Units( UnitTypes::Zerg_Zergling, friendly_inventory ) || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Melee_Attacks ) == 3 ) );

    busy += Check_N_Upgrade( UpgradeTypes::Pneumatized_Carapace, building, tech_starved && (Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) > 0 || Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0) );

    busy += Check_N_Upgrade( UpgradeTypes::Zerg_Flyer_Attacks, building, tech_starved && Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) > 0 && (ui.stock_fliers_ > ui.stock_ground_units_ || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Carapace ) == 3 ));
    busy += Check_N_Upgrade( UpgradeTypes::Zerg_Flyer_Carapace, building, tech_starved && Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) > 0 && (ui.stock_fliers_ > ui.stock_ground_units_ || BWAPI::Broodwar->self()->getUpgradeLevel( UpgradeTypes::Zerg_Carapace ) == 3 ));

    busy += Check_N_Upgrade( UpgradeTypes::Adrenal_Glands, building, tech_starved && Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0 );
    busy += Check_N_Upgrade( UpgradeTypes::Anabolic_Synthesis, building, tech_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0 );
    busy += Check_N_Upgrade( UpgradeTypes::Chitinous_Plating, building, tech_starved && Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0 );

    busy += Check_N_Build( UnitTypes::Zerg_Lair, building, ui, tech_starved &&
        Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Lair ) <= 0 && //don't need lair if we have a lair
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Hive ) <= 0 && //don't need lair if we have a hive.
        building->getType() == UnitTypes::Zerg_Hatchery );

    busy += Check_N_Build( UnitTypes::Zerg_Hive, building, ui, tech_starved &&
        Count_Units( UnitTypes::Zerg_Queens_Nest, friendly_inventory ) >= 0 &&
        building->getType() == UnitTypes::Zerg_Lair &&
        Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) == 0 ); //If you're tech-starved at this point, don't make random hives.

    busy += Check_N_Upgrade( UpgradeTypes::Antennae, building, tech_starved && (Count_Units( UnitTypes::Zerg_Lair, friendly_inventory ) > 0 || Count_Units( UnitTypes::Zerg_Hive, friendly_inventory ) > 0) ); //don't need lair if we have a hive. This upgrade is terrible, thus last.
    
    return busy > 0;
}
