#pragma once
#include <BWAPI.h>
#include "CUNYAIModule.h"

class Base {
public:
    Base::Base();
    Base::Base(const Unit &u);
    bool gas_taken_;
    bool gas_tolerable_;
    bool air_weak_;
    bool ground_weak_;
    int spore_count_;
    int sunken_count_;
    int creep_count_;
    int gas_gatherers_;
    int mineral_gatherers_;
    Unit unit_;
};

class BaseManager {
private:
    map<Position, Base> baseMap_;
public:
    void updateBases();
    void displayBaseData(); //vital for testing.
    Base getClosestBaseGround(const Position &pos);
    Base getClosestBaseAir(const Position &pos);
    string test;
};
