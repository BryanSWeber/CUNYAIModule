#pragma once

#include "CUNYAIModule.h"
#include "Unit_Inventory.h"
#include "BWEM\include\bwem.h"

//Movement and Combat Functions
class Mobility {

public:
    // Basic retreat logic
    void Retreat_Logic(const Stored_Unit &e_unit, const Unit_Inventory &u_squad, Unit_Inventory &e_squad, Unit_Inventory &ei, const Unit_Inventory &ui, const int &passed_distance, const Color &color, const bool &force);
    // Tells the unit to fight. If it can attack both air and ground.
    void Tactical_Logic(const Stored_Unit &e_unit, Unit_Inventory & ei, const Unit_Inventory &ui, const int &passed_dist, const Color & color);
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something.
    void Pathing_Movement(const int &passed_distance, const Position &e_pos );
    //Forces a unit to surround the concerning ei. Does not advance.
    //void Surrounding_Movement(const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, const Map_Inventory &inv);
    bool BWEM_Movement() const;

    Position Output;

    // Causes a unit to match headings with neighboring units.
    Position setAlignment(const Unit_Inventory &ui );
    // Causes UNIT to run directly from enemy.
    Position setDirectRetreat(const Position &e_pos);
    // Causes a unit to move towards central map veins. "High Altitude in BWEM"
    //Position setCentralize();
    // causes a unit to move about in a random (brownian) fashion.
    Position setStutter(const double &n);
    // Causes a unit to be pulled towards others of their kind.
    Position setCohesion(const Unit_Inventory &ui);
    // causes a unit to be pulled towards (map) center.
    Position setAttractionMap(const vector<vector<int>>& map, const Position &map_center);
    Position setAttractionField(const vector<vector<int>>& field, const Position & map_center);
    // causes a unit to be pushed away from (map) center. Dangerous for ground units, could lead to them running down dead ends.
    Position setRepulsionMap(const vector<vector<int>>& map, const Position & map_center);
    Position setRepulsionField(const vector<vector<int>>& field, const Position & map_center);

    // causes a unit to move directly towards the enemy base.
    Position scoutEnemyBase(Map_Inventory & inv);
    // causes a unit to seperate itself from others.
    Position setSeperation(const Unit_Inventory &ui );
    // causes a unit to seperate itself from others at a distance of its own vision.
    Position setSeperationScout(const Unit_Inventory & ui);
    //void setUnwalkability( const Unit &unit, const Position &pos);
    // Causes a unit to avoid units in its distant future, near future, and immediate position.
    Position setObjectAvoid(const Position &current_pos, const Position &future_pos, const vector<vector<int>> &map);
    bool adjust_lurker_burrow(const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target);

    // gives a vector that has the direction towards center on (map). returns a direction.
    Position getVectorTowardsMap(const vector<vector<int>>& map) const;
    // gives a vector that has the direction towards higher values on the field.  returns a direction.
    Position getVectorTowardsField(const vector<vector<int>>& field) const;
    // gives a vector that has the direction towards lower values on the field.  returns a direction.
    Position getVectorAwayField(const vector<vector<int>>& field) const;


    Mobility::Mobility(const Unit &unit) {
        unit_ = unit;
        pos_ = unit->getPosition();
        distance_metric_ = CUNYAIModule::getProperSpeed(unit) * 24.0; // in pixels
    };

private:
    Position pos_;
    Unit unit_;
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

    int rng_direction_ ; // send unit in a random tilt direction if blocked

    bool move_to_next(const BWEM::CPPath &cpp) const;
};
