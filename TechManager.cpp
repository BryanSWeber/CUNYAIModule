#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include "Source\Map_Inventory.h"

using namespace BWAPI;


// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool CUNYAIModule::Tech_Avail() {

    for (auto tech : CUNYAIModule::friendly_player_model.tech_cartridge_) {
        if ( CUNYAIModule::Count_Units(tech.first.requiredUnit()) )  return true; // If we can make it and don't have it.
    }

    for (auto upgrade : CUNYAIModule::friendly_player_model.upgrade_cartridge_) {
        bool muta_upgrade = upgrade.first == UpgradeTypes::Zerg_Flyer_Attacks || upgrade.first == UpgradeTypes::Zerg_Flyer_Carapace;
        bool hydra_upgrade = upgrade.first == UpgradeTypes::Zerg_Missile_Attacks || upgrade.first == UpgradeTypes::Grooved_Spines || upgrade.first == UpgradeTypes::Muscular_Augments;
        bool ling_upgrade = upgrade.first == UpgradeTypes::Zerg_Melee_Attacks || upgrade.first == UpgradeTypes::Metabolic_Boost;

        if ((muta_upgrade || hydra_upgrade || ling_upgrade) && CUNYAIModule::Count_Units(upgrade.first.whatsRequired()) ) {

            bool upgrade_conditionals = (hydra_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) > Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_)) ||
                (ling_upgrade && Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) < Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_)) ||
                (muta_upgrade && friendly_player_model.units_.stock_fliers_ > Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_));

            if (upgrade_conditionals) { // if it is not maxed, and nothing is upgrading it, then there must be some tech work we could do. We do not require air upgrades at this time, but they could still plausibly occur.
                return true;
            }
        }
        else if (CUNYAIModule::Count_Units(upgrade.first.whatsRequired())) return true; // If we can make it and don't have it.
    }

    for (auto building : CUNYAIModule::friendly_player_model.building_cartridge_) {
        bool pass_guard = true;
        for (auto req : building.first.requiredUnits()) {
            if(Count_Units(req.first) == 0 || !pass_guard) pass_guard = false;
        }
        if( pass_guard && Count_Units(building.first) == 0) return true; // If we can make it and don't have it.
    }

    return false;
}

// Tells a building to begin the next tech on our list. Now updates the unit if something has changed.
bool CUNYAIModule::Tech_Begin(Unit building, Unit_Inventory &ui, const Map_Inventory &inv) {
    bool busy = false;
    bool upgrade_bool = (tech_starved || (Count_Units( UnitTypes::Zerg_Larva) == 0 && !army_starved));
    bool have_declared_lurkers = BWAPI::Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
    bool have_declared_mutas = Count_Units(UnitTypes::Zerg_Spire) > 0;
    bool have_declared_a_major_unit_type = have_declared_lurkers || have_declared_mutas;
    bool have_hive = Count_Units(UnitTypes::Zerg_Hive) > 0;
    bool maxed_melee = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Melee_Attacks) == 3;
    bool maxed_range = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Missile_Attacks) == 3;
    bool maxed_armor = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Carapace) == 3;

    bool more_hydras_than_lings = Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_) > Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_);
    bool more_flyers_than_hydras = ui.stock_fliers_ > Stock_Units(UnitTypes::Zerg_Hydralisk, friendly_player_model.units_);

    // Major Upgrades:
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Metabolic_Boost, building, upgrade_bool && Stock_Units(UnitTypes::Zerg_Zergling, friendly_player_model.units_) > 0);
    if (!busy) busy = Check_N_Research(TechTypes::Lurker_Aspect, building, upgrade_bool && (Count_Units(UnitTypes::Zerg_Lair) > 0 || Count_Units(UnitTypes::Zerg_Hive) > 0) && Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0);

    //Midgame/situational upgrades.
    if (!have_declared_a_major_unit_type || have_hive || buildorder.checkUpgrade_Desired(UpgradeTypes::Muscular_Augments) || buildorder.checkUpgrade_Desired(UpgradeTypes::Grooved_Spines)) {
        if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Muscular_Augments, building, upgrade_bool && more_hydras_than_lings || maxed_melee || maxed_range);
        if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Grooved_Spines, building, upgrade_bool && more_hydras_than_lings || maxed_melee || maxed_range);
    }

    //Super game upgrades.
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Adrenal_Glands, building, upgrade_bool);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Anabolic_Synthesis, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) > 0);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Chitinous_Plating, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Ultralisk_Cavern) > 0);

    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, building, upgrade_bool && have_declared_a_major_unit_type && (Count_Units(UnitTypes::Zerg_Lair) > 0 || Count_Units(UnitTypes::Zerg_Hive) > 0));
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Antennae, building, tech_starved && have_hive); //This upgrade is terrible, thus last.

    // Unit buffs
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Carapace, building, upgrade_bool);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Melee_Attacks, building, upgrade_bool && !more_hydras_than_lings  || maxed_range);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Missile_Attacks, building, upgrade_bool && more_hydras_than_lings || maxed_melee);
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Flyer_Attacks, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Spire) > 0 && (more_flyers_than_hydras || maxed_armor));
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Zerg_Flyer_Carapace, building, upgrade_bool && Count_Units(UnitTypes::Zerg_Spire) > 0 && (more_flyers_than_hydras || maxed_armor));

    //should auto upgrade if there is a build order requirement for any of these three types.
  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Lair, building, upgrade_bool &&
            current_map_inventory.hatches_ > 1 &&
            Count_Units(UnitTypes::Zerg_Lair) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) == 0 && //don't need lair if we have a lair
            Count_Units(UnitTypes::Zerg_Hive) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0 && //don't need lair if we have a hive.
            building->getType() == UnitTypes::Zerg_Hatchery);

  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Hive, building, upgrade_bool &&
            current_map_inventory.hatches_ > 2 &&
            Count_Units(UnitTypes::Zerg_Queens_Nest) - Count_Units_In_Progress(UnitTypes::Zerg_Queens_Nest) > 0 &&
            building->getType() == UnitTypes::Zerg_Lair &&
            Count_Units(UnitTypes::Zerg_Hive) == 0); //If you're tech-starved at this point, don't make random hives.

  if(!busy) busy = Check_N_Build(UnitTypes::Zerg_Greater_Spire, building, upgrade_bool &&
       current_map_inventory.hatches_ > 3 &&
       Count_Units(UnitTypes::Zerg_Hive) - Count_Units_In_Progress(UnitTypes::Zerg_Hive) > 0 &&
       building->getType() == UnitTypes::Zerg_Spire &&
       Count_Units(UnitTypes::Zerg_Greater_Spire) == 0); //If you're tech-starved at this point, don't make random hives.

    return busy;
}
