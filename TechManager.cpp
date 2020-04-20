#pragma once
// Remember not to use "Broodwar" in any global class constructor!

# include "Source\CUNYAIModule.h"
# include "Source\TechManager.h"
# include "Source\Diagnostics.h"
# include "Source\PlayerModelManager.h" // needed for cartidges.
# include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
# include "Source/Reservation_Manager.h"

using namespace BWAPI;

bool TechManager::tech_avail_ = true;
std::map<UpgradeType, int> TechManager::upgrade_cycle_ = Player_Model::getUpgradeCartridge(); // persistent valuation of buildable upgrades. Should build most valuable one every opportunity.
std::map<TechType, int> TechManager::tech_cycle_ = Player_Model::getTechCartridge(); // persistent valuation of buildable techs. Only used to determine gas requirements at the moment.

bool TechManager::checkBuildingReady(const UpgradeType up) {
    return CUNYAIModule::countUnitsAvailableToPerform(up) > 0;
}

bool TechManager::checkBuildingReady(const TechType tech) {
    return CUNYAIModule::countUnitsAvailableToPerform(tech) > 0;
}

bool TechManager::checkUpgradeFull(const UpgradeType up) {
    return CUNYAIModule::friendly_player_model.researches_.upgrades_[up] >= up.maxRepeats();
}

bool TechManager::checkUpgradeUseable(const UpgradeType up) {
    for (auto u : up.whatUses()) {
        if (CUNYAIModule::countUnits(u) > 0) return true;
    }
    return false;
}

// updates the upgrade cycle.
void TechManager::updateOptimalTech() {
    for (auto & potential_up : upgrade_cycle_) {
        // should only upgrade if units for that upgrade exist on the field for me. Or reset every time a new upgrade is found. Need a baseline null upgrade- Otherwise we'll upgrade things like range damage with only lings, when we should be saving for carapace.
        if (!checkUpgradeFull(potential_up.first) && canUpgradeCUNY(potential_up.first) || potential_up.first == UpgradeTypes::None) {
            bool isOneTimeUpgrade = potential_up.first.maxRepeats() == 1 && potential_up.first != UpgradeTypes::None; //Speed, range, adrenal glands, etc. are regularly undervalued. This is an ad-hoc adjustment.
            FAP::FastAPproximation<StoredUnit*> upgradeFAP; // attempting to integrate FAP into building decisions.
            CUNYAIModule::friendly_player_model.units_.addToBuildFAP(upgradeFAP, true, CUNYAIModule::friendly_player_model.researches_, potential_up.first);
            CUNYAIModule::enemy_player_model.units_.addToBuildFAP(upgradeFAP, false, CUNYAIModule::enemy_player_model.researches_);
            upgradeFAP.simulate(FAP_SIM_DURATION); // a complete simulation cannot always be ran... medics & firebats vs air causes a lockup.
            int score = CUNYAIModule::getFAPScore(upgradeFAP, true) + abs(CUNYAIModule::getFAPScore(upgradeFAP, true)) * 0.25 * isOneTimeUpgrade - CUNYAIModule::getFAPScore(upgradeFAP, false);
            upgradeFAP.clear();
            potential_up.second = static_cast<int>( ((24.0 * 20.0 - 1) * upgrade_cycle_[potential_up.first] + score) / (24.0 * 20.0) ); //moving average over 24*20 * 1 simulations. Long because the asymtotics really do not take hold easily.
        }
        else {
            potential_up.second = upgrade_cycle_[UpgradeTypes::None]; // If you can't use it yet, keep it up to date as equivilent to "none"
        }
    }

    chooseTech();
}

bool TechManager::checkTechAvail()
{
    return tech_avail_;
}


