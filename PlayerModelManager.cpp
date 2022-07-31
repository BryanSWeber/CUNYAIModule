#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\ResearchInventory.h"
#include "Source\UnitInventory.h"
#include "Source\CobbDouglas.h"
#include "Source\Diagnostics.h"
#include <numeric>

using namespace std;
using namespace BWAPI;


void PlayerModel::onStartSelf(LearningManager l)
{
    spending_model_.onStartSelf(l);
}

void PlayerModel::updateOtherOnFrame(const Player & other_player)
{
    // Store Player
    bwapi_player_ = other_player;
    player_race_ = bwapi_player_->getRace();
    if (player_race_ == Races::Random || player_race_ == Races::Unknown || player_race_ == Races::None )
        player_race_ = Races::Terran; // if random, assume terran for starters. (There are race conditions on zerg.

    // Update Enemy Units
    units_.updateUnitsControlledBy(other_player);
    units_.purgeBrokenUnits();
    units_.updateUnitInventorySummary();
    casualties_.updateUnitInventorySummary();
    firstAirThreatSeen_ = (firstAirThreatSeen_ == 0 && units_.stock_fliers_ > 0) ? Broodwar->getFrameCount() : firstAirThreatSeen_;
    firstDetectorSeen_ = (firstDetectorSeen_ == 0 && units_.detector_count_ > 0) ? Broodwar->getFrameCount() : firstDetectorSeen_;

    // Update Researches
    researches_.updateResearch(other_player);

    if (other_player->isEnemy(Broodwar->self())) {
        evaluatePotentialUnitExpenditures(); // How much is being bought?
        evaluatePotentialTechExpenditures(); // How much is being upgraded/researched?

        evaluateCurrentWorth(); // How much do they appear to have?
        estimated_workers_ = units_.worker_count_ + estimated_unseen_workers_;     // Combine unseen and seen workers for a total worker count.


        int worker_value = StoredUnit(player_race_.getWorker()).stock_value_;
        int estimated_worker_stock_ = static_cast<int>(estimated_workers_ * worker_value);

        //spending_model_.estimateUnknownCD(units_.stock_fighting_total_ + static_cast<int>(estimated_unseen_army_),
        //    researches_.research_stock_ + static_cast<int>(estimated_unseen_tech_),
        //    estimated_worker_stock_);

        //spending_model_.setStockObserved(units_.stock_fighting_total_,
        //    researches_.research_stock_,
        //    units_.worker_count_* worker_value);

        spending_model_.setEnemyCD(units_.stock_fighting_total_ + static_cast<int>(estimated_unseen_army_), researches_.research_stock_ + static_cast<int>(estimated_unseen_tech_), estimated_worker_stock_);

        updatePlayerAverageCD(); // For saving/printing on game end, what is this guy's style like?
    }
};

void PlayerModel::updateSelfOnFrame()
{
    bwapi_player_ = Broodwar->self();
    player_race_ = Broodwar->self()->getRace();

    //Bans units you don't want for a particular matchup.
    if (CUNYAIModule::enemy_player_model.player_race_ == Races::Zerg) {
        dropBuildingType(UnitTypes::Zerg_Hydralisk_Den);
        dropUnitType(UnitTypes::Zerg_Hydralisk);
        dropUnitType(UnitTypes::Zerg_Lurker);
    }

    //Update Enemy Units
    //Update friendly unit inventory.
    updateUnit_Counts();
    if (units_.unit_map_.size() == 0) units_ = UnitInventory(Broodwar->self()->getUnits()); // if you only do this you will lose track of all of your locked minerals. 
    else units_.updateUnitInventory(Broodwar->self()->getUnits()); // safe for locked minerals.
    units_.purgeBrokenUnits();
    units_.purgeUnseenUnits(); //Critical for self!
    units_.updateUnitInventorySummary();
    casualties_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(Broodwar->self());

    int worker_value = StoredUnit(player_race_.getWorker()).stock_value_;
    spending_model_.setCD(units_.stock_fighting_total_, researches_.research_stock_, units_.worker_count_ * worker_value);

    if constexpr (TIT_FOR_TAT_ENGAGED) {
        if (Broodwar->getFrameCount() % (30 * 24) == 0) {
            //Update existing CD functions to more closely mirror opponent. Do every 15 sec or so.
            if (!CUNYAIModule::enemy_player_model.units_.unit_map_.empty()) {
                spending_model_.enemy_mimic(CUNYAIModule::enemy_player_model);
                //Diagnostics::DiagnosticWrite("Matching expenditures,L:%4.2f to %4.2f,K:%4.2f to %4.2f,T:%4.2f to %4.2f", spending_model_.alpha_econ, target_player.spending_model_.alpha_econ, spending_model_.alpha_army, target_player.spending_model_.alpha_army, spending_model_.alpha_tech, target_player.spending_model_.alpha_army);
            }
            else {
                spending_model_.onStartSelf(CUNYAIModule::learnedPlan);
            }
        }
    }

    CUNYAIModule::tech_starved = spending_model_.tech_starved();
    CUNYAIModule::army_starved = spending_model_.army_starved();
    CUNYAIModule::econ_starved = spending_model_.econ_starved();

    //Update general weaknesses.
    bool seen_air = CUNYAIModule::enemy_player_model.units_.stock_fliers_ + CUNYAIModule::enemy_player_model.casualties_.stock_fliers_ > 0;
    bool possible_air = CUNYAIModule::enemy_player_model.estimated_unseen_flyers_ > 0;
    if(CUNYAIModule::army_starved) u_have_active_air_problem_ = (bool)( (CUNYAIModule::assemblymanager.testActiveAirDefenseBest() && (seen_air || possible_air)) || (CUNYAIModule::assemblymanager.testAirAttackBest() && seen_air) );
    if(CUNYAIModule::army_starved) e_has_air_vunerability_ = (bool)(CUNYAIModule::assemblymanager.testActiveAirDefenseBest(false) || CUNYAIModule::assemblymanager.testAirAttackBest(false));

    //Update map inventory
    //radial_distances_from_enemy_ground_ = CUNYAIModule::current_MapInventory.getRadialDistances(units_, true);
    //closest_ground_combatant_ = *std::min_element(radial_distances_from_enemy_ground_.begin(), radial_distances_from_enemy_ground_.end());

}

