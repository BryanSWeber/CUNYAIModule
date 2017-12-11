#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\Unit_Inventory.h"


//Unit_Inventory functions.
//Creates an instance of the unit inventory class.

Unit_Inventory::Unit_Inventory(){}

Unit_Inventory::Unit_Inventory( const Unitset &unit_set) {

	for (const auto & u : unit_set) {
		unit_inventory_.insert({ u, Stored_Unit(u) });
	}

    updateUnitInventorySummary(); //this call is a CPU sink.
}

void Unit_Inventory::updateUnitInventory(const Unitset &unit_set){
		if (unit_inventory_.empty()){ // it thinks it's ALWAYS empty.
			for (const auto & u : unit_set) {
				unit_inventory_.insert({ u, Stored_Unit(u) });
			}
		}
		else {
			for (const auto & u : unit_set) {
				if (unit_inventory_.count(u) > 0){
					unit_inventory_.find(u)->second.updateStoredUnit(u); // explicitly does not change locked mineral.
				}
				else {
					unit_inventory_.insert({ u, Stored_Unit(u) });
				}
			}
		}
		updateUnitInventorySummary(); //this call is a CPU sink.
}

// Updates the count of units.
void Unit_Inventory::addStored_Unit( Unit unit ) {
    unit_inventory_.insert( { unit, Stored_Unit( unit ) } );
};

void Unit_Inventory::addStored_Unit( Stored_Unit stored_unit ) {
    unit_inventory_.insert( { stored_unit.bwapi_unit_ , stored_unit } );
};

void Stored_Unit::updateStoredUnit(const Unit &unit){

		valid_pos_ = true;
		pos_ = unit->getPosition();
		type_ = unit->getType();
		build_type_ = unit->getBuildType();
		current_hp_ = unit->getHitPoints();

        //Get unit's status. Precalculated, precached.
        int modified_supply = unit->getType().getRace() == Races::Zerg && unit->getType().isBuilding() ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
        modified_supply = unit->getType() == UnitTypes::Terran_Barracks ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Assume bunkers are loaded.
        int modified_min_cost = unit->getType() == UnitTypes::Terran_Barracks ? unit->getType().mineralPrice() + 50 : unit->getType().mineralPrice(); // Assume bunkers are loaded.
        int modified_gas_cost = unit->getType().gasPrice();

        stock_value_ = modified_min_cost + modified_gas_cost + 25 * modified_supply;


        if ( unit->getType().isTwoUnitsInOneEgg() ) {
            stock_value_ = stock_value_ / 2;
        }
		current_stock_value_ = (int)(stock_value_ * (double)current_hp_ / (double)(unit->getType().maxHitPoints())); // Precalculated, precached.
}

//Removes units that have died
void Unit_Inventory::removeStored_Unit( Unit e_unit ) {
    unit_inventory_.erase( e_unit );
};


 Position Unit_Inventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    Position out = Position(0,0);
    for ( const auto &u : this->unit_inventory_ ) {
        x_sum += u.second.pos_.x;
        y_sum += u.second.pos_.y;
        count++;
    }
    if ( count > 0 ) {
        out = Position( x_sum / count, y_sum / count );
    }
    return out;
}

 Position Unit_Inventory::getMeanBuildingLocation() const {
     int x_sum = 0;
     int y_sum = 0;
     int count = 0;
     for ( const auto &u : this->unit_inventory_ ) {
         if ( u.second.type_.isBuilding() && !u.second.type_.isSpecialBuilding() || u.second.bwapi_unit_->isMorphing() ) {
             x_sum += u.second.pos_.x;
             y_sum += u.second.pos_.y;
             count++;
         }
     }
     if ( count > 0 ) {
         Position out = { x_sum / count, y_sum / count };
         return out;
     }
     else {
         return Position( 0, 0 ); // you're dead at this point, fyi.
     }
 }

 Position Unit_Inventory::getMeanCombatLocation() const {
     int x_sum = 0;
     int y_sum = 0;
     int count = 0;
     for ( const auto &u : this->unit_inventory_ ) {
         if ( u.second.type_.canAttack() ) {
             x_sum += u.second.pos_.x;
             y_sum += u.second.pos_.y;
             count++;
         }
     }
     if ( count > 0 ) {
         Position out = { x_sum / count, y_sum / count };
         return out;
     }
     else {
         return Position( 0, 0 );  // you might be dead at this point, fyi.
     }

 }

 Unit_Inventory operator+(const Unit_Inventory& lhs, const Unit_Inventory& rhs)
 {
    Unit_Inventory total;
    total.unit_inventory_.insert(lhs.unit_inventory_.begin(), lhs.unit_inventory_.end());
    total.unit_inventory_.insert(rhs.unit_inventory_.begin(), rhs.unit_inventory_.end());
    total.updateUnitInventorySummary();
    return total;
 }

