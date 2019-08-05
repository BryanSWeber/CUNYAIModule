#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\CobbDouglas.h"
#include <numeric>

using namespace std;
using namespace BWAPI;


void Player_Model::updateOtherOnFrame(const Player & other_player)
{
    bwapi_player_ = other_player;
    //Update Enemy Units
    units_.updateUnitsControlledBy(other_player);
    units_.purgeBrokenUnits();
    units_.updateUnitInventorySummary();
    casualties_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(other_player, units_);

    evaluatePotentialWorkerCount();
    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    int estimated_worker_stock = static_cast<int>(round(estimated_workers_) * worker_value);
    //if (other_player->isEnemy(Broodwar->self())) Broodwar->printf("%3.0f, %3.3f", estimated_bases_, estimated_workers_);
    evaluatePotentialArmyExpenditures(); // how much is being bought?
    evaluatePotentialTechExpenditures(); // How much is being upgraded/researched?
    evaluateCurrentWorth(); // how much do they appear to have?

    spending_model_.estimateCD(units_.stock_fighting_total_ + static_cast<int>(estimated_unseen_army_), researches_.research_stock_ + static_cast<int>(estimated_unseen_tech_), estimated_worker_stock);
    updatePlayerAverageCD();
};

void Player_Model::updateSelfOnFrame()
{
    bwapi_player_ = Broodwar->self();

    //Update Enemy Units
    //Update friendly unit inventory.
    updateUnit_Counts();
    if (units_.unit_map_.size() == 0) units_ = Unit_Inventory(Broodwar->self()->getUnits()); // if you only do this you will lose track of all of your locked minerals. 
    else units_.updateUnitInventory(Broodwar->self()->getUnits()); // safe for locked minerals.
    units_.purgeBrokenUnits();
    units_.purgeUnseenUnits(); //Critical for self!
    units_.updateUnitInventorySummary();
    casualties_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(Broodwar->self(), units_);

    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    spending_model_.evaluateCD(units_.stock_fighting_total_, researches_.research_stock_ , units_.worker_count_ * worker_value );

    if constexpr (TIT_FOR_TAT_ENGAGED) {

        //Update existing CD functions to more closely mirror opponent. Do every 15 sec or so.
        if ( Broodwar->elapsedTime() % 15 == 0 && building_cartridge_.empty() && units_.stock_fighting_total_ > 0) {
            spending_model_.enemy_mimic(CUNYAIModule::enemy_player_model, CUNYAIModule::adaptation_rate);
            //CUNYAIModule::DiagnosticText("Matching expenditures,L:%4.2f to %4.2f,K:%4.2f to %4.2f,T:%4.2f to %4.2f", spending_model_.alpha_econ, target_player.spending_model_.alpha_econ, spending_model_.alpha_army, target_player.spending_model_.alpha_army, spending_model_.alpha_tech, target_player.spending_model_.alpha_army);
        }
        else if (Broodwar->elapsedTime() % 15 == 0 && units_.stock_fighting_total_ == 0) {
            spending_model_.alpha_army = CUNYAIModule::alpha_army_original;
            spending_model_.alpha_econ = CUNYAIModule::alpha_econ_original;
            spending_model_.alpha_tech = CUNYAIModule::alpha_tech_original;
            //CUNYAIModule::DiagnosticText("Reseting expenditures,%4.2f, %4.2f,%4.2f", spending_model_.alpha_econ, spending_model_.alpha_army, spending_model_.alpha_tech);
        }
    }

    CUNYAIModule::tech_starved = spending_model_.tech_starved();
    CUNYAIModule::army_starved = spending_model_.army_starved();
    CUNYAIModule::econ_starved = spending_model_.econ_starved();

    spending_model_.econ_derivative = spending_model_.econ_derivative;
    spending_model_.army_derivative = spending_model_.army_derivative;
    spending_model_.tech_derivative = spending_model_.tech_derivative;

    //Update general weaknesses.
    u_have_active_air_problem_ = (bool)(CUNYAIModule::assemblymanager.testActiveAirProblem(researches_, true) || (CUNYAIModule::assemblymanager.testPotentialAirVunerability(researches_, false)  && CUNYAIModule::enemy_player_model.units_.stock_fliers_ + CUNYAIModule::enemy_player_model.casualties_.stock_fliers_> 0)) ;
    e_has_air_vunerability_ = (bool)(CUNYAIModule::assemblymanager.testActiveAirProblem(researches_, false) || CUNYAIModule::assemblymanager.testPotentialAirVunerability(researches_, true));

    //Update map inventory
    radial_distances_from_enemy_ground_ = Map_Inventory::getRadialDistances(units_, CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_, true);
    closest_ground_combatant_ = *std::min_element(radial_distances_from_enemy_ground_.begin(), radial_distances_from_enemy_ground_.end());

};


