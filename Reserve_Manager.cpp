#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\InventoryManager.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"
#include "Source\Reservation_Manager.h"
#include <algorithm>

using namespace std;
using namespace BWAPI;

Reservation::Reservation() {
    min_reserve_ = 0;
    gas_reserve_ = 0;
    building_timer_ = 0;
    last_builder_sent_ = 0;
}

bool Reservation::addReserveSystem( UnitType type, TilePosition pos ) {
    bool safe = true;
    map<UnitType, TilePosition>::iterator it = reservation_map_.find( type );
    if ( it == reservation_map_.end() ) {
        min_reserve_ += type.mineralPrice();
        gas_reserve_ += type.gasPrice();
        safe = reservation_map_.insert( { type, pos } ).second;
        building_timer_ = type.buildTime() > building_timer_ ? type.buildTime() : building_timer_;
    }
    last_builder_sent_ = Broodwar->getFrameCount();

    return safe;
}

void Reservation::removeReserveSystem( UnitType type ) {
    if ( reservation_map_.empty() ) {
        min_reserve_ = 0;
        gas_reserve_ = 0;
    }
    else {
        map<UnitType, TilePosition>::iterator it = reservation_map_.find( type );
        if ( it != reservation_map_.end() ) {
            reservation_map_.erase( type );
            min_reserve_ -= type.mineralPrice();
            gas_reserve_ -= type.gasPrice();
        }
        else if ( type != UnitTypes::None ) {
            Broodwar->sendText( "We're trying to remove %s from the reservation queue but can't find it.", type.c_str() );
        }
    }
};


void Reservation::decrementReserveTimer() {
    if ( Broodwar->getFrameCount() == 0 ) {
        building_timer_ = 0;
    }
    else {
        building_timer_ > 0 ? --building_timer_ : 0;
    }
}


bool Reservation::checkAffordablePurchase( const UnitType type ) {
    bool affordable = Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
    bool open_reservation = reservation_map_.find(type)==reservation_map_.end();
    return affordable && open_reservation;
}

bool Reservation::checkAffordablePurchase( const UpgradeType type ) {
    return Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
}

void Reservation::confirmOngoingReservations( const Unit_Inventory &ui) {

    for (auto res_it = reservation_map_.begin(); res_it != reservation_map_.end() && !reservation_map_.empty(); ) {
        bool keep = false;

        for ( auto unit_it = ui.unit_inventory_.begin(); unit_it != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); unit_it++ ) {
            if ( res_it->second == TilePosition( unit_it->second.bwapi_unit_->getLastCommand().getTargetPosition() ) ) {
                keep = true;
            }
        } // check if we have a unit building it.

        if ( keep ) {
            ++res_it;
        }
        else {
            Broodwar->sendText( "No worker is building this reserved %s. Freeing up the $", res_it->first.c_str() );
            UnitType remove_me = res_it->first;
            res_it++;
            removeReserveSystem( remove_me );  // contains an erase.
        }
    }
    
    if ( !reservation_map_.empty() && last_builder_sent_ < Broodwar->getFrameCount() - 30 * 24) {
        Broodwar->sendText( "...We're stuck, aren't we? Have a friendly nudge." );
        reservation_map_.clear();
        min_reserve_ = 0;
        gas_reserve_ = 0;
    }
}