void PlayerModel::imputeUnits(const Unit &unit)
{
    StoredUnit eu = StoredUnit(unit);
    double temp_estimated_unseen_army_ = 0;

    //subtracted observations from estimates. The estimates could be quite wrong, but the discovery of X when Y is already imputed leads to a surplus of (Y-X) which will be a persistent error. Otherwise I have to remove Y itself or guess.
    auto matching_unseen_units = unseen_units_.find(eu.type_);
    if (matching_unseen_units != unseen_units_.end()) {
        unseen_units_.at(eu.type_)--; // reduce the count of this unite by one.
        if (eu.type_.isBuilding())
            unseen_units_.at(eu.type_) = max(unseen_units_.at(eu.type_), 0.0); // there cannot be negative buildings, this may lead to some awkward problems as they could (?) produce negative amounts of troops, maybe.
    }
    else {
        auto alternative_uts = findAlternativeProducts(eu.type_);
        for (auto ut : alternative_uts) {
            auto mistakenly_assumed_produced = unseen_units_.find(ut);
            if (mistakenly_assumed_produced != unseen_units_.end() && mistakenly_assumed_produced->second >= static_cast<double>(eu.type_.buildTime()) / static_cast<double>(mistakenly_assumed_produced->first.buildTime()) ) {
                mistakenly_assumed_produced->second -= static_cast<double>(eu.type_.buildTime()) / static_cast<double>(mistakenly_assumed_produced->first.buildTime());
                break;
            }
        }
    }

    //Check if we have overdrafted our units.
    for (auto ut : unseen_units_) {
        if (CUNYAIModule::isFightingUnit(ut.first)) {
            temp_estimated_unseen_army_ += static_cast<double>(StoredUnit(ut.first).stock_value_) * ut.second;
        }
    }

    //insert some unseen units if we have overdrafted.
    if (temp_estimated_unseen_army_ < 0) {
        UnitType expected_producer = UnitTypes::None;
        if (eu.type_.whatBuilds().first.isBuilding()) {
            StoredUnit imputed_unit = StoredUnit(eu.type_.whatBuilds().first);
            imputed_unit.time_first_observed_ = eu.type_.buildTime(); // it must be at least old enough to build it.
            incrementUnseenUnits(imputed_unit.type_);
            expected_producer = eu.type_.whatBuilds().first;
        }
        if (eu.type_.whatBuilds().first == UnitTypes::Zerg_Larva) {
            StoredUnit imputed_unit = UnitTypes::Zerg_Hatchery;
            imputed_unit.time_first_observed_ = eu.type_.buildTime() + LARVA_BUILD_TIME;
            incrementUnseenUnits(imputed_unit.type_);
            expected_producer = UnitTypes::Zerg_Hatchery;
        }

        // This buffer is pretty critical. How much production has been made from the unseen facility?
        int longest_known_unit = Broodwar->getFrameCount() - inferEarliestPossible(eu.type_);

        //Infer based on what we have seen!
        //for (auto u : units_.unit_map_) {
        //    if (u.second.type_ == expected_producer || u.second.type_ == eu.type_) {
        //        longest_known_unit = max(Broodwar->getFrameCount() - u.second.time_first_observed_, longest_known_unit);
        //    }
        //}

        //for (auto u : casualties_.unit_map_) {
        //    if (u.second.type_ == expected_producer || u.second.type_ == eu.type_) {
        //        longest_known_unit = max(Broodwar->getFrameCount() - u.second.time_first_observed_, longest_known_unit);
        //    }
        //}

        UnitType produced_unit = getDistributedProduct(expected_producer);
        double maximum_possible_missed_product = static_cast<double>(max(longest_known_unit, eu.type_.buildTime())) / static_cast<double>(eu.type_.buildTime());
        if (produced_unit != UnitTypes::None) {
            //Add it to the simply map tallying enemy units.
            auto found = unseen_units_.find(produced_unit);
            if (found != unseen_units_.end())
                unseen_units_[produced_unit] += maximum_possible_missed_product;
            else
                unseen_units_.insert({ produced_unit , maximum_possible_missed_product });
        }
    }
}

