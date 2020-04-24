#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "UnitInventory.h"
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

struct MapInventory {
    MapInventory();
    MapInventory(const UnitInventory &ui, const Resource_Inventory &ri);

    int nScouts = 2; // How many scouts will we have? Set by fiat.
    Position screen_position_;
    bool discovered_enemy_this_frame = false;
    bool enemy_found = false;

    int hatches_;
    int last_gas_check_;
    int my_portion_of_the_map_;
    int expo_portion_of_the_map_;
    int estimated_enemy_workers_;
    int map_x; //Map width in tiles.
    int map_y; //Map height in tiles.

    //Marks Data for each area if it is "ground safe"
    void updateGroundDangerousAreas();
    vector<TilePosition> MapInventory::getExpoTilePositions(); //returns all possible expos and starting bases, found with BWEM.

    Position enemy_base_ground_;
    Position enemy_base_air_;
    Position front_line_base_;
    Position safe_base_;
    vector<Position> scouting_bases_;
    vector<Position> air_scouting_bases_;


    // treatment order is as follows unwalkable->smoothed->veins->map veins from/to bases.
    vector< vector<bool> > buildable_positions_; // buildable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_with_buildings_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > smoothed_barriers_; // unwalkablity+buffer >= 1, otherwise 0. Totally cool idea but a trap. Base nothing off this.
    vector< vector<int> > map_veins_; //updates for building locations 1 if blocked, counts up around blocked squares if otherwise.

    int vision_tile_count_;
    int est_enemy_stock_;

    //Fields:
    vector< vector<int> > pf_air_threat_; // which air areas are under threat?
    vector< vector<int> > pf_detect_threat_; // which areas are detected?
    vector< vector<int> > pf_ground_threat_; // which ground areas are under threat?

    // Updates the count of our vision total, in tiles
    void updateVision_Count();
    // Updates our screen poisition. A little gratuitous but nevertheless useful.
    void updateScreen_Position();
    // Updates the (safe) log gas ratios, ln(gas)/(ln(min)+ln(gas))
    double getGasRatio();
    // Updates the (safe) log of our supply total. Returns very high int instead of infinity.
    double getLn_Supply_Ratio();

    // Updates the number of hatcheries (and decendents).
    void MapInventory::updateHatcheries();

    // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void MapInventory::updateBuildablePos();
    // Updates the unwalkable portions of the map.
    void MapInventory::updateUnwalkable();
    // Updates unwalkable portions with existing blockades. Currently flawed.
    void MapInventory::updateUnwalkableWithBuildings();

    // Marks and smooths the edges of the map. Dangerous- In progress.
    void MapInventory::updateSmoothPos();
    // Marks the distance from each obstacle. Requires updateunwalkablewithbuildings. //[Old usage:]Marks the main arteries of the map. 
    void MapInventory::updateMapVeins();

    // simply gets the map value at a particular position.
    static int getFieldValue(const Position & pos, const vector<vector<int>>& field);

    // Simply gets the distance between two points using cpp, will not fail if a spot is inside a building.
    int getDistanceBetween(const Position A, const Position B) const;

    // Gets distance using
    int MapInventory::getRadialDistanceOutFromEnemy(const Position A) const; //Distance from enemy base in pixels, called a lot.
    int MapInventory::getRadialDistanceOutFromHome(const Position A) const; //Distance from home in pixels, called a lot.
    bool MapInventory::checkViableGroundPath(const Position A, const Position B) const; // Is there a viable ground path? CPP, will not fail if thrown at a building.
    bool MapInventory::isOnIsland(const Position A) const; //Is this place pathable from home? Will not return error if your base IS the island.

    vector<int> getRadialDistances(const UnitInventory &ui, const bool combat_units);  // gets the radial distance of all units to the enemy base in pixels.

    Position MapInventory::getBaseWithMostSurvivors(const bool &friendly = true, const bool &fodder = true) const;     // Returns the Position of the base with the most surviving units. Friendly is true (by default) to checking -yourself- for the strongest base. Fodder (T/F) is for the inclusion of fodder in that calculation.

    Position getBasePositionNearest(Position &p); //Which base position is most nearby this spot?

    void MapInventory::writeMap(const vector< vector<int> > &mapin, const WalkPosition &center); // write one of the map objects have created, centered around the passed position.
    void MapInventory::readMap(vector< vector<int> > &mapin, const WalkPosition &center); // read one of the map objects we have created, centered around the passed position.

    bool checkExploredAllStartPositions(); //returns true if you have explored all start positions, false otherwise.

    // Calls most of the map update functions when needed at a reduced and somewhat reasonable rate.
    void mainCurrentMap();

    //Potential field stuff. These potential fields are coomputationally quite lazy and only consider local maximums, they do not sum together properly.
    vector<vector<int>> completeField(vector<vector<int>> pf, const int & reduction);
    void createAirThreatField(PlayerModel & enemy_player);
    void createDetectField(PlayerModel & enemy_player);
    void createGroundThreatField(PlayerModel & enemy_player);


    void DiagnosticField(vector<vector<int>>& pf); //Diagnostic to show "potential fields"
    void DiagnosticTile();

    //void updateScoutLocations(const int &nScouts ); //Updates all visible scout locations. Chooses them if they DNE.
    //Position MapInventory::createStartScoutLocation(); //Creates 1 scout position at time 0 for overlords. Selects from start positions only. Returns origin if fails.
    //Position getStartEnemyLocation(); // gets an enemy start location that hasn't been explored. Will not move it if I am already marching towards it.
    //bool isScoutingOrMarchingOnPosition(const Position & pos, const bool & explored_sufficient = false, const bool &check_marching = true);
    void assignArmyDestinations();
    void assignScoutDestinations();
    void assignAirDestinations();
    //returns true if a position is being scouted or marched towards. checks for area ID matchs.
    bool isScoutingPosition(const Position & pos);
    bool isMarchingPosition(const Position & pos);
    Position getClosestInVector(vector<Position>& posVector); // This command returns the closest position to my safe_base_.
    Position getFurthestInVector(vector<Position>& posVector); // This command returns the furthest position to my safe_base_.
    bool isStartPosition(const Position & p); //returns true if the position is a start position.
    double distanceTransformation(const int currentDistance); //transforms the distance into a weighted distance based on time of game, distance from enemy, and size of map.
    double distanceTransformation(const double distanceFromTarget);  //transforms the distance into a weighted distance based on time of game, distance from enemy, and size of map. Overload.
    void assignLateArmyMovement(const Position closest_enemy);
    void assignLateAirMovement(const Position closest_enemy);
    void assignLateScoutMovement(const Position closest_enemy);
    Position getEarlyGameScoutPosition();
    Position getEarlyGameArmyPosition();
    Position getEarlyGameAirPosition();
    Position getDistanceWeightedPosition(const Position & target_pos ); //Returns a position that is 1) not visible, 2) not already being scouted 3) randomly chosen based on a weighted distance from target_pos. Uses CPP and will consider walled-off positions. Will return origin if fails.
    static bool isTileDetected(const Position &p); //Checks if a tile is detected by an enemy. Inaccurate.
    static bool isTileAirThreatened(const Position &p); //Checks if a tile is detected by an enemy. Inaccurate.
    static bool isTileGroundThreatened(const Position &p);//Checks if a tile is detected by an enemy. Inaccurate.
};