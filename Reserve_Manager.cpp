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

void Reservation::addReserveSystem( UnitType type, TilePosition pos ) {
    map<UnitType, TilePosition>::iterator it = reservation_map_.find( type );
    if ( it == reservation_map_.end() ) {
        min_reserve_ += type.mineralPrice();
        gas_reserve_ += type.gasPrice();
        reservation_map_.insert( { type, pos } );
        building_timer_ = type.buildTime() > building_timer_ ? type.buildTime() : building_timer_;
    }
    last_builder_sent_ = Broodwar->getFrameCount();
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
    return Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
}

bool Reservation::checkAffordablePurchase( const UpgradeType type ) {
    return Broodwar->self()->minerals() - min_reserve_ >= type.mineralPrice() && Broodwar->self()->gas() - gas_reserve_ >= type.gasPrice();
}