void PlayerModel::incrementUnseenUnits(const UnitType & ut)
{
    auto matching_unseen_units = unseen_units_.find(ut);
    if (matching_unseen_units != unseen_units_.end())
        unseen_units_.at(ut)++; // reduce the count of this unite by one.
    else {
        unseen_units_.insert({ ut, 1.0 });
    }
}

void PlayerModel::setUnseenUnits(const UnitType & ut, const double &d)
{
    auto matching_unseen_units = unseen_units_.find(ut);
    if (matching_unseen_units != unseen_units_.end())
        unseen_units_.at(ut)++; // reduce the count of this unite by one.
    else {
        unseen_units_.insert({ ut, 1.0 });
    }
}

void PlayerModel::decrementUnseenUnits(const UnitType & ut)
{
    auto matching_unseen_units = unseen_units_.find(ut);
    if (matching_unseen_units != unseen_units_.end())
        unseen_units_.at(ut)--; // reduce the count of this unite by one.
}

double PlayerModel::countUnseenUnits(const UnitType & ut)
{
    auto matching_unseen_units = unseen_units_.find(ut);
    if (matching_unseen_units != unseen_units_.end())
        return unseen_units_.at(ut); // reduce the count of this unite by one.
    return 0.0;
}

void PlayerModel::evaluatePotentialUnitExpenditures() {
    double temp_estimated_unseen_supply_ = 0;
    double temp_estimated_unseen_army_ = 0;
    double temp_estimated_unseen_flyers_ = 0;
    double temp_estimated_unseen_ground_ = 0;
    double temp_estimated_worker_supply = 0;
    double temp_estimated_worker_value = 0;
    double temp_estimated_army_supply = 0;
    double temp_estimated_unseen_value = 0;
    std::set<UnitType> unitTypeExist = {};

    if (Broodwar->getFrameCount() == 0) {
        for(int i = 0; i < 4; i++)
            incrementUnseenUnits(player_race_.getWorker()); // at game start there are 4 workers.
        incrementUnseenUnits(player_race_.getResourceDepot()); // there is also one base.
    }

    if (countUnseenUnits(player_race_.getResourceDepot()) <= 0) 
        incrementUnseenUnits(player_race_.getResourceDepot()); // there is always one base.
    if (countUnseenUnits(player_race_.getResourceDepot()) < ceil( countUnseenUnits(player_race_.getWorker()) / (16.0 + player_race_.getResourceDepot().buildTime() / player_race_.getWorker().buildTime()) ) )
        incrementUnseenUnits(player_race_.getResourceDepot()); // For every 16 workers, there is another base.
    if (countUnseenUnits(player_race_.getWorker()) <= 0)
        incrementUnseenUnits(player_race_.getWorker()); // there is always one worker.

    //Add logically necessary units to the unit inventory.
    for (auto u : units_.unit_map_) {
        unitTypeExist.insert(u.second.type_);
    }
    for (auto i : researches_.getUpgrades()) {
        if (i.second && i.first.whatsRequired(i.second) != UnitTypes::None)
            unitTypeExist.insert(i.first.whatsRequired(i.second));
    }
    for (auto i : researches_.getTech()) {
        if (i.second && i.first.whatResearches() != UnitTypes::None)
            unitTypeExist.insert(i.first.whatResearches());
    }
    for (auto u : inferUnits(unitTypeExist)) { // if we needed it for observed units, observed upgrades, or observed tech, there has to have been at least 1 of them.
        if (countUnseenUnits(u) + CUNYAIModule::countUnits(u,casualties_)  == 0)
            incrementUnseenUnits(u);
    }


    //consider the production of the enemy you can see.
    considerUnseenProducts(units_);
    considerWorkerUnseenProducts(units_);
    //consider the production of the enemy you imagine.
    considerUnseenProducts(unseen_units_);
    considerWorkerUnseenProducts(unseen_units_);

    for (auto ut : unseen_units_) {
        temp_estimated_unseen_supply_ += ut.first.supplyRequired() * ut.second;
        temp_estimated_unseen_value += StoredUnit(ut.first).stock_value_ * ut.second;
        if (CUNYAIModule::isFightingUnit(ut.first)) {
            temp_estimated_army_supply += ut.first.supplyRequired() * ut.second;
            temp_estimated_unseen_army_ += StoredUnit(ut.first).stock_value_ * ut.second;
            temp_estimated_unseen_flyers_ += StoredUnit(ut.first).stock_value_ * ut.first.isFlyer() * ut.second;
            temp_estimated_unseen_ground_ += StoredUnit(ut.first).stock_value_ * !ut.first.isFlyer() * ut.second;
        }
        if (ut.first.isWorker()) {
            temp_estimated_worker_supply += ut.first.supplyRequired() * ut.second;
            temp_estimated_worker_value += StoredUnit(ut.first).stock_value_ * ut.second;
        }
    }

    units_.updateUnitInventorySummary();
    double remaining_supply_capacity = (400.0 - units_.total_supply_);
    double average_army_per_supply = temp_estimated_unseen_army_ / temp_estimated_army_supply;
    double average_worker_per_supply = temp_estimated_worker_value / temp_estimated_worker_supply;
    double army_proportion = temp_estimated_army_supply / temp_estimated_unseen_supply_;
    double worker_proportion = temp_estimated_worker_supply / temp_estimated_unseen_supply_;
    double supply_cost_per_worker = player_race_.getWorker().supplyRequired();

    if (remaining_supply_capacity < temp_estimated_unseen_supply_) {
        estimated_unseen_army_ = max(remaining_supply_capacity * average_army_per_supply * army_proportion, 0.0); //Their unseen army can't be bigger than their leftovers, or less than 0.
        estimated_unseen_workers_ = max(remaining_supply_capacity / supply_cost_per_worker * worker_proportion, 0.0); //Their unseen workers is the proportion of remaining units that are not army.

        estimated_unseen_flyers_ = max(temp_estimated_unseen_flyers_ / static_cast<double>(temp_estimated_unseen_army_) * estimated_unseen_army_, 0.0); //Their unseen fliers remain proportional
        estimated_unseen_ground_ = max(temp_estimated_unseen_ground_ / static_cast<double>(temp_estimated_unseen_army_) * estimated_unseen_army_, 0.0); //Their unseen ground remains proportional
    }
    else {
        estimated_unseen_army_ = max(temp_estimated_unseen_army_, 0.0);
        estimated_unseen_workers_ = max(temp_estimated_worker_supply / supply_cost_per_worker, 0.0);
        estimated_unseen_flyers_ = max(temp_estimated_unseen_flyers_, 0.0);
        estimated_unseen_ground_ = max(temp_estimated_unseen_ground_, 0.0);
    }


    if (Broodwar->getFrameCount() % (60 * 24) == 0) {
        Diagnostics::DiagnosticWrite("Total Supply Observed: %d", units_.total_supply_);
        Diagnostics::DiagnosticWrite("What do we think is happening behind the scenes?");
        Diagnostics::DiagnosticWrite("This is the unseen units of an %s:", this->bwapi_player_->isEnemy(Broodwar->self()) ? "ENEMY" : (this->bwapi_player_->isAlly(Broodwar->self()) ? "FRIEND" : "NEUTRAL") );
        for (auto ut : unseen_units_)
            Diagnostics::DiagnosticWrite("They have %4.2f of %s", ut.second, ut.first.c_str());
        Diagnostics::DiagnosticWrite("I count an unused supply of %4.2f and imagine units taking %4.2f supply", remaining_supply_capacity, temp_estimated_unseen_supply_);
    }
}