void Unit_Inventory::updateUnitInventorySummary() {
    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?

    int fliers = 0;
    int ground_unit = 0;
    int shoots_up = 0;
    int shoots_down = 0;
    int high_ground = 0;
    int range = 0;
	int worker_count = 0;
	int volume = 0;
    int detector_count = 0;
    int cloaker_count = 0;
    int max_cooldown = 0;

    vector<UnitType> already_seen_types;

    for ( auto const & u_iter : unit_inventory_ ) { // should only search through unit types not per unit.
        if ( find( already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_ ) == already_seen_types.end() ) { // if you haven't already checked this unit type.
            if ( u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_.maxEnergy() > 0 || u_iter.second.type_ == UnitTypes::Terran_Bunker || u_iter.second.type_ == UnitTypes::Protoss_Carrier || u_iter.second.type_ == UnitTypes::Protoss_Reaver ) {

                if ( u_iter.second.type_.isFlyer() ) {
                    fliers += MeatAIModule::Stock_Units( u_iter.second.type_, *this ); // add the value of that type of unit to the flier stock.
                }
                else {
                    ground_unit += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.airWeapon() != WeaponTypes::None ) {
                    shoots_up += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.groundWeapon() != WeaponTypes::None ) {
                    shoots_down += MeatAIModule::Stock_Units( u_iter.second.type_, *this );
                }

                if ( u_iter.second.type_.groundWeapon().maxRange() > range || u_iter.second.type_.airWeapon().maxRange() > range ) {
                    range = u_iter.second.type_.groundWeapon().maxRange() > u_iter.second.type_.airWeapon().maxRange() ? u_iter.second.type_.groundWeapon().maxRange() : u_iter.second.type_.airWeapon().maxRange();
                }

                if ( u_iter.second.type_.groundWeapon().damageCooldown() > max_cooldown || u_iter.second.type_.airWeapon().damageCooldown() > max_cooldown ) {
                    max_cooldown = u_iter.second.type_.groundWeapon().damageCooldown() > u_iter.second.type_.airWeapon().damageCooldown() ? u_iter.second.type_.groundWeapon().damageCooldown() : u_iter.second.type_.airWeapon().damageCooldown();
                }
				if (u_iter.second.type_ == UnitTypes::Terran_Bunker && 7 * 32 > range){
					range = 7 * 32; // depends on upgrades and unit contents.
				}

                if ( u_iter.second.type_.isDetector() ) {
                    detector_count++;
                }

                if ( u_iter.second.type_.isCloakable() || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_.hasPermanentCloak() ) {
                    cloaker_count++;
                }
                already_seen_types.push_back( u_iter.second.type_ );
            }

			if (!u_iter.second.type_.isFlyer()){
				volume += u_iter.second.type_.height()*u_iter.second.type_.width() * MeatAIModule::Count_Units(u_iter.second.type_, *this);
			}

			Region r = Broodwar->getRegionAt( u_iter.second.pos_ );
            if ( r && u_iter.second.valid_pos_ && u_iter.second.type_ != UnitTypes::Buildings ) {
                if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
                    high_ground += u_iter.second.current_stock_value_;
                }
            }
        }
    } 

	worker_count = MeatAIModule::Count_Units(UnitTypes::Zerg_Drone, *this) + MeatAIModule::Count_Units(UnitTypes::Protoss_Probe, *this) + MeatAIModule::Count_Units(UnitTypes::Terran_SCV, *this);

    stock_fliers_ = fliers;
    stock_ground_units_ = ground_unit;
    stock_shoots_up_ = shoots_up;
    stock_shoots_down_ = shoots_down;
    stock_high_ground_= high_ground;
    stock_total_ = stock_ground_units_ + stock_fliers_;
    max_range_ = range;
    max_cooldown_ = max_cooldown;
	worker_count_ = worker_count;
	volume_ = volume;
    detector_count_ = detector_count;
    cloaker_count_ = cloaker_count;
}

