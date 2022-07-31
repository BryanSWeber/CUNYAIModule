#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include "ResourceInventory.h"
#include "PlayerModelManager.h"

using namespace std;
using namespace BWAPI;

#define BUFFER_SIZE 4

//Might benifit from seperatating maps entirely.
//Several types of maps:
// A)Buildable positions, basic map no dependencies.
// B)Unwalkable barriers, basic map no dependencies.
// Unwalkable barriers w buildings. Depends on unwalkable barriers.
// Smoothed barriers, depends on unwalkable barriers.
// map_veins_, depends on unwalkable barriers WITH buildings.
// Map veins in and out from enemy - depends on Unwalkable barriers. Does not depend on buildings.

class MapInventory {
private:
    Position enemy_base_ground_;
    Position enemy_base_air_;
    Position front_line_base_;
    Position safe_base_;
    vector<Position> scouting_bases_;
    vector<Position> air_scouting_bases_;

    int nScouts_ = 2; // How many scouts will we have? Set by fiat.

    bool discovered_enemy_this_frame_ = false;
    bool enemy_found_ = false;
    bool enemy_start_location_found_ = false;

    int map_x; //Map width in tiles.
    int map_y; //Map height in tiles.
    int buffer = 3; // buffer since our vision estimator is imperfect

    //Fields:
    //double pfAirThreat_[256][256] = { 0 }; // which air areas are under threat?
    double pfDetectThreat_[256][256] = { 0 }; // which areas are detected?
    //double pfGroundThreat_[256][256] = { 0 }; // which ground areas are under threat?
    //double pfVisible_[256][256] = { 0 }; // which ground areas are visible?
    //double pfBlindness_[256][256] = { 0 }; // The region about 2 tiles out of sight of the opponent's vision. Counts down from 2 to 0.
    double pfThreat_[256][256] = { 0 }; // which areas are under threat? Consider that melee units can MOVE so give a buffer around melee units.
    int pfOccupation_[256][256] = { 0 }; // How many units are on each tile? This only tracks if a VISIBLE square is occupied. It is distinct from the other fields and only uses INT.
    double pfThreatBuffer_[256][256] = { 0 }; // The region about BUFFER_SIZE tiles out of sight of the opponent's threat field. Counts down from 4 to 0.
    bool pfSurroundSquare_[256][256] = { 0 }; //Is the square a viable square to move a unit to and improve the surround?
    void completeField(double pf[256][256], int reduction); //Creates a buffer around a field roughly REDUCTION units wide.
    void overfillField(double pfIn[256][256], double pfOut[256][256], int reduction); //Creates a buffer of an area SURROUNDING a field roughly REDUCTION units wide.

    // treatment order is as follows unwalkable->smoothed->veins->map veins from/to bases.
    vector< vector<bool> > buildable_positions_; // buildable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > unwalkable_barriers_with_buildings_; // unwalkable = 1, otherwise 0.
    vector< vector<int> > smoothed_barriers_; // unwalkablity+buffer >= 1, otherwise 0. Totally cool idea but a trap. Base nothing off this.
    vector< vector<int> > map_veins_; //updates for building locations 1 if blocked, counts up around blocked squares if otherwise.

    bool isScoutingPosition(const Position & pos) const;     //returns true if a position has been added to scoutpositions as determined by assignScoutDestionation.

    Position getBaseWithMostSurvivors(const bool &friendly = true, const bool &fodder = true) const; // Returns the Position of the base with the most surviving units. Friendly is true (by default) to checking -yourself- for the strongest base. Fodder (T/F) is for the inclusion of fodder in that calculation.

    double distanceTransformation(const int currentDistance) const; //returns 0.3 if 0 or 100/distacnce. Intended to be replaced by something smarter.
    double distanceTransformation(const double distanceFromTarget) const;  //returns 0.3 if 0 or 100/distacnce. Intended to be replaced by something smarter. Overload.

    //Sets points you want to move your army towards on the map.
    void assignArmyDestinations(); //Choses a ground location to move anti-ground towards.
    void assignAirDestinations(); //Choses an air location to move AA towards.
    void assignScoutDestinations(); //Choses positions at bases to scout.
    void assignSafeBase(); //sets safe_base_ to base with "Most survivors" in a FAP sim.

    void sendArmyTowardsPosition(const Position closest_enemy = Positions::Origin); //Sends anti-ground forces to postion. Has safety catches.
    void sendAntiAirTowardsPosition(const Position closest_enemy = Positions::Origin); //Sends anti-ground forces to postion. Has safety catches.
    void sendScoutTowardsPosition(const Position closest_enemy = Positions::Origin); //Sends scouts towards a position. Has safety catches.



    //Potential field stuff. These potential fields are coomputationally quite lazy and only consider local maximums, they do not sum together properly.
    void createDetectField(PlayerModel & enemy_player); //Marks all tiles that are pluasibly detected by enemy player.
    void createOccupationField(); //Marks all the tiles you have occupied.
    void createThreatBufferField(PlayerModel & enemy_player); // Must run after CreateThreatField
    void createExtraWideBufferField(PlayerModel & enemy_player); // Must run after CreateThreatField, this is even wider than the threat buffer field.
    void createThreatField(PlayerModel & enemy_player); // This marks all potentially threatened OR visible squares.
    void createSurroundField(PlayerModel & enemy_player); //Must run after createThreatBuffer and CreatOccupationField

