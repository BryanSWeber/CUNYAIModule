#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\CobbDouglas.h"

using namespace std;
using namespace BWAPI;

void Player_Model::updateOtherOnFrame(const Player & other_player)
{
    bwapi_player_ = other_player;
    enemy_race_ = bwapi_player_->getRace();

    //Update Enemy Units
    units_.updateUnitsControlledBy(other_player);
    units_.purgeBrokenUnits();
    units_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(other_player, units_);

    evaluateWorkerCount();
    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    int estimated_worker_stock = estimated_workers_ * worker_value;

    evaluateCurrentWorth();

    spending_model_.estimateCD(units_.stock_fighting_total_, researches_.research_stock_, estimated_worker_stock);
};

void Player_Model::updateSelfOnFrame(const Player_Model & target_player)
{
    bwapi_player_ = Broodwar->self();
    //Update Enemy Units
    //Update friendly unit inventory.
    if (units_.unit_inventory_.size() == 0) units_ = Unit_Inventory(Broodwar->self()->getUnits()); // if you only do this you will lose track of all of your locked minerals. 
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
    map<UnitType, int> air_test_2 = { { UnitTypes::Zerg_Guardian, INT_MIN } ,{ UnitTypes::Zerg_Lurker, INT_MIN } }; // Maybe two attempts with hydras?  Noting there is no such thing as splash damage, these units have identical costs.
    u_relatively_weak_against_air_ = CUNYAIModule::returnOptimalUnit(air_test_1, researches_) == UnitTypes::Zerg_Spore_Colony;
    e_relatively_weak_against_air_ = CUNYAIModule::returnOptimalUnit(air_test_2, researches_) == UnitTypes::Zerg_Guardian;

};


void Player_Model::evaluateWorkerCount() {

    if (Broodwar->getFrameCount() == 0) {
        estimated_workers_ = 4;
    }
    else {
        //inventory.estimated_enemy_workers_ *= exp(rate_of_worker_growth); // exponential growth.
        estimated_workers_ += max(units_.resource_depot_count_, 1) * 1 / (double)(UnitTypes::Zerg_Drone.buildTime());
        estimated_workers_ = min(estimated_workers_, (double)85); // there exists a maximum reasonable number of workers.
    }
    int est_worker_count = min(max((double)units_.worker_count_, estimated_workers_), (double)85);

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
        for (auto i : units_.unit_inventory_) {
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

        for (auto i : casualties_.unit_inventory_) {

            min_losses_ += i.second.modified_min_cost_;
            gas_losses_ += i.second.modified_gas_cost_;
            supply_losses_ += i.second.modified_supply_;
        }

        //Find the relative rates at which the opponent has been spending these resources.
        double min_proportion = (min_expenditures_ + min_losses_) / (double)(gas_expenditures_ + gas_losses_ + min_expenditures_ + min_losses_); //minerals per each unit of resources mined.
        double supply_proportion = (supply_expenditures_ + supply_losses_) / (double)(gas_expenditures_ + gas_losses_ + min_expenditures_ + min_losses_); //Supply bought resource collected- very rough.
        double resources_collected_this_frame = 0.045 * estimated_workers_ * min_proportion + 0.07 * estimated_workers_ * (1 - min_proportion) * 1.25; // If we assign them in the same way they have been assigned over the course of this game...
        // Churchill, David, and Michael Buro. "Build Order Optimization in StarCraft." AIIDE. 2011.  Workers gather minerals at a rate of about 0.045/frame and gas at a rate of about 0.07/frame.
        estimated_cumulative_worth_ += resources_collected_this_frame + resources_collected_this_frame * supply_proportion * 25; // 

        int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
        double observed_current_worth = units_.stock_fighting_total_ + researches_.research_stock_ + units_.worker_count_ * worker_value;
        estimated_net_worth_ = max(observed_current_worth, estimated_cumulative_worth_ - min_losses_ - gas_losses_ - supply_losses_);
    }
}
