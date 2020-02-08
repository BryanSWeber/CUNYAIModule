#pragma once

// Keeps track of each base and manages creation of creep colonies.

#include <BWAPI.h>
#include "CUNYAIModule.h"

class Base {
public:
    Base::Base();
    Base::Base(const Unit &u);
    //bool gas_taken_;
    //bool gas_tolerable_;
    int spore_count_;
    int sunken_count_;
    int creep_count_;
    int gas_gatherers_;
    int mineral_gatherers_;
    int returners_; // Those returning minerals/gas.
    int overlords_;
    int distance_to_ground_; // Very good pixel ground distance approximation to nearest ground unit.
    int distance_to_air_; // Pixel distance to nearest air unit, as the crow flies.
    bool emergency_spore_; //Do we want an emergency spore?
    bool emergency_sunken_; // Do we want an emerency sunken?
    Unit unit_; // Directly to the base hatchery itself.
    bool checkHasGroundBuffer(const Position& threat_pos); // checks if there is another base on the ground path between the enemy and this base.
};

class BaseManager {
private:
    map<Position, Base> baseMap_; //Map of base positions and their base objects.
public:
    //Returns a copy.
    map<Position, Base> getBases(); //returns the baseMap_
    int getBaseCount(); // Gets base count, must be nonnegative.
    void updateBases(); // Run on frame.
    void displayBaseData(); //vital for testing.
    Base getClosestBaseGround(const Position &pos); // Gets a closest base. will return something but could return a mock "Null base"
    Base getClosestBaseAir(const Position &pos); // Gets a closest base. will return something but could return a mock "Null base"
    string test;
};