void Player_Model::evaluatePotentialWorkerCount() {

    if (Broodwar->getFrameCount() == 0) {
        estimated_workers_ = 4;
    }
    else {
        int count_of_occupied_bases = 0;
        int bases_in_start_positions = 0;
        for (auto & a : BWEM::Map::Instance().Areas()) {
            bool found_a_base = false;
            if (!units_.getBuildingInventoryAtArea(a.Id()).unit_map_.empty() && !a.Bases().empty() ) {
                count_of_occupied_bases++;
                found_a_base = true;
            }
            if (found_a_base) {
                for (auto start_pos : CUNYAIModule::current_map_inventory.start_positions_) {
                    if (BWEM::Map::Instance().GetArea(TilePosition(start_pos))->Id() == a.Id()) {
                        bases_in_start_positions++;
                    }
                }
            }
        }
        if (bases_in_start_positions == 0 && count_of_occupied_bases > 0 ) count_of_occupied_bases++; // if they have no bases in start positions but have an expansion, they have another base in a start position.
        count_of_occupied_bases = max(count_of_occupied_bases, 1); // surely, they occupy at least one base.
        estimated_bases_ = static_cast<double>(max(units_.resource_depot_count_, count_of_occupied_bases));
        double functional_worker_cap = static_cast<double>(estimated_bases_ * 21);// 9 * 2 patches per base + 3 workers on gas = 21 per base max.

        estimated_workers_ += static_cast<double>( estimated_bases_ ) / static_cast<double>(UnitTypes::Zerg_Drone.buildTime());
        estimated_workers_ = min(estimated_workers_, min(static_cast<double>(85), functional_worker_cap)); // there exists a maximum reasonable number of workers.

    }
    estimated_workers_ = min(max(static_cast<double>(units_.worker_count_), estimated_workers_), 85.0);
}

void Player_Model::evaluatePotentialArmyExpenditures() {
    double value_possible_ = 0;

    double value_possible_per_frame_ = 0;

    //collect how much of the enemy you can see.
    for (auto i : units_.unit_map_) {

        double value_holder_ = 0;

        // These are possible troop expenditures.
        if (i.second.type_ == UnitTypes::Zerg_Larva || i.second.type_.isWorker()) {
            continue;   
        }
        else if (i.second.type_.producesLarva()) {
            for (auto p : UnitTypes::Zerg_Larva.buildsWhat()) {
                if (opponentHasRequirements(p) && CUNYAIModule::IsFightingUnit(p)) {
                    value_holder_ = max(value_holder_, Stored_Unit(p).stock_value_ / static_cast<double>(p.buildTime())); // assume the largest of these. (worst for me, risk averse).

                }
            }

            value_possible_per_frame_ += value_holder_;
            //value_possible_ += value_holder_ * i.second.time_since_last_seen_;
        }
        else {
            for (auto p : i.second.type_.buildsWhat()) {
                if (opponentHasRequirements(p) && CUNYAIModule::IsFightingUnit(p)) {
                    value_holder_ = max(value_holder_, Stored_Unit(p).stock_value_ / static_cast<double>(p.buildTime()) ); // assume the largest of these. (worst for me, risk averse).
                }
            }
            value_possible_per_frame_ += value_holder_;
            //value_possible_ += value_holder_ * i.second.time_since_last_seen_;
        }
    }

    estimated_unseen_army_per_frame_ = value_possible_per_frame_;
    estimated_unseen_army_ += value_possible_per_frame_;
}

