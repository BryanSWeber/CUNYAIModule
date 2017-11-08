#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"


using namespace std;
using namespace BWAPI;

struct Inventory {
    Inventory();
    Inventory( const Unit_Inventory &ui, const Resource_Inventory &ri );

    double ln_army_stock_;
    double ln_tech_stock_;
    double ln_worker_stock_;

    double ln_supply_remain_;
    double ln_supply_total_;
    double ln_gas_total_;
    double ln_min_total_;

    int gas_workers_;
    int min_workers_;
    int min_fields_;
    int hatches_;
    int last_gas_check_;

	vector<Position> start_positions_;
	vector<TilePosition> expo_positions_;

    vector< vector<bool> > buildable_positions_ ;
    vector< vector<int> > smoothed_barriers_;
    vector< vector<int> > map_veins_;
    vector< vector<int> > map_veins_out_;
    vector< vector<int> > base_values_;
    vector< vector<int> > map_chokes_;

    int vision_tile_count_;
    int est_enemy_stock_;

    TilePosition next_expo_;
	bool list_cleared_;

    // Updates the (safe) log of net investment in technology.
    void updateLn_Tech_Stock( const Unit_Inventory &ui );
    // Updates the (safe) log of our army stock.
    void updateLn_Army_Stock( const Unit_Inventory &ui );
    // Updates the (safe) log of our worker stock.
    void updateLn_Worker_Stock();

    // Updates the (safe) log of our supply stock.
    void updateLn_Supply_Remain( const Unit_Inventory &ui );
    // Updates the (safe) log of our supply total.
    void updateLn_Supply_Total();
    // Updates the (safe) log of our gas total.
    void updateLn_Gas_Total();
    // Updates the (safe) log of our min total.
    void updateLn_Min_Total();
    // Updates the count of our vision total, in tiles
    void updateVision_Count();

    // Updates the (safe) log gas ratios, ln(gas)/(ln(min)+ln(gas))
    double getLn_Gas_Ratio();
    // Updates the (safe) log of our supply total. Returns very high int instead of infinity.
    double getLn_Supply_Ratio();

    // Updates the count of our gas workers.
    void Inventory::updateGas_Workers();
    // Updates the count of our min workers.
    void Inventory::updateMin_Workers();

    // Updates the number of mineral fields we "possess".
    void Inventory::updateMin_Possessed();

    // Updates the number of hatcheries (and decendents).
    void Inventory::updateHatcheries( const Unit_Inventory &ui );

    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void Inventory::updateBuildablePos();
    // Marks and smooths the edges of the map.
    void Inventory::updateSmoothPos();
    // Marks the main arteries of the map.
    void Inventory::updateMapVeins();

    // Updates the visible map arteries. Only checks buildings.
    void updateLiveMapVeins( const Unit & building, const Unit_Inventory &ui, const Unit_Inventory &ei );    
    // Updates the chokes on the map.
    void Inventory::updateMapChokes(); //in progress
    // Updates veins going out of the main base for attacking ease.
    void Inventory::updateMapVeinsOut();

    // Marks and scores base locations.
    void Inventory::updateBaseLoc( const Resource_Inventory &ri );
    
	// updates the next target expo.
	void Inventory::getExpoPositions(const Unit_Inventory &e_inv, const Unit_Inventory &u_inv);
	// Changes the next expo to X:
	void Inventory::setNextExpo(const TilePosition tp);

	// Adds start positions to inventory object.
	void Inventory::getStartPositions();
	// Updates map positions and removes all visible ones;
	void Inventory::updateStartPositions();

}; 
