#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"
#include "Unit_Inventory.h"

using namespace std;
using namespace BWAPI;

struct Inventory {
    Inventory();
    Inventory( const Unit_Inventory &ui );

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

    int min_reserve_;
    int gas_reserve_;
    int building_timer_;


    vector<BWAPI::Position> resource_positions_;
    vector< vector<bool> > buildable_positions_ ;
    vector< vector<int> > smoothed_barriers_;
    vector< vector<int> > base_values_;

    int vision_tile_count_;
    int est_enemy_stock_;

    TilePosition next_expo_;
    bool acceptable_expo_;

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

    // Updates the static locations of minerals and gas on the map. Should only be called on game start.
    void Inventory::updateMineralPos();
    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void Inventory::updateBuildablePos();
    // Marks and smooths the edges of the map.
    void Inventory::updateSmoothPos();
    // Marks and scores base locations.
    void Inventory::updateBaseLoc();
    // Updates mineral, gas, and time reserves.
    void Inventory::updateReserveSystem();
    // updates the next target expo.
    void Inventory::updateNextExpo( const Unit_Inventory &e_inv, const Unit_Inventory &u_inv );
}; 
