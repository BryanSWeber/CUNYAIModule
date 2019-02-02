#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"
#include "PlayerModelManager.h"


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

struct Map_Inventory {
    Map_Inventory();
    Map_Inventory(const Unit_Inventory &ui, const Resource_Inventory &ri);

    Position screen_position_;

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
    int expo_portion_of_the_map_;
    int estimated_enemy_workers_;
    int map_x;
    int map_y;

    vector<Position> start_positions_;
    vector<TilePosition> expo_positions_;
    vector<TilePosition> expo_positions_complete_;
    Position enemy_base_ground_;
    Position enemy_base_air_;
    Position home_base_;
    Position safe_base_;

    // treatment order is as follows unwalkable->smoothed->veins->map veins from/to bases.
    vector< vector<bool> > buildable_positions_; // buildable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_with_buildings_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > smoothed_barriers_; // unwalkablity+buffer >= 1, otherwise 0. Totally cool idea but a trap. Base nothing off this.
    vector< vector<int> > map_veins_; //updates for building locations 1 if blocked, counts up around blocked squares if otherwise.
    vector< vector<int> > map_out_from_home_; // distance from our own main. 1 if blocked/inaccessable by ground.
    vector< vector<int> > map_out_from_enemy_ground_; // distance from enemy base. 1 if blocked/inaccessable by ground.
    vector< vector<int> > map_out_from_enemy_air_; // distance from enemy base. 1 if blocked/inaccessable by ground.
    vector< vector<int> > map_out_from_safety_; // distance from enemy base. 1 if blocked/inaccessable by ground.
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
    int frames_since_safe_base = 0;
    int frames_since_enemy_base_ground_ = 0;
    int frames_since_enemy_base_air_ = 0;

    //Fields:
    vector< vector<int> > pf_threat_;
    vector< vector<int> > pf_attract_;
    vector< vector<int> > pf_aa_;

    // Updates the (safe) log of net investment in technology.
    void updateLn_Tech_Stock(const Unit_Inventory &ui);
    // Updates the (safe) log of our army stock.
    void updateLn_Army_Stock(const Unit_Inventory &ui);
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
    void Map_Inventory::updateGas_Workers();
    // Updates the count of our min workers.
    void Map_Inventory::updateMin_Workers();

    // Updates the number of mineral fields we "possess".
    void Map_Inventory::updateMin_Possessed(const Resource_Inventory & ri);

    // Updates the number of hatcheries (and decendents).
    void Map_Inventory::updateHatcheries();

    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void Map_Inventory::updateBuildablePos();
    // Updates the unwalkable portions of the map.
    void Map_Inventory::updateUnwalkable();
    // Updates unwalkable portions with existing blockades. Currently flawed.
    void Map_Inventory::updateUnwalkableWithBuildings(const Unit_Inventory & ui, const Unit_Inventory & ei, const Resource_Inventory & ri, const Unit_Inventory & ni);

    // Marks and smooths the edges of the map. Dangerous- In progress.
    void Map_Inventory::updateSmoothPos();
    // Marks the distance from each obstacle. Requires updateunwalkablewithbuildings. //[Old usage:]Marks the main arteries of the map. 
    void Map_Inventory::updateMapVeins();

    // simply gets the map value at a particular position.
    static int getMapValue(const Position &pos, const vector<vector<int>> &map);
    static int getFieldValue(const Position & pos, const vector<vector<int>>& map);

    // Updates the visible map arteries. Only checks buildings.
    //void Map_Inventory::updateLiveMapVeins( const Unit & building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri );
    //void Map_Inventory::updateLiveMapVeins( const Unit_Inventory & ui, const Unit_Inventory & ei, const Resource_Inventory & ri );
    // Updates the chokes on the map.
    //void Map_Inventory::updateMapChokes(); //in progress
    // Updates the spiral counting out from the new_center. Replaces old (map), prints.
    void Map_Inventory::updateMapVeinsOut(const Position & newCenter, Position & oldCenter, vector<vector<int>>& map, const bool &print = true);

    // Gets distance using
    int Map_Inventory::getDifferentialDistanceOutFromEnemy(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutFromEnemy(const Position A) const;
    int Map_Inventory::getDifferentialDistanceOutFromHome(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutFromHome(const Position A) const;
    bool Map_Inventory::checkViableGroundPath(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutOnMap(const Position A, const vector<vector<int>>& map) const;

    // Marks and scores base locations.
    void Map_Inventory::updateBaseLoc(const Resource_Inventory &ri);
    void Map_Inventory::updateWorkersClearing(Unit_Inventory & ui, Resource_Inventory & ri); // updates number of workers clearing.
    void Map_Inventory::updateWorkersLongDistanceMining(Unit_Inventory & ui, Resource_Inventory & ri); // updates number of workers distance mining.

    // gets the radial distance of all units to the enemy base.
    static vector<int> getRadialDistances(const Unit_Inventory &ui, const vector<vector<int>> &map);

    // Returns the position of the weakest base.
    Position Map_Inventory::getWeakestBase(const Unit_Inventory &ei) const;
    // Returns the Position of the strongest base.
    //Position Map_Inventory::getStrongestBase(const Unit_Inventory & ei) const;
    // Returns the Position of a base with heaviest set of attackers...
    //Position Map_Inventory::getAttackedBase(const Unit_Inventory & ei, const Unit_Inventory &ui) const;
    //Position getBaseWithMostAttackers(const Unit_Inventory & ei, const Unit_Inventory & ui) const;
    // Returns the position of a base with the least casualties...
    Position Map_Inventory::getNonCombatBase(const Unit_Inventory & ui, const Unit_Inventory &di) const;
    // Returns the position of a base with the most fodder at it...
    Position getMostValuedBase(const Unit_Inventory & ui) const;

    // updates the next target expo.
    void Map_Inventory::getExpoPositions();
    // Changes the next expo to X:
    void Map_Inventory::setNextExpo(const TilePosition tp);

    //Visualizations
    void Map_Inventory::drawExpoPositions() const;
    void Map_Inventory::drawBasePositions() const;

    void Map_Inventory::writeMap(const vector< vector<int> > &mapin, const WalkPosition &center); // write one of the map objects have created, centered around the passed position.
    void Map_Inventory::readMap(vector< vector<int> > &mapin, const WalkPosition &center); // read one of the map objects we have created, centered around the passed position.

    // Adds start positions to inventory object.
    void Map_Inventory::getStartPositions();
    // Updates map positions and removes all visible ones;
    void Map_Inventory::updateStartPositions(const Unit_Inventory &ei);


    // Calls most of the map update functions when needed at a reduced and somewhat reasonable rate.
    void updateBasePositions(Unit_Inventory & ui, Unit_Inventory & ei, const Resource_Inventory & ri, const Unit_Inventory & ni, const Unit_Inventory &di);


   //Potential field stuff. These potential fields are coomputationally quite lazy and only consider local maximums, they do not sum together properly.
    vector<vector<int>> createEmptyField();
    vector<vector<int>> createThreatField(vector<vector<int>>& pf, Player_Model & enemy_player);
    vector<vector<int>> createAAField(vector<vector<int>>& pf, Player_Model & enemy_player);
    vector<vector<int>> createAttractField(vector<vector<int>>& pf, Player_Model & enemy_player);

    void DiagnosticField(vector<vector<int>>& pf);

};
