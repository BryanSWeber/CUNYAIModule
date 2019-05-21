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
    void Retreat_Logic();
    // Tells the unit to fight. If it can attack both air and ground.
    void Tactical_Logic(const Stored_Unit &e_unit, Unit_Inventory & ei, const Unit_Inventory &ui, const int &passed_dist, const Color & color);
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something. Now a backup.
    void Pathing_Movement(const int &passed_distance, const Position &e_pos );
    //Forces a unit to surround the concerning ei. Does not advance.
    //void Surrounding_Movement(const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, const Map_Inventory &inv);
    bool BWEM_Movement() const;

    // causes a unit to move to the left or the right of a position.
    Position encircle(const Position & p);
    // causes a unit to move towards a position.
    Position approach(const Position & p);

    bool adjust_lurker_burrow(const Position position_of_target);

    // gives a vector that has the direction towards center on (map). returns a direction.
    Position getVectorTowardsMap(const vector<vector<int>>& map) const;
    // gives a vector that has the direction towards higher values on the field.  returns a direction.
    Position getVectorTowardsField(const vector<vector<int>>& field) const;
    // gives a vector that has the direction towards lower values on the field.  returns a direction.
    Position getVectorAwayField(const vector<vector<int>>& field) const;

    bool move_to_next(const BWEM::CPPath & cpp, const int & plength) const;

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

    int rng_direction_ ; // send unit in a random tilt direction if blocked

};
