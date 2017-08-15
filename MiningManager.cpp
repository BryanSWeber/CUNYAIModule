#pragma once
# include "Source\MeatAIModule.h"

using namespace BWAPI;
using namespace Filter;
using namespace std;


//Builds an expansion. No recognition of past build sites. Needs a drone=unit, some extra boolian logic that you might need, and your inventory, containing resource locations.
void MeatAIModule::Expo( const Unit &unit, const bool &extra_critera, const Inventory &inv) {
    if ( Broodwar->self()->minerals() >= 300 && (buildorder.checkBuilding_Desired( UnitTypes::Zerg_Hatchery ) || (extra_critera && buildorder.checkEmptyBuildOrder() && !buildorder.active_builders_ )) ) {
        
        if ( inv.acceptable_expo_ )
        {
            //clear all obstructions, if any.
            Unitset obstructions = Broodwar->getUnitsInRadius( { inv.next_expo_.x * 32, inv.next_expo_.y * 32 }, 4 * 32 );
            obstructions.move( { inv.next_expo_.x * 32 + (rand() % 200 - 100) * 5 * 32, inv.next_expo_.y * 32 + (rand() % 200 - 100) * 5 * 32 } );
            obstructions.build( UnitTypes::Zerg_Hatchery, inv.next_expo_ );// I think some thing quirky is happening here. Sometimes gets caught in rebuilding loop.

            if ( unit->build( UnitTypes::Zerg_Hatchery, inv.next_expo_ ) ) {
                buildorder.setBuilding_Complete( UnitTypes::Zerg_Hatchery );
                Broodwar->sendText( "Expoing at ( %d , %d ).", inv.next_expo_.x, inv.next_expo_.y );
            }
            else if ( !Broodwar->isExplored( inv.next_expo_ ) ) {
                unit->move( { inv.next_expo_.x * 32 + UnitTypes::Zerg_Hatchery.dimensionUp() , inv.next_expo_.y * 32 + UnitTypes::Zerg_Hatchery.dimensionLeft() } );
                buildorder.active_builders_ = true;
                Broodwar->sendText( "Unexplored Expo at ( %d , %d ). Moving there to check it out.", inv.next_expo_.x , inv.next_expo_.y );
                Broodwar->drawLineMap( { inv.next_expo_.x * 32 + UnitTypes::Zerg_Hatchery.dimensionUp() , inv.next_expo_.y * 32 + UnitTypes::Zerg_Hatchery.dimensionLeft() } , unit->getPosition(), Colors::White);
            }
        }
    } // closure affordablity.
}

//Sends a worker to mine minerals.
void MeatAIModule::Worker_Mine( const Unit &unit ) {

    if ( isIdleEmpty( unit ) ) {
        Unit anchor_min = unit->getClosestUnit( IsMineralField );
        if ( anchor_min && anchor_min->exists() ) {
            unit->gather( anchor_min ); // if you are idle, get to work.
        }
    } // stopgap command.

    Unit local_base = unit->getClosestUnit( IsResourceDepot && IsOwned && IsCompleted );

    if ( local_base && local_base->exists() ) {

            Unitset local_sat = local_base->getUnitsInRadius( 250, IsWorker && (IsGatheringMinerals || IsCarryingMinerals) );
            Unitset local_min = local_base->getUnitsInRadius( 250, IsMineralField );
            int local_dist = unit->getDistance( local_base );

            bool acceptable_local = local_dist < 500 && (int)local_sat.size() < (int)local_min.size() * 1.75; // if local conditions are fine (and you are working), continue, no commands.

            if ( !acceptable_local && rand() % 100 + 1 < 25 ) { // if local conditions are bad, we will consider tranfering 25% of these workers elsewhere.
                Unitset bases = unit->getUnitsInRadius( 999999, IsResourceDepot && IsOwned );

                int nearest_dist = 9999999;
                for ( auto base = bases.begin(); base != bases.end() && !bases.empty(); ++base ) {
                    if ( (*base)->exists() ) {
                        int dist = unit->getDistance( *base );

                        Unitset base_sat = (*base)->getUnitsInRadius( 500, IsWorker && (IsGatheringMinerals || IsCarryingMinerals) );
                        Unitset base_min = (*base)->getUnitsInRadius( 500, IsMineralField );
                        bool acceptable_foreign = !base_min.empty() && (int)base_sat.size() < (int)base_min.size(); // there must be a severe desparity to switch, and please only switch to ones clearly different than your local mine, 250 p.

                        if ( dist > 750 && dist < nearest_dist && acceptable_foreign) {
                            nearest_dist = dist; // transfer to the nearest undersaturated base.
                                Unit anchor_min = (*base)->getClosestUnit( IsMineralField );
                                if ( anchor_min && anchor_min->exists() ) {
                                    unit->gather( anchor_min );
                                }// closure act of transferring drones themselves.
                        } // closure for base being a canidate for transfer.
                    } // closure for base existance.
                } // iterate through all possible bases
            } // closure for worker transfer
        } // closure safety check for existance of local base.
} // closure worker mine