void PlayerModel::evaluatePotentialTechExpenditures() {
    double min_possible_ = 0;
    double gas_possible_ = 0;
    double supply_possible_ = 0;

    double value_possible_per_frame_ = 0;
    double value_possible_per_unit_ = 0;

    //Estimate the tech benifit from research buildings.
    for (auto i : researches_.getTechBuildings()) {// includes imputed buildings.

        double value_holder_up_ = 0;
        int time_since_last_benificiary_seen_up_ = INT_MAX;
        bool benificiary_exists_up_ = true;
        int oldest_up_class = 0;


        double value_holder_tech_ = 0;
        bool benificiary_exists_tech_ = false;
        int time_since_last_benificiary_seen_tech_ = INT_MAX;
        int oldest_tech_class_ = 0;

        double value_holder_building_ = 0;
        int slowest_building_class_ = 0;

        // These are possible upgrade expenditures.
        for (auto p : i.first.upgradesWhat()) {
            if (opponentCouldBeUpgrading(p)) { // can they upgrade?
                for (auto j : units_.unit_map_) {
                    if (j.second.type_.upgrades().contains(p)) { // is there a benifitiary in their inventory?  upgrade does not depend on time last seen but time last dependent unit was seen. 
                        benificiary_exists_up_ = true;
                        time_since_last_benificiary_seen_up_ = min(time_since_last_benificiary_seen_up_, j.second.time_since_last_seen_);

                        int level = 0;
                        if (CUNYAIModule::enemy_player_model.researches_.getUpgrades().find(p) != CUNYAIModule::enemy_player_model.researches_.getUpgrades().end()) 
                            level = CUNYAIModule::enemy_player_model.researches_.getUpLevel(p);
                        value_holder_up_ = max(value_holder_up_, p.mineralPrice() / static_cast<double>(p.upgradeTime() + level * p.upgradeTimeFactor())) + 1.25 * (p.gasPrice() / static_cast<double>(p.upgradeTime() + level * p.upgradeTimeFactor()));
                        oldest_up_class = min(max(oldest_up_class, time_since_last_benificiary_seen_up_ * benificiary_exists_up_), p.upgradeTime() + level * p.upgradeTimeFactor()); // we want the youngest benificiary from the oldest class of units, and they couldn't be working on the upgrade longer than it takes to complete.
                    };
                }
            }
        }

        // These are possible tech expenditures.
        for (auto p : i.first.researchesWhat()) {
            if (opponentCouldBeTeching(p)) {
                for (auto j : units_.unit_map_) {
                    for (auto flagged_unit_type : p.whatUses()) {
                        if (j.second.type_ == flagged_unit_type) {
                            benificiary_exists_tech_ = true;
                            time_since_last_benificiary_seen_tech_ = min(time_since_last_benificiary_seen_tech_, j.second.time_since_last_seen_);

                            value_holder_tech_ = max(value_holder_tech_, p.mineralPrice() / static_cast<double>(p.researchTime()) + 1.25 * (p.gasPrice() / static_cast<double>(p.researchTime())));
                            oldest_tech_class_ = min(max(oldest_tech_class_, time_since_last_benificiary_seen_tech_ * benificiary_exists_tech_), p.researchTime()); // we want the youngest benificiary from the oldest class of units, and they couldn't be working on the research longer than it takes to complete.
                        };
                    }
                }
            }
        }

        for (auto p : UnitTypes::allUnitTypes()) {
            bool permits_new_unit = false;
            for (auto possible_new_unit : p.buildsWhat()) { // a building allows new units if it produces something and is not a duplicate.
                if (CUNYAIModule::countUnits(possible_new_unit, units_) == 0 && CUNYAIModule::countUnits(p, units_) == 0 && (possible_new_unit.isBuilding() || possible_new_unit.isAddon()) && (!possible_new_unit.upgradesWhat().empty() || !possible_new_unit.researchesWhat().empty())) {
                    permits_new_unit = true;
                    break;
                }
            }
            if (opponentHasRequirements(p) && !CUNYAIModule::isFightingUnit(p) && (p.isBuilding() || p.isAddon()) && (!p.upgradesWhat().empty() || !p.researchesWhat().empty() || permits_new_unit) && p != UnitTypes::Zerg_Hatchery && !researches_.countResearchBuildings(p)) {
                value_holder_building_ = max(value_holder_building_, StoredUnit(p).stock_value_ / static_cast<double>(p.buildTime())); // assume the largest of these. (worst for me, risk averse).
                slowest_building_class_ = max(p.buildTime(), slowest_building_class_); // is the priciest unit a flier?
            }
        }

        if (!benificiary_exists_tech_) oldest_tech_class_ = 0; // if they've never been seen, they're probably not getting made.
        if (!benificiary_exists_up_) oldest_up_class = 0;

        value_possible_per_frame_ += value_holder_up_ + value_holder_tech_ + value_holder_building_;
        value_possible_per_unit_ += value_holder_up_ * oldest_up_class + value_holder_tech_ * oldest_tech_class_ + value_holder_building_ * slowest_building_class_;
    }

    estimated_unseen_tech_ = value_possible_per_unit_;
    estimated_unseen_tech_per_frame_ = value_possible_per_frame_;
}