void Player_Model::evaluatePotentialTechExpenditures() {
    double min_possible_ = 0;
    double gas_possible_ = 0;
    double supply_possible_ = 0;

    double value_possible_per_frame_ = 0;
    double value_possible_per_unit_ = 0;

    //collect how much of the enemy you can see.
    for (auto i : units_.unit_map_) {

        double value_holder_up_ = 0;
        int time_since_last_benificiary_seen_up_ = INT_MAX;
        bool benificiary_exists_up_ = true;
        int oldest_up_class = 0;


        double value_holder_tech_ = 0;
        bool benificiary_exists_tech_ = false;
        int time_since_last_benificiary_seen_tech_ = INT_MAX;
        int oldest_tech_class_ = 0;

        int max_duration = 0;
        // These are possible upgrade expenditures.
        for (auto p : i.second.type_.upgradesWhat()) {
            if (opponentHasRequirements(p)) { // can they upgrade?
                for (auto j : units_.unit_map_) { 
                    if (j.second.type_.upgrades().contains(p)) { // is there a benifitiary in their inventory?  upgrade does not depend on time last seen but time last dependent unit was seen. 
                        benificiary_exists_up_ = true;
                        time_since_last_benificiary_seen_up_ = min(time_since_last_benificiary_seen_up_, j.second.time_since_last_seen_);

                        int level = 0;
                        if (CUNYAIModule::enemy_player_model.researches_.upgrades_.find(p) != CUNYAIModule::enemy_player_model.researches_.upgrades_.end()) level = CUNYAIModule::enemy_player_model.researches_.upgrades_[p];
                        value_holder_up_ = max( value_holder_up_, p.mineralPrice() / static_cast<double>(p.upgradeTime() + level * p.upgradeTimeFactor())) + 1.25 * (p.gasPrice() / static_cast<double>(p.upgradeTime() + level * p.upgradeTimeFactor()) );
                        oldest_up_class = max(oldest_up_class, time_since_last_benificiary_seen_up_ * benificiary_exists_up_); // we want the youngest benificiary from the oldest class of units.
                        max_duration = max(p.upgradeTime() + level * p.upgradeTimeFactor(), max_duration);
                    };
                }
            }
        }

        // These are possible tech expenditures.
        for (auto p : i.second.type_.researchesWhat()) {
            if (opponentHasRequirements(p)) {
                for (auto j : units_.unit_map_) {
                    for (auto flagged_unit_type : p.whatUses()) {
                        if (j.second.type_ == flagged_unit_type) {
                            benificiary_exists_tech_ = true;
                            time_since_last_benificiary_seen_tech_ = min(time_since_last_benificiary_seen_tech_, j.second.time_since_last_seen_);

                            value_holder_tech_ = max(value_holder_tech_, p.mineralPrice() / static_cast<double>(p.researchTime()) + 1.25 * ( p.gasPrice() / static_cast<double>( p.researchTime() ) ) );
                            oldest_tech_class_ = max(oldest_tech_class_, time_since_last_benificiary_seen_tech_ * benificiary_exists_tech_); // we want the youngest benificiary from the oldest class of units.
                            max_duration = max(p.researchTime(), max_duration);
                        }; 
                    }
                }
            }
        }

        if (!benificiary_exists_tech_) oldest_tech_class_ = 0; // if they've never been seen, they're probably not getting made.
        if (!benificiary_exists_up_) oldest_up_class = 0;

        value_possible_per_frame_ += max(value_holder_up_, value_holder_tech_);
        value_possible_per_unit_ += max(value_holder_up_ * oldest_up_class , value_holder_tech_ * oldest_tech_class_);
    }

    estimated_unseen_tech_ = value_possible_per_unit_;
    estimated_unseen_tech_per_frame_ = value_possible_per_frame_;
}

void Player_Model::evaluateCurrentWorth()
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

        for (auto i : researches_.upgrades_ ) {
            int number_of_times_factor_triggers = max((i.second * (i.second + 1)) / 2 - 1, 0);
            min_expenditures_ += i.first.mineralPrice() * i.second + i.first.mineralPriceFactor() * number_of_times_factor_triggers;
            gas_expenditures_ += (i.first.gasPrice() * i.second + i.first.gasPriceFactor() * number_of_times_factor_triggers);
        }

        for (auto i : researches_.tech_) {
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

        estimated_resources_per_frame_ = estimated_workers_  * (0.045 * min_proportion + 0.07 * (1 - min_proportion) * 1.25); // If we assign them in the same way they have been assigned over the course of this game...
        // Churchill, David, and Michael Buro. "Build Order Optimization in StarCraft." AIIDE. 2011.  Workers gather minerals at a rate of about 0.045/frame and gas at a rate of about 0.07/frame.
        estimated_cumulative_worth_ += max(estimated_resources_per_frame_, estimated_unseen_army_per_frame_ + estimated_unseen_tech_per_frame_); // 

        double min_on_field = min_expenditures_ - min_losses_;
        double gas_on_field = gas_expenditures_ - gas_losses_;
        double supply_on_field = supply_expenditures_ - supply_losses_;

        double observed_current_worth = min_on_field + gas_on_field * 1.25 + supply_on_field * 25;

        estimated_net_worth_ = max(observed_current_worth, estimated_cumulative_worth_ - min_losses_ - gas_losses_ * 1.25 - supply_spent * 25);
    }
}

