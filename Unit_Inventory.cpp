#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
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

void Unit_Inventory::updateUnitsControlledByOthers()
{
    for (auto e = unit_inventory_.begin(); e != unit_inventory_.end() && !unit_inventory_.empty(); e++) {
        if ((*e).second.bwapi_unit_ && (*e).second.bwapi_unit_->exists()) { // If the unit is visible now, update its position.
            (*e).second.pos_ = (*e).second.bwapi_unit_->getPosition();
            (*e).second.type_ = (*e).second.bwapi_unit_->getType();
            (*e).second.current_hp_ = (*e).second.bwapi_unit_->getHitPoints() + (*e).second.bwapi_unit_->getShields();
            (*e).second.valid_pos_ = true;
            //Broodwar->sendText( "Relocated a %s.", (*e).second.type_.c_str() );
        }
        else if (Broodwar->isVisible(TilePosition(e->second.pos_))) {  // if you can see the tile it SHOULD be at Burned down buildings will pose a problem in future.

            bool present = false;

            Unitset enemies_tile = Broodwar->getUnitsOnTile(TilePosition(e->second.pos_), IsEnemy || IsNeutral);  // Confirm it is present.  Addons convert to neutral if their main base disappears.
            for (auto et = enemies_tile.begin(); et != enemies_tile.end(); ++et) {
                present = (*et)->getID() == e->second.unit_ID_ /*|| (*et)->isCloaked() || (*et)->isBurrowed()*/;
                if (present) {
                    (*e).second.pos_ = (*e).second.bwapi_unit_->getPosition();
                    (*e).second.type_ = (*e).second.bwapi_unit_->getType();
                    (*e).second.current_hp_ = (*e).second.bwapi_unit_->getHitPoints() + (*e).second.bwapi_unit_->getShields();
                    (*e).second.valid_pos_ = true;
                    break;
                }
            }
            if ((!present || enemies_tile.empty()) && e->second.valid_pos_ && e->second.type_.canMove()) { // If the last known position is visible, and the unit is not there, then they have an unknown position.  Note a variety of calls to e->first cause crashes here. Let us make a linear projection of their position 24 frames (1sec) into the future.
                Position potential_running_spot = Position(e->second.pos_.x + e->second.velocity_x_, e->second.pos_.y + e->second.velocity_y_);
                if (!potential_running_spot.isValid() || Broodwar->isVisible(TilePosition(potential_running_spot))) {
                    e->second.valid_pos_ = false;
                }
                else if (potential_running_spot.isValid() && !Broodwar->isVisible(TilePosition(potential_running_spot)) &&
                    (e->second.type_.isFlyer() || Broodwar->isWalkable(WalkPosition(potential_running_spot)))) {
                    e->second.pos_ = potential_running_spot;
                    e->second.valid_pos_ = true;
                }
                else {
                    e->second.valid_pos_ = false;
                }
                //Broodwar->sendText( "Lost track of a %s.", e->second.type_.c_str() );
            }
            else {
                e->second.valid_pos_ = false;
            }
        }

        if (e->second.type_ == UnitTypes::Resource_Vespene_Geyser || e->second.type_ == UnitTypes::Unknown ) { // Destroyed refineries revert to geyers, requiring the manual catch. Unknowns should be removed as well.
            e->second.valid_pos_ = false;
        }

    }
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


// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelations(const Unit &unit, Resource_Inventory &ri, Inventory &inv, Reservation &res)
{
    UnitCommand command = unit->getLastCommand();
    Stored_Unit& miner = this->unit_inventory_.find(unit)->second;
    miner.stopMine(ri);

    if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build ) {
        res.removeReserveSystem(unit->getBuildType());
    }
    if (command.getTargetPosition() == Position(inv.next_expo_) ) {
        res.removeReserveSystem( UnitTypes::Zerg_Hatchery );
    }
    unit->stop();
    miner.time_of_last_purge_ = Broodwar->getFrameCount();
}