void PlayerModel::evaluateCurrentWorth()
{
    if (Broodwar->getFrameCount() == 0) {
        estimated_cumulative_worth_ = 50;
    }
    else { // what is the net worth of everything he has bought so far and has reasonably collected?

        //Collect how much of the enemy that has been bought.
        int min_expenditures_ = 0;
        int gas_expenditures_ = 0;
        int supply_expenditures_ = 0;
        double min_proportion = 0.0;

        // collect how much of the enemy has died.
        int min_losses_ = 0;
        int gas_losses_ = 0;
        int supply_losses_ = 0;

        //collect how much of the enemy you can see.
        for (auto i : units_.unit_map_) {
            // These have been observed.
            min_expenditures_ += i.second.modified_min_cost_;
            gas_expenditures_ += i.second.modified_gas_cost_;
            supply_expenditures_ += i.second.modified_supply_;
        }

        for (auto i : researches_.getUpgrades()) {
            int number_of_times_factor_triggers = max((i.second * (i.second + 1)) / 2 - 1, 0);
            min_expenditures_ += i.first.mineralPrice() * i.second + i.first.mineralPriceFactor() * number_of_times_factor_triggers;
            gas_expenditures_ += (i.first.gasPrice() * i.second + i.first.gasPriceFactor() * number_of_times_factor_triggers);
        }

        for (auto i : researches_.getTech()) {
            min_expenditures_ += i.first.mineralPrice() * i.second;
            gas_expenditures_ += i.first.gasPrice() * i.second;
        }

        for (auto i : casualties_.unit_map_) {
            min_losses_ += i.second.modified_min_cost_;
            gas_losses_ += i.second.modified_gas_cost_;
            supply_losses_ += i.second.modified_supply_;
        }

        //Find the relative rates at which the opponent has been spending these resources.
        double min_spent = min_expenditures_ + min_losses_; //minerals per each unit of resources mined.
        double gas_spent = gas_expenditures_ + gas_losses_;
        double supply_spent = supply_expenditures_ + supply_losses_; //Supply bought resource collected- very rough.

        if ((gas_spent + min_spent) != 0) min_proportion = min_spent / (gas_spent + min_spent);

        estimated_resources_per_frame_ = estimated_workers_ * (0.045 * min_proportion + 0.07 * (1 - min_proportion) * 1.25); // If we assign them in the same way they have been assigned over the course of this game...
        // Churchill, David, and Michael Buro. "Build Order Optimization in StarCraft." AIIDE. 2011.  Workers gather minerals at a rate of about 0.045/frame and gas at a rate of about 0.07/frame.
        estimated_cumulative_worth_ += max(estimated_resources_per_frame_, estimated_unseen_army_per_frame_ + estimated_unseen_tech_per_frame_); // 

        double min_on_field = min_expenditures_ - min_losses_;
        double gas_on_field = gas_expenditures_ - gas_losses_;
        double supply_on_field = supply_expenditures_ - supply_losses_;

        double observed_current_worth = min_on_field + gas_on_field * 1.25 + supply_on_field * 25;

        estimated_net_worth_ = max(observed_current_worth, estimated_cumulative_worth_ - min_losses_ - gas_losses_ * 1.25 - supply_spent * 25);
    }
}

