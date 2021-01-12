#pragma once

#include "CUNYAIModule.h"
#include "UnitInventory.h"
#include <bwem.h>

//Movement and Combat Functions
class Mobility {
private:
    Position pos_;
    Unit unit_;
    StoredUnit* stored_unit_;
    UnitType u_type_;
    double distance_metric_;
    Position stutter_vector_ = Positions::Origin;
    Position attune_vector_ = Positions::Origin;
    Position cohesion_vector_ = Positions::Origin;
    Position centralization_vector_ = Positions::Origin;
    Position seperation_vector_ = Positions::Origin;
    Position attract_vector_ = Positions::Origin;
    Position repulse_vector_ = Positions::Origin;
    Position retreat_vector_ = Positions::Origin;
    Position walkability_vector_ = Positions::Origin;
    Position encircle_vector_ = Positions::Origin;

    int rng_direction_; // send unit in a random tilt direction if blocked

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
    };

    // Basic retreat logic
    bool Retreat_Logic();
    // Scatter (from given position, or if blank, any present storms or spells)
    bool Scatter_Logic(const Position pos = Positions::Origin);
    // Tells the unit to fight. Uses a simple priority system and a diving limit for targeting.
    bool Tactical_Logic(UnitInventory & ei, const UnitInventory &ui, const int &passed_dist, const Color & color);
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something. Now a backup.
    bool simplePathing(const Position &e_pos, const StoredUnit::Phase phase);
    // Uses choke points when outside of local area, otherwise uses basic rules of attraction. Positive means move out, negative means move home.
    bool BWEM_Movement(const bool & in_or_out);

    // Surrounds by moving to the surround field.
    bool surroundLogic();

    // Causes a unit to move away from its neighbors.
    Position isolate();
    // causes a unit to move into a peremiter outside of enemy threat that is lower occupied.
    Position encircle();
    // Causes a unit to move into a location outside of enemy threat, perimieter nonwithstanding.
    Position escape();
    // causes a unit to avoid low-altitude areas.
    Position avoid_edges();
    // causes a unit to move towards a position.
    Position approach(const Position & p);

    //Checks if all except the first area are safe, since we are trying to run.
    bool checkSafeEscapePath(const Position & finish);
    //Checks first area for safety.
    bool checkSafeGroundPath(const Position & finish);

    bool prepareLurkerToAttack(const Position position_of_target);

    // gives a vector that has the direction towards higher values on the field.  returns a direction.
    Position getVectorTowardsField(const vector<vector<int>>& field) const;
    // gives a vector that has the direction towards lower values on the field.  returns a direction.
    Position getVectorAwayField(const vector<vector<int>>& field) const;

    bool moveTo(const Position & start, const Position & finish, const StoredUnit::Phase phase);

    // gives how far the unit can move in one second.
    int getDistanceMetric();

    //Checks if a particular tile is worth running to (for attacking or retreating). Heuristic, needs work.
    bool isTileApproachable(const TilePosition tp);

    Unit pickTarget(int MaxDiveDistance, UnitInventory & ui); // selects a target from a unit map. Can return NULL

    bool checkGoingDifferentDirections(Unit e);

    bool checkEnemyApproachingUs(Unit e);
    bool checkEnemyApproachingUs(StoredUnit & e);
    bool isMoreOpen(TilePosition &tp);

    //Seriously, this is easy to flip around sometimes. 
    Position getVectorToDestination(Position &p);
    Position getVectorToEnemyDestination(Unit e);
    Position getVectorToBeyondEnemy(Unit e);
};

// returns the total, nondirected speed of an enemy unit. Highly variable.
double getEnemySpeed(Unit e);

//returns the vector of an enemy unit.
Position getEnemyVector(Unit e);

bool checkSameDirection(const Position vector_a, const Position vector_b);
// returns true if the angles are within 90 degrees of each other (0.5*pi)
bool checkAngleSimilar(double angle1, double angle2);

//returns the center of a tile rather than the top right corner.
Position getCenterTile(const TilePosition tpos);

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