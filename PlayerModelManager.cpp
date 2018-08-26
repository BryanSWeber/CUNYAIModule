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
    //Update Enemy Units
    units_.updateUnitsControlledBy(other_player);
    units_.purgeBrokenUnits();
    units_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(other_player, units_);

    evaluateWorkerCount();
    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    int estimated_worker_stock = estimated_workers_ * worker_value;

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
            //CUNYAIModule::DiagnosticText("Matching expenditures,%4.2f, %4.2f,%4.2f", alpha_econ_temp, alpha_army_temp, alpha_tech_temp);
        }
        else if (Broodwar->elapsedTime() % 15 == 0 && target_player.units_.stock_fighting_total_ == 0) {
            spending_model_.alpha_army = CUNYAIModule::alpha_army_original;
            spending_model_.alpha_econ = CUNYAIModule::alpha_econ_original;
            spending_model_.alpha_tech = CUNYAIModule::alpha_tech_original;
            //CUNYAIModule::DiagnosticText("Reseting expenditures,%4.2f, %4.2f,%4.2f", alpha_econ_temp, alpha_army_temp, alpha_tech_temp);
        }
    }

    CUNYAIModule::tech_starved = spending_model_.tech_starved();
    CUNYAIModule::army_starved = spending_model_.army_starved();
    CUNYAIModule::econ_starved = spending_model_.econ_starved();

    spending_model_.econ_derivative = spending_model_.econ_derivative;
    spending_model_.army_derivative = spending_model_.army_derivative;
    spending_model_.tech_derivative = spending_model_.tech_derivative;
};


void Player_Model::evaluateWorkerCount() {

    if (Broodwar->getFrameCount() == 0) {
        estimated_workers_ = 4;
    }
    else {
        //inventory.estimated_enemy_workers_ *= exp(rate_of_worker_growth); // exponential growth.
        estimated_workers_ += max(units_.resource_depot_count_, 1) * 1 / (double)(UnitTypes::Zerg_Drone.buildTime());
        estimated_workers_ = min(estimated_workers_, 85); // there exists a maximum reasonable number of workers.
    }
    int est_worker_count = min(max(units_.worker_count_, estimated_workers_), 85);

}
