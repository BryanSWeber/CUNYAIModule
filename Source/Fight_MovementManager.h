#pragma once

#include "CUNYAIModule.h"
#include "Unit_Inventory.h"

//Movement and Combat Functions
class Mobility {

public:
    // Basic retreat logic
    void Retreat_Logic( const Unit &unit, const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, Inventory &inventory, const Color &color );
    // Tells the unit to fight. If it can attack both air and ground.
    void Tactical_Logic( const Unit & unit, const Unit_Inventory & ei, const Unit_Inventory &ui, const Inventory &inv, const Color & color );
    //Forces a unit to flock in a (previously) Mobility manner. Will attack if it sees something.
    void Mobility_Movement( const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inventory, const bool &army_starved, const bool &potential_fears);

    //Forces the closest Overlord or Zergling to lock itself onto a mineral patch so I can keep vision of it. Helps expos.
    //void Vision_Locking( const Unit &unit );

    Position Output;

    // Causes a unit to match headings with neighboring units.
    void setAlignment( const Unit &unit, const Unit_Inventory &ui );
    // Causes UNIT to run directly from enemy.
    void setDirectRetreat(const Position & pos, const Position &e_pos, const UnitType & type);
    // Causes a unit to move towards central map veins.
    void setCentralize( const Position &pos, const Inventory &inventory );
    // causes a unit to move about in a random (brownian) fashion.
    void setStutter( const Unit &unit, const double &n );
    // Causes a unit to be pulled towards others of their kind.
    void setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    // causes a unit to be pulled towards enemy base.
    void setAttractionEnemy( const Unit &unit, const Position &pos, Unit_Inventory &ei, const Inventory & inv, const bool &potential_fears);
    // causes a unit to move directly towards the enemy base.
    void scoutEnemyBase(const Unit & unit, const Position & pos, Inventory & inv);
    // causes a unit to be pulled homeward.
    void setAttractionHome( const Unit & unit, const Position & pos, const Unit_Inventory & ei, const Inventory & inv);
    // causes a unit to seperate itself from others.
    void setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui );
    // causes a unit to seperate itself from others at a distance of its own vision.
    void setSeperationScout(const Unit & unit, const Position & pos, const Unit_Inventory & ui);
    //void setUnwalkability( const Unit &unit, const Position &pos, const Inventory &inventory );
    // 
    void setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory );

    bool Mobility::adjust_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target);

    // gives a vector that has the direction on ground towards home.
    vector<double> getVectorTowardsHome(const Position & pos, const Inventory & inv) const;
    // gives a vector that has the direction on ground towards enemy.
    vector<double> getVectorTowardsEnemy(const Position & pos, const Inventory & inv) const;

private:
    int distance_metric = 0;
    double x_stutter_ = 0;
    double y_stutter_ = 0;
    double attune_dx_ = 0;
    double attune_dy_ = 0;
    double cohesion_dx_ = 0;
    double cohesion_dy_ = 0;
    double centralization_dx_ = 0;
    double centralization_dy_ = 0;
    double seperation_dx_ = 0;
    double seperation_dy_ = 0;
    double attract_dx_ = 0;
    double attract_dy_ = 0;
    double retreat_dx_ = 0;
    double retreat_dy_ = 0;
    double walkability_dx_ = 0;
    double walkability_dy_ = 0;

    int rng_direction_ ; // send unit in a random tilt direction if blocked

};