//Takes a unit inventory and increments the unit map as if everything in the unit map was producing.  This does not include depots producing workers.
void PlayerModel::considerUnseenProducts(const UnitInventory &ui)
{
    for (auto i : ui.unit_map_) { // each unit is individually stored in this unit map.

        UnitType the_worst_unit = getDistributedProduct(i.second.type_);

        if (the_worst_unit != UnitTypes::None) {
            //Add it to the simply map tallying enemy units.
            auto found = unseen_units_.find(the_worst_unit);
            if (found != unseen_units_.end())
                unseen_units_[the_worst_unit] += 1.0 / static_cast<double>(max(the_worst_unit.buildTime(), 1));
            else
                unseen_units_.insert({ the_worst_unit , 1.0 / static_cast<double>(max(the_worst_unit.buildTime(),1)) });
        }
    }
}


//Takes an unseen unit map and increments the unseen_units map as if everything in the unit map was producing.  This does not include depots producing workers.
void PlayerModel::considerUnseenProducts(const map< UnitType, double>  &ui)
{
    for (auto i : ui) { // each unit type is collectively stored in this unit map.

        UnitType the_worst_unit = getDistributedProduct(i.first);

        if (the_worst_unit != UnitTypes::None) {
            //Add it to the simply map tallying enemy units.
            auto found = unseen_units_.find(the_worst_unit);
            if (found != unseen_units_.end())
                unseen_units_[the_worst_unit] += 1.0 / static_cast<double>(max(the_worst_unit.buildTime(), 1)) * i.second; // there could be multiple producers.
            else
                unseen_units_.insert({ the_worst_unit , 1.0 / static_cast<double>(max(the_worst_unit.buildTime(),1)) });
        }
    }
}


void PlayerModel::considerWorkerUnseenProducts(const UnitInventory & ui)
{
    for (auto i : ui.unit_map_) { // each unit is individually stored in this unit map.

        UnitType the_worst_unit = UnitTypes::None;
        //Workers are made automatically.
        if (i.second.type_.isResourceDepot()) {
            the_worst_unit = i.second.type_.getRace().getWorker();
        }

        if (the_worst_unit != UnitTypes::None) {
            //Add it to the simply map tallying enemy units.
            auto found = unseen_units_.find(the_worst_unit);
            if (found != unseen_units_.end())
                unseen_units_[the_worst_unit] += 1.0 / static_cast<double>(max(the_worst_unit.buildTime(), 1));
            else
                unseen_units_.insert({ the_worst_unit , 1.0 / static_cast<double>(max(the_worst_unit.buildTime(),1)) });
        }
    }
}

void PlayerModel::considerWorkerUnseenProducts(const map<UnitType, double>& ui)
{
    for (auto i : ui) {  // each unit type is collectively stored in this unit map.

        UnitType the_worst_unit = UnitTypes::None;
        //Workers are made automatically.
        if (i.first.isResourceDepot()) {
            the_worst_unit = i.first.getRace().getWorker();
        }

        if (the_worst_unit != UnitTypes::None) {
            //Add it to the simply map tallying enemy units.
            auto found = unseen_units_.find(the_worst_unit);
            if (found != unseen_units_.end())
                unseen_units_[the_worst_unit] += 1.0 / static_cast<double>(max(the_worst_unit.buildTime(), 1)) * i.second; // there could be multiple producers.
            else
                unseen_units_.insert({ the_worst_unit , 1.0 / static_cast<double>(max(the_worst_unit.buildTime(),1)) });
        }
    }
}