// Returns true if there are any new technology improvements available at this time (new buildings, upgrades, researches, mutations).
bool TechManager::updateCanMakeTechExpenditures() {

    //for (auto tech : CUNYAIModule::friendly_player_model.tech_cartridge_) {
    //    if (CUNYAIModule::Count_Units(tech.first.requiredUnit()))  tech_avail_ = true; // If we can make it and don't have it yet, we have tech we can make.
    //}

    tech_avail_ = false;

    for (auto &potential_up : upgrade_cycle_) {
        if (canUpgradeCUNY(potential_up.first)){
            tech_avail_ = true;
            if(Broodwar->getFrameCount() % 24 * 30 == 0)
                Diagnostics::DiagnosticText("I can make a: %s, gas is important.", potential_up.first.c_str());
            return tech_avail_;
        }
    }

    for (auto building : CUNYAIModule::CUNYAIModule::friendly_player_model.getBuildingCartridge()) {
        if ((building.first == UnitTypes::Zerg_Lair || building.first == UnitTypes::Zerg_Hydralisk_Den) && CUNYAIModule::countUnits(UnitTypes::Zerg_Drone) < 12) // lair & den are only worth considering if we have 12 or more workers.
            continue;
        if (building.first == UnitTypes::Zerg_Evolution_Chamber && CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair) == 0) // Evo is not worth considering unless lair is done.
            continue;
        if (building.first != UnitTypes::Zerg_Evolution_Chamber && building.first.gasPrice() == 0)
            continue;

        if (AssemblyManager::canMakeCUNY(building.first) && CUNYAIModule::countUnits(building.first) + CUNYAIModule::countSuccessorUnits(building.first, CUNYAIModule::friendly_player_model.units_) + CUNYAIModule::my_reservation.isInReserveSystem(building.first) == 0) {
            tech_avail_ = true; // If we can make it and don't have it.
            if (Broodwar->getFrameCount() % 24 * 30 == 0)
                Diagnostics::DiagnosticText("I can make a: %s, gas is important.", building.first.c_str());
            return tech_avail_;
        }
    }

    //Otherwise, nothing's available.
    return tech_avail_;
}

bool TechManager::chooseTech() {
    UpgradeType up_type = UpgradeTypes::None;
    std::map<UpgradeType, int> local_upgrade_cycle(upgrade_cycle_);
    int best_sim_score = local_upgrade_cycle[up_type];// Baseline, an upgrade must be BETTER than null upgrade. But this requirement causes freezing. So until further notice, do the "best" upgrade.

    //Only consider upgrades we can do.
    for (auto potential_up = local_upgrade_cycle.begin(); potential_up != local_upgrade_cycle.end(); potential_up++) {
        if (!canUpgradeCUNY(potential_up->first, false)) {
            local_upgrade_cycle.erase(potential_up++);
        }
    }

    //Identify the best upgrade. Requirements override this, reserve them first.
    for (auto potential_up : local_upgrade_cycle) {
        if (potential_up.second > best_sim_score) { // there are several cases where the test return ties, ex: cannot see enemy units and they appear "empty", extremely one-sided combat...
            best_sim_score = potential_up.second;
            up_type = potential_up.first;
        }
        if (CUNYAIModule::checkFeasibleRequirement(potential_up.first)) {
            up_type = potential_up.first;
            break; // if it's required, we are done. Build it!
        }
    }

    //Check to make sure there are not 2 upgrades for a single building type. 
    UpgradeType matching_upgrade = UpgradeTypes::None;
    for (auto match_check : CUNYAIModule::my_reservation.getReservedUpgrades()){
        if (up_type.whatUpgrades() == match_check.whatUpgrades()) {
            matching_upgrade = match_check;
        }
    }
    CUNYAIModule::my_reservation.removeReserveSystem(matching_upgrade, false);

    //If we have not reserved because it is unaffordable now, let us reserve it now.
    if (canUpgradeCUNY(up_type, false) && !CUNYAIModule::my_reservation.isInReserveSystem(up_type)) { //Huh? Why is this not triggering often enough?
        CUNYAIModule::my_reservation.addReserveSystem(up_type);
    }
}

