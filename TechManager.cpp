#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "Source\CUNYAIModule.h"
# include "Source\TechManager.h"
#include "Source\PlayerModelManager.h" // needed for cartidges.
# include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.


using namespace BWAPI;

std::map<UpgradeType, int> TechManager::upgrade_cycle = CUNYAIModule::friendly_player_model.upgrade_cartridge_; //persistent valuation of upgrades. Start with upgrade cartdrige.
bool TechManager::tech_avail_ = true;

// updates the upgrade cycle.
void TechManager::updateOptimalTech() {


    for (auto potential_up : upgrade_cycle) {
        // should only upgrade if units for that upgrade exist on the field for me. Or reset every time a new upgrade is found. Need a baseline null upgrade- Otherwise we'll upgrade things like range damage with only lings, when we should be saving for carapace.
        //if (potential_up.first == UpgradeTypes::None || (CUNYAIModule::friendly_player_model.researches_.upgrades_[potential_up.first] < potential_up.first.maxRepeats() && CUNYAIModule::checkDesirable(potential_up.first, true))){
            FAP::FastAPproximation<Stored_Unit*> upgradeFAP; // attempting to integrate FAP into building decisions.
            CUNYAIModule::friendly_player_model.units_.addToBuildFAP(upgradeFAP, true, CUNYAIModule::friendly_player_model.researches_, potential_up.first);
            CUNYAIModule::enemy_player_model.units_.addToBuildFAP(upgradeFAP, false, CUNYAIModule::enemy_player_model.researches_);
            upgradeFAP.simulate(24 * 3); // a complete simulation cannot be ran... medics & firebats vs air causes a lockup.
            int score = CUNYAIModule::getFAPScore(upgradeFAP, true) - CUNYAIModule::getFAPScore(upgradeFAP, false);
            upgradeFAP.clear();
            if (upgrade_cycle.find(potential_up.first) == upgrade_cycle.end()) upgrade_cycle[potential_up.first] = score;
            else upgrade_cycle[potential_up.first] = static_cast<int>(static_cast<double>(23.0 / 24.0) * upgrade_cycle[potential_up.first] + static_cast<double>(1.0 / 24.0) * score); //moving average over 24 simulations, 1 second.  Short because units lose types very often.
        //}
    }
}

bool TechManager::checkTechAvail()
{
    return tech_avail_;
}

// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
void TechManager::updateTech_Avail() {

    for (auto tech : CUNYAIModule::friendly_player_model.tech_cartridge_) {
        if (CUNYAIModule::Count_Units(tech.first.requiredUnit()))  tech_avail_ = true; // If we can make it and don't have it yet, we have tech we can make.
    }

    updateOptimalTech();

    UpgradeType up_type = UpgradeTypes::None;
    int best_sim_score = upgrade_cycle[up_type];// Baseline, an upgrade must be BETTER than null upgrade.  May cause freezing in tech choices. Restored to simple plausibility.

    for (auto &potential_up : upgrade_cycle) {
        if (CUNYAIModule::Count_Units(potential_up.first.whatsRequired()) - CUNYAIModule::Count_Units_In_Progress(potential_up.first.whatsRequired()) > 0) {
            if (potential_up.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
                best_sim_score = potential_up.second;
                up_type = potential_up.first;
                //Broodwar->sendText("Found a Best_sim_score of %d, for %s", best_sim_score, up_type.c_str());
            }
        }
    }
    if (up_type != UpgradeTypes::None && CUNYAIModule::Count_Units(up_type.whatsRequired()) && CUNYAIModule::friendly_player_model.researches_.upgrades_[up_type] < up_type.maxRepeats() ) tech_avail_ = true; // If we can make it and don't have it.

    for (auto building : CUNYAIModule::CUNYAIModule::friendly_player_model.building_cartridge_) {
        bool pass_guard = true;
        bool own_successor = false;
        for (auto req : building.first.requiredUnits()) {
            if(CUNYAIModule:: Count_Units(req.first) == 0 || !pass_guard) pass_guard = false;
        }
        for (auto possessed_unit : CUNYAIModule::friendly_player_model.units_.unit_inventory_) {
            if (possessed_unit.second.type_.isSuccessorOf(building.first)) own_successor = true;
        }
        if( pass_guard && CUNYAIModule:: Count_Units(building.first) == 0 && !own_successor) tech_avail_ = true; // If we can make it and don't have it.
    }

    if (tech_avail_) return;
    else tech_avail_ = false;
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
    if (!busy) busy = Check_N_Research(TechTypes::Lurker_Aspect, building, upgrade_bool && (CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) > 0 || CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) > 0) && CUNYAIModule::Count_Units(UnitTypes::Zerg_Hydralisk_Den) > 0);

    UpgradeType up_type = UpgradeTypes::None;
    std::map<UpgradeType, int> local_upgrade_cycle(upgrade_cycle);
    int best_sim_score = local_upgrade_cycle[up_type];// Baseline, an upgrade must be BETTER than null upgrade. But this requirement causes freezing. So until further notice, do the "best" upgrade.

    for (auto potential_up = local_upgrade_cycle.begin(); potential_up != local_upgrade_cycle.end(); potential_up++) {
        if (!busy && potential_up->first) {
            if ( !CUNYAIModule::checkDesirable(building, potential_up->first, true) ) {
                local_upgrade_cycle.erase(potential_up++);
            }
        }
    }

    for (auto potential_up : local_upgrade_cycle) {
        if (potential_up.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_up.second;
            up_type = potential_up.first;
        }
        if (CUNYAIModule::checkFeasibleRequirement(building, potential_up.first)) {
            up_type = potential_up.first;
            break; // if it's required, we are done. Build it!
        }
    }

    if (!busy) busy = Check_N_Upgrade(up_type, building, true);

    // Will probably not improve combat performance in FAP (will get units killed instead).
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, building, upgrade_bool && have_declared_a_major_unit_type && (CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) > 0 || CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) > 0));
    if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Antennae, building, CUNYAIModule::tech_starved && have_hive); //This upgrade is terrible, thus last.

    //should auto upgrade if there is a build order requirement for any of these three types.
    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Lair, building, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 1 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) == 0 && //don't need lair if we have a lair
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0 && //don't need lair if we have a hive.
        building->getType() == UnitTypes::Zerg_Hatchery);

    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Hive, building, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Queens_Nest) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Queens_Nest) > 0 &&
        building->getType() == UnitTypes::Zerg_Lair &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) == 0); //If you're tech-starved at this point, don't make random hives.

    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Greater_Spire, building, upgrade_bool &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Extractor) >= 3 &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hive) > 0 &&
        building->getType() == UnitTypes::Zerg_Spire &&
        CUNYAIModule::Count_Units(UnitTypes::Zerg_Greater_Spire) == 0); //If you're tech-starved at this point, don't make random hives.


    return busy;
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera. Updates CUNYAIModule::friendly_player_model.units_.
bool TechManager::Check_N_Upgrade(const UpgradeType &ups, const Unit &unit, const bool &extra_critera)
{
    bool upgrade_in_cartridges = CUNYAIModule::friendly_player_model.upgrade_cartridge_.find(ups) != CUNYAIModule::friendly_player_model.upgrade_cartridge_.end();
    if (unit->canUpgrade(ups) && CUNYAIModule::my_reservation.checkAffordablePurchase(ups) && upgrade_in_cartridges && (CUNYAIModule::buildorder.checkUpgrade_Desired(ups) || (extra_critera && CUNYAIModule::buildorder.isEmptyBuildOrder()))) {
        if (unit->upgrade(ups)) {
            CUNYAIModule::buildorder.updateRemainingBuildOrder(ups);
            Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_inventory_.find(unit)->second;
            morphing_unit.phase_ = "Upgrading";
            morphing_unit.updateStoredUnit(unit);
            CUNYAIModule::DiagnosticText("Upgrading %s.", ups.c_str());
            return true;
        }
    }
    return false;
}

//Checks if a research can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Updates CUNYAIModule::friendly_player_model.units_.
bool TechManager::Check_N_Research(const TechType &tech, const Unit &unit, const bool &extra_critera)
{
    bool research_in_cartridges = CUNYAIModule::friendly_player_model.tech_cartridge_.find(tech) != CUNYAIModule::friendly_player_model.tech_cartridge_.end();
    if (unit->canResearch(tech) && CUNYAIModule::my_reservation.checkAffordablePurchase(tech) && research_in_cartridges && (CUNYAIModule::buildorder.checkResearch_Desired(tech) || (extra_critera && CUNYAIModule::buildorder.isEmptyBuildOrder()))) {
        if (unit->research(tech)) {
            CUNYAIModule::buildorder.updateRemainingBuildOrder(tech);
            Stored_Unit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_inventory_.find(unit)->second;
            morphing_unit.phase_ = "Researching";
            morphing_unit.updateStoredUnit(unit);
            CUNYAIModule::DiagnosticText("Researching %s.", tech.c_str());
            return true;
        }
    }
    return false;
}

void TechManager::Print_Upgrade_FAP_Cycle(const int &screen_x, const int &screen_y) {
    int another_sort_of_upgrade = 0;
    std::multimap<int, UpgradeType> sorted_list;

    for (auto it : upgrade_cycle) {
        sorted_list.insert({ it.second, it.first });
    }

    for (auto tech_idea = sorted_list.rbegin(); tech_idea != sorted_list.rend(); ++tech_idea) {
            Broodwar->drawTextScreen(screen_x, screen_y, "UpgradeSimResults:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: %d", tech_idea->second.c_str(), tech_idea->first);
            another_sort_of_upgrade++;
    }
}

void TechManager::clearSimulationHistory() {
    upgrade_cycle = CUNYAIModule::friendly_player_model.upgrade_cartridge_;
    for (auto upgrade : upgrade_cycle) {
        upgrade.second = 0;
    }
    upgrade_cycle[UpgradeTypes::None] = 0;
}

//Simply returns the techtype that is the "best" of a BuildFAP sim.
int TechManager::returnTechRank(const UpgradeType &ut) {
    int postion_in_line = 0;
    multimap<int, UpgradeType> sorted_list;
    for (auto it : upgrade_cycle) {
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
