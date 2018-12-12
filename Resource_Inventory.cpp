#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Resource_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\Map_Inventory.h"

//Resource_Inventory functions.
//Creates an instance of the resource inventory class.


Resource_Inventory::Resource_Inventory(){
    // Updates the static locations of minerals and gas on the map. Should only be called on game start.
    //if (Broodwar->getFrameCount() == 0){
    //    Unitset min = Broodwar->getStaticMinerals();
    //    Unitset geysers = Broodwar->getStaticGeysers();

    //    for (auto m = min.begin(); m != min.end(); ++m) {
    //            this->addStored_Resource(*m);
    //    }
    //    for (auto g = geysers.begin(); g != geysers.end(); ++g) {
    //        this->addStored_Resource(*g);
    //    }
    //}
}

Resource_Inventory::Resource_Inventory(const Unitset &unit_set) {

    for (const auto & u : unit_set) {
        resource_inventory_.insert({ u, Stored_Resource(u) });
    }

    if (unit_set.empty()){
        resource_inventory_;
    }

}

// Updates the count of enemy units.
void Resource_Inventory::addStored_Resource(Unit resource) {
    resource_inventory_.insert({ resource, Stored_Resource(resource) });
};

void Resource_Inventory::addStored_Resource(Stored_Resource stored_resource) {
    resource_inventory_.insert({ stored_resource.bwapi_unit_, stored_resource });
};


//Removes enemy units that have died
void Resource_Inventory::removeStored_Resource(Unit resource) {
    resource_inventory_.erase(resource);
};

Position Resource_Inventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    Position out =  Positions::Origin;
    for (const auto &u : this->resource_inventory_) {
        x_sum += u.second.pos_.x;
        y_sum += u.second.pos_.y;
        count++;
    }
    if (count > 0) {
        out = Position(x_sum / count, y_sum / count);
    }
    return out;
}


//Stored_Resource functions.
Stored_Resource::Stored_Resource() = default;

// We must be able to create Stored_Resource objects as well.
Stored_Resource::Stored_Resource(Unit resource) {

    current_stock_value_ = resource->getResources();
    if (Broodwar->getFrameCount() == 0) {
        max_stock_value_ = current_stock_value_;
    }
    number_of_miners_ = 0;
    full_resource_ = false;
    occupied_natural_ = false;
    valid_pos_ = true;

    //local_natural_;
    bwapi_unit_ = resource;
    type_ = resource->getType();
    pos_ = resource->getPosition();
}

//void Stored_Resource::addMiner(Stored_Unit miner) {
//    if (miner.bwapi_unit_ && miner.bwapi_unit_->exists()){
//        miner_inventory_.push_back(miner.bwapi_unit_);
//        number_of_miners_++;
//    }
//}

void Resource_Inventory::updateResourceInventory(Unit_Inventory &ui, Unit_Inventory &ei, Map_Inventory &inv) {
    for (auto r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty();) {
        TilePosition resource_pos = TilePosition(r->second.pos_);
        bool erasure_sentinel = false;

        if (Broodwar->isVisible(resource_pos)) {
            if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists()) {
                r->second.current_stock_value_ = r->second.bwapi_unit_->getResources();
                r->second.valid_pos_ = true;
                r->second.type_ = r->second.bwapi_unit_->getType();
                Unit_Inventory local_area = CUNYAIModule::getUnitInventoryInRadius(ui, r->second.pos_, 320 );
                r->second.occupied_natural_ = CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery, local_area) - CUNYAIModule::Count_Units_In_Progress(UnitTypes::Zerg_Hatchery, local_area) > 0 ||
                    CUNYAIModule::Count_Units(UnitTypes::Zerg_Lair, local_area) > 0 ||
                    CUNYAIModule::Count_Units(UnitTypes::Zerg_Hive, local_area) > 0; // is there a resource depot in 10 tiles of it?
                if (r->first->getPlayer()->isEnemy(Broodwar->self())) { // if his gas is taken, sometimes they become enemy units. We'll insert it as such.
                    Stored_Unit eu = Stored_Unit(r->first);
                    if (ei.unit_inventory_.insert({ r->first, eu }).second) {
                        Broodwar->sendText("Huh, a geyser IS an enemy. Even the map is against me now...");
                    }
                }
            } else {
                    r = resource_inventory_.erase(r); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
                    erasure_sentinel = true;
            }
        }
        if (!erasure_sentinel) {
            r++;
        }
    }
    countViableMines();
}


