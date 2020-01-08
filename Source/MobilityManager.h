#pragma once

#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include <bwem.h>

//Movement and Combat Functions
class Mobility {

public:
    //When we think about moving a unit, don't do it yourself, use the mobility wrapper.
    Mobility::Mobility(const Unit &unit) {
        unit_ = unit;
        pos_ = unit->getPosition();
        u_type_ = unit->getType();
        distance_metric_ = CUNYAIModule::getProperSpeed(unit) * 24.0; // in pixels

        auto found_item = CUNYAIModule::friendly_player_model.units_.unit_map_.find(unit);
        if (found_item != CUNYAIModule::friendly_player_model.units_.unit_map_.end()) {
            stored_unit_ = &found_item->second;
        }
    };

    // Basic retreat logic
    bool Retreat_Logic(Stored_Unit &su);
    // Scatter (from given position, or if blank, any present storms or spells)
    bool Scatter_Logic(const Position pos = Positions::Origin);
    // Tells the unit to fight. Uses a simple priority system and a diving limit for targeting.
    bool Tactical_Logic(const Stored_Unit &e_unit, Unit_Inventory & ei, const Unit_Inventory &ui, const int &passed_dist, const Color & color);
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something. Now a backup.
    bool local_pathing(const int &passed_distance, const Position &e_pos, const Stored_Unit::Phase phase);
    // Uses choke points when outside of local area, otherwise uses basic rules of attraction. Positive means move out, negative means move home.
    bool BWEM_Movement(const bool & in_or_out);
    // Surrounds position of choice slowly.
    bool surround(const Position &pos);

    // causes a unit to move to the left or the right of a position.
    Position encircle(const Position & p);
    // causes a unit to avoid low-altitude areas.
    Position avoid_edges();
    // causes a unit to move towards a position.
    Position approach(const Position & p);

    //Checks if all except the first area are safe, since we are trying to run.
    bool checkSafeEscapePath(const Position & finish);
    //Checks first area for safety.
    bool checkSafePath(const Position & finish);

    bool adjust_lurker_burrow(const Position position_of_target);

    // gives a vector that has the direction towards center on (map). returns a direction.
    Position getVectorTowardsMap(const vector<vector<int>>& map) const;
    // gives a vector that has the direction towards higher values on the field.  returns a direction.
    Position getVectorTowardsField(const vector<vector<int>>& field) const;
    // gives a vector that has the direction towards lower values on the field.  returns a direction.
    Position getVectorAwayField(const vector<vector<int>>& field) const;

    bool moveTo(const Position & start, const Position & finish, const Stored_Unit::Phase phase);
    //Gets the next waypoint the unit would move to if using the moveTo command.
    Position getNextWaypoint(const Position & start, const Position & finish);
    // gives how far the unit can move in one second.
    int getDistanceMetric();

    // Returns true if the enemy unit is too far up another hill.
    bool isOnDifferentHill(const Stored_Unit &e);

    Unit pickTarget(int MaxDiveDistance, Unit_Inventory & ui); // selects a target from a unit map. Can return NULL

    bool checkGoingDifferentDirections(Unit e);
    bool checkEnemyApproachingUs(Unit e);

    Position getVectorToEnemyDestination(Unit e);

    Position getVectorToEnemyBack(Unit e);

private:
    Position pos_;
    Unit unit_;
    Stored_Unit* stored_unit_;
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

};

// returns the total, nondirected speed of an enemy unit. Highly variable.
int getEnemySpeed(Unit e);

//returns the vector of an enemy unit.
Position getEnemyVector(Unit e);

// returns the a 32-pixel vector in the direction of an enemy unit.
Position getEnemyUnitaryVector(Unit e);
