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

public:

    Build_Order_Object( UnitType unit ) {
        _unit_in_queue = unit;
        _upgrade_in_queue = UpgradeTypes::None;
    };

    Build_Order_Object( UpgradeType up ) {
        _unit_in_queue = UnitTypes::None;
        _upgrade_in_queue = up;
    };

    UnitType Build_Order_Object::getUnit() {
            return _unit_in_queue;
    };

    UpgradeType Build_Order_Object::getUpgrade() {
            return _upgrade_in_queue;
    };
};

struct Building_Gene {
    vector<Build_Order_Object> building_gene_;  // how many of each of these do we want? Base build is going to be rushing mutalisk.

    Building_Gene() { // unspecified items are unrestricted.

        Build_Order_Object hatch = Build_Order_Object( UnitTypes::Zerg_Hatchery);
        Build_Order_Object extract = Build_Order_Object( UnitTypes::Zerg_Extractor);
        Build_Order_Object pool = Build_Order_Object( UnitTypes::Zerg_Spawning_Pool);
        Build_Order_Object speed = Build_Order_Object( UpgradeTypes::Metabolic_Boost );
        Build_Order_Object ling = Build_Order_Object( UnitTypes::Zerg_Zergling);
        Build_Order_Object creep = Build_Order_Object( UnitTypes::Zerg_Creep_Colony);
        Build_Order_Object sunken = Build_Order_Object( UnitTypes::Zerg_Sunken_Colony );
        Build_Order_Object lair = Build_Order_Object( UnitTypes::Zerg_Lair );
        Build_Order_Object spire = Build_Order_Object( UnitTypes::Zerg_Spire );
        Build_Order_Object muta = Build_Order_Object( UnitTypes::Zerg_Mutalisk );

        building_gene_.push_back( hatch );
        building_gene_.push_back( pool );
        //building_gene_.push_back( extract );
        //building_gene_.push_back( lair );
        //building_gene_.push_back( speed );
        //building_gene_.push_back( creep );
        //building_gene_.push_back( creep );
        //building_gene_.push_back( sunken );
        //building_gene_.push_back( sunken ); // doesn't work.
        //building_gene_.push_back( ling );
        //building_gene_.push_back( spire );
        //building_gene_.push_back( muta );
        //building_gene_.push_back( muta );
        //building_gene_.push_back( muta );
        //building_gene_.push_back( muta );
        //building_gene_.push_back( muta );
        //building_gene_.push_back( muta );
    } // for zerg 9 pool speed into 3 hatch muta, T: 3 hatch muta, P: 10 hatch into 3 hatch hydra bust.

    bool active_builders_= false;
    int building_timer_ = 0;
    UnitType last_build_order;

    void updateBuildingTimer( const Unit_Inventory &ui ); //adds time to timer.
    void updateRemainingBuildOrder( const Unit &u ); // drops item from list as complete.
    void updateRemainingBuildOrder( const UpgradeType &ups ); // drops item from list as complete.
	void updateRemainingBuildOrder(const UnitType &ut); // drops item from list as complete.
    void setBuilding_Complete( UnitType ut );  // do we have a guy going to build it?
    bool checkBuilding_Desired( UnitType ut ); 
    bool checkUpgrade_Desired( UpgradeType upgrade );
    bool checkEmptyBuildOrder();

};

