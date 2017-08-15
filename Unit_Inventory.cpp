#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\Unit_Inventory.h"


//Unit_Inventory functions.
//Creates an instance of the enemy inventory class.

Unit_Inventory::Unit_Inventory(){}

Unit_Inventory::Unit_Inventory(const Unitset &unit_set) {
    
    for ( const auto & u : unit_set) {
        unit_inventory_.insert( { u, Stored_Unit( u ) } );
    }
    updateUnitInventorySummary(); //this call is a CPU sink.
}

// Updates the count of enemy units.
void Unit_Inventory::addStored_Unit( Unit unit ) {
    unit_inventory_.insert( { unit, Stored_Unit( unit ) } );
};

void Unit_Inventory::addStored_Unit( Stored_Unit stored_unit ) {
    unit_inventory_.insert( { stored_unit.bwapi_unit_ , stored_unit } );
};


//Removes enemy units that have died
void Unit_Inventory::removeStored_Unit( Unit e_unit ) {
    unit_inventory_.erase( e_unit );
};

// Checks if we already have enemy unit. If so, we remove it and replace it with the updated version.
void Unit_Inventory::updateStored_Unit( Unit e_unit ) {
    auto key = unit_inventory_.find(e_unit);
    //Did we find it? If so, let's update it.
    if ( key == unit_inventory_.end() ) {
        unit_inventory_.insert( { e_unit, Stored_Unit( e_unit ) } );
    }
    else {
        unit_inventory_.erase( e_unit );
    }
}

 Position Unit_Inventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    for ( const auto &u : this->unit_inventory_ ) {
        x_sum += u.second.pos_.x;
        y_sum += u.second.pos_.y;
        count++;
    }
    Position out = { x_sum / count, y_sum / count };
    return out ;
}

void Unit_Inventory::updateUnitInventorySummary() {
    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?
    int fliers = 0;
    int ground_unit = 0;
    int cannot_shoot_up = 0;
    int cannot_shoot_down = 0;
    int high_ground = 0;
    int range = 0;
    vector<UnitType> already_seen_types;

    for ( auto const & u_iter : unit_inventory_ ) { // should only search through unit types not per unit.
        if ( find( already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_ ) == already_seen_types.end() ) { // if you haven't already checked this unit type.
            if ( u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_.maxEnergy() > 0 ) {

                if ( u_iter.second.type_.isFlyer() ) {
                    fliers += MeatAIModule::Stock_Units( u_iter.second.type_, *this ); // add the value of that type of unit to the flier stock.
                }
                else {
                    ground_unit += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.airWeapon() == WeaponTypes::None ) {
                    cannot_shoot_up += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.groundWeapon() == WeaponTypes::None ) {
                    cannot_shoot_down += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.groundWeapon().maxRange() > range || u_iter.second.type_.airWeapon().maxRange() > range ) {
                    range = u_iter.second.type_.groundWeapon().maxRange() > u_iter.second.type_.airWeapon().maxRange() ? u_iter.second.type_.groundWeapon().maxRange() : u_iter.second.type_.airWeapon().maxRange();
                }
                already_seen_types.push_back( u_iter.second.type_ );
            }
            Region r = Broodwar->getRegionAt( u_iter.second.pos_ );
            if ( r && u_iter.second.valid_pos_ && u_iter.second.type_ != UnitTypes::Buildings ) {
                if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
                    high_ground += u_iter.second.current_stock_value_;
                }
            }
        }
    } // get closest unit in inventory.

    stock_fliers_ = fliers;
    stock_ground_units_ = ground_unit;
    stock_cannot_shoot_up_ = cannot_shoot_up;
    stock_cannot_shoot_down_ = stock_cannot_shoot_down_;
    stock_high_ground_= high_ground;
    stock_total_ = stock_ground_units_ + stock_fliers_;
    max_range_ = range;
}

//Stored_Unit functions.
// We must be able to create Stored_Unit objects as well.
Stored_Unit::Stored_Unit( Unit unit ) {
    valid_pos_ = true;
    unit_ID_ = unit->getID();
    bwapi_unit_ = unit;
    pos_ = unit->getPosition();
    type_ = unit->getType();
    build_type_ = unit->getBuildType();
    current_hp_ = unit->getHitPoints();

    //Get unit's status. Precalculated, precached.
    stock_value_ = sqrt( pow( unit->getType().mineralPrice(), 2 ) + pow( 1.25 * unit->getType().gasPrice(), 2 ) + pow( 25 * unit->getType().supplyRequired(), 2 ) ); 
    if ( unit->getType().isTwoUnitsInOneEgg() ) {
        stock_value_ = stock_value_ / 2;
    }
    current_stock_value_ = stock_value_ * (double)current_hp_ / (double)(unit->getType().maxHitPoints()) ; // Precalculated, precached.
}