// scrape over every resource to determine the lowest number of miners. Only looks at COMPLETED mines.
void Resource_Inventory::countViableMines() {
    local_mineral_patches_ = 0;
    local_refineries_ = 0;
    local_miners_ = 0;
    local_gas_collectors_ = 0;

    for (auto& r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty(); r++) {
        if (r->second.type_.isMineralField() && r->second.max_stock_value_ >= 8) {
            local_mineral_patches_++; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
            local_miners_ += r->second.number_of_miners_;
        }
        if (r->second.type_.isRefinery() && r->second.bwapi_unit_ && IsOwned(r->second.bwapi_unit_) && r->second.bwapi_unit_->isCompleted() ) {
            local_refineries_++;
            local_miners_ += r->second.number_of_miners_;
        }
    } // find drone minima.
}
void Resource_Inventory::drawMineralRemaining(const Map_Inventory &inv) const
{
    for (auto u : resource_inventory_) {
        CUNYAIModule::DiagnosticMineralsRemaining(u.second, inv.screen_position_);
    }

}

void Resource_Inventory::drawUnreachablePatch(const Map_Inventory & inv) const
{
    if constexpr (DRAWING_MODE) {
        for (auto r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty(); r++) {
            if (CUNYAIModule::isOnScreen(r->second.pos_, inv.screen_position_)) {
                if (inv.unwalkable_barriers_with_buildings_[WalkPosition(r->second.pos_).x][WalkPosition(r->second.pos_).y] == 1) {
                    Broodwar->drawCircleMap(r->second.pos_, (r->second.type_.dimensionUp() + r->second.type_.dimensionLeft()) / 2, Colors::Red, true); // Mark as RED if not in a walkable spot.
                }
                else if (inv.unwalkable_barriers_with_buildings_[WalkPosition(r->second.pos_).x][WalkPosition(r->second.pos_).y] == 0) {
                    Broodwar->drawCircleMap(r->second.pos_, (r->second.type_.dimensionUp() + r->second.type_.dimensionLeft()) / 2, Colors::Blue, true); // Mark as blue if in a walkable spot.
                }
            }
        }
    }
}


//how many workers are mining?
void Resource_Inventory::updateMiners()
{
    total_miners_ = 0;
    for (auto& r = this->resource_inventory_.begin(); r != this->resource_inventory_.end() && !this->resource_inventory_.empty(); r++) {
        if ( r->second.pos_.isValid() && r->second.type_.isMineralField() ) {
            total_miners_ += r->second.number_of_miners_;
        }
    } // find drone minima.
}

//how many workers are gathering gas?
void Resource_Inventory::updateGasCollectors()
{
    total_gas_ = 0;
    for (auto& r = this->resource_inventory_.begin(); r != this->resource_inventory_.end() && !this->resource_inventory_.empty(); r++) {
        if ( r->second.bwapi_unit_ && r->second.pos_.isValid() && r->second.type_.isRefinery() ) {
            total_gas_ += r->second.number_of_miners_;
        }
    } 
}

Resource_Inventory operator+(const Resource_Inventory& lhs, const Resource_Inventory& rhs)
{
    Resource_Inventory total = lhs;
    //total.unit_inventory_.insert(lhs.unit_inventory_.begin(), lhs.unit_inventory_.end());
    total.resource_inventory_.insert(rhs.resource_inventory_.begin(), rhs.resource_inventory_.end());
    return total;
}

Resource_Inventory operator-(const Resource_Inventory& lhs, const Resource_Inventory& rhs)
{
    Resource_Inventory total;
    total.resource_inventory_.insert(lhs.resource_inventory_.begin(), lhs.resource_inventory_.end());

    for (map<Unit, Stored_Resource>::const_iterator& it = rhs.resource_inventory_.begin(); it != rhs.resource_inventory_.end();) {
        if (total.resource_inventory_.find(it->first) != total.resource_inventory_.end()) {
            total.resource_inventory_.erase(it->first);
        }
        else {
            it++;
        }
    }

    return total;
}