// Decrements all resources worker was attached to, clears all reservations associated with that worker. Stops Unit.
void Unit_Inventory::purgeWorkerRelationsNoStop(const Unit &unit, Resource_Inventory &ri, Inventory &inv, Reservation &res)
{
    UnitCommand command = unit->getLastCommand();
    Stored_Unit& miner = this->unit_inventory_.find(unit)->second;
    miner.stopMine(ri);

    if (command.getType() == UnitCommandTypes::Morph || command.getType() == UnitCommandTypes::Build) {
        res.removeReserveSystem(unit->getBuildType());
    }
    if (command.getTargetPosition() == Position(inv.next_expo_)) {
        res.removeReserveSystem(UnitTypes::Zerg_Hatchery);
    }
    miner.time_of_last_purge_ = Broodwar->getFrameCount();
}

void Unit_Inventory::drawAllVelocities(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        Position destination = Position(u.second.pos_.x + u.second.velocity_x_ * 24, u.second.pos_.y + u.second.velocity_y_ * 24);
        CUNYAIModule::Diagnostic_Line(u.second.pos_, destination, inv.screen_position_, Colors::Green);
    }
}

void Unit_Inventory::drawAllHitPoints(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        CUNYAIModule::DiagnosticHitPoints(u.second, inv.screen_position_);
    }

}
void Unit_Inventory::drawAllSpamGuards(const Inventory &inv) const
{
    for (auto u : unit_inventory_) {
        CUNYAIModule::DiagnosticSpamGuard(u.second, inv.screen_position_);
    }
}

void Unit_Inventory::drawAllWorkerLocks(const Inventory & inv, Resource_Inventory &ri) const
{
    for (auto u : unit_inventory_) {
        if (u.second.locked_mine_ && !u.second.isAssignedResource(ri) && !u.second.isAssignedClearing(ri)) {
            CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), inv.screen_position_, Colors::White);
        } 
        else if (u.second.isAssignedMining(ri)) {
            CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), inv.screen_position_, Colors::Green);
        }
        else if (u.second.isAssignedGas(ri)) {
            CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), inv.screen_position_, Colors::Brown);
        }
        else if (u.second.isAssignedClearing(ri)) {
            CUNYAIModule::Diagnostic_Line(u.second.pos_, u.second.locked_mine_->getPosition(), inv.screen_position_, Colors::Blue);
        }
    }
}

