# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std; 



void MeatAIModule::Expo( Unit unit ) {
    if ( Broodwar->self()->minerals() >= 300 ) {
        int dist = 999999;
            int* dist_ptr = &dist;
        size_t local_min_threshold = 1;
            size_t* local_min_threshold_ptr = &local_min_threshold;
        int build_order = false;

        Unitset min_set = Broodwar->getMinerals();
        for ( auto min = min_set.begin(); min != min_set.end(); ++min ) { // search for closest resource group. They are our potential expos.

            if ( (*min) && (*min)->exists() ) {

                TilePosition min_pos_t = (*min)->getTilePosition();

                for ( auto tile_x = min_pos_t.x - 7; tile_x != min_pos_t.x + 7; ++tile_x ) {
                    for ( auto tile_y = min_pos_t.y - 7 ; tile_y != min_pos_t.y + 7 ; ++tile_y ) { //my math says x-7 and x+4, and y-6 and y+7 are the last funtional build locations, since hatcheries are 4x3.  My math is pointless, use the CPU to check.

                        if ( (tile_x > min_pos_t.x + 3 || tile_x < min_pos_t.x - 3) ||
                            (tile_y > min_pos_t.y + 3 || tile_y < min_pos_t.y - 3) ) {

                            bool buildable = Broodwar->canBuildHere( { tile_x, tile_y }, UnitTypes::Zerg_Hatchery, unit );  
                            
                            Position center = { tile_x * 32 + UnitTypes::Zerg_Hatchery.dimensionLeft() , tile_y  * 32 + UnitTypes::Zerg_Hatchery.dimensionUp() }; // The build location is upper left tile of the building. The true center is further along in both x and y dimensions. The upper left tile is 0,0. 

                            if ( buildable ) {
                                Broodwar->drawCircleMap( center, 10, Colors::Green );
                            }
                            else {
                                Broodwar->drawCircleMap( center, 10, Colors::Red );
                            }

                            int walk = unit->getDistance( center );

                            Unitset bases = Broodwar->getUnitsInRadius(center, 500, IsResourceDepot && IsOwned);
                            Unitset local_min = Broodwar->getUnitsInRadius( center, 160, IsRefinery || IsResourceContainer ); //includes gas now. 96 pixels is minimum distance, you also must leave the body of the hatchery, so about 160 pixels should work. Will be too distant if vertically above or below the mineral line. Left-right is ok though.

                            if ( buildable && bases.empty() /*&& walk <= dist + 250*/ && local_min.size() >= local_min_threshold ) { // walk function has some substantial tolarances built in.
                                *local_min_threshold_ptr = local_min.size(); // the most minerals.
                                //if ( walk < dist ) {
                                //    *dist_ptr = walk;  // a tolarable walk to the expo.
                                //}
                                unit->build( UnitTypes::Zerg_Hatchery, { tile_x, tile_y } );
                                build_order = true;
                            }
                        }
                    }
                }
            }
        }

        if ( build_order )
        {
            t_build += 150;
        }

    }
}


void MeatAIModule::Worker_Mine( Unit unit ) {

    if ( isIdleEmpty( unit ) ) {
        Unit anchor_min = unit->getClosestUnit( IsMineralField );
        if ( anchor_min && anchor_min->exists() ) {
            unit->gather( anchor_min ); // if you are idle, get to work.
        }
    } // stopgap command.

    Unit local_base = unit->getClosestUnit( IsResourceDepot && IsOwned && IsCompleted );

    if ( local_base && local_base->exists() ) {

            Unitset local_sat = local_base->getUnitsInRadius( 250, IsWorker && (IsGatheringMinerals || IsCarryingMinerals) );
            Unitset local_gas = local_base->getUnitsInRadius( 250, IsWorker && (IsGatheringGas || IsCarryingGas) );
            Unitset local_min = local_base->getUnitsInRadius( 250, IsMineralField );
            int local_dist = unit->getDistance( local_base );
            Broodwar->drawTextMap( local_base->getPosition(), "Local Minerals: %d, Local Miners: %d, Local Gas: %d", (int)local_min.size(), (int)local_sat.size(), (int)local_gas.size() );

            bool acceptable_local = local_dist < 250 && (int)local_sat.size() < (int)local_min.size() * 2.5; // if local conditions are fine (and you are working), continue, no commands.

            int nearest_dist = 9999999;

            if ( !acceptable_local && rand() % 100 + 1 < 25 ) { // if local conditions are bad, we will consider tranfering 25% of these workers elsewhere.
                Unitset bases = unit->getUnitsInRadius( 999999, IsResourceDepot && IsOwned );
                for ( auto base = bases.begin(); base != bases.end() && bases.empty(); ++base ) {
                    if ( (*base)->exists() ) {
                        int dist = unit->getDistance( *base );
                        if ( dist > 500 && dist < 9999999 ) {
                            nearest_dist = dist; // transfer to the nearest undersaturated base.
                            Unitset base_sat = (*base)->getUnitsInRadius( 250, IsWorker && (IsGatheringMinerals || IsCarryingMinerals) );
                            Unitset base_gas = (*base)->getUnitsInRadius( 250, IsWorker && (IsGatheringGas || IsCarryingGas) );
                            Unitset base_min = (*base)->getUnitsInRadius( 250, IsMineralField );
                            bool acceptable_foreign = !base_min.empty() && (int)base_sat.size() < (int)base_min.size() ; // there must be a severe desparity to switch, and please only switch to ones clearly different than your local mine, 250 p.
                            Broodwar->drawTextMap( (*base)->getPosition(), "Local Minerals: %d, Local Miners: %d, Local Gas: %d", (int)base_min.size(), (int)base_sat.size(), (int)base_gas.size() );
                            if ( acceptable_foreign ) {
                                Unit anchor_min = (*base)->getClosestUnit( IsMineralField );
                                if ( anchor_min && anchor_min->exists() ) {
                                    unit->gather( anchor_min );
                                }
                            } // closure act of transferring drones themselves.
                        } // closure for base being a canidate for transfer.
                    } // closure for base existance.
                } // iterate through all possible bases
            } // closure for worker transfer
        } // closure safety check for existance of local base.
} // closure worker mine