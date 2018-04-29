#pragma once
#include "MeatAIModule.h"
#include "InventoryManager.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;

class Build_Order_Object {
private:

    UnitType _unit_in_queue;
    UpgradeType _upgrade_in_queue;
    TechType _research_in_queue;

public:
    //bool operator==( const Build_Order_Object &rhs );
    //bool operator!=( const Build_Order_Object &rhs );

    Build_Order_Object( UnitType unit ) {
        _unit_in_queue = unit;
        _upgrade_in_queue = UpgradeTypes::None;
        _research_in_queue = TechTypes::None;
    };

    Build_Order_Object( UpgradeType up ) {
        _unit_in_queue = UnitTypes::None;
        _upgrade_in_queue = up;
        _research_in_queue = TechTypes::None;
    };

    Build_Order_Object( TechType tech ) {
        _unit_in_queue = UnitTypes::None;
        _upgrade_in_queue = UpgradeTypes::None;
        _research_in_queue = tech;
    };

    UnitType Build_Order_Object::getUnit() {
            return _unit_in_queue;
    };

    UpgradeType Build_Order_Object::getUpgrade() {
            return _upgrade_in_queue;
    };

    TechType Build_Order_Object::getResearch() {
        return _research_in_queue;
    };
};


//template<typename T>
//class Build_Order_Object {
//private:
//
//    UnitType _unit_in_queue;
//    UpgradeType _upgrade_in_queue;
//    TechType _research_in_queue;
//
//public:
//    _unit_in_queue = typename;
//    _upgrade_in_queue = UpgradeTypes::None;
//    _research_in_queue = TechTypes::None;
//}



struct Building_Gene {
    Building_Gene();
    Building_Gene( string s );

    vector<Build_Order_Object> building_gene_;  // how many of each of these do we want? Base build is going to be rushing mutalisk.
    string initial_building_gene_;

    bool ever_clear_ = false;
    UnitType last_build_order;

    void getInitialBuildOrder(string s);
    void updateRemainingBuildOrder( const Unit &u ); // drops item from list as complete.
    void updateRemainingBuildOrder( const UpgradeType &ups ); // drops item from list as complete.
    void updateRemainingBuildOrder( const TechType & research );// drops item from list as complete.
	void updateRemainingBuildOrder( const UnitType &ut ); // drops item from list as complete.
    void clearRemainingBuildOrder(); // empties the build order.
    void announceBuildingAttempt( UnitType ut );  // do we have a guy going to build it?
    bool checkBuilding_Desired( UnitType ut ); 
    bool checkUpgrade_Desired( UpgradeType upgrade );
    bool checkResearch_Desired( TechType upgrade );
    bool checkEmptyBuildOrder();
    //bool checkExistsInBuild( UnitType unit );
};