// Tells a building to begin the next tech on our list. Now updates the unit if something has changed.
bool TechManager::tryToTech(Unit building, Unit_Inventory &ui, const MapInventory &inv) {

    bool busy = false;
    bool upgrade_bool = (CUNYAIModule::tech_starved || checkResourceSlack());
    bool have_declared_lurkers = BWAPI::Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect);
    bool have_declared_mutas = CUNYAIModule::countUnits(UnitTypes::Zerg_Spire) > 0;
    bool cannot_build_major_unit_type = !CUNYAIModule::checkInCartridge(UnitTypes::Zerg_Lurker) && !CUNYAIModule::checkInCartridge(UnitTypes::Zerg_Mutalisk);
    bool have_declared_a_major_unit_type = have_declared_lurkers || have_declared_mutas || cannot_build_major_unit_type;
    bool have_hive = CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) > 0;
    bool maxed_melee = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Melee_Attacks) == 3;
    bool maxed_range = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Missile_Attacks) == 3;
    bool maxed_armor = BWAPI::Broodwar->self()->getUpgradeLevel(UpgradeTypes::Zerg_Carapace) == 3;

    // Researchs, not upgrades per se:
    if (!busy) busy = Check_N_Research(TechTypes::Lurker_Aspect, building, (upgrade_bool || CUNYAIModule::enemy_player_model.units_.detector_count_ + CUNYAIModule::enemy_player_model.casualties_.detector_count_ == 0) && (CUNYAIModule::countUnits(UnitTypes::Zerg_Lair) > 0 || CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) > 0));

    if (building->getType() == UnitTypes::Zerg_Hydralisk_Den) Diagnostics::DiagnosticText("Let's look at a Hydra Den!");

    //first let's do reserved upgrades:
    for (auto up : CUNYAIModule::my_reservation.getReservedUpgrades()) {
        busy = Check_N_Upgrade(up, building, true);
        if (busy) break;
    }

    // Will probably not improve combat performance in FAP (will get units killed instead).
    //if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Pneumatized_Carapace, building, upgrade_bool && have_declared_a_major_unit_type && CUNYAIModule::countSuccessorUnits(UnitTypes::Zerg_Lair) > 0);
    //if (!busy) busy = Check_N_Upgrade(UpgradeTypes::Antennae, building, CUNYAIModule::tech_starved && have_hive); //This upgrade is terrible, thus last. It's actually been removed in the cartridge, since it's so distracting. This will stop it from upgrading, but the logic is best I have so far.

    //should auto upgrade if there is a build order requirement for any of these three types.
    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Lair, building, upgrade_bool &&
        (CUNYAIModule::basemanager.getBaseCount() >= 1) && // This is often too early - we either have 2bases (or the hydra den so that we can do lurkers, see steamhammer), or we have seveeral sunkens and are being forced to one-base.
        CUNYAIModule::countUnits(UnitTypes::Zerg_Lair) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Lair) == 0 && //don't need lair if we have a lair
        CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0 && //don't need lair if we have a hive.
        building->getType() == UnitTypes::Zerg_Hatchery);

    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Hive, building, upgrade_bool &&
        CUNYAIModule::basemanager.getBaseCount() >= 3 &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Queens_Nest) - CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Queens_Nest) > 0 &&
        building->getType() == UnitTypes::Zerg_Lair &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Hive) == 0); //If you're tech-starved at this point, don't make random hives.

    if (!busy) busy = CUNYAIModule::assemblymanager.Check_N_Build(UnitTypes::Zerg_Greater_Spire, building, upgrade_bool &&
        CUNYAIModule::basemanager.getBaseCount() >= 3 &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Hive) - CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Hive) > 0 &&
        building->getType() == UnitTypes::Zerg_Spire &&
        CUNYAIModule::countUnits(UnitTypes::Zerg_Greater_Spire) + Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Greater_Spire) == 0); //If you're tech-starved at this point, don't make random hives.

    if (busy) {
        Diagnostics::DiagnosticText("Looks like we wanted to upgrade something. Here's the general inputs I was thinking about:");
        Diagnostics::DiagnosticText("Slackness: %s", checkResourceSlack() ? "TRUE" : "FALSE");
        Diagnostics::DiagnosticText("Tech Starved: %s", CUNYAIModule::tech_starved ? "TRUE" : "FALSE");
        Diagnostics::DiagnosticText("For this %s", building->getType().getName().c_str());
        for (auto potential_up : upgrade_cycle_) {
            Diagnostics::DiagnosticText("Upgrades: %s, %d", potential_up.first.c_str(), potential_up.second);
        }
    }

    return busy;
}

