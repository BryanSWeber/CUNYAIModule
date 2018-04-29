#pragma once

#include <BWAPI.h>
#include "Source\MeatAIModule.h"
#include "Source\Unit_Inventory.h"
#include "Source\InventoryManager.h"
#include "Source\Reservation_Manager.h"


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
    for (const auto & u : unit_set) {
        if (unit_inventory_.find(u) != unit_inventory_.end()) {
            unit_inventory_.find(u)->second.updateStoredUnit(u); // explicitly does not change locked mineral.
        }
        else {
            unit_inventory_.insert({ u, Stored_Unit(u) });
        }
    }
    updateUnitInventorySummary(); //this call is a CPU sink.
}

void Unit_Inventory::purgeBrokenUnits()
{
    for (auto e = this->unit_inventory_.begin(); e != this->unit_inventory_.end() && !this->unit_inventory_.empty(); ) {
        if (e->second.type_ == UnitTypes::Resource_Vespene_Geyser || // Destroyed refineries revert to geyers, requiring the manual catc.
            e->second.type_ == UnitTypes::None) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            e = this->unit_inventory_.erase(e); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++e;
        }
    }
}

void Unit_Inventory::purgeUnseenUnits()
{
    for (auto f = this->unit_inventory_.begin(); f != this->unit_inventory_.end() && !this->unit_inventory_.empty(); ) {
        if (!f->second.bwapi_unit_ || !f->second.bwapi_unit_->exists()) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            f = this->unit_inventory_.erase(f); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++f;
        }
    }
}


void Unit_Inventory::purgeWorkerRelations(const Unit &unit, Resource_Inventory &ri, Inventory &inv, Reservation &res)
{
    UnitCommand command = unit->getLastCommand();
    map<Unit, Stored_Unit>::iterator iter = unit_inventory_.find(unit);
    if (iter != unit_inventory_.end()) {
        Stored_Unit& miner = iter->second;
        miner.stopMine(ri);
    }
    if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build ) {
        res.removeReserveSystem(unit->getBuildType());
    }
    if (command.getTargetPosition() == Position(inv.next_expo_) ) {
        res.removeReserveSystem( UnitTypes::Zerg_Hatchery );
    }
}

void Unit_Inventory::purgeWorkerMineRelations(const Unit & unit, Resource_Inventory & ri)
{
    map<Unit, Stored_Unit>::iterator iter = unit_inventory_.find(unit);
    if (iter != unit_inventory_.end()) {
        Stored_Unit& miner = iter->second;
        miner.stopMine(ri);
    }
}

void Unit_Inventory::purgeWorkerBuildRelations(const Unit & unit, Inventory & inv, Reservation & res)
{
    UnitCommand command = unit->getLastCommand();
    if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build) {
        res.removeReserveSystem(unit->getBuildType());
    }
    if (command.getTargetPosition() == Position(inv.next_expo_)) {
        res.removeReserveSystem(UnitTypes::Zerg_Hatchery);
    }
}

void Unit_Inventory::drawAllVelocities(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        Position destination = Position(u.second.pos_.x + u.second.velocity_x_ * 24, u.second.pos_.y + u.second.velocity_y_ * 24);
        MeatAIModule::Diagnostic_Line(u.second.pos_, destination, inv.screen_position_, Colors::Green);
    }
}

void Unit_Inventory::drawAllHitPoints(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        MeatAIModule::DiagnosticHitPoints(u.second, inv.screen_position_);
    }

}
void Unit_Inventory::drawAllSpamGuards(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        MeatAIModule::DiagnosticSpamGuard(u.second, inv.screen_position_);
    }
}

void Unit_Inventory::drawAllWorkerLocks(const Inventory & inv) const
{
    for (auto u : unit_inventory_) {
        if (u.second.locked_mine_) {
            MeatAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), inv.screen_position_, Colors::Green);
        }
    }
}

// Updates the count of units.
void Unit_Inventory::addStored_Unit( const Unit &unit ) {
    unit_inventory_.insert( { unit, Stored_Unit( unit ) } );
};

void Unit_Inventory::addStored_Unit( const Stored_Unit &stored_unit ) {
    unit_inventory_.insert( { stored_unit.bwapi_unit_ , stored_unit } );
};

