#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "Source\CUNYAIModule.h"
# include "Source\TechManager.h"
# include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.


using namespace BWAPI;

std::map<UpgradeType, int> TechManager::upgrade_cycle = {};

// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool TechManager::Tech_Avail() {

    for (auto tech : CUNYAIModule::friendly_player_model.tech_cartridge_) {
        if (CUNYAIModule::Count_Units(tech.first.requiredUnit()))  return true; // If we can make it and don't have it.
    }

    int best_sim_score = INT_MIN;
    UpgradeType up_type = UpgradeTypes::None;

    for (auto potential_up : CUNYAIModule::friendly_player_model.upgrade_cartridge_) {
        auto buildfap_copy = CUNYAIModule::buildfap;
        CUNYAIModule::friendly_player_model.units_.addToBuildFAP(buildfap_copy, true, CUNYAIModule::friendly_player_model.researches_, potential_up.first);
        buildfap_copy.simulate(24 * 20); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
        int score = CUNYAIModule::getFAPScore(buildfap_copy, true) - CUNYAIModule::getFAPScore(buildfap_copy, false);
        buildfap_copy.clear();
        upgrade_cycle.insert({ potential_up.first, score });
    }
    for (auto &potential_up : upgrade_cycle) {
        if (potential_up.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_up.second;
            up_type = potential_up.first;
            Broodwar->sendText("Found a Best_sim_score of %d, for %s", best_sim_score, up_type.c_str());
        }
    }
    if (up_type != UpgradeTypes::None && CUNYAIModule::Count_Units(up_type.whatsRequired()) && CUNYAIModule::friendly_player_model.researches_.upgrades_[up_type] < up_type.maxRepeats() ) return true; // If we can make it and don't have it.

    for (auto building : CUNYAIModule::CUNYAIModule::friendly_player_model.building_cartridge_) {
        bool pass_guard = true;
        bool own_successor = false;
        for (auto req : building.first.requiredUnits()) {
            if(CUNYAIModule:: Count_Units(req.first) == 0 || !pass_guard) pass_guard = false;
        }
        for (auto possessed_unit : CUNYAIModule::friendly_player_model.units_.unit_inventory_) {
            if (possessed_unit.second.type_.isSuccessorOf(building.first)) own_successor = true;
        }
        if( pass_guard && CUNYAIModule:: Count_Units(building.first) == 0 && !own_successor) return true; // If we can make it and don't have it.
    }

    return false;
}

// Tells a building to begin the next tech on our list. Now updates the unit if something has changed.
bool TechManager::Tech_BeginBuildFAP(Unit building, Unit_Inventory &ui, const Map_Inventory &inv) {
    bool busy = false;
    bool upgrade_bool = (CUNYAIModule::tech_starved || (CUNYAIModule::Count_Units(UnitTypes::Zerg_Larva) == 0 && !CUNYAIModule::army_starved));
    bool have_declared_lurkers = BWAPI::Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
    bool have_declared_mutas = CUNYAIModule::Count_Units(UnitTypes::Zerg_Spire) > 0;
    bool cannot_build_major_unit_type = !CUNYAIModule::checkInCartridge(UnitTypes::Zerg_Lurker) && !CUNYAIModule::checkInCartridge(UnitTypes::Zerg_Mutalisk);
    bool have_declared_a_major_unit_type = have_declared_lurkers || have_declared_mutas || cannot_build_major_unit_type;
    bool have_hive = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) > 0;
    bool maxed_melee = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Melee_Attacks) == 3;
    bool maxed_range = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Missile_Attacks) == 3;
    bool maxed_armor = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Carapace) == 3;

    // Researchs, not upgrades per se:
    if (!busy) busy = CUNYAIModule::Check_N_Research(TechTypes::Lurker_Aspect, building, upgrade_bool && (CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) > 0 || CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) > 0) && CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0);

    int best_sim_score = INT_MIN;
    UpgradeType up_type = UpgradeTypes::None;

    for (auto potential_up : CUNYAIModule::friendly_player_model.upgrade_cartridge_) {
        if (!busy) {
            if (CUNYAIModule::checkDesirable(building, potential_up.first, true)) {
                auto buildfap_copy = CUNYAIModule::buildfap;
                CUNYAIModule::friendly_player_model.units_.addToBuildFAP(buildfap_copy, true, CUNYAIModule::friendly_player_model.researches_, potential_up.first);
                buildfap_copy.simulate(24 * 20); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
                int score = CUNYAIModule::getFAPScore(buildfap_copy, true) - CUNYAIModule::getFAPScore(buildfap_copy, false);
                buildfap_copy.clear();
                upgrade_cycle.insert({ potential_up.first, score });
                //Broodwar->sendText("Considering a %s", potential_up.first.c_str());
            }
        }
    }

    for (auto &potential_up : upgrade_cycle) {
        if (potential_up.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_up.second;
            up_type = potential_up.first;
            //Broodwar->sendText("Found a Best_sim_score of %d, for %s", best_sim_score, up_type.c_str());
        }
    }

    if (building->upgrade(up_type)) busy = true;

    // Will probably not improve combat performance in FAP (will get units killed instead).
    if (!busy) busy = CUNYAIModule::Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, building, upgrade_bool && have_declared_a_major_unit_type && (CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) > 0 || CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) > 0));
    if (!busy) busy = CUNYAIModule::Check_N_Upgrade(UpgradeTypes::Antennae, building, CUNYAIModule::tech_starved && have_hive); //This upgrade is terrible, thus last.

    //should auto upgrade if there is a build order requirement for any of these three types.
    if (!busy) busy = CUNYAIModule::Check_N_Build(UnitTypes::Zerg_Lair, building, upgrade_bool &&
        CUNYAIModule::current_map_inventory.hatches_ > 1 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) == 0 && //don't need lair if we have a lair
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0 && //don't need lair if we have a hive.
        building->getType() == UnitTypes::Zerg_Hatchery);

    if (!busy) busy = CUNYAIModule::Check_N_Build(UnitTypes::Zerg_Hive, building, upgrade_bool &&
        CUNYAIModule::current_map_inventory.hatches_ > 2 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Queens_Nest) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Queens_Nest) > 0 &&
        building->getType() == UnitTypes::Zerg_Lair &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) == 0); //If you're tech-starved at this point, don't make random hives.

    if (!busy) busy = CUNYAIModule::Check_N_Build(UnitTypes::Zerg_Greater_Spire, building, upgrade_bool &&
        CUNYAIModule::current_map_inventory.hatches_ > 3 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hive) > 0 &&
        building->getType() == UnitTypes::Zerg_Spire &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) == 0); //If you're tech-starved at this point, don't make random hives.

    upgrade_cycle.clear(); // clear so it's fresh every time.

    return busy;
}