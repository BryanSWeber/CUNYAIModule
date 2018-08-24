#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\CobbDouglas.h"

using namespace std;
using namespace BWAPI;

void Player_Model::updateOnFrame(const Player & enemy_player)
{


    //Update Enemy Units
    units_.updateUnitsControlledByOthers();
    units_.purgeBrokenUnits();
    units_.updateUnitInventorySummary();

    //Update Researches
    researches_.updateResearch(enemy_player, units_);

    evaluateWorkerCount();
    int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
    int estimated_worker_stock = estimated_workers_ * worker_value;

    spending_model_.estimateCD(units_.stock_fighting_total_, researches_.research_stock_, estimated_worker_stock);
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

};