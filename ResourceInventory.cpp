#pragma once

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\ResourceInventory.h"
#include "Source\UnitInventory.h"
#include "Source\MapInventory.h"
#include "Source/Diagnostics.h"
#include <bwem.h>

//ResourceInventory functions.
//Creates an instance of the resource inventory class.


ResourceInventory::ResourceInventory() {
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

ResourceInventory::ResourceInventory(const Unitset &unit_set) {

    for (const auto & u : unit_set) {
        ResourceInventory_.insert({ u, Stored_Resource(u) });
    }

    if (unit_set.empty()) {
        ResourceInventory_;
    }

}

// Updates the count of enemy units.
void ResourceInventory::addStored_Resource(Unit resource) {
    ResourceInventory_.insert({ resource, Stored_Resource(resource) });
};

void ResourceInventory::addStored_Resource(Stored_Resource stored_resource) {
    ResourceInventory_.insert({ stored_resource.bwapi_unit_, stored_resource });
};


//Removes enemy units that have died
void ResourceInventory::removeStored_Resource(Unit resource) {
    ResourceInventory_.erase(resource);
};

Position ResourceInventory::getMeanLocation() const {
    int x_sum = 0;
    int y_sum = 0;
    int count = 0;
    Position out = Positions::Origin;
    for (const auto &u : this->ResourceInventory_) {
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

//void Stored_Resource::addMiner(StoredUnit miner) {
//    if (miner.bwapi_unit_ && miner.bwapi_unit_->exists()){
//        miner_inventory_.push_back(miner.bwapi_unit_);
//        number_of_miners_++;
//    }
//}

void ResourceInventory::updateResourceInventory(UnitInventory &ui, UnitInventory &ei, MapInventory &inv) {
    // Update "My Bases"
    UnitInventory hatches;
    for (auto u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        if ((u.second.type_ != UnitTypes::Zerg_Hatchery && u.second.type_.isSuccessorOf(UnitTypes::Zerg_Hatchery)) || u.second.type_ == UnitTypes::Zerg_Hatchery && u.first->isCompleted()) hatches.addStoredUnit(u.second);
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

    for (auto r = ResourceInventory_.begin(); r != ResourceInventory_.end() && !ResourceInventory_.empty();) {
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
                    StoredUnit eu = StoredUnit(r->first);
                    if (ei.unit_map_.insert({ r->first, eu }).second) {
                        Diagnostics::DiagnosticWrite("Huh, a geyser IS an enemy. Even the map is against me now...");
                    }
                }
            }
            else {
                r = ResourceInventory_.erase(r); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
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
void ResourceInventory::updateMines() {
    local_mineral_patches_ = 0;
    local_refineries_ = 0;
    local_geysers_ = 0;
    local_miners_ = 0;
    local_gas_collectors_ = 0;
    for (auto& r = ResourceInventory_.begin(); r != ResourceInventory_.end() && !ResourceInventory_.empty(); r++) {
        if (r->second.type_.isMineralField() && !r->second.blocking_mineral_ && r->second.occupied_resource_) {
            local_mineral_patches_++; // Only gather from "Real" mineral patches with substantive value. Don't mine from obstacles.
            local_miners_ += r->second.number_of_miners_;
        }
        if (r->second.type_.isRefinery() && r->second.bwapi_unit_ && r->second.occupied_resource_ && IsOwned(r->second.bwapi_unit_) && r->second.bwapi_unit_->isCompleted()) {
            local_refineries_++;
            local_gas_collectors_ += r->second.number_of_miners_;
        }
        if (r->second.type_ == UnitTypes::Resource_Vespene_Geyser) {
            local_geysers_++;
        }
    } // find drone minima.
}
void ResourceInventory::drawMineralRemaining() const
{
    for (auto u : ResourceInventory_) {
        Diagnostics::drawMineralsRemaining(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

}

int ResourceInventory::countLocalMiners()
{
    return local_miners_;
}

int ResourceInventory::countLocalGasCollectors()
{
    return local_gas_collectors_;
}

int ResourceInventory::countLocalMinPatches()
{
    return local_mineral_patches_;
}

int ResourceInventory::countLocalGeysers()
{
    return local_geysers_;
}

int ResourceInventory::countLocalRefineries()
{
    return local_refineries_;
}

void ResourceInventory::onFrame() {
    if (Broodwar->getFrameCount() == 0) {
        //update local resources
        //current_MapInventory.updateMapVeinsOut(current_MapInventory.start_positions_[0], current_MapInventory.enemy_base_ground_, current_MapInventory.map_out_from_enemy_ground_);
        ResourceInventory mineral_inventory = ResourceInventory(Broodwar->getStaticMinerals());
        ResourceInventory geyser_inventory = ResourceInventory(Broodwar->getStaticGeysers());
        *this = mineral_inventory + geyser_inventory; // for first initialization.
    }
};

ResourceInventory operator+(const ResourceInventory& lhs, const ResourceInventory& rhs)
{
    ResourceInventory total = lhs;
    //total.UnitInventory_.insert(lhs.UnitInventory_.begin(), lhs.UnitInventory_.end());
    total.ResourceInventory_.insert(rhs.ResourceInventory_.begin(), rhs.ResourceInventory_.end());
    return total;
}

ResourceInventory operator-(const ResourceInventory& lhs, const ResourceInventory& rhs)
{
    ResourceInventory total;
    total.ResourceInventory_.insert(lhs.ResourceInventory_.begin(), lhs.ResourceInventory_.end());

    for (map<Unit, Stored_Resource>::const_iterator& it = rhs.ResourceInventory_.begin(); it != rhs.ResourceInventory_.end();) {
        if (total.ResourceInventory_.find(it->first) != total.ResourceInventory_.end()) {
            total.ResourceInventory_.erase(it->first);
        }
        else {
            it++;
        }
    }

    return total;
}