//Assumes each unit produces the a random output of whatever type it can make. This does not include depots producing workers.
UnitType PlayerModel::getDistributedProduct(const UnitType &ut) {
    map< UnitType, double> value_holder_;
    vector<UnitType> unittype_holder_;
    // These are possible troop expenditures. find the "worst" one they could make.
    if (ut == UnitTypes::Zerg_Larva || ut.isWorker() || ut == UnitTypes::Protoss_High_Templar || ut == UnitTypes::Protoss_Dark_Templar || ut == UnitTypes::Zerg_Hydralisk || ut == UnitTypes::Zerg_Mutalisk || ut == UnitTypes::Zerg_Creep_Colony || ut.isAddon() ) {
        //Do nothing.
    }
    else if (ut.producesLarva()) {
        for (auto p : UnitTypes::Zerg_Larva.buildsWhat()) {
            if (opponentHasRequirements(p) && CUNYAIModule::isFightingUnit(p) && !p.isAddon()) {
                value_holder_.insert({ p, StoredUnit(p).stock_value_ / static_cast<double>(p.buildTime()) }); // assume the largest of these. (worst for me, risk averse).
                unittype_holder_.push_back(p);
            }
        }
    }
    else {
        for (auto p : ut.buildsWhat()) {
            if (opponentHasRequirements(p) && CUNYAIModule::isFightingUnit(p) && !p.isAddon()) {
                value_holder_.insert({ p, StoredUnit(p).stock_value_ / static_cast<double>(p.buildTime()) }); // assume the largest of these. (worst for me, risk averse).
                unittype_holder_.push_back(p);
            }
        }
    }

    //Assume they're making that one.
    //double max_value = 0.0;
    UnitType the_worst_unit = UnitTypes::None;
    //for (auto v : value_holder_) {
    //    if (v.second > max_value) {
    //        the_worst_unit = v.first;
    //    }
    //}

    if (!unittype_holder_.empty()) {
        std::random_device rd;  //Will be used to obtain a seed for the random number engine
        std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<size_t> rand_unit(0, unittype_holder_.size() - 1);
        the_worst_unit = unittype_holder_[rand_unit(gen)];
    }

    return the_worst_unit;
}

vector<UnitType> PlayerModel::findAlternativeProducts(const UnitType & ut)
{
    vector<UnitType> possible_products;

    UnitType creatorUnit = ut.whatBuilds().first;
    // These are possible troop expenditures. find the "worst" one they could make.
    if (creatorUnit == UnitTypes::Zerg_Larva) {
        for (auto p : UnitTypes::Zerg_Larva.buildsWhat()) {
            if (opponentHasRequirements(p) && CUNYAIModule::isFightingUnit(p)) {
                possible_products.push_back(p); // assume the largest of these. (worst for me, risk averse).
            }
        }
    }
    else {
        for (auto p : creatorUnit.buildsWhat()) {
            if (opponentHasRequirements(p) && CUNYAIModule::isFightingUnit(p)) {
                possible_products.push_back(p); // assume the largest of these. (worst for me, risk averse).
            }
        }
    }

    return vector<UnitType>();
}

bool PlayerModel::opponentHasRequirements(const UnitType &ut)
{
    // only tech-requiring unit is the lurker. If they don't have lurker aspect they can't get it.
    if (ut.requiredTech() == TechTypes::Lurker_Aspect && !researches_.getTech().at(TechTypes::Lurker_Aspect)) return false;
    
    for (auto u : ut.requiredUnits()) {
        bool unit_present_but_unseen = CUNYAIModule::enemy_player_model.countUnseenUnits(u.first) + CUNYAIModule::enemy_player_model.researches_.countResearchBuildings(u.first) >= u.second;
        bool has_necessity = (CUNYAIModule::countUnits(u.first, CUNYAIModule::enemy_player_model.units_) + unit_present_but_unseen);
        if (u.first == UnitTypes::Zerg_Larva || u.first.isResourceDepot() || has_necessity) continue; // if you have the requirements, keep going.
        return false; // If you do not, you do not and exit.
    }
    return true;
}

bool PlayerModel::opponentHasRequirements(const TechType &tech)
{
    if (tech.whatResearches() != UnitTypes::Zerg_Larva && !tech.whatResearches().isResourceDepot() && CUNYAIModule::countUnits(tech.whatResearches(), CUNYAIModule::enemy_player_model.units_) == 0) return false;
    return true;
}

bool PlayerModel::opponentHasRequirements(const UpgradeType &up)
{
    if (up.whatUpgrades() != UnitTypes::Zerg_Larva && !up.whatUpgrades().isResourceDepot() && CUNYAIModule::countUnits(up.whatUpgrades(), CUNYAIModule::enemy_player_model.units_) == 0) return false;
    return true;
}