void Unit_Inventory::drawAllLocations(const Inventory & inv) const
{
    if (_ANALYSIS_MODE) {
        for (auto e = unit_inventory_.begin(); e != unit_inventory_.end() && !unit_inventory_.empty(); e++) {
            if (CUNYAIModule::isOnScreen(e->second.pos_, inv.screen_position_)) {
                if (e->second.valid_pos_) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red); // Plot their last known position.
                }
                else if (!e->second.valid_pos_) {
                    Broodwar->drawCircleMap(e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Blue); // Plot their last known position.
                }
            }
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
    type_ = unit->getType();
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

 //for the army that can actually move. Removed for usage of Broodwar->getclosest(), a very slow function.
 //Position Unit_Inventory::getClosestMeanArmyLocation() const {
 //    Position mean_pos = getMeanArmyLocation();
 //    if( mean_pos && mean_pos != Position(0,0) && mean_pos.isValid()){
 //       Unit nearest_neighbor = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500);
 //        if (nearest_neighbor && nearest_neighbor->getPosition() ) {
 //            Position out = Broodwar->getClosestUnit(mean_pos, !IsFlyer && IsOwned, 500)->getPosition();
 //            return out;
 //        }
 //        else {
 //            return Position(0, 0);  // you might be dead at this point, fyi.
 //        }
 //    }
 //    else {
 //        return Position(0, 0);  // you might be dead at this point, fyi.
 //    }
 //}


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
    int resource_depots = 0;

    vector<UnitType> already_seen_types;

    for ( auto const & u_iter : unit_inventory_ ) { // should only search through unit types not per unit.
        if ( find( already_seen_types.begin(), already_seen_types.end(), u_iter.second.type_ ) == already_seen_types.end() ) { // if you haven't already checked this unit type.
            
            bool flying_unit = u_iter.second.type_.isFlyer();
            int unit_value = CUNYAIModule::Stock_Units(u_iter.second.type_, *this);
            int count_of_unit = CUNYAIModule::Count_Units(u_iter.second.type_, *this) ;

            if ( CUNYAIModule::IsFightingUnit(u_iter.second) ) {

                bool up_gun = u_iter.second.type_.airWeapon() != WeaponTypes::None || u_iter.second.type_== UnitTypes::Terran_Bunker;
                bool down_gun = u_iter.second.type_.groundWeapon() != WeaponTypes::None || u_iter.second.type_ == UnitTypes::Terran_Bunker;
                bool cloaker = u_iter.second.type_.isCloakable() || u_iter.second.type_ == UnitTypes::Zerg_Lurker || u_iter.second.type_.hasPermanentCloak();
                int range_temp = (bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getProperRange(u_iter.second.type_, u_iter.second.bwapi_unit_->getPlayer()) + !(bool)(u_iter.second.bwapi_unit_) * CUNYAIModule::getProperRange(u_iter.second.type_, Broodwar->enemy());
                
                fliers          += flying_unit * unit_value; // add the value of that type of unit to the flier stock.
                ground_unit     += !flying_unit * unit_value;
                shoots_up       += up_gun * unit_value;
                shoots_down     += down_gun * unit_value;
                shoots_both     += (up_gun && down_gun) * unit_value;
                cloaker_count   += cloaker * count_of_unit;
                detector_count  += u_iter.second.type_.isDetector() * count_of_unit;
                max_cooldown = max(max(u_iter.second.type_.groundWeapon().damageCooldown(), u_iter.second.type_.airWeapon().damageCooldown()), max_cooldown);
                resource_depots += u_iter.second.type_.isResourceDepot() * count_of_unit;
                range = (range_temp > range) * range_temp + !(range_temp > range) * range;

                //if (u_iter.second.type_ == UnitTypes::Terran_Bunker && 7 * 32 < range) {
                //    range = 7 * 32; // depends on upgrades and unit contents.
                //}

                already_seen_types.push_back( u_iter.second.type_ );
            }
            else {

                air_fodder += flying_unit * unit_value; // add the value of that type of unit to the flier stock.
                ground_fodder += !flying_unit * unit_value;
            
            }

            volume += !flying_unit * u_iter.second.type_.height()*u_iter.second.type_.width() * count_of_unit;

			Region r = Broodwar->getRegionAt( u_iter.second.pos_ );
            if ( r && u_iter.second.valid_pos_ && u_iter.second.type_ != UnitTypes::Buildings ) {
                if ( r->isHigherGround() || r->getDefensePriority() > 1 ) {
                    high_ground += u_iter.second.current_stock_value_;
                }
            }
        }
    } 

	worker_count = CUNYAIModule::Count_Units(UnitTypes::Zerg_Drone, *this) + CUNYAIModule::Count_Units(UnitTypes::Protoss_Probe, *this) + CUNYAIModule::Count_Units(UnitTypes::Terran_SCV, *this);

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
    resource_depot_count_ = resource_depots;
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

    //stock_value_ = stock_value_ / (1 + unittype.isTwoUnitsInOneEgg()); // condensed /2 into one line to avoid if-branch prediction.

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
    command_ = unit->getLastCommand();
    time_since_last_command_ = Broodwar->getFrameCount() - unit->getLastCommandFrame();

    //Get unit's status. Precalculated, precached.
    int modified_supply = type_.getRace() == Races::Zerg && type_.isBuilding() ? type_.supplyRequired() + 2 : type_.supplyRequired(); // Zerg units cost a supply (2, technically since BW cuts it in half.)
    modified_supply = type_ == UnitTypes::Terran_Bunker ? type_.supplyRequired() + 2 : type_.supplyRequired(); // Assume bunkers are loaded.
    int modified_min_cost = type_ == UnitTypes::Terran_Bunker ? type_.mineralPrice() + 50 : type_.mineralPrice(); // Assume bunkers are loaded.
    int modified_gas_cost = type_.gasPrice();

    stock_value_ = modified_min_cost + 1.25 * modified_gas_cost + 25 * modified_supply;

    //stock_value_ = stock_value_ / (1 + type_.isTwoUnitsInOneEgg()); // condensed /2 into one line to avoid if-branch prediction.


    current_stock_value_ = (int)(stock_value_ * (double)current_hp_ / (double)(type_.maxHitPoints() + type_.maxShields())) ; // Precalculated, precached.
}


//Increments the number of miners on a resource.
void Stored_Unit::startMine(Stored_Resource &new_resource, Resource_Inventory &ri){
	locked_mine_ = new_resource.bwapi_unit_;
	ri.resource_inventory_.find(locked_mine_)->second.number_of_miners_++;
}

//Decrements the number of miners on a resource.
void Stored_Unit::stopMine(Resource_Inventory &ri){
	if (locked_mine_){
        if (getMine(ri)) {
            getMine(ri)->number_of_miners_ = max(getMine(ri)->number_of_miners_ - 1, 0);
        }
	}
    locked_mine_ = nullptr;
}

//finds mine- Will return true something even if the mine DNE.
Stored_Resource* Stored_Unit::getMine(Resource_Inventory &ri) {
    Stored_Resource* tenative_resource = nullptr;
    tenative_resource = &ri.resource_inventory_.find( locked_mine_ )->second;
    return tenative_resource;
}

//checks if mine started with less than 8 resource
bool Stored_Unit::isAssignedClearing( Resource_Inventory &ri ) {
    if ( locked_mine_ ) {
        if (Stored_Resource* mine_of_choice = this->getMine(ri)) { // if it has an associated mine.
            return mine_of_choice->max_stock_value_ <= 8;
        }
    }
    return false;
}

//checks if mine started with less than 8 resource
bool Stored_Unit::isAssignedLongDistanceMining(Resource_Inventory &ri) {
    if (locked_mine_) {
        if (Stored_Resource* mine_of_choice = this->getMine(ri)) { // if it has an associated mine.
            return mine_of_choice->max_stock_value_ >= 8 && !mine_of_choice->local_natural_;
        }
    }
    return false;
}

//checks if worker is assigned to a mine that started with more than 8 resources (it is a proper mine).
bool Stored_Unit::isAssignedMining(Resource_Inventory &ri) {
    if (locked_mine_) {
        if (ri.resource_inventory_.find(locked_mine_) != ri.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine(ri);
            return mine_of_choice->max_stock_value_ >= 8 && mine_of_choice->type_.isMineralField();
        }
    }
    return false;
}

bool Stored_Unit::isAssignedGas(Resource_Inventory &ri) {
    if (locked_mine_) {
        if (ri.resource_inventory_.find(locked_mine_) != ri.resource_inventory_.end()) {
            Stored_Resource* mine_of_choice = this->getMine(ri);
            return mine_of_choice->type_.isRefinery();
        }
    }
    return false;
}

bool Stored_Unit::isAssignedResource(Resource_Inventory  &ri) {

    return Stored_Unit::isAssignedMining(ri) || Stored_Unit::isAssignedGas(ri);


}
// Warning- depends on unit being updated.
bool Stored_Unit::isAssignedBuilding() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    bool building_sent = (build_type_.isBuilding() || order_ == Orders::Move || order_ == Orders::ZergBuildingMorph || command_.getType() == UnitCommandTypes::Build || command_.getType() == UnitCommandTypes::Morph ) && time_since_last_command_ < 15 * 24;
    return building_sent;
}

//if the miner is not doing any thing
bool Stored_Unit::isNoLock(){
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return  bwapi_unit_ && !bwapi_unit_->getOrderTarget();
}

//if the miner is not mining his target. Target must be visible.
bool Stored_Unit::isBrokenLock(Resource_Inventory &ri) {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    Stored_Resource* targeted_mine = this->getMine(ri);
    return  //bwapi_unit_ && targeted_mine->bwapi_unit_ && bwapi_unit_->getOrderTarget() &&  // needs to not be nullptr, have a mine, and an order target. Otherwise, we have a broken lock.
        ( bwapi_unit_->getOrderTarget() != targeted_mine->bwapi_unit_ ); // if its order target is not the mine, then we have a broken lock.
}

//prototypeing
bool Stored_Unit::isLongRangeLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
    return bwapi_unit_ && locked_mine_ && !locked_mine_->exists();
}

bool Stored_Unit::isMovingLock() {
    this->updateStoredUnit(this->bwapi_unit_); // unit needs to be updated to confirm this.
   return bwapi_unit_ && locked_mine_ && locked_mine_->exists() && bwapi_unit_->getOrderTargetPosition() == locked_mine_->getPosition() && bwapi_unit_->getOrder() == Orders::Move;
}