    void updateGroundDangerousAreas();     //Marks Data for each area if it is "ground safe"
    void updateBuildablePos();     // Updates the static locations of buildability on the map. Should only be called on game start. MiniTiles!
    void updateUnwalkable();     // Updates the unwalkable portions of the map.
    void updateMapVeins(); // Marks the distance from each obstacle. Requires updateunwalkablewithbuildings.


    //Diagnostic Functions
    void DiagnosticField(const double pf[256][256]) const; //Diagnostic to show "potential fields"
    void DiagnosticField(const int pf[256][256]) const;  //Overload: Diagnostic to show "potential fields"
    void DiagnosticField(const bool pf[256][256]) const; //Overload: Diagnostic to show "potential fields"
    void DiagnosticTile() const; //Draws a single tile, useful for other diagnostics.

public:
    MapInventory();
    MapInventory(const UnitInventory &ui, const ResourceInventory &ri);

    void onStart(); //Run this on game start.  Note game start does not actually have everything loaded - frame 0 is when the map is "ready" for most accessing.
    void onFrame(); //Run this every frame to update based on new information.

    bool checkViableGroundPath(const Position A, const Position B) const;     // Is there a viable ground path? CPP, will not fail if thrown at a building.

    // Calls most of the map update functions when needed at a reduced and somewhat reasonable rate.
    void mainCurrentMap();

    const int getDetectField(const TilePosition & t) const;
    const int getOccupationField(const TilePosition &t) const; // returns 1 for occupation by small units, 2 for larger units, and the sum for more.

    void setSurroundField(TilePosition &t, bool newVal); //Marks a position as occupied on the surround field.

    Position getEarlyGameScoutPosition() const;
    Position getEarlyGameArmyPosition() const;
    Position getEarlyGameAirPosition() const;
    Position getDistanceWeightedPosition(const Position & target_pos ) const; //Returns a position that is 1) not visible, 2) not already being scouted 3) randomly chosen based on a weighted distance from target_pos. Uses CPP and will consider walled-off positions. Will return origin if fails.
    Position getSafeBase() const;
    Position getEnemyBaseGround() const;
    Position getEnemyBaseAir() const; // Gets enemy air closest.
    Position getFrontLineBase() const; // Gets base I believe to be under threat.
    Position getBasePositionNearest(const Position &p) const;     //Which base position is most nearby this spot?
    Position getClosestInVector(const vector<Position>& posVector) const; // This command returns the closest position to my safe_base_.
    Position getFurthestInVector(const vector<Position>& posVector) const; // This command returns the furthest position to my safe_base_.
    int getMyMapPortion() const; // Gets the distance from spawn of the map that "belongs" to me, currently about 1/nth of the map where N is the number of bases.
    int getFieldValue(const Position & pos, const vector<vector<int>>& field) const;     // simply gets the value in FIELD at a particular POS.
    int getRadialDistanceOutFromEnemy(const Position A) const;     //Distance from enemy base in pixels, called a lot.
    int getRadialDistanceOutFromHome(const Position A) const;     //Distance from home in pixels, called a lot.
    int getDistanceBetween(const Position A, const Position B) const; // Simply gets the distance between two points using cpp, will not fail if a spot is inside a building.
    int getExpoPositionScore(const Position &p); //Returns a score based on how good the expo appears to be.
    double getGasRatio() const;     // gets the (safe) log gas ratios, ln(gas)/(ln(min)+ln(gas))
    double getLn_Supply_Ratio() const; // gets the (safe) log of our supply total. Returns very high int instead of infinity.
    double getTileThreat(const TilePosition & tp) const; //Returns the amount of threat on the tile.
    vector<TilePosition> getExpoTilePositions() const; //returns all possible expos and starting bases, found with BWEM.
    vector<TilePosition> getInsideWallTilePositions() const; //Returns the plausible macro hatch positions only.
    vector<Position> getScoutingBases() const;
    vector<int> getRadialDistances(const UnitInventory &ui, const bool combat_units) const;      // gets the radial distance of all units to the enemy base in pixels.

    bool isTileThreatened(const TilePosition & tp) const; //Returns true if the tile is under threat.
    bool isInBufferField(const TilePosition & t) const; //Returns true if the tile is in a buffer field.
    bool isInExtraWideBufferField(const TilePosition &t) const; //Returns true if the tile is in an EXTRA WIDE buffer field.
    bool isInSurroundField(const TilePosition &t) const; //Returns true if the tile is in a surround field.
    bool isStartPosition(const Position & p)  const; //returns true if the position is a start position.
    bool isOnIsland(const Position A) const;  //Is this place pathable from home? Will not return error if your base IS the island.

    bool checkExploredAllStartPositions(); //returns true if you have explored all start positions, false otherwise.

    void DiagnosticOccupiedTiles() const;     //Draw some of the important stuff we have stored.
    void DiagnosticDetectedTiles() const;    //Draw some of the important stuff we have stored.
    void DiagnosticSurroundTiles() const;    //Draw some of the important stuff we have stored.
    void DiagnosticThreatTiles() const;    //Draw some of the important stuff we have stored.
    void DiagnosticExtraWideBufferTiles() const;    //Draw some of the important stuff we have stored.
};
