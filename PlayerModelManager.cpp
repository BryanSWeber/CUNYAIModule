#pragma once
#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Research_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\CobbDouglas.h"
#include <fstream>
#include <iomanip>
#include <limits>

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
    int estimated_worker_stock = static_cast<int>(round(estimated_workers_) * worker_value);

    evaluateCurrentWorth();

    spending_model_.estimateCD(units_.stock_fighting_total_, researches_.research_stock_, estimated_worker_stock);
};

void Player_Model::updateSelfOnFrame(const Player_Model & target_player)
{
    bwapi_player_ = Broodwar->self();

    //Update Enemy Units
    //Update friendly unit inventory.
    updateUnit_Counts();
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
    map<UnitType, int> air_test_2 = { { UnitTypes::Zerg_Guardian, INT_MIN } ,{ UnitTypes::Zerg_Lurker, INT_MIN } }; // Noting there is no such thing as splash damage, these units have identical costs and statistics.
    u_relatively_weak_against_air_ = (bool)(CUNYAIModule::returnOptimalUnit(air_test_1, researches_) == UnitTypes::Zerg_Spore_Colony);
    e_relatively_weak_against_air_ = (bool)(CUNYAIModule::returnOptimalUnit(air_test_2, researches_) == UnitTypes::Zerg_Guardian);

    //Update map inventory
    radial_distances_from_enemy_ground_ = Map_Inventory::getRadialDistances(units_, CUNYAIModule::current_map_inventory.map_out_from_enemy_ground_);
    closest_radial_distance_enemy_ground_ = *std::min_element(radial_distances_from_enemy_ground_.begin(), radial_distances_from_enemy_ground_.end());

};


void Player_Model::evaluateWorkerCount() {

    if (Broodwar->getFrameCount() == 0) {
        estimated_workers_ = 4;
    }
    else {
        //inventory.estimated_enemy_workers_ *= exp(rate_of_worker_growth); // exponential growth.
        estimated_workers_ += max(units_.resource_depot_count_, 1) / static_cast<double>(UnitTypes::Zerg_Drone.buildTime());
        estimated_workers_ = min(estimated_workers_, static_cast<double>(85)); // there exists a maximum reasonable number of workers.
    }
    int est_worker_count = min(max(units_.worker_count_, static_cast<int>(round(estimated_workers_))), 85);


}


void Player_Model::playerStock(Player_Model & enemy_player_model)
{
	enemy_player_model.units_.inventoryCopy[25] = static_cast<int>(enemy_player_model.spending_model_.worker_stock);
	enemy_player_model.units_.inventoryCopy[26] = static_cast<int>(enemy_player_model.spending_model_.army_stock);
	enemy_player_model.units_.inventoryCopy[27] = static_cast<int>(enemy_player_model.spending_model_.tech_stock);
}


void Player_Model::readPlayerLog(Player_Model & enemy_player_model) // Function that reads in previous game's data
{
	string data;
	int index = 0;
	int iteration = 0;
	ifstream infile(".\\bwapi-data\\write\\" + Broodwar->enemy()->getName() + ".txt", ios_base::in);

	int minStockCounter[29];
	int maxStockCounter[29];
	int minTimeCounter[29];
	int maxTimeCounter[29];

	fill_n(minStockAverage, 29, 0);
	fill_n(minTimeAverage, 29, 0);
	fill_n(maxTimeAverage, 29, 0);
	fill_n(maxStockAverage, 29, 0);
	fill_n(minStockCounter, 29, 0);
	fill_n(minTimeCounter, 29, 0);
	fill_n(maxStockCounter, 29, 0);
	fill_n(maxTimeCounter, 29, 0);

	int numoflines = 0;
	getline(infile, data); //Skip 1 line


	infile.clear();
	infile.seekg(std::ios::beg); // Move the start position to the second line

	infile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');//ignore first line

	while (infile >> data) // Read in the data
	{

		if (data == ",")
			infile >> data;

		stringstream conversion(data);
		conversion >> oldMinStock[index];

		if (oldMinStock[index] != 0)
		{
			minStockAverage[index] += oldMinStock[index];
			minStockCounter[index]++;
		}

		conversion.str(std::string());
		conversion.clear();
		infile >> data;
		infile >> data;
		stringstream conversion2(data);
		conversion2 >> oldMinTime[index];

		if (oldMinTime[index] != 0)
		{
			minTimeAverage[index] += oldMinTime[index];
			minTimeCounter[index] = minTimeCounter[index] + 1;
		}

		conversion2.str(std::string());
		conversion2.clear();
		infile >> data;
		infile >> data;
		stringstream conversion3(data);
		conversion3 >> oldMaxStock[index];
		conversion3.str(std::string());
		conversion3.clear();

		if (oldMaxStock[index] != 0)
		{
			maxStockAverage[index] += oldMaxStock[index];
			maxStockCounter[index] = maxStockCounter[index] + 1;
			cout << maxStockCounter[index] << endl;
		}
		infile >> data;
		infile >> data;
		stringstream conversion4(data);
		conversion4 >> oldMaxTime[index];
		conversion4.str(std::string());
		conversion4.clear();

		if (oldMaxTime[index] != 0)
		{
			maxTimeAverage[index] += oldMaxTime[index];
			maxTimeCounter[index]++;
		}

		index++;

		if (index == 29 && !infile.eof())
			index = 0;

		iteration++;
	}

	for (int i = 0; i < 29; i++)
	{
		if (minStockCounter[i] > 0)
			minStockAverage[i] /= minStockCounter[i];
		if (minTimeCounter[i] > 0)
			minTimeAverage[i] /= minTimeCounter[i];
		if (maxStockCounter[i] > 0)
			maxStockAverage[i] /= maxStockCounter[i];
		if (maxTimeCounter[i] > 0)
			maxTimeAverage[i] /= maxTimeCounter[i];
		cout << "avg is " << minStockAverage[i] << endl;
	}

}
void Player_Model::writePlayerLog(Player_Model & enemy_player_model, bool gameComplete) { //Function that records a player's noticed inventory

	//Initialize all unit inventories seen to -1
	if (Broodwar->getFrameCount() == 1)
		for (int i = 0; i < 29; i++)
			enemy_player_model.minTime[i] = 0;

	for (int i = 0; i < 29; i++)
	{
		if (enemy_player_model.units_.inventoryCopy[i] > 0 && enemy_player_model.minTime[i] == 0)
		{
			enemy_player_model.minTime[i] = Broodwar->elapsedTime();
			enemy_player_model.minStock[i] = enemy_player_model.units_.inventoryCopy[i];
		}
		if (enemy_player_model.units_.inventoryCopy[i] > enemy_player_model.maxStock[i])
		{
			enemy_player_model.maxStock[i] = enemy_player_model.units_.inventoryCopy[i];
			enemy_player_model.maxTime[i] = Broodwar->elapsedTime();
		}
	}

			//Write to file once at the end of the game
			if (gameComplete)
			{
				ifstream inFile(".\\bwapi-data\\write\\" + Broodwar->enemy()->getName() + ".txt", ios_base::in);
				if (!inFile)
				{
					ofstream enemyData;
					enemyData.open(".\\bwapi-data\\write\\" + Broodwar->enemy()->getName() + ".txt", ios_base::app);
					for (int i = 0; i < 29; i++)
							enemyData << left << setw(30) << enemy_player_model.units_.unitInventoryLabel[i];
					enemyData << endl;
				}
				ofstream earliestDate;
				earliestDate.open(".\\bwapi-data\\write\\" + Broodwar->enemy()->getName() + ".txt", ios_base::app);

				for (int i = 0; i < 29; i++)
				{
					stringstream ss;
					ss << enemy_player_model.minStock[i] << " , " << enemy_player_model.minTime[i] << " , " << enemy_player_model.maxStock[i] << " , " << enemy_player_model.maxTime[i];
					earliestDate << left << setw(30) << ss.str();
				}
				earliestDate << endl;
				earliestDate.close();
			}
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
    for (auto const & u_iter : units_.unit_inventory_) { // should only search through unit types not per unit.
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
void Player_Model::setLockedOpeningValuesLingRush() {
     
    // sample command set to explore zergling rushing.
     spending_model_.alpha_army = CUNYAIModule::alpha_army_original = 0.90;
     spending_model_.alpha_econ = CUNYAIModule::alpha_econ_original = 0.10;
     spending_model_.alpha_tech = CUNYAIModule::alpha_tech_original = 0.05;

     CUNYAIModule::delta = 0.00;
     CUNYAIModule::gamma = 0.55;
     CUNYAIModule::buildorder = Building_Gene("drone pool drone drone ling ling ling overlord");

    //unit cartridges (while these only are relevant for CUNYBot, they are still  passed to all players anyway by default on construction), Combat unit cartridge is all mobile noneconomic units we may consider building (excludes static defense).
    combat_unit_cartridge_ = { { UnitTypes::Zerg_Zergling , INT_MIN } };
    //eco_unit_cartridge_ = // Don't change this unless you plan on changing races. Needs some more time to correct, also.
    building_cartridge_ = { { UnitTypes::Zerg_Hatchery, INT_MIN }, { UnitTypes::Zerg_Spawning_Pool, INT_MIN } , {UnitTypes::Zerg_Evolution_Chamber, INT_MIN},{ UnitTypes::Zerg_Queens_Nest, INT_MIN },{ UnitTypes::Zerg_Lair, INT_MIN }, { UnitTypes::Zerg_Hive, INT_MIN } };
    upgrade_cartridge_ = { { UpgradeTypes::Zerg_Carapace, INT_MIN } ,{ UpgradeTypes::Zerg_Melee_Attacks, INT_MIN },{ UpgradeTypes::Pneumatized_Carapace, INT_MIN },{ UpgradeTypes::Metabolic_Boost, INT_MIN }, { UpgradeTypes::Adrenal_Glands, INT_MIN } };
    tech_cartridge_ = {  };
    
}

// Generic command set for locked values
// Must pass cartridges, every other parameter can be left to default (passing builds in is currently bugged)
void Player_Model::setLockedOpeningValues(const map<UnitType, int>& unit_cart, const map<UnitType, int>& building_cart, const map<UpgradeType, int>& upgrade_cart, const map<TechType, int>& tech_cart, 
                                          const string& build, const int& a_army, const int& a_econ, const int& a_tech, const int& delta, const int& gamma) {

    if (a_army)
        spending_model_.alpha_army = CUNYAIModule::alpha_army_original = a_army;
    if (a_econ)
        spending_model_.alpha_econ = CUNYAIModule::alpha_econ_original = a_econ;
    if (a_tech)
        spending_model_.alpha_tech = CUNYAIModule::alpha_tech_original = a_tech;

    if (delta)
        CUNYAIModule::delta = delta;
    if (gamma)
        CUNYAIModule::gamma = gamma;

    //CUNYAIModule::buildorder = Building_Gene(build);  Some sort of bug currently with this

    //unit cartridges (while these only are relevant for CUNYBot, they are still  passed to all players anyway by default on construction), Combat unit cartridge is all mobile noneconomic units we may consider building (excludes static defense).
    combat_unit_cartridge_ = unit_cart;
    //eco_unit_cartridge_ = // Don't change this unless you plan on changing races. Needs some more time to correct, also.
    building_cartridge_ = building_cart;
    upgrade_cartridge_ = upgrade_cart;
    tech_cartridge_ = tech_cart;

}