void Stored_Unit::updateStoredUnit(const Unit &unit){

    valid_pos_ = true;
    pos_ = unit->getPosition();
    build_type_ = unit->getBuildType();
    current_hp_ = unit->getHitPoints() + unit->getShields();
    velocity_x_ = unit->getVelocityX();
    velocity_y_ = unit->getVelocityY();
    order_ = unit->getOrder();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();

        if (type_ != unit->getType() ) {
            type_ = unit->getType();
            stock_value_ = Stored_Unit(type_).stock_value_;
        }

		current_stock_value_ = (int)(stock_value_ * (double)current_hp_ / (double)(type_.maxHitPoints() + type_.maxShields())); // Precalculated, precached.
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
        if (u.second.valid_pos_) {
            x_sum += u.second.pos_.x;
            y_sum += u.second.pos_.y;
            count++;
        }
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
         if ( ( (u.second.type_.isBuilding() && !u.second.type_.isSpecialBuilding()) || u.second.bwapi_unit_->isMorphing() ) && u.second.valid_pos_ ) {
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
     for ( const auto &u : this->unit_inventory_) {
         if ( u.second.type_.canAttack() && u.second.valid_pos_ ) {
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

 //for the army that can actually move.
 Position Unit_Inventory::getMeanArmyLocation() const {
     int x_sum = 0;
     int y_sum = 0;
     int count = 0;
     for (const auto &u : this->unit_inventory_) {
         if (u.second.type_.canAttack() && u.second.valid_pos_ && u.second.type_.canMove() && !u.second.type_.isWorker() ) {
             x_sum += u.second.pos_.x;
             y_sum += u.second.pos_.y;
             count++;
         }
     }
     if (count > 0) {
         Position out = { x_sum / count, y_sum / count };
         return out;
     }
     else {
         return Position(0, 0);  // you might be dead at this point, fyi.
     }
 }

 //for the army that can actually move.
 Position Unit_Inventory::getClosestMeanArmyLocation() const {
     Position mean_pos = getMeanArmyLocation();
     if( mean_pos && mean_pos != Position(0,0) && mean_pos.isValid()){
        Unit nearest_neighbor = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500);
         if (nearest_neighbor && nearest_neighbor->getPosition() ) {
             Position out = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500)->getPosition();
             return out;
         }
         else {
             return Position(0, 0);  // you might be dead at this point, fyi.
         }
     }
     else {
         return Position(0, 0);  // you might be dead at this point, fyi.
     }
 }


 Unit_Inventory operator+(const Unit_Inventory& lhs, const Unit_Inventory& rhs)
 {
    Unit_Inventory total = lhs;
    total.unit_inventory_.insert(rhs.unit_inventory_.begin(), rhs.unit_inventory_.end());
    total.updateUnitInventorySummary();
    return total;
 }

 Unit_Inventory operator-(const Unit_Inventory& lhs, const Unit_Inventory& rhs)
 {
     Unit_Inventory total;
     total.unit_inventory_.insert(lhs.unit_inventory_.begin(), lhs.unit_inventory_.end());

     for (map<Unit,Stored_Unit>::const_iterator& it = rhs.unit_inventory_.begin(); it != rhs.unit_inventory_.end();) {
         if (total.unit_inventory_.find(it->first) != total.unit_inventory_.end() ) {
             total.unit_inventory_.erase(it->first);
         }
         else {
             it++;
         }
     }
     total.updateUnitInventorySummary();
     return total;
 }

void Unit_Inventory::updateUnitInventorySummary() {
    //Tally up crucial details about enemy. Should be doing this onclass. Perhaps make an enemy summary class?

    int fliers = 0;
    int ground_unit = 0;
    int shoots_up = 0;
    int shoots_down = 0;
    int shoots_both = 0;
    int high_ground = 0;
    int range = 0;
	int worker_count = 0;
	int volume = 0;
    int detector_count = 0;
    int cloaker_count = 0;
    int max_cooldown = 0;
    int ground_fodder = 0;
    int air_fodder = 0;

    vector<UnitType> already_seen_types;

    for ( auto const & u_iter : unit_inventory_ ) { // should only search through unit types not per unit.
        if ( find( already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_ ) == already_seen_types.end() ) { // if you haven't already checked this unit type.
            
            bool flying_unit = u_iter.second.type_.isFlyer();
            int unit_value = MeatAIModule::Stock_Units(u_iter.second.type_, *this);

            if ( MeatAIModule::IsFightingUnit(u_iter.second) ) {

                bool up_gun = u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_== UnitTypes::Terran_Bunker;
                bool down_gun = u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker;
                bool cloaker = u_iter.second.type_.isCloakable() || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_.hasPermanentCloak();

                fliers          += flying_unit * unit_value; // add the value of that type of unit to the flier stock.
                ground_unit     += !flying_unit * unit_value;
                shoots_up       += up_gun * unit_value;
                shoots_down     += down_gun * unit_value;
                shoots_both     += (up_gun && down_gun) * unit_value;
                cloaker_count   += cloaker * MeatAIModule::Count_Units(u_iter.second.type_, *this);
                detector_count  += u_iter.second.type_.isDetector() * MeatAIModule::Count_Units(u_iter.second.type_, *this);
                max_cooldown = max(max(u_iter.second.type_.groundWeapon().damageCooldown(), u_iter.second.type_.airWeapon().damageCooldown()), max_cooldown);

                if ( u_iter.second.bwapi_unit_ && MeatAIModule::getProperRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer()) > range ) { // if you can see it, get the proper type.
                    range = MeatAIModule::getProperRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer());
                }
                else if (!u_iter.second.bwapi_unit_ && MeatAIModule::getProperRange(u_iter.second.type_, Broodwar->enemy() ) > range) { // if you cannot see it, it must be an enemy unit.
                    range = MeatAIModule::getProperRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer());
                }

                //if (u_iter.second.type_ == UnitTypes::Terran_Bunker && 7 * 32 < range) {
                //    range = 7 * 32; // depends on upgrades and unit contents.
                //}

                already_seen_types.push_back( u_iter.second.type_ );
            }
            else {

                air_fodder += flying_unit * unit_value; // add the value of that type of unit to the flier stock.
                ground_fodder += !flying_unit * unit_value;
            
            }

			if (!flying_unit){
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
    stock_both_up_and_down_ = shoots_both;
    stock_shoots_up_ = shoots_up;
    stock_shoots_down_ = shoots_down;
    stock_high_ground_= high_ground;
    stock_fighting_total_ = stock_ground_units_ + stock_fliers_;
    stock_ground_fodder_ = ground_fodder;
    stock_air_fodder_ = air_fodder;
    stock_total_ = stock_fighting_total_ + stock_ground_fodder_ + stock_air_fodder_;
    max_range_ = range;
    max_cooldown_ = max_cooldown;
	worker_count_ = worker_count;
	volume_ = volume;
    detector_count_ = detector_count;
    cloaker_count_ = cloaker_count;
}

void Unit_Inventory::stopMine(Unit u, Resource_Inventory& ri) {
    if (u->getType().isWorker()) {
        Stored_Unit& miner = unit_inventory_.find(u)->second;
        miner.stopMine(ri);
    }
}

//Stored_Unit functions.
Stored_Unit::Stored_Unit() = default;

//returns a steryotypical unit only.
Stored_Unit::Stored_Unit( const UnitType &unittype ) {
    valid_pos_ = false;
    type_ = unittype;

    //Get unit's status. Precalculated, precached.
    int modified_supply =unittype.getRace() == Races::Zerg &&unittype.isBuilding() ?unittype.supplyRequired() + 2 :unittype.supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
    modified_supply =unittype == UnitTypes::Terran_Bunker ?unittype.supplyRequired() + 2 :unittype.supplyRequired(); // Assume bunkers are loaded.
    int modified_min_cost =unittype == UnitTypes::Terran_Bunker ?unittype.mineralPrice() + 50 :unittype.mineralPrice(); // Assume bunkers are loaded.
    int modified_gas_cost = unittype.gasPrice();

    stock_value_ = modified_min_cost + 1.25 * modified_gas_cost + 25 * modified_supply;

    stock_value_ = stock_value_ / (1 + unittype.isTwoUnitsInOneEgg()); // condensed /2 into one line to avoid if-branch prediction.

    current_stock_value_ = (int)(stock_value_); // Precalculated, precached.

};
// We must be able to create Stored_Unit objects as well.
Stored_Unit::Stored_Unit( const Unit &unit ) {
    valid_pos_ = true;
    unit_ID_ = unit->getID();
    bwapi_unit_ = unit;
    pos_ = unit->getPosition();
    type_ = unit->getType();
    build_type_ = unit->getBuildType();
    current_hp_ = unit->getHitPoints() + unit->getShields();
	locked_mine_ = nullptr;
    velocity_x_ = unit->getVelocityX();
    velocity_y_ = unit->getVelocityY();
    order_ = unit->getOrder();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();

    //Get unit's status. Precalculated, precached.
    int modified_supply = unit->getType().getRace() == Races::Zerg && unit->getType().isBuilding() ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
    modified_supply = unit->getType() == UnitTypes::Terran_Bunker ? unit->getType().supplyRequired() + 2 : unit->getType().supplyRequired(); // Assume bunkers are loaded.
    int modified_min_cost = unit->getType() == UnitTypes::Terran_Bunker ? unit->getType().mineralPrice() + 50 : unit->getType().mineralPrice(); // Assume bunkers are loaded.
    int modified_gas_cost = unit->getType().gasPrice();

    stock_value_ = modified_min_cost + 1.25 * modified_gas_cost + 25 * modified_supply;

    if ( unit->getType().isTwoUnitsInOneEgg() ) {
        stock_value_ = stock_value_ / 2;
    }

    current_stock_value_ = (int)(stock_value_ * (double)current_hp_ / (double)(unit->getType().maxHitPoints() + unit->getType().maxShields())) ; // Precalculated, precached.
}


void Stored_Unit::startMine(Stored_Resource &new_resource, Resource_Inventory &ri){
	locked_mine_ = new_resource.bwapi_unit_;
	ri.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}

void Stored_Unit::stopMine(Resource_Inventory &ri){
	if (locked_mine_ /*&& locked_mine_->exists()*/){
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

bool Stored_Unit::isClearing( Resource_Inventory &ri ) {
    if ( locked_mine_ ) {
        map<Unit, Stored_Resource>::iterator iter = ri.resource_inventory_.find(locked_mine_);
        if (iter != ri.resource_inventory_.end() && iter->second.current_stock_value_ <= 8) {
            return true;
        }
    }
    return false;
}