//Sends a Worker to gather Gas.
void MeatAIModule::Worker_Gas( const Unit &unit ) {

    if ( isIdleEmpty( unit ) ) {
        Unit anchor_gas = unit->getClosestUnit( IsRefinery );
        if ( anchor_gas && anchor_gas->exists() ) {
            unit->gather( anchor_gas ); // if you are idle, get to work.
        }
    } // stopgap command.

    Unit local_base = unit->getClosestUnit( IsResourceDepot && IsOwned && IsCompleted );

    if ( local_base && local_base->exists() ) {

        Unitset local_gas = local_base->getUnitsInRadius( 250, IsWorker && (IsGatheringGas || IsCarryingGas) );
        Unitset local_refinery = local_base->getUnitsInRadius( 250, IsRefinery );
        int local_dist = unit->getDistance( local_base );

        bool acceptable_local = local_dist < 500 && (int)local_gas.size() <= (int)local_refinery.size() * 3; // if local conditions are fine (and you are working), continue, no commands.

        if ( !acceptable_local && rand() % 100 + 1 < 25 ) { // if local conditions are bad, we will consider tranfering 25% of these workers elsewhere.
            Unitset bases = unit->getUnitsInRadius( 999999, IsResourceDepot && IsOwned );
            int nearest_dist = 9999999;

            for ( auto base = bases.begin(); base != bases.end() && !bases.empty(); ++base ) {
                if ( (*base)->exists() ) {
                    int dist = unit->getDistance( *base );

                        Unitset base_gas = (*base)->getUnitsInRadius( 500, IsWorker && (IsGatheringGas || IsCarryingGas) );
                        Unitset base_refinery = (*base)->getUnitsInRadius( 500, IsRefinery );
                        bool acceptable_foreign = !base_refinery.empty() && (int)base_gas.size() <= (int)base_refinery.size(); // there must be a severe desparity to switch, and please only switch to ones clearly different than your local mine, 250 p.

                        if ( dist > 750 && dist < nearest_dist && acceptable_foreign ) {
                            nearest_dist = dist; // transfer to the nearest undersaturated base.
                            Unit anchor_gas = (*base)->getClosestUnit( IsRefinery );
                            if ( anchor_gas && anchor_gas->exists() ) {
                                unit->gather( anchor_gas );
                            }
                        } // closure act of transferring drones themselves.
                } // closure for base existance.
            } // iterate through all possible bases
        } // closure for worker transfer
    } // closure safety check for existance of local base.
} // closure worker mine

//Returns True if there is an out for gas. Does not consider all possible gas outlets.
bool MeatAIModule::Gas_Outlet() {
    bool outlet_avail = false;

    if ( MeatAIModule::Tech_Avail()/* && tech_starved*/ && Count_Units( BWAPI::UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0 ) {
        outlet_avail = true;
    }

    //for ( auto & u : BWAPI::Broodwar->self()->getUnits() ) {
    //    if ( u->getType()== UnitTypes::Zerg_Larva ) {
    //        bool long_condition = u->canMorph( UnitTypes::Zerg_Hydralisk ) ||
    //                       u->canMorph( UnitTypes::Zerg_Mutalisk ) ||
    //                       u->canMorph( UnitTypes::Zerg_Ultralisk );
    //        if ( long_condition ) {
    //            outlet_avail = true;
    //        }
    //    }
    //} // turns off gas interest when larve are 0.

    bool long_condition = Count_Units(UnitTypes::Zerg_Hydralisk_Den, friendly_inventory) > 0 ||
        Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) > 0 ||
        Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) > 0;
    if ( long_condition ) {
        outlet_avail = true;
    } 

    return outlet_avail;
}