//Checks if an upgrade can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Requires extra critera. Updates CUNYAIModule::friendly_player_model.units_.
bool TechManager::Check_N_Upgrade(const UpgradeType &ups, const Unit &unit, const bool &extra_critera)
{
    if (unit->canUpgrade(ups) && CUNYAIModule::my_reservation.isInReserveSystem(ups) && isInUpgradeCartridge(ups) && (CUNYAIModule::buildorder.checkUpgrade_Desired(ups) || (extra_critera && CUNYAIModule::buildorder.isEmptyBuildOrder()))) {
        if (unit->upgrade(ups)) {
            CUNYAIModule::buildorder.updateRemainingBuildOrder(ups);
            StoredUnit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
            morphing_unit.phase_ = StoredUnit::Phase::Upgrading;
            morphing_unit.updateStoredUnit(unit);
            CUNYAIModule::my_reservation.removeReserveSystem(ups, false);
            Diagnostics::DiagnosticText("Upgrading %s.", ups.c_str());
            return true;
        }
    }
    return false;
}

//Checks if a research can be built, and passes additional boolean criteria.  If all critera are passed, then it performs the upgrade. Updates CUNYAIModule::friendly_player_model.units_.
bool TechManager::Check_N_Research(const TechType &tech, const Unit &unit, const bool &extra_critera)
{
    if (unit->canResearch(tech) && CUNYAIModule::my_reservation.checkAffordablePurchase(tech) && isInResearchCartridge(tech) && (CUNYAIModule::buildorder.checkResearch_Desired(tech) || (extra_critera && CUNYAIModule::buildorder.isEmptyBuildOrder()))) {
        if (unit->research(tech)) {
            CUNYAIModule::buildorder.updateRemainingBuildOrder(tech);
            StoredUnit& morphing_unit = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit)->second;
            morphing_unit.phase_ = StoredUnit::Phase::Researching;
            morphing_unit.updateStoredUnit(unit);
            Diagnostics::DiagnosticText("Researching %s.", tech.c_str());
            return true;
        }
    }
    return false;
}

void TechManager::Print_Upgrade_FAP_Cycle(const int &screen_x, const int &screen_y) {
    int another_sort_of_upgrade = 0;
    std::multimap<int, UpgradeType> sorted_list;

    for (auto it : upgrade_cycle_) {
        sorted_list.insert({ it.second, it.first });
    }

    for (auto tech_idea = sorted_list.rbegin(); tech_idea != sorted_list.rend(); ++tech_idea) {
        Broodwar->drawTextScreen(screen_x, screen_y, "UpgradeSimResults:");  //
        Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: %d", tech_idea->second.c_str(), tech_idea->first);
        another_sort_of_upgrade++;
    }
}

void TechManager::clearSimulationHistory() {
    for (auto & upgrade : CUNYAIModule::friendly_player_model.getUpgradeCartridge()) {
        upgrade.second = 0;
    }
}

//Simply returns the techtype that is the "best" of a BuildFAP sim.
int TechManager::returnUpgradeRank(const UpgradeType &ut) {
    int postion_in_line = 0;
    vector<tuple<int, UpgradeType>> sorted_list;
    for (auto it : upgrade_cycle_) {
        sorted_list.push_back(tuple{ it.second, it.first });
    }

    sort(sorted_list.end(), sorted_list.begin()); //by default sorts by first element of tuples in DECENDING order! 

    for (auto tech_idea = sorted_list.rbegin(); tech_idea != sorted_list.rend(); ++tech_idea) {
        if (std::get<1>(*tech_idea) == ut) {
            return postion_in_line;
        }
        else postion_in_line++;
    }
    return postion_in_line;
}