bool PlayerModel::opponentCouldBeUpgrading(const UpgradeType &up)
{
    // If they don't have it or it could be further created...
    if (!CUNYAIModule::enemy_player_model.researches_.getUpLevel(up) || CUNYAIModule::enemy_player_model.researches_.getUpLevel(up) < up.maxRepeats()) {
        return true;
    }
    return false;
}

bool PlayerModel::opponentCouldBeTeching(const TechType &tech)
{
    // If they have it, they're not building it...
    if (CUNYAIModule::enemy_player_model.researches_.hasTech(tech)) {
        return false;
    }
    return true;
}



// Tallies up my units for rapid counting.
void PlayerModel::updateUnit_Counts() {
    vector <UnitType> already_seen;
    vector <int> unit_count_temp;
    vector <int> unit_incomplete_temp;
    for (auto const & u_iter : units_.unit_map_) { // should only search through unit types not per unit.
        UnitType u_type = u_iter.second.type_;
        bool new_unit_type = find(already_seen.begin(), already_seen.end(), u_type) == already_seen.end();
        if (new_unit_type) {
            int found_units = CUNYAIModule::countUnits(u_type, units_);
            int incomplete_units = CUNYAIModule::countUnitsInProgress(u_type, units_);
            already_seen.push_back(u_type);
            unit_count_temp.push_back(found_units);
            unit_incomplete_temp.push_back(incomplete_units);
        }
    }

    unit_type_ = already_seen;
    unit_count_ = unit_count_temp;
    unit_incomplete_ = unit_incomplete_temp;
}


const double PlayerModel::getCumArmy()
{
    return average_army_;
}

const double PlayerModel::getCumEco()
{
    return average_econ_;
}

const double PlayerModel::getCumTech()
{
    return average_tech_;
}

const double PlayerModel::getNetWorth()
{
    return estimated_net_worth_;
}

void PlayerModel::decrementUnseenWorkers()
{
    estimated_unseen_workers_--;
}

void PlayerModel::updatePlayerAverageCD()
{
    int time = Broodwar->getFrameCount();
    if (time > 0) {
        average_army_ = static_cast<double>(average_army_ * (time - 1) + spending_model_.getParameter(BuildParameterNames::ArmyAlpha)) / static_cast<double>(time);
        average_econ_ = static_cast<double>(average_econ_ * (time - 1) + spending_model_.getParameter(BuildParameterNames::EconAlpha)) / static_cast<double>(time);
        average_tech_ = static_cast<double>(average_tech_ * (time - 1) + spending_model_.getParameter(BuildParameterNames::TechAlpha)) / static_cast<double>(time);
    }
}

void PlayerModel::Print_Average_CD(const int & screen_x, const int & screen_y)
{
    Broodwar->drawTextScreen(screen_x, screen_y, "CD_History:");  //
    Broodwar->drawTextScreen(screen_x, screen_y + 10, "Army: %.2g", average_army_);
    Broodwar->drawTextScreen(screen_x, screen_y + 20, "Econ: %.2g", average_econ_);
    Broodwar->drawTextScreen(screen_x, screen_y + 30, "Tech: %.2g", average_tech_);
}

std::map<UnitType, int> PlayerModel::getCombatUnitCartridge()
{
    return combat_unit_cartridge_;
}

std::map<UnitType, int> PlayerModel::getEcoUnitCartridge()
{
    return eco_unit_cartridge_;
}

std::map<UnitType, int> PlayerModel::getBuildingCartridge()
{
    return building_cartridge_;
}

std::map<UpgradeType, int> PlayerModel::getUpgradeCartridge()
{
    return upgrade_cartridge_;
}

std::map<TechType, int> PlayerModel::getTechCartridge()
{
    return tech_cartridge_;
}

bool PlayerModel::dropBuildingType(UnitType u)
{
    return building_cartridge_.erase(u);
}

bool PlayerModel::dropUnitType(UnitType u)
{
    return combat_unit_cartridge_.erase(u);
}

double PlayerModel::getEstimatedUnseenArmy()
{
    return this->estimated_unseen_army_;
}

double PlayerModel::getEstimatedUnseenFliers()
{
    return this->estimated_unseen_flyers_;
}

double PlayerModel::getEstimatedUnseenGround()
{
    return this->estimated_unseen_ground_;
}

double PlayerModel::getEstimatedUnseenTech()
{
    return this->estimated_unseen_tech_;
}

double PlayerModel::getEstimatedUnseenWorkers()
{
    return this->estimated_unseen_workers_;
}

double PlayerModel::getEstimatedWorkers()
{
    return this->estimated_workers_;
}

int PlayerModel::getFirstAirSeen()
{
    return firstAirThreatSeen_;
}

int PlayerModel::getFirstDetectorSeen()
{
    return firstDetectorSeen_;
}

Player PlayerModel::getPlayer()
{
    return this->bwapi_player_;
}