bool Player_Model::opponentHasRequirements(const UnitType &ut)
{
    // only tech-requiring unit is the lurker. If they don't have lurker aspect they can't get it.
    if (ut.requiredTech() == TechTypes::Lurker_Aspect && !researches_.tech_.at(TechTypes::Lurker_Aspect)) return false;
    for (auto u : ut.requiredUnits()) {
        if (u.first != UnitTypes::Zerg_Larva && !u.first.isResourceDepot() && CUNYAIModule::Count_Units(u.first, CUNYAIModule::enemy_player_model.units_) < u.second) return false;
    }
    return true;
}

bool Player_Model::opponentHasRequirements(const UpgradeType &up)
{
    // If they don't have it or it could be further created...
    if (!CUNYAIModule::enemy_player_model.researches_.upgrades_[up] || CUNYAIModule::enemy_player_model.researches_.upgrades_[up] < up.maxRepeats()) {
        return true;
    }
    return false;
}

bool Player_Model::opponentHasRequirements(const TechType &tech)
{
    // If they have it, they're not building it...
    if (CUNYAIModule::enemy_player_model.researches_.tech_[tech]) {
        return false;
    }
    return true;
}



// Tallies up my units for rapid counting.
void Player_Model::updateUnit_Counts() {
    vector <UnitType> already_seen;
    vector <int> unit_count_temp;
    vector <int> unit_incomplete_temp;
    for (auto const & u_iter : units_.unit_map_) { // should only search through unit types not per unit.
        UnitType u_type = u_iter.second.type_;
        bool new_unit_type = find(already_seen.begin(), already_seen.end(), u_type) == already_seen.end();
        if (new_unit_type) {
            int found_units = CUNYAIModule::Count_Units(u_type, units_);
            int incomplete_units = CUNYAIModule::Count_Units_In_Progress(u_type, units_);
            already_seen.push_back(u_type);
            unit_count_temp.push_back(found_units);
            unit_incomplete_temp.push_back(incomplete_units);
        }
    }

    unit_type_ = already_seen;
    unit_count_ = unit_count_temp;
    unit_incomplete_ = unit_incomplete_temp;
}

// sample command set to explore zergling rushing.
void Player_Model::setLockedOpeningValues() {
     
    // sample command set to explore zergling rushing.
     spending_model_.alpha_army = CUNYAIModule::alpha_army_original = 0.90;
     spending_model_.alpha_econ = CUNYAIModule::alpha_econ_original = 0.10;
     spending_model_.alpha_tech = CUNYAIModule::alpha_tech_original = 0.05;

     CUNYAIModule::delta = 0.00;
     CUNYAIModule::gamma = 0.55;
     CUNYAIModule::buildorder = Building_Gene("drone pool drone drone ling ling ling overlord");

    //This no longer works after declaring the inventories as const.
    //combat_unit_cartridge_ = { { UnitTypes::Zerg_Zergling , INT_MIN } };
    //eco_unit_cartridge_ = // Don't change this unless you plan on changing races. Needs some more time to correct, also.
    //building_cartridge_ = { { UnitTypes::Zerg_Hatchery, INT_MIN }, { UnitTypes::Zerg_Spawning_Pool, INT_MIN } , {UnitTypes::Zerg_Evolution_Chamber, INT_MIN},{ UnitTypes::Zerg_Queens_Nest, INT_MIN },{ UnitTypes::Zerg_Lair, INT_MIN }, { UnitTypes::Zerg_Hive, INT_MIN } };
    //upgrade_cartridge_ = { { UpgradeTypes::Zerg_Carapace, INT_MIN } ,{ UpgradeTypes::Zerg_Melee_Attacks, INT_MIN },{ UpgradeTypes::Pneumatized_Carapace, INT_MIN },{ UpgradeTypes::Metabolic_Boost, INT_MIN }, { UpgradeTypes::Adrenal_Glands, INT_MIN } };
    //tech_cartridge_ = {  };
    
}

void Player_Model::updatePlayerAverageCD()
{
    int time = Broodwar->getFrameCount();
    if (time > 0) {
        average_army_ = static_cast<double>(average_army_ * (time - 1) + spending_model_.alpha_army) / static_cast<double>(time);
        average_econ_ = static_cast<double>(average_econ_ * (time - 1) + spending_model_.alpha_econ) / static_cast<double>(time);
        average_tech_ = static_cast<double>(average_tech_ * (time - 1) + spending_model_.alpha_tech) / static_cast<double>(time);
    }
}

void Player_Model::Print_Average_CD(const int & screen_x, const int & screen_y)
{
            Broodwar->drawTextScreen(screen_x, screen_y, "CD_History:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10, "Army: %.2g", average_army_);
            Broodwar->drawTextScreen(screen_x, screen_y + 20, "Econ: %.2g", average_econ_);
            Broodwar->drawTextScreen(screen_x, screen_y + 30, "Tech: %.2g", average_tech_);
}
