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

    //Update Researches
    researches_.updateResearch(other_player, units_);

    evaluateWorkerCount();
    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    int estimated_worker_stock = static_cast<int>(round(estimated_workers_) * worker_value);

    evaluateCurrentWorth();

    spending_model_.estimateCD(units_.stock_fighting_total_, researches_.research_stock_, estimated_worker_stock);
    updatePlayerAverageCD();
};

void Player_Model::updateSelfOnFrame(const Player_Model & target_player)
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

    //Update Researches
    researches_.updateResearch(Broodwar->self(), units_);

    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    spending_model_.evaluateCD(units_.stock_fighting_total_, researches_.research_stock_ , units_.worker_count_ * worker_value );

    if constexpr (TIT_FOR_TAT_ENGAGED) {

        //Update existing CD functions to more closely mirror opponent. Do every 15 sec or so.
        if (Broodwar->elapsedTime() % 15 == 0 && target_player.units_.stock_fighting_total_ > 0) {
            spending_model_.enemy_mimic(target_player, CUNYAIModule::adaptation_rate);
            //CUNYAIModule::DiagnosticText("Matching expenditures,L:%4.2f to %4.2f,K:%4.2f to %4.2f,T:%4.2f to %4.2f", spending_model_.alpha_econ, target_player.spending_model_.alpha_econ, spending_model_.alpha_army, target_player.spending_model_.alpha_army, spending_model_.alpha_tech, target_player.spending_model_.alpha_army);
        }
        else if (Broodwar->elapsedTime() % 15 == 0 && target_player.units_.stock_fighting_total_ == 0) {
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
    map<UnitType, int> air_test_1 = { { UnitTypes::Zerg_Sunken_Colony, INT_MIN } ,{ UnitTypes::Zerg_Spore_Colony, INT_MIN } };
    map<UnitType, int> air_test_2 = { { UnitTypes::Zerg_Guardian, INT_MIN } ,{ UnitTypes::Zerg_Lurker, INT_MIN } }; // Noting there is no such thing as splash damage, these units have identical costs and statistics.
    u_relatively_weak_against_air_ = (bool)(CUNYAIModule::assemblymanager.testAirWeakness(researches_) == UnitTypes::Zerg_Spore_Colony);
    e_relatively_weak_against_air_ = (bool)(CUNYAIModule::assemblymanager.returnOptimalUnit(air_test_2, researches_) == UnitTypes::Zerg_Guardian);

    //Update map inventory
    radial_distances_from_enemy_ground_ = Map_Inventory::getRadialDistances(units_, CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_, true);
    closest_ground_combatant_ = *std::min_element(radial_distances_from_enemy_ground_.begin(), radial_distances_from_enemy_ground_.end());

};


void Player_Model::evaluateWorkerCount() {

    if (Broodwar->getFrameCount() == 0) {
        estimated_workers_ = 4;
    }
    else {
        auto areas = BWEM::Map::Instance().Areas();
        int count_of_occupied_bases = 0;
        for (auto a : areas) {
            if (!a.Minerals().empty() && !units_.getBuildingInventoryAtArea(a.Id()).unit_map_.empty()) count_of_occupied_bases++;
        }
        count_of_occupied_bases = max(count_of_occupied_bases, 1); // surely, they occupy at least one base.

        double functional_worker_cap = max(units_.resource_depot_count_ * 21, count_of_occupied_bases * 21);// 9 * 2 patches per base + 3 workers on gas = 21 per base max.
        estimated_workers_ += max(units_.resource_depot_count_, 1) / static_cast<double>(UnitTypes::Zerg_Drone.buildTime());
        estimated_workers_ = min(estimated_workers_, min(static_cast<double>(85), functional_worker_cap)); // there exists a maximum reasonable number of workers.
    }
    estimated_workers_ = min(max(static_cast<double>(units_.worker_count_), estimated_workers_), 85.0);

}

void Player_Model::evaluateCurrentWorth()
{
    if (Broodwar->getFrameCount() == 0) {
        estimated_cumulative_worth_ = 50;
    }
    else { // what is the net worth of everything he has bought so far and has reasonably collected?
        int min_expenditures_ = 0;
        int gas_expenditures_ = 0;
        int supply_expenditures_ = 0;

        //collect how much of the enemy you can see.
        for (auto i : units_.unit_map_) {
            min_expenditures_ += i.second.modified_min_cost_;
            gas_expenditures_ += i.second.modified_gas_cost_;
            supply_expenditures_ += i.second.modified_supply_;
        }

        for (auto i : researches_.upgrades_ ) {
            int number_of_times_factor_triggers = (i.second * (i.second + 1)) / 2 - 1;
            min_expenditures_ += i.first.mineralPrice() * i.second + i.first.mineralPriceFactor() * number_of_times_factor_triggers;
            gas_expenditures_ += (i.first.gasPrice() * i.second + i.first.gasPriceFactor() * number_of_times_factor_triggers);
        }
        for (auto i : researches_.tech_) {
            min_expenditures_ += i.first.mineralPrice() * i.second;
            gas_expenditures_ += i.first.gasPrice() * i.second;
        }

        // collect how much of the enemy has died.
        int min_losses_ = 0;
        int gas_losses_ = 0;
        int supply_losses_ = 0;

        for (auto i : casualties_.unit_map_) {

            min_losses_ += i.second.modified_min_cost_;
            gas_losses_ += i.second.modified_gas_cost_;
            supply_losses_ += i.second.modified_supply_;
        }

        //Find the relative rates at which the opponent has been spending these resources.
        double min_proportion = (min_expenditures_ + min_losses_) / static_cast<double>(gas_expenditures_ + gas_losses_ + min_expenditures_ + min_losses_); //minerals per each unit of resources mined.
        double supply_proportion = (supply_expenditures_ + supply_losses_) / static_cast<double>(gas_expenditures_ + gas_losses_ + min_expenditures_ + min_losses_); //Supply bought resource collected- very rough.
        double resources_collected_this_frame = 0.045 * estimated_workers_ * min_proportion + 0.07 * estimated_workers_ * (1 - min_proportion) * 1.25; // If we assign them in the same way they have been assigned over the course of this game...
        // Churchill, David, and Michael Buro. "Build Order Optimization in StarCraft." AIIDE. 2011.  Workers gather minerals at a rate of about 0.045/frame and gas at a rate of about 0.07/frame.
        estimated_cumulative_worth_ += resources_collected_this_frame + resources_collected_this_frame * supply_proportion * 25; // 

        int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
        double observed_current_worth = units_.stock_fighting_total_ + researches_.research_stock_ + units_.worker_count_ * worker_value;
        estimated_net_worth_ = max(observed_current_worth, estimated_cumulative_worth_ - min_losses_ - gas_losses_ - supply_losses_);
    }
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