int TechManager::getMaxGas()
{
    int max_gas_value_ = 0;
    for (auto potential_up : upgrade_cycle_) {
        if (checkBuildingReady(potential_up.first) && !checkUpgradeFull(potential_up.first) && canUpgradeCUNY(potential_up.first)) {
            max_gas_value_ = max(potential_up.first.gasPrice(), max_gas_value_); // just a check to stay sharp on max gas.
        }
    }
    for (auto potential_tech : tech_cycle_) {
        if (checkBuildingReady(potential_tech.first) && canResearchCUNY(potential_tech.first)) {
            max_gas_value_ = max(potential_tech.first.gasPrice(), max_gas_value_); // just a check to stay sharp on max gas.
        }
    }

    return max_gas_value_;
}

bool TechManager::canUpgradeCUNY(const UpgradeType type, const bool checkAffordable, const Unit &builder)
{
    Player self = Broodwar->self();

    if (builder)
    {
        if (builder->getPlayer() != self)
            return false;
        if (!builder->getType().isSuccessorOf(type.whatUpgrades()))
            return false;
        if ((builder->isLifted() || !builder->isIdle() || !builder->isCompleted()))
            return false;
    }

    if (!self)
        return false;
    int nextLvl = self->getUpgradeLevel(type) + 1;

    if (!self->hasUnitTypeRequirement(type.whatUpgrades()))
        return false;
    if (!self->hasUnitTypeRequirement(type.whatsRequired(nextLvl)))
        return false;
    if (self->isUpgrading(type))
        return false;
    if (self->getUpgradeLevel(type) >= self->getMaxUpgradeLevel(type))
        return false;
    if (checkAffordable) {
        if (self->minerals() < type.mineralPrice(nextLvl))
            return false;
        if (self->gas() < type.gasPrice(nextLvl))
            return false;
    }
    return true;
}

bool TechManager::canResearchCUNY(TechType type, const bool checkAffordable, const Unit &builder)
{
    // Error checking
    if (!Broodwar->self())
        return Broodwar->setLastError(Errors::Unit_Not_Owned);

    if (builder)
    {
        if (builder->getPlayer() != Broodwar->self())
            return false;
        if (!builder->getType().isSuccessorOf(type.whatResearches()))
            return false;
        if ((builder->isLifted() || !builder->isIdle() || !builder->isCompleted()))
            return false;
    }

    if (Broodwar->self()->isResearching(type))
        return false;
    if (Broodwar->self()->hasResearched(type))
        return false;
    if (!Broodwar->self()->isResearchAvailable(type))
        return false;

    if (checkAffordable) {
        if (Broodwar->self()->minerals() < type.mineralPrice())
            return false;
        if (Broodwar->self()->gas() < type.gasPrice())
            return false;
    }

    if (!Broodwar->self()->hasUnitTypeRequirement(type.requiredUnit()))
        return false;

    return Broodwar->setLastError();
}

bool TechManager::checkResourceSlack()
{
    bool idle_buildings = false;
    for (auto up : upgrade_cycle_) {
        if (CUNYAIModule::countUnitsAvailableToPerform(up.first) > 0) {
            idle_buildings = true;
            break;
        }
    }
    return (CUNYAIModule::my_reservation.getExcessMineral() >= 100 && CUNYAIModule::my_reservation.getExcessGas() >= 100) || CUNYAIModule::countUnits(UnitTypes::Zerg_Larva) == 0 || idle_buildings;
}

bool TechManager::isInUpgradeCartridge(const UpgradeType & ut)
{
    auto mapTech = CUNYAIModule::friendly_player_model.getUpgradeCartridge();
    bool upgradeIsInCartridge = mapTech.find(ut) != mapTech.end();
    return upgradeIsInCartridge;
}

bool TechManager::isInResearchCartridge(const TechType & ut)
{
    auto mapTech = CUNYAIModule::friendly_player_model.getTechCartridge();
    bool techIsInCartridge = mapTech.find(ut) != mapTech.end();
    return techIsInCartridge;
}
