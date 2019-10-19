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

    int hatches_;
    int last_gas_check_;
    int my_portion_of_the_map_;
    int expo_portion_of_the_map_;
    int estimated_enemy_workers_;
    int map_x;
    int map_y;

    //Marks Data for each area if it is "ground safe"
    void updateGroundDangerousAreas();

    vector<Position> start_positions_;
    vector<TilePosition> expo_tilepositions_;
    vector<TilePosition> expo_positions_complete_;
    Position enemy_base_ground_;
    Position enemy_base_air_;
    Position front_line_base_;
    Position safe_base_;
    Position scouting_base_;

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
    vector< vector<int> > map_out_from_scouting_; // distance from enemy base. 1 if blocked/inaccessable by ground.
    vector< vector<int> > base_values_;
    vector< vector<int> > map_chokes_;

    int vision_tile_count_;
    int est_enemy_stock_;

    TilePosition next_expo_;
    bool cleared_all_start_positions_;
    bool checked_all_expo_positions_ = false;

    int frames_since_unwalkable = 0;
    int frames_since_map_veins = 0;
    int frames_since_front_line_base = 0;
    int frames_since_safe_base = 0;
    int frames_since_enemy_base_ground_ = 0;
    int frames_since_enemy_base_air_ = 0;
    int frames_since_scouting_base_ = 0;

    //Fields:
    vector< vector<int> > pf_threat_;
    vector< vector<int> > pf_attract_;
    vector< vector<int> > pf_aa_;
    vector< vector<int> > pf_explore_;

    // Updates the (safe) log of our supply stock.
    void updateLn_Supply_Remain();
    // Updates the (safe) log of our supply total.
    void updateLn_Supply_Total();
    // Updates the count of our vision total, in tiles
    void updateVision_Count();
    // Updates our screen poisition. A little gratuitous but nevertheless useful.
    void updateScreen_Position();
    // Updates the (safe) log gas ratios, ln(gas)/(ln(min)+ln(gas))
    double getGasRatio();
    // Updates the (safe) log of our supply total. Returns very high int instead of infinity.
    double getLn_Supply_Ratio();

    // Updates the number of hatcheries (and decendents).
    void Map_Inventory::updateHatcheries();

    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void Map_Inventory::updateBuildablePos();
    // Updates the unwalkable portions of the map.
    void Map_Inventory::updateUnwalkable();
    // Updates unwalkable portions with existing blockades. Currently flawed.
    void Map_Inventory::updateUnwalkableWithBuildings();

    // Marks and smooths the edges of the map. Dangerous- In progress.
    void Map_Inventory::updateSmoothPos();
    // Marks the distance from each obstacle. Requires updateunwalkablewithbuildings. //[Old usage:]Marks the main arteries of the map. 
    void Map_Inventory::updateMapVeins();

    // simply gets the map value at a particular position.
    static int getMapValue(const Position &pos, const vector<vector<int>> &map);
    static int getFieldValue(const Position & pos, const vector<vector<int>>& field);

    // Updates the visible map arteries. Only checks buildings.
    //void Map_Inventory::updateLiveMapVeins( const Unit & building, const Unit_Inventory &ui, const Unit_Inventory &ei, const Resource_Inventory &ri );
    //void Map_Inventory::updateLiveMapVeins( const Unit_Inventory & ui, const Unit_Inventory & ei, const Resource_Inventory & ri );
    // Updates the chokes on the map.
    //void Map_Inventory::updateMapChokes(); //in progress
    // Updates the spiral counting out from the new_center. Replaces old (map), prints.
    void Map_Inventory::updateMapVeinsOut(const Position & newCenter, Position & oldCenter, vector<vector<int>>& map, const bool &print = false);

    // Gets distance using
    int Map_Inventory::getDifferentialDistanceOutFromEnemy(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutFromEnemy(const Position A) const;
    int Map_Inventory::getDifferentialDistanceOutFromHome(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutFromHome(const Position A) const;
    bool Map_Inventory::checkViableGroundPath(const Position A, const Position B) const;
    int Map_Inventory::getRadialDistanceOutOnMap(const Position A, const vector<vector<int>>& map) const;

    // gets the radial distance of all units to the enemy base.
    static vector<int> getRadialDistances(const Unit_Inventory &ui, const vector<vector<int>> &map, const bool combat_units);

    // Returns the position of the base with the most casualtis. Friendly is true (by default) to checking -yourself- for the weakest base. Fodder (T/F) is for the inclusion of fodder in that calculation.
    Position Map_Inventory::getBaseWithMostCausalties(const bool &friendly = true, const bool &fodder = true) const;
    // Returns the Position of the base with the most surviving units. Friendly is true (by default) to checking -yourself- for the strongest base. Fodder (T/F) is for the inclusion of fodder in that calculation.
    Position Map_Inventory::getBaseWithMostSurvivors(const bool &friendly = true, const bool &fodder = true) const;

    Position getBaseNearest();


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
    void updateCurrentMap();

    //Potential field stuff. These potential fields are coomputationally quite lazy and only consider local maximums, they do not sum together properly.
    vector<vector<int>> completeField(vector<vector<int>> pf, const int & reduction);
    void createThreatField(Player_Model & enemy_player);
    void createAAField(Player_Model & enemy_player);
    void createExploreField();
    void createAttractField(Player_Model & enemy_player);

    void DiagnosticField(vector<vector<int>>& pf);

    void DiagnosticTile();

};
