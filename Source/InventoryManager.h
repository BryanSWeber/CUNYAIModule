#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"


using namespace std;
using namespace BWAPI;


//Might benifit from seperatating maps entirely.
//Several types of maps:
// A)Buildable positions, basic map no dependencies.
// B)Unwalkable barriers, basic map no dependencies.
// Unwalkable barriers w buildings. Depends on unwalkable barriers.
// Smoothed barriers, depends on unwalkable barriers.
// map_veins_, depends on unwalkable barriers WITH buildings.
// Map veins in and out from enemy - depends on Unwalkable barriers. Does not depend on buildings.

struct Inventory {
    Inventory();
    Inventory( const Unit_Inventory &ui, const Resource_Inventory &ri );

    Position screen_position_;
    double ln_army_stock_;
    double ln_tech_stock_;
    double ln_worker_stock_;

    double ln_supply_remain_;
    double ln_supply_total_;
    double ln_gas_total_;
    double ln_min_total_;

    int gas_workers_; // sometimes this count may be off by one when units are in the geyser.
    int min_workers_;
    int min_fields_;
    int hatches_;
    int last_gas_check_;
    int my_portion_of_the_map_;
    int estimated_enemy_workers_;
    int map_x;
    int map_y;

	vector<Position> start_positions_;
	vector<TilePosition> expo_positions_;
    vector<TilePosition> expo_positions_complete_;
    Position enemy_base_;
    Position home_base_;
    vector< UnitType > unit_type_;
    vector< int > unit_count_;
    vector< int > unit_incomplete_;
    // treatment order is as follows unwalkable->smoothed->veins->map veins from/to bases.
    vector< vector<bool> > buildable_positions_ ; // buildable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_with_buldings_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > smoothed_barriers_; // unwalkablity+buffer >= 1, otherwise 0. Totally cool idea but a trap. Base nothing off this.
    vector< vector<int> > map_veins_; //updates for building locations 1 if blocked, 1+ if otherwise. Veins decend from a value of 300.
    vector< vector<int> > map_veins_out_from_main_; // distance from our own main.
    vector< vector<int> > map_veins_out_from_enemy_; // distance from enemy base.
    vector< vector<int> > base_values_;
    vector< vector<int> > map_chokes_;

    int vision_tile_count_;
    int est_enemy_stock_;

    TilePosition next_expo_;
	bool cleared_all_start_positions_;
    bool checked_all_expo_positions_ = false;
    int workers_clearing_;
    int workers_distance_mining_;

    int frames_since_unwalkable = 0;
    int frames_since_map_veins = 0;
    int frames_since_home_base = 0;
    int frames_since_enemy_base = 0;

    // Counts my units so I don't have to do this for each unit onframe.
    void updateUnit_Counts(const Unit_Inventory & ui);
    // Updates the (safe) log of net investment in technology.
    void updateLn_Tech_Stock( const Unit_Inventory &ui );
    // Updates the (safe) log of our army stock.
    void updateLn_Army_Stock( const Unit_Inventory &ui );
    // Updates the (safe) log of our worker stock.
    void updateLn_Worker_Stock();

    // Updates the (safe) log of our supply stock.
    void updateLn_Supply_Remain();
    // Updates the (safe) log of our supply total.
    void updateLn_Supply_Total();
    // Updates the (safe) log of our gas total.
    void updateLn_Gas_Total();
    // Updates the (safe) log of our min total.
    void updateLn_Min_Total();
    // Updates the count of our vision total, in tiles
    void updateVision_Count();
    // Updates our screen poisition. A little gratuitous but nevertheless useful.
    void updateScreen_Position();
    // Updates the (safe) log gas ratios, ln(gas)/(ln(min)+ln(gas))
    double getLn_Gas_Ratio();
    // Updates the (safe) log of our supply total. Returns very high int instead of infinity.
    double getLn_Supply_Ratio();

    // Updates the count of our gas workers.
    void Inventory::updateGas_Workers();
    // Updates the count of our min workers.
    void Inventory::updateMin_Workers();

    // Updates the number of mineral fields we "possess".
    void Inventory::updateMin_Possessed(const Resource_Inventory & ri);

    // Updates the number of hatcheries (and decendents).
    void Inventory::updateHatcheries();

    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void Inventory::updateBuildablePos();
    // Updates the unwalkable portions of the map.
    void Inventory::updateUnwalkable();
    // Updates unwalkable portions with existing blockades. Currently flawed.
    void Inventory::updateUnwalkableWithBuildings(const Unit_Inventory & ui, const Unit_Inventory & ei, const Resource_Inventory & ri, const Unit_Inventory & ni);

    // Marks and smooths the edges of the map. Dangerous- In progress.
    void Inventory::updateSmoothPos();
    // Marks the main arteries of the map. Requires updateunwalkablewithbuildings.
    void Inventory::updateMapVeins(); 


    // Updates the visible map arteries. Only checks buildings.
    //void Inventory::updateLiveMapVeins( const Unit & building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri );
    //void Inventory::updateLiveMapVeins( const Unit_Inventory & ui, const Unit_Inventory & ei, const Resource_Inventory & ri );
    // Updates the chokes on the map.
    //void Inventory::updateMapChokes(); //in progress
    // Updates veins going out of the main base for attacking ease.
    void Inventory::updateMapVeinsOutFromMain( const Position center );
    // Updates veins going out of the enemy base for attacking ease.
    void Inventory::updateMapVeinsOutFromFoe( const Position center );
    // Gets distance using
    int Inventory::getDifferentialDistanceOutFromEnemy( const Position A, const Position B ) const;
    int Inventory::getRadialDistanceOutFromEnemy(const Position A ) const;
    int Inventory::getDifferentialDistanceOutFromHome(const Position A, const Position B ) const;
    int Inventory::getRadialDistanceOutFromHome(const Position A ) const;
    bool Inventory::checkViableGroundPath(const Position A, const Position B) const;
    // Marks and scores base locations.
    void Inventory::updateBaseLoc( const Resource_Inventory &ri );
    void Inventory::updateWorkersClearing( Unit_Inventory & ui, Resource_Inventory & ri); // updates number of workers clearing.
    void Inventory::updateWorkersLongDistanceMining(Unit_Inventory & ui, Resource_Inventory & ri); // updates number of workers distance mining.

    // Returns the position of the weakest base.

    Position Inventory::getWeakestBase(const Unit_Inventory &ei) const;
    // Returns the Position of the strongest base.
    Position Inventory::getStrongestBase(const Unit_Inventory & ei) const;

	// updates the next target expo.
	void Inventory::getExpoPositions();
	// Changes the next expo to X:
	void Inventory::setNextExpo(const TilePosition tp);

    //Visualizations
    void Inventory::drawExpoPositions() const;
    void Inventory::drawBasePositions() const;

	// Adds start positions to inventory object.
	void Inventory::getStartPositions();
	// Updates map positions and removes all visible ones;
	void Inventory::updateStartPositions(const Unit_Inventory &ei);


    // Calls most of the map update functions when needed at a reduced and somewhat reasonable rate.
    void updateEnemyBasePosition(Unit_Inventory & ui, Unit_Inventory & ei, const Resource_Inventory & ri, const Unit_Inventory & ni);

}; 