//Stored_Unit functions.
Stored_Unit::Stored_Unit() = default;

//returns a steryotypical unit only.
Stored_Unit::Stored_Unit( const UnitType &unittype ) {
    valid_pos_ = false;
    type_ = unittype;

    //Get unit's status. Precalculated, precached.
    int modified_supply =unittype.getRace() == Races::Zerg &&unittype.isBuilding() ?unittype.supplyRequired() + 2 :unittype.supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
    modified_supply =unittype == UnitTypes::Terran_Barracks ?unittype.supplyRequired() + 2 :unittype.supplyRequired(); // Assume bunkers are loaded.
    int modified_min_cost =unittype == UnitTypes::Terran_Barracks ?unittype.mineralPrice() + 50 :unittype.mineralPrice(); // Assume bunkers are loaded.
    int modified_gas_cost =unittype.gasPrice();

    stock_value_ = modified_min_cost + modified_gas_cost + 25 * modified_supply;

    if (unittype.isTwoUnitsInOneEgg() ) {
        stock_value_ = stock_value_ / 2;
    }

    current_stock_value_ = (int)(stock_value_); // Precalculated, precached.

};
// We must be able to create Stored_Unit objects as well.
Stored_Unit::Stored_Unit( Unit unit ) {
    valid_pos_ = true;
    unit_ID_ = unit->getID();
    bwapi_unit_ = unit;
    pos_ = unit->getPosition();
    type_ = unit->getType();
    build_type_ = unit->getBuildType();
    current_hp_ = unit->getHitPoints();
	locked_mine_ = nullptr;

    //Get unit's status. Precalculated, precached.
    int modified_supply = unit->getType().getRace() == Races::Zerg && unit->getType().isBuilding() ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
    modified_supply = unit->getType() == UnitTypes::Terran_Barracks ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Assume bunkers are loaded.
    int modified_min_cost = unit->getType() == UnitTypes::Terran_Barracks ? unit->getType().mineralPrice() + 50 : unit->getType().mineralPrice(); // Assume bunkers are loaded.
    int modified_gas_cost = unit->getType().gasPrice();

    stock_value_ = modified_min_cost + 1.25 * modified_gas_cost + 25 * modified_supply;

    if ( unit->getType().isTwoUnitsInOneEgg() ) {
        stock_value_ = stock_value_ / 2;
    }

    current_stock_value_ = (int)(stock_value_ * (double)current_hp_ / (double)(unit->getType().maxHitPoints())) ; // Precalculated, precached.
}

void Stored_Unit::startMine(Stored_Resource &new_resource, Resource_Inventory &ri){
	locked_mine_ = new_resource.bwapi_unit_;
	ri.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}


void Stored_Unit::stopMine(Resource_Inventory &ri){
	if (locked_mine_ && locked_mine_->exists()){
		map<Unit, Stored_Resource>::iterator iter = ri.resource_inventory_.find(locked_mine_);
		if (iter != ri.resource_inventory_.end()){
			iter->second.number_of_miners_--;
		}
		locked_mine_ = nullptr;
	}
}

Stored_Resource* Stored_Unit::getMine(Resource_Inventory &ri) {
    Stored_Resource* tenative_resource = nullptr;
    tenative_resource = &ri.resource_inventory_.find( locked_mine_ )->second;
    return tenative_resource;
}

bool Stored_Unit::isClearing(Resource_Inventory &ri) {
    if ( locked_mine_ ) {
        map<Unit, Stored_Resource>::iterator iter = ri.resource_inventory_.find(locked_mine_);
        if (iter != ri.resource_inventory_.end() && iter->second.current_stock_value_ <= 8) {
            return true;
        }
    }
    return false;
}
