#pragma once

#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include "Diagnostics.h"
#include <bwem.h>

//Movement and Combat Functions
// Style notes: All functions to privately return a LOCATION.  So a safe position relative to position X that avoids low ground should be safe(avoidlow(x)), or similar. Note the ordering will likely matter.
// Public functions should simply do work and return TRUE if they work as intended.
// SimplePathing does not type the unit in all cases?
// Everything should take positions, even if it has to convert to tileposition.

class Mobility {
private:
    Position pos_;
    Unit unit_;
    StoredUnit* stored_unit_;
    UnitType u_type_;
    double distance_metric_;

    Position getVectorApproachingPosition(const Position & p);     // Gets a short vector going to a local position that should make us look at the unit just before the spam guard triggers.
    //Position getVectorAwayFromNeighbors();  // Gets a vector away from nearest neighbors.
    Position getVectorAwayFromEdges();     // causes a unit to avoid low-"altitude areas" - altitude refers to distance to unwalkable tiles.  In general, one prefers to be on a high altitude position, more options.

    bool isMoreOpen(TilePosition &tp); //Returns true if the position is less occupied than pos_.

    Position getSaferPositionNear(const Position p);  // Gets unoccupied position at lower level of threat, then sorted by shortest walk from p.
    Position getSurroundingPosition(const Position p);  // Returns a position that meets several critiera - the position falls a safe distance away from P and is less ocupied. Declares the destination tile "occupied" for the moment. Has a limiter in it that could potentially be put elsewhere.  May fail to find, in which case it defaults to getSaferPosition(p), which gurantees a return.
    Position getPositionThatLetsUsAttack(const Position p, const double proportion = 0.90); // Returns a position such that p is just within range.  Using 100% might get some pixel missing.
    Position getPositionOneTurnCloser(const Position & p);     // Gets a short vector going to a local position that should make us look at the unit just before the spam guard triggers.

    Unit pickTarget(int MaxDiveDistance, UnitInventory & ui); // selects a target from a unit map. Can return NULL


    bool prepareLurkerToMove();     //Gets a lurker ready to move. Returns TRUE if the lurker needed fixing. 
    bool moveAndUpdate(const Position p, const StoredUnit::Phase phase); //Moves the unit and updates required items. Convenience function. Returns true if the unit was fiddled with in some way.

public:
    //When we think about moving a unit, don't do it yourself, use the mobility wrapper.
    Mobility::Mobility(const Unit &unit) {
        unit_ = unit;
        pos_ = unit->getPosition();
        u_type_ = unit->getType();
        distance_metric_ = CUNYAIModule::getProperSpeed(unit) * 24.0; // in pixels

        auto found_item = CUNYAIModule::friendly_player_model.units_.getStoredUnit(unit);
        if (found_item) {
            stored_unit_ = found_item;
        }
        else {
            Diagnostics::DiagnosticText("Just tried to mobilize a unit that did not exist.");
        }
    };

    bool Retreat_Logic();     // Basic retreat logic
    bool Scatter_Logic(const Position pos = Positions::Origin); // Scatter (from given position, or if blank, any present storms or spells)
    bool Tactical_Logic(UnitInventory & ei, const UnitInventory &ui, const int &passed_dist, const Color & color); // Tells the unit to fight. Uses a simple priority system and a diving limit for targeting.
    bool simplePathing(const Position &e_pos, const StoredUnit::Phase phase, const bool caution = false);     //Forces a unit to move in a nearly straight line to the position. Contains greedy ideas - if the target is unsafe or your position is unsafe, you may abort the trip and instead take a surrounding position.
    bool BWEM_Movement(const bool & moveOut); // Uses choke points when outside of local area, otherwise uses basic rules of attraction. True means move out, false means move home.
    bool surroundLogic();     // Orders unit to move to its nearest more-empty surrounding position.


    bool checkSafeEscapePath(const Position & finish);     //Checks if all areas between here and the finish - except the first area - are safe, since we are trying to run.
    bool checkSafeGroundPath(const Position & finish);     //Checks all areas between here and the finish for safety, including the first.

    bool moveTo(const Position & start, const Position & finish, const StoredUnit::Phase phase, const bool caution = false);     // Moves to a location, if caution is TRUE then it will modify an order to move to a threatened area and instead send it to the nearest suitable surround tile.

    bool isTileApproachable(const TilePosition tp);     //Checks if a particular tile is worth running to (for attacking or retreating). Heuristic, needs work.

    bool checkGoingDifferentDirections(Unit e);

    bool prepareLurkerToAttack(const Position position_of_target);     //Gets a lurker ready to attack a particular position. Returns TRUE if the lurker needed fixing.

    bool checkEnemyApproachingUs(Unit e); //Unused
    bool checkEnemyApproachingUs(StoredUnit & e);//Unused

    Position getVectorFromUnitToDestination(Position &p);     //Seriously, this is easy to flip around sometimes. 
    Position getVectorFromUnitToEnemyDestination(Unit e);     //Seriously, this is easy to flip around sometimes. 
    Position getVectorFromUnitToBeyondEnemy(Unit e);     //Seriously, this is easy to flip around sometimes. 
};

double getEnemySpeed(Unit e); // returns the total, nondirected speed of an enemy unit. Highly variable.
Position getEnemyVector(Unit e); //returns the vector of an enemy unit.
bool checkSameDirection(const Position vector_a, const Position vector_b); // returns true if the angles are within 90 degrees of each other (0.5*pi)
bool checkAngleSimilar(double angle1, double angle2); // returns true if the angles are within 90 degrees of each other (0.5*pi)
Position getCenterOfTile(const TilePosition tpos); //returns the center of a tile rather than the top right corner.
Position getCenterOfTile(const Position pos); //Adjusts to center of tile rather than another position.

class SpiralOut { // from SO
protected:
    unsigned layer;
    unsigned leg;
public:
    int x, y; //read these as output from next, do not modify.
    SpiralOut() :layer(1), leg(0), x(0), y(0) {}
    void goNext() {
        switch (leg) {
        case 0: ++x; if (x == layer)  ++leg;                break;
        case 1: ++y; if (y == layer)  ++leg;                break;
        case 2: --x; if (-x == layer)  ++leg;                break;
        case 3: --y; if (-y == layer) { leg = 0; ++layer; }   break;
        }
    }
};