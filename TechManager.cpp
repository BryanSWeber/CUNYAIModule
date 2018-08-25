#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include "Source\InventoryManager.h"

using namespace BWAPI;


// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool CUNYAIModule::Tech_Avail() {

    for ( auto & u : BWAPI::Broodwar->self()->getUnits() ) {

        if ( u->getType() == BWAPI::UnitTypes::Zerg_Drone ) {
            bool long_condition = (u->canBuild( BWAPI::UnitTypes::Zerg_Spawning_Pool ) && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, inventory ) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Evolution_Chamber ) && Count_Units( BWAPI::UnitTypes::Zerg_Evolution_Chamber, inventory) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Hydralisk_Den ) && Count_Units( BWAPI::UnitTypes::Zerg_Hydralisk_Den, inventory) == 0)||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Spire ) && Count_Units( BWAPI::UnitTypes::Zerg_Spire, inventory) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Queens_Nest ) && Count_Units( BWAPI::UnitTypes::Zerg_Queens_Nest, inventory) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Spire, inventory) > 0 ) || // I have hardcoded spire before queens nest.
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern ) && Count_Units( BWAPI::UnitTypes::Zerg_Ultralisk_Cavern, inventory) == 0) ||
                    (u->canBuild( BWAPI::UnitTypes::Zerg_Greater_Spire) && Count_Units(BWAPI::UnitTypes::Zerg_Greater_Spire, inventory) == 0);
            if ( long_condition ) {
                return true;
            }
        }
        else if ( (u->getType() == BWAPI::UnitTypes::Zerg_Hatchery || u->getType() == BWAPI::UnitTypes::Zerg_Lair || u->getType() == BWAPI::UnitTypes::Zerg_Hive) && !u->isUpgrading() && !u->isMorphing() ) {
            bool long_condition = (u->canMorph( BWAPI::UnitTypes::Zerg_Lair ) && Count_Units( BWAPI::UnitTypes::Zerg_Lair, inventory) == 0 && Count_Units( BWAPI::UnitTypes::Zerg_Hive, inventory) == 0) ||
                    (u->canMorph( BWAPI::UnitTypes::Zerg_Hive ) && Count_Units( BWAPI::UnitTypes::Zerg_Hive, inventory) == 0);
            if ( long_condition ) {
                return true;
            }
        }
        else if ( u->getType().isBuilding() && !u->isUpgrading() && !u->isMorphing() ){ // check idle buildings for potential upgrades.
            for ( int i = 0; i != 13; i++ )
            { // iterating through the main upgrades we have available and CUNYAI "knows" about. 
                int known_ups[13] = { 3, 4, 10, 11, 12, 25, 26, 27, 28, 29, 30, 52, 53 }; // Identifies zerg upgrades of that we have initialized at this time. See UpgradeType definition for references, listed below for conveinence.
                UpgradeType up_current = (UpgradeType) known_ups[i];
                UpgradeType::set building_up_set = u->getType().upgradesWhat(); // does this idle building make that upgrade?
                if ( building_up_set.find( up_current ) != building_up_set.end() ) {

					bool upgrade_incomplete = BWAPI::Broodwar->self()->getUpgradeLevel(up_current) < up_current.maxRepeats() && !BWAPI::Broodwar->self()->isUpgrading(up_current);

					bool hydra_upgrade = up_current == UpgradeTypes::Zerg_Missile_Attacks || up_current == UpgradeTypes::Grooved_Spines || up_current == UpgradeTypes::Muscular_Augments;
					bool ling_upgrade = up_current == UpgradeTypes::Zerg_Melee_Attacks || up_current == UpgradeTypes::Metabolic_Boost;

					bool upgrade_conditionals = ( hydra_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) > Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_)) ||
						(ling_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) < Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_));

					if (upgrade_incomplete && upgrade_conditionals) { // if it is not maxed, and nothing is upgrading it, then there must be some tech work we could do. We do not require air upgrades at this time, but they could still plausibly occur.
                        return true;
                    }
                }
            }
        } // if condition
        else if ( u->getType().isBuilding() && !u->isUpgrading() && !u->isMorphing() ) { // check idle buildings for potential upgrades.
            for ( int i = 0; i != 1; i++ )
            { // iterating through the main researches we have available and CUNYAI "knows" about. 
                int known_techs[1] = { 32 }; // Identifies zerg upgrades of that we have initialized at this time. See UpgradeType definition for references, listed below for conveinence.
                TechType tech_current = (TechType)known_techs[i];
                TechType::set building_tech_set = u->getType().researchesWhat(); // does this idle building make that upgrade?
                if ( building_tech_set.find( tech_current ) != building_tech_set.end() ) {
                    bool tech_incomplete = Broodwar->self()->hasResearched( tech_current );
                    if ( tech_incomplete ) { // if it is not maxed, and nothing is upgrading it, then there must be some tech work we could do.
                        return true;
                    }
                }
            }
        } // if condition
    }// for every unit

    return false;
}
// Tells a building to begin the next tech on our list. Now updates the unit if something has changed.
bool CUNYAIModule::Tech_Begin(Unit building, Unit_Inventory &ui, const Inventory &inv) {
    bool busy = false;
    bool upgrade_bool = (tech_starved || (Count_Units( UnitTypes::Zerg_Larva, inv ) == 0 && !army_starved));
    bool have_declared_lurkers = BWAPI::Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
    bool have_declared_mutas = Count_Units(UnitTypes::Zerg_Spire, inv) > 0;
    bool have_declared_a_major_unit_type = have_declared_lurkers || have_declared_mutas;
    bool have_hive = Count_Units(UnitTypes::Zerg_Hive, inv) > 0;
    bool maxed_melee = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Melee_Attacks) == 3;
    bool maxed_range = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Missile_Attacks) == 3;
    bool maxed_armor = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Carapace) == 3;

    bool more_hydras_than_lings = Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) > Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_);
    bool more_flyers_than_ground = ui.stock_fliers_ > ui.stock_ground_units_;

    // Major Upgrades:
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Metabolic_Boost, building, upgrade_bool && Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_) > 0);
    if (!busy) busy = Check_N_Research(TechTypes::Lurker_Aspect, building, upgrade_bool && (Count_Units(UnitTypes::Zerg_Lair, inv) > 0 || Count_Units(UnitTypes::Zerg_Hive, inv) > 0) && Count_Units(UnitTypes::Zerg_Hydralisk_Den, inv) > 0);

    //Midgame/situational upgrades.
    if (!have_declared_a_major_unit_type || have_hive || buildorder.checkUpgrade_Desired(UpgradeTypes::Muscular_Augments) || buildorder.checkUpgrade_Desired(UpgradeTypes::Grooved_Spines)) {
        if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Muscular_Augments, building, upgrade_bool && more_hydras_than_lings || maxed_melee || maxed_range);
        if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Grooved_Spines, building, upgrade_bool && more_hydras_than_lings || maxed_melee || maxed_range);
    }

    //Super game upgrades.
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Adrenal_Glands, building, upgrade_bool);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Anabolic_Synthesis, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) > 0);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Chitinous_Plating, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern, inv) > 0);

    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, building, upgrade_bool && have_declared_a_major_unit_type && (Count_Units(UnitTypes::Zerg_Lair, inv) > 0 || Count_Units(UnitTypes::Zerg_Hive, inv) > 0));
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Antennae, building, tech_starved && have_hive); //This upgrade is terrible, thus last.

    // Unit buffs
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Carapace, building, upgrade_bool);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Melee_Attacks, building, upgrade_bool && !more_hydras_than_lings  || maxed_range);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Missile_Attacks, building, upgrade_bool && more_hydras_than_lings || maxed_melee);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Flyer_Attacks, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Spire, inv) > 0 && (more_flyers_than_ground || maxed_armor));
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Flyer_Carapace, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Spire, inv) > 0 && (more_flyers_than_ground || maxed_armor));

  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Lair, building, ui, upgrade_bool &&
            inventory.hatches_ > 1 &&
            Count_Units(UnitTypes::Zerg_Lair, inv) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) == 0 && //don't need lair if we have a lair
            Count_Units(UnitTypes::Zerg_Hive, inv) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0 && //don't need lair if we have a hive.
            building->getType() == UnitTypes::Zerg_Hatchery);

  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Hive, building, ui, upgrade_bool &&
            inventory.hatches_ > 2 &&
            Count_Units(UnitTypes::Zerg_Queens_Nest, inv) - Count_Units_In_Progress(UnitTypes::Zerg_Queens_Nest, inv) > 0 &&
            building->getType() == UnitTypes::Zerg_Lair &&
            Count_Units(UnitTypes::Zerg_Hive, inv) == 0); //If you're tech-starved at this point, don't make random hives.

  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Greater_Spire, building, ui, upgrade_bool &&
       inventory.hatches_ > 3 &&
       Count_Units(UnitTypes::Zerg_Hive, inv) - Count_Units_In_Progress(UnitTypes::Zerg_Hive, inv) > 0 &&
       building->getType() == UnitTypes::Zerg_Spire &&
       Count_Units(UnitTypes::Zerg_Greater_Spire, inv) == 0); //If you're tech-starved at this point, don't make random hives.

    return busy;
}
