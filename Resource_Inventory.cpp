#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Resource_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\Map_Inventory.h"
#include <bwem.h>

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
    occupied_resource_ = false;
    valid_pos_ = true;


    bwapi_unit_ = resource;
    type_ = resource->getType();
    pos_ = resource->getPosition();

    auto area = BWEM::Map::Instance().GetArea(TilePosition(pos_));
    if (area) {
        areaID_ = area->Id();
    }
    auto bwemMin = BWEM::Map::Instance().GetMineral(resource);
    bwemMin ? blocking_mineral_ = bwemMin->Blocking() : blocking_mineral_ = false;
}

//void Stored_Resource::addMiner(Stored_Unit miner) {
//    if (miner.bwapi_unit_ && miner.bwapi_unit_->exists()){
//        miner_inventory_.push_back(miner.bwapi_unit_);
//        number_of_miners_++;
//    }
//}

void Resource_Inventory::updateResourceInventory(Unit_Inventory &ui, Unit_Inventory &ei, Map_Inventory &inv) {
    // Update "My Bases"
    Unit_Inventory hatches;
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if ((u.second.type_ != UnitTypes::Zerg_Hatchery && u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery)) || u.second.type_ == UnitTypes::Zerg_Hatchery && u.first->isCompleted() ) hatches.addStored_Unit(u.second);
    }
    vector<Position> my_bases_;

    vector<Position> resources_spots;
    for (auto & area : BWEM::Map::Instance().Areas()) {
        for (auto & base : area.Bases()) {
            for (auto h : hatches.unit_map_) {
                if (h.second.pos_ == base.Center()) {
                    my_bases_.push_back(Position(base.Location()));
                    for (auto resource : base.Minerals()) {
                        resources_spots.push_back(resource->Pos());
                    }
                    for (auto resource : base.Geysers()) {
                        resources_spots.push_back(resource->Pos());
                    }
                }
            }
        }
    }

    for (auto r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty();) {
        TilePosition resource_pos = TilePosition(r->second.pos_);
        bool erasure_sentinel = false;



        if (Broodwar->isVisible(resource_pos)) {
            if (r->second.bwapi_unit_ && r->second.bwapi_unit_->exists()) {
                r->second.current_stock_value_ = r->second.bwapi_unit_->getResources();
                r->second.valid_pos_ = true;
                r->second.type_ = r->second.bwapi_unit_->getType();
                auto bwemMin = BWEM::Map::Instance().GetMineral(r->second.bwapi_unit_);
                bwemMin ? r->second.blocking_mineral_ = bwemMin->Blocking() : r->second.blocking_mineral_ = false;

                r->second.occupied_resource_ = false;
                for (auto bwem_r_spot : resources_spots) {
                    if (bwem_r_spot == r->second.pos_) r->second.occupied_resource_ = true;
                }

                if (r->first->getPlayer()->isEnemy(Broodwar->self())) { // if his gas is taken, sometimes they become enemy units. We'll insert it as such.
                    Stored_Unit eu = Stored_Unit(r->first);
                    if (ei.unit_map_.insert({ r->first, eu }).second) {
                        CUNYAIModule::DiagnosticText("Huh, a geyser IS an enemy. Even the map is against me now...");
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
    updateMines();
}


// scrape over every resource to determine how many of them are actually  occupied.
void Resource_Inventory::updateMines() {
    local_mineral_patches_ = 0;
    local_refineries_ = 0;

    local_miners_ = 0;
    local_gas_collectors_ = 0;
    for (auto& r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty(); r++) {
        if (r->second.type_.isMineralField() && !r->second.blocking_mineral_ && r->second.occupied_resource_) {
            local_mineral_patches_++; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
            local_miners_ += r->second.number_of_miners_;
        }
        if (r->second.type_.isRefinery() && r->second.bwapi_unit_ && r->second.occupied_resource_ && IsOwned(r->second.bwapi_unit_) && r->second.bwapi_unit_->isCompleted() ) {
            local_refineries_++;
            local_gas_collectors_ += r->second.number_of_miners_;

        }
    } // find drone minima.
}
void Resource_Inventory::drawMineralRemaining() const
{
    for (auto u : resource_inventory_) {
        CUNYAIModule::DiagnosticMineralsRemaining(u.second, CUNYAIModule::current_map_inventory.screen_position_);
    }

}

void Resource_Inventory::drawUnreachablePatch(const Map_Inventory & inv) const
{
    if constexpr (DRAWING_MODE) {
        for (auto r = resource_inventory_.begin(); r != resource_inventory_.end() && !resource_inventory_.empty(); r++) {
            if (CUNYAIModule::isOnScreen(r->second.pos_, CUNYAIModule::current_map_inventory.screen_position_)) {
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

int Resource_Inventory::getLocalMiners()
{
    return local_miners_;
}

int Resource_Inventory::getLocalGasCollectors()
{
    return local_gas_collectors_;
}

int Resource_Inventory::getLocalMinPatches()
{
    return local_mineral_patches_;
}

int Resource_Inventory::getLocalRefineries()
{
    return local_refineries_;
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