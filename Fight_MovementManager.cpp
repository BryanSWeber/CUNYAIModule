#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.
# include <numeric>


#define DISTANCE_METRIC (int)CUNYAIModule::getProperSpeed(unit) * 24;

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Mobility::Mobility_Movement(const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inv) {
    Position pos = unit->getPosition();
    distance_metric = DISTANCE_METRIC;
    double normalization = pos.getDistance(inv.home_base_) / (double)inv.my_portion_of_the_map_; // It is a boids type algorithm.
    Unit_Inventory local_neighborhood = CUNYAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 250);
    UnitType u_type = unit->getType();

    bool healthy = unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints();
    bool ready_to_fight = CUNYAIModule::checkSuperiorFAPForecast(ui, ei);
    bool enemy_scouted = ei.getMeanBuildingLocation() != Position(0, 0);
    bool scouting_returned_nothing = inv.checked_all_expo_positions_ && !enemy_scouted;
    bool in_my_base = local_neighborhood.getMeanBuildingLocation() != Position(0, 0);

    if (u_type != UnitTypes::Zerg_Overlord) {
        // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
        if (healthy && ready_to_fight ) {
            setAttractionEnemy(unit, pos, ei, inv);
            normalization = pos.getDistance(inv.enemy_base_) / (double)inv.my_portion_of_the_map_;
        }
        else { // Otherwise, return home.
            setAttractionHome(unit, pos, ei, inv);
        }
        
        if (healthy && scouting_returned_nothing) { // If they don't exist, then wander about searching. 
            setSeperationScout(unit, pos, local_neighborhood); //This is triggering too often and your army is scattering, not everything else.  
        }

        int average_side = ui.unit_inventory_.find(unit)->second.circumference_/4;
        Unit_Inventory neighbors = CUNYAIModule::getUnitInventoryInRadius(local_neighborhood, pos, 32 + average_side * 2);
        setSeperation(unit, pos, neighbors);
        setCohesion(unit, pos, local_neighborhood);

    }
    else { //If you are an overlord, follow an abbreviated version of this.

        if (!ready_to_fight) { // Otherwise, return home.
            setAttractionHome(unit, pos, ei, inv);
        }
        else {
            setSeperationScout(unit, pos, local_neighborhood); //This is triggering too often and your army is scattering, not everything else. 
        }

    }

    //Avoidance vector:
    int avoidance_vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int avoidance_vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position avoidance_pos = { (int)(pos.x + avoidance_vector_x * normalization), (int)(pos.y + avoidance_vector_y * normalization) };
    setObjectAvoid(unit, pos, avoidance_pos, inv);

    //Move to the final position.
    int vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position brownian_pos = { (int)(pos.x + vector_x * normalization), (int)(pos.y + vector_y * normalization) };
    
    if (brownian_pos != pos) {

        // lurkers should move when we need them to scout.
        if (u_type == UnitTypes::Zerg_Lurker && unit->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(ei, unit, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), ei.max_range_))) {
            unit->unburrow();
            Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
            morphing_unit.updateStoredUnit(unit);
            return;
        }

        unit->move(brownian_pos);

        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + x_stutter_ * normalization)        , (int)(pos.y + y_stutter_ * normalization) }, inv.screen_position_, Colors::Black);//Stutter
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attune_dx_ * normalization)        , (int)(pos.y + attune_dy_ * normalization) }, inv.screen_position_, Colors::Green);//Alignment
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + centralization_dx_ * normalization), (int)(pos.y + centralization_dy_ * normalization) }, inv.screen_position_, Colors::Blue); // Centraliziation.
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + cohesion_dx_ * normalization)      , (int)(pos.y + cohesion_dy_ * normalization) }, inv.screen_position_, Colors::Purple); // Cohesion
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attract_dx_ * normalization)       , (int)(pos.y + attract_dy_ * normalization) }, inv.screen_position_, Colors::Red); //Attraction towards attackable enemies.
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - seperation_dx_ * normalization)    , (int)(pos.y - seperation_dy_ * normalization) }, inv.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
        CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - walkability_dx_ * normalization)   , (int)(pos.y - walkability_dy_ * normalization) }, inv.screen_position_, Colors::Cyan); // Push from unwalkability, different regions. May tilt to become parallel with obstructions to get around them.
    }


    Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    morphing_unit.updateStoredUnit(unit);
};

// This is basic combat logic for nonspellcasting units.
void Mobility::Tactical_Logic(const Unit &unit, Unit_Inventory &ei, const Unit_Inventory &ui, const Inventory &inv, const Color &color = Colors::White)
{
    UnitType u_type = unit->getType();
    bool melee = CUNYAIModule::getProperRange(unit) < 32;
    int widest_dim = max(u_type.height(), u_type.width());
    Stored_Unit* target;
    int priority = 0;
    //bool u_flyer = unit->isFlying();
    int chargeable_dist = CUNYAIModule::getChargableDistance(unit, ei);
    vector<int> useful_stocks = CUNYAIModule::getUsefulStocks(ui, ei);
    int helpful_u = useful_stocks[0];
    int helpful_e = useful_stocks[1]; // both forget value of psi units.
    bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 150);
    double limit_units_diving = weak_enemy_or_small_armies ? 2 : 2 * log(helpful_e - helpful_u);
    int max_dist = (ei.max_range_ + chargeable_dist);
    int max_chargable_dist = max_dist / (double)limit_units_diving;
    int max_dist_no_priority = INT_MAX;
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    Unit last_target = unit->getLastCommand().getTarget();

    for (auto e = ei.unit_inventory_.begin(); e != ei.unit_inventory_.end() && !ei.unit_inventory_.empty(); ++e) {
        if (e->second.valid_pos_) {
            UnitType e_type = e->second.type_;
            int e_priority = 0;
            bool can_continue_to_surround = !melee || (melee && e->second.circumference_remaining_ > widest_dim);
            if (CUNYAIModule::Can_Fight(unit, e->second) && can_continue_to_surround) { // if we can fight this enemy 
                int dist_to_enemy = unit->getDistance(e->second.pos_);

                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                    (e_type.isDetector() && ui.cloaker_count_ > ei.detector_count_) ||
                    e_type == UnitTypes::Protoss_Carrier ||
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                    //(e->second.current_hp_ < 0.25 * e_type.maxHitPoints() && CUNYAIModule::Can_Fight( e->second, unit )) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units
                bool lurkers_diving = u_type == UnitTypes::Zerg_Lurker && dist_to_enemy > UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

                if (critical_target && dist_to_enemy <= max_chargable_dist && !lurkers_diving) {
                    e_priority = 6;
                }
                else if (e->second.bwapi_unit_ && CUNYAIModule::Can_Fight(e->second, unit) &&
                    dist_to_enemy < min(chargeable_dist, 32) &&
                    last_target &&
                    (last_target == e->second.bwapi_unit_ || (e->second.type_ == last_target->getType() && e->second.current_hp_ < last_target->getHitPoints()))) {
                    e_priority = 5;
                }
                else if (CUNYAIModule::Can_Fight(e->second, unit)) {
                    e_priority = 4;
                }
                else if (e_type.isWorker()) {
                    e_priority = 3;
                }
                else if (e_type.isResourceDepot()) {
                    e_priority = 2;
                }
                else if (CUNYAIModule::IsFightingUnit(e->second) || e_type.spaceProvided() > 0) {
                    e_priority = 1;
                }
                else if (e->second.type_.mineralPrice() > 25 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) {
                    e_priority = 0; // or if they cant fight back we'll get those last.
                }
                else {
                    e_priority = -1; // should leave stuff like larvae and eggs in here. Low, low priority.
                }


                if (e_priority >= priority && e_priority >= 3 && dist_to_enemy < max_dist) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                    target_sentinel = true;
                    priority = e_priority;
                    max_dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                    target = &e->second;
                }
                else if (dist_to_enemy < max_dist_no_priority && target_sentinel == false) {
                    target_sentinel_poor_target_atk = true;
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = &e->second;
                }

            }
        }
    }

    if ((target_sentinel || target_sentinel_poor_target_atk) && unit->hasPath(target->pos_)) {
        if (target->bwapi_unit_ && target->bwapi_unit_->exists()) {
            if (adjust_lurker_burrow(unit, ui, ei, target->pos_)) {
                //
            }
            else {
                unit->attack(target->bwapi_unit_);
                if (melee) target->circumference_remaining_ -= widest_dim;
                CUNYAIModule::Diagnostic_Line(unit->getPosition(), target->pos_, inv.screen_position_, color);
            }
        }
        else if (target->valid_pos_) {
            if (adjust_lurker_burrow(unit, ui, ei, target->pos_)) {
                //
            }
            else {
                unit->attack(target->pos_);
                if (melee) target->circumference_remaining_ -= widest_dim;
                CUNYAIModule::Diagnostic_Line(unit->getPosition(), target->pos_, inv.screen_position_, color);
            }
        }
    }
    Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    morphing_unit.updateStoredUnit(unit);
}
// Basic retreat logic, range = enemy range
void Mobility::Retreat_Logic(const Unit &unit, const Stored_Unit &e_unit, const Unit_Inventory &u_squad, Unit_Inventory &e_squad, Unit_Inventory &ei, const Unit_Inventory &ui, Inventory &inventory, const Color &color = Colors::White) {

    int dist = unit->getDistance(e_unit.pos_);
    //int air_range = e_unit.type_.airWeapon().maxRange();
    //int ground_range = e_unit.type_.groundWeapon().maxRange();
    distance_metric = DISTANCE_METRIC; // retreating must be done very fast.
    int chargable_distance = CUNYAIModule::getChargableDistance(unit, ei); // seems to have been abandoned in favor of the spamguard as the main time unit.
                                                                               //int range = unit->isFlying() ? air_range : ground_range;
    int e_range = ei.max_range_;
    int f_range = ui.max_range_;

    Position pos = unit->getPosition();
    //Unit_Inventory local_neighborhood = CUNYAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 1250);
    //Position e_mean = ei.getMeanArmyLocation();
    bool order_sent = false;

    if (_ANALYSIS_MODE) {
        Broodwar->drawCircleMap(e_unit.pos_, e_range, Colors::Red);
        Broodwar->drawCircleMap(e_unit.pos_, chargable_distance, Colors::Cyan);
        Broodwar->drawCircleMap(e_unit.pos_, e_range + chargable_distance, Colors::Green);
    }

    // Seperate from enemy:
    //Unit_Inventory e_neighbors = CUNYAIModule::getUnitInventoryInRadius(ei, pos, max(64, e_range + chargable_distance ));
    //e_neighbors.updateUnitInventorySummary();

    if (CUNYAIModule::getThreateningStocks(unit, e_squad) > 0) {
        setSeperation(unit, pos, e_squad); // might return false positives.
        if (unit->isFlying()) {
            setAttractionHome(unit, pos, ei, inventory); // otherwise a flying unit will be saticated by simply not having a dangerous weapon directly under them.
        }
        //setStutter(unit, 1000);
    }
    else {
        setAttractionHome(unit, pos, ei, inventory);
    }
    //setAlignment( unit, ui );
    //setAlignment( unit, local_neighborhood);
    //setCohesion( unit, pos, local_neighborhood);
    //setCentralize(pos, inventory); // causes problems with kiting.
    //setDirectRetreat(pos, e_unit.pos_, unit->getType());//might need this to solve scourge problem?

    //if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->isDetected() && ei.stock_ground_units_ == 0) {
    //    unit->unburrow();
    //    //Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    //    //morphing_unit.updateStoredUnit(unit);
    //    return;
    //}
    //else if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && ei.stock_ground_units_ > 0 && ei.detector_count_ == 0) {
    //    unit->burrow();
    //    Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    //    morphing_unit.updateStoredUnit(unit);
    //    return;
    //}

    //Avoidance vector:
    int avoidance_vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int avoidance_vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position avoidance_pos = { (int)(pos.x + avoidance_vector_x), (int)(pos.y + avoidance_vector_y) };
    setObjectAvoid(unit, pos, avoidance_pos, inventory);

    //final vector
    int vector_x = attract_dx_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + centralization_dx_ + retreat_dx_;
    int vector_y = attract_dy_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + centralization_dy_ + retreat_dy_;

    //Make sure the end destination is one suitable for you.
    Position retreat_spot = { (int)(pos.x + vector_x), (int)(pos.y + vector_y) }; //attract is zero when it's not set.

    bool clear_walkable = retreat_spot.isValid() &&
        (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            CUNYAIModule::isClearRayTrace(pos, retreat_spot, inventory.unwalkable_barriers_with_buildings_, 1)); //or does it cross an unwalkable position? Includes buildings.
    bool safe_walkable = e_range < retreat_spot.getDistance(e_unit.pos_) || unit->isFlying();
    bool cooldown = unit->getGroundWeaponCooldown() > 0 || unit->getAirWeaponCooldown() > 0;
    bool kiting = cooldown && dist < 64 && CUNYAIModule::getProperRange(unit) > 64 && CUNYAIModule::getProperRange(e_unit.bwapi_unit_) < 64 && CUNYAIModule::Can_Fight(e_unit, unit); // only kite if he's in range,

    bool scourge_retreating = unit->getType() == UnitTypes::Zerg_Scourge && dist < e_range;
    bool unit_death_in_1_second = Stored_Unit::unitAliveinFuture(ui.unit_inventory_.at(unit),48);
    bool squad_death_in_1_second = u_squad.moving_average_fap_stock_ <= u_squad.stock_full_health_ * 0.33333;
    bool never_suicide = unit->getType() == UnitTypes::Zerg_Mutalisk || unit->getType() == UnitTypes::Zerg_Overlord || unit->getType() == UnitTypes::Zerg_Drone;
    bool melee_fight = CUNYAIModule::getProperRange(unit) < 64 && CUNYAIModule::getProperRange(e_unit.bwapi_unit_) < 64;

    if (retreat_spot && ((!unit_death_in_1_second && !squad_death_in_1_second && melee_fight) || kiting || never_suicide) && clear_walkable  /*|| safe_walkable*/ && !scourge_retreating) {
        if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->isDetected() && ei.stock_ground_units_ == 0) {
            unit->unburrow();
        }
        else {
            unit->move(retreat_spot); //run away.
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + retreat_dx_)       , (int)(pos.y + retreat_dy_) }, inventory.screen_position_, Colors::White);//Run directly away
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, inventory.screen_position_, Colors::Red);//Alignment
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, inventory.screen_position_, Colors::Blue); // Centraliziation.
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, inventory.screen_position_, Colors::Purple); // Cohesion
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, inventory.screen_position_, Colors::Green); //Attraction towards attackable enemies or home base.
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
            CUNYAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different 
        }
        return;
    }
    else { // if that spot will not work for you, prep to die.

        //if ( unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed()) {
        //    unit->burrow();
        //    Stored_Unit& morphing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
        //    morphing_unit.updateStoredUnit(unit);
        //    return;
        //}
        //else { // if your death is immenent fight back.
        Tactical_Logic(unit, e_squad, u_squad, inventory);
        return;
        //}
    }


}


//Brownian Stuttering, causes unit to move about randomly.
void Mobility::setStutter(const Unit &unit, const double &n) {

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(-1, 1);

    x_stutter_ = n * dis(gen) * 32;
    y_stutter_ = n * dis(gen) * 32; // The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
void Mobility::setAlignment(const Unit &unit, const Unit_Inventory &ui) {
    int temp_tot_x = 0;
    int temp_tot_y = 0;
    int speed = CUNYAIModule::getProperSpeed(unit->getType());
    int flock_count = 0;
    if (!ui.unit_inventory_.empty()) {
        for (auto i = ui.unit_inventory_.begin(); i != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); ++i) {
            if (i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings) {
                //temp_tot_x += cos(i->second.bwapi_unit_->getAngle()); //get the horiz element.
                //temp_tot_y += sin(i->second.bwapi_unit_->getAngle()); // get the vertical element. Averaging angles was trickier than I thought. 
                temp_tot_x += i->second.bwapi_unit_->getVelocityX(); //get the horiz element.
                temp_tot_y += i->second.bwapi_unit_->getVelocityY(); // get the vertical element. Averaging angles was trickier than I thought. 

                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if (flock_count > 1) {
            //attune_dx_ = ( ( temp_tot_x - cos(unit->getAngle()) ) / (flock_count - 1) + cos(unit->getAngle()) ) * speed * 6;
            //attune_dy_ = ( ( temp_tot_y - sin(unit->getAngle()) ) / (flock_count - 1) + sin(unit->getAngle()) ) * speed * 6 ; // think the velocity is per frame, I'd prefer it per second so its scale is sensical.
            attune_dx_ = ((temp_tot_x - unit->getVelocityX()) / (flock_count - 1)) + unit->getVelocityX();
            attune_dy_ = ((temp_tot_y - unit->getVelocityY()) / (flock_count - 1)) + unit->getVelocityY(); // think the velocity is per frame, I'd prefer it per second so its scale is 
        }
        else {
            attune_dx_ = cos(unit->getAngle()) * speed * 6;
            attune_dy_ = sin(unit->getAngle()) * speed * 6;
        }
    }
}

void Mobility::setDirectRetreat(const Position &pos, const Position &e_pos, const UnitType &type) {
    int dist_x = e_pos.x - pos.x;
    int dist_y = e_pos.y - pos.y;
    double theta = atan2(dist_y, dist_x); // att_y/att_x = tan (theta).
    retreat_dx_ = -cos(theta) * CUNYAIModule::getProperSpeed(type) * 4;
    retreat_dy_ = -sin(theta) * CUNYAIModule::getProperSpeed(type) * 4; // get slightly away from opponent.
}

//Centralization, all units prefer sitting along map veins to edges.
//void Mobility::setCentralize(const Position &pos, const Inventory &inventory) {
//    double temp_centralization_dx_ = 0;
//    double temp_centralization_dy_ = 0;
//    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
//    for (int x = -5; x <= 5; ++x) {
//        for (int y = -5; y <= 5; ++y) {
//            int mini_x = WalkPosition(pos).x;
//            int mini_y = WalkPosition(pos).y;
//            double centralize_x = mini_x + x;
//            double centralize_y = mini_y + y;
//            if (!(x == 0 && y == 0) &&
//                centralize_x < map_dim.x &&
//                centralize_y < map_dim.y &&
//                centralize_x > 0 &&
//                centralize_y > 0 &&
//                (inventory.map_veins_[centralize_x][centralize_y] > inventory.map_veins_[mini_x][mini_y] /*|| inventory.map_veins_[centralize_x][centralize_y] > 20*/))
//            {
//                double theta = atan2(y, x);
//                temp_centralization_dx_ += cos(theta);
//                temp_centralization_dy_ += sin(theta);
//            }
//        }
//    }
//    if (temp_centralization_dx_ != 0 && temp_centralization_dy_ != 0) {
//        double theta = atan2(temp_centralization_dy_, temp_centralization_dx_);
//        centralization_dx_ = cos(theta) * distance_metric * 0.125;
//        centralization_dy_ = sin(theta) * distance_metric * 0.125;
//    }
//}

//Cohesion, all units tend to prefer to be together.
void Mobility::setCohesion(const Unit &unit, const Position &pos, const Unit_Inventory &ui) {

    const Position loc_center = ui.getMeanArmyLocation();
    if (loc_center != Position(0, 0)) {
        double cohesion_x = loc_center.x - pos.x;
        double cohesion_y = loc_center.y - pos.y;
        double theta = atan2(cohesion_y, cohesion_x);
        cohesion_dx_ = cos(theta) * 0.75 * unit->getDistance(loc_center);
        cohesion_dy_ = sin(theta) * 0.75 * unit->getDistance(loc_center);
    }
}

void Mobility::scoutEnemyBase(const Unit &unit, const Position &pos, Inventory &inv) {
    if (!inv.start_positions_.empty() && find(inv.start_positions_.begin(), inv.start_positions_.end(), unit->getLastCommand().getTargetPosition()) == inv.start_positions_.end()) {
        Position possible_base = inv.start_positions_[0];
        int dist = unit->getDistance(possible_base);
        int dist_x = possible_base.x - pos.x;
        int dist_y = possible_base.y - pos.y;
        double theta = atan2(dist_y, dist_x);
        attract_dx_ = cos(theta) * dist; // run 100% towards them.
        attract_dy_ = sin(theta) * dist;
        cohesion_dx_ = seperation_dx_ = attune_dx_ = walkability_dx_ = centralization_dx_ = cohesion_dy_ = seperation_dy_ = attune_dy_ = walkability_dy_ = centralization_dy_ = 0;
        std::rotate(inv.start_positions_.begin(), inv.start_positions_.begin() + 1, inv.start_positions_.end());
    }
}

//Attraction, pull towards enemy units that we can attack. Requires some macro variables to be in place. Only sees visible units.
void Mobility::setAttractionEnemy(const Unit &unit, const Position &pos, Unit_Inventory &ei, const Inventory &inv) {

    bool enemy_found = false;
    if (!unit->isFlying()) {

        vector<double> direction = getVectorTowardsEnemy(unit->getPosition(), inv);
        attract_dx_ = direction[0] * distance_metric;
        attract_dy_ = direction[1] * distance_metric;
        return;

    }
    else if (unit->isFlying()) {

        Stored_Unit* e = CUNYAIModule::getMostAdvancedThreatOrTargetStored(ei, unit, inv, 999999); //test replace for above line.

        enemy_found = (bool)e; // did we get it?

        if (!enemy_found) {
            int dist_x = inv.enemy_base_.x - pos.x;
            int dist_y = inv.enemy_base_.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * distance_metric; // run towards them.
            attract_dy_ = sin(theta) * distance_metric;
            return;
        }
        else if (enemy_found) {
            int dist_x = e->pos_.x - pos.x;
            int dist_y = e->pos_.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * distance_metric; // run towards them.
            attract_dy_ = sin(theta) * distance_metric;
            return;
        }
    }
}

//Attraction, pull towards homes that we can attack. Requires some macro variables to be in place.
void Mobility::setAttractionHome(const Unit &unit, const Position &pos, const Unit_Inventory &ei, const Inventory &inv) {
    if (!ei.unit_inventory_.empty()) { // if there is an existant enemy.

        if (!inv.map_veins_out_from_main_.empty() && !unit->isFlying()) {
            WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
            vector<double> direction = getVectorTowardsHome(unit->getPosition(), inv);
            attract_dx_ = direction[0] * distance_metric;
            attract_dy_ = direction[1] * distance_metric;
        }
        else {
            int dist_x = inv.home_base_.x - pos.x;
            int dist_y = inv.home_base_.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * distance_metric; // run home!
            attract_dy_ = sin(theta) * distance_metric;
        }
    }
}


//Seperation from nearby units, search very local neighborhood of usually about 1-2 tiles.
void Mobility::setSeperation(const Unit &unit, const Position &pos, const Unit_Inventory &ui) {
    int seperation_x = 0;
    int seperation_y = 0;
    for (auto &u : ui.unit_inventory_) { // don't seperate from yourself, that would be a disaster.
        if (unit != u.first) {
            seperation_x += u.second.pos_.x - pos.x;
            seperation_y += u.second.pos_.y - pos.y;
        }
    }

    //int sgn_x = (seperation_x > 0) - (seperation_x < 0); //sign_x;
    //int sgn_y = (seperation_y > 0) - (seperation_y < 0); //sign_y;

   // seperation_x /= max((int) ui.unit_inventory_.size(), 1);
   // seperation_y /= max((int) ui.unit_inventory_.size(), 1);
   
    //seperation_x = min(seperation_x, distance_metric / 4 );
    //seperation_y = min(seperation_y, distance_metric / 4 );

   //seperation_dx_ = seperation_x;
   //seperation_dy_ = seperation_y;  // will be subtracted later because seperation is a repulsuion rather than a pull.

    if (seperation_y != 0 || seperation_x != 0) {
        double theta = atan2(seperation_y, seperation_x);
        seperation_dx_ += cos(theta) * distance_metric * 0.75; // run away from everyone. Should help avoid being stuck in those wonky spots.
        seperation_dy_ += sin(theta) * distance_metric * 0.75;
    }
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Mobility::setSeperationScout(const Unit &unit, const Position &pos, const Unit_Inventory &ui) {
    UnitType type = unit->getType();
    bool overlord_with_upgrades = type == UnitTypes::Zerg_Overlord && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Antennae) > 0;
    int distance = (type.sightRange() + overlord_with_upgrades * 2 * 32);
    int largest_dim = max(type.height(), type.width());
    Unit_Inventory neighbors = CUNYAIModule::getUnitInventoryInRadius(ui, pos, distance * 2 + largest_dim);
    int seperation_x = 0;
    int seperation_y = 0;
    for (auto &u : neighbors.unit_inventory_) { // don't seperate from yourself, that would be a disaster.
        seperation_x += u.second.pos_.x - pos.x;
        seperation_y += u.second.pos_.y - pos.y;
    }

    // move away from map edge too, as if they were barriers. Don't waste vision. Note how the above is enemy_pos-our_pos, it will assist with the +/- below.
    seperation_x += pos.x + distance > Broodwar->mapWidth() * 32;
    seperation_x -= pos.x - distance < 0;
    seperation_y += pos.y + distance > Broodwar->mapHeight() * 32;
    seperation_y -= pos.y - distance < 0;

    if (seperation_y != 0 || seperation_x != 0) {
        double theta = atan2(seperation_y, seperation_x);
        seperation_dx_ = cos(theta) * distance * 2; // run 2 tiles away from everyone. Should help avoid being stuck in those wonky spots.
        seperation_dy_ = sin(theta) * distance * 2;
    }
}

void Mobility::setObjectAvoid(const Unit &unit, const Position &current_pos, const Position &future_pos, const Inventory &inventory) { 
        double temp_walkability_dx_ = 0;
        double temp_walkability_dy_ = 0;
        double theta = 0;
        WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
        Position avoidance_pos = {future_pos.x - current_pos.x, future_pos.y - current_pos.y};
        vector<Position> trial_positions = { current_pos, future_pos, avoidance_pos };
        
        int all_xs = 0;
        int all_ys = 0;
        int count_of_unwalkable_tiles = 0;

        if (!unit->isFlying()) {
            for (auto pos : trial_positions) {
                for (int x = -5; x <= 5; ++x) {
                    for (int y = -5; y <= 5; ++y) {
                        double centralize_x = WalkPosition(future_pos).x + x;
                        double centralize_y = WalkPosition(future_pos).y + y;
                        if (!(x == 0 && y == 0) &&
                            centralize_x < map_dim.x &&
                            centralize_y < map_dim.y &&
                            centralize_x > 0 &&
                            centralize_y > 0 &&
                            centralize_y > 0) // Is the spot acceptable?
                        {

                            if (inventory.map_veins_[centralize_x][centralize_y] <= 1) // repulse from unwalkable.
                            {
                                all_xs += centralize_x;
                                all_ys += centralize_y;
                                count_of_unwalkable_tiles++;
                            }
                        }
                    }
                }
                if (count_of_unwalkable_tiles > 0) {
                    Position centroid_avoid = Position(all_xs / count_of_unwalkable_tiles, all_ys / count_of_unwalkable_tiles);

                    double theta = atan2(future_pos.x - centroid_avoid.x, future_pos.y - centroid_avoid.y);
                    walkability_dx_ += cos(theta) * distance_metric * 0.25;
                    walkability_dy_ += sin(theta) * distance_metric * 0.25;
                    break;
                }
            }
        }

}


// returns TRUE if the lurker needed fixing. For Attack.
bool Mobility::adjust_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target) {
    int dist_to_threat_or_target = unit->getDistance(position_of_target);
    bool dist_condition = dist_to_threat_or_target < UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

    if (unit->getType() == UnitTypes::Zerg_Lurker) {
        if ( !unit->isBurrowed() && dist_condition) {
            unit->burrow();
            return true;
        }
        else if ( unit->isBurrowed() && !dist_condition) {
            unit->unburrow();
            return true;
        }
        else if ( !unit->isBurrowed() && !dist_condition) {
            double theta = atan2(position_of_target.y - unit->getPosition().y, position_of_target.x - unit->getPosition().x);
            Position closest_loc_to_permit_attacking = Position(position_of_target.x + cos(theta) * UnitTypes::Zerg_Lurker.groundWeapon().maxRange() * 0.75, position_of_target.y + sin(theta) * UnitTypes::Zerg_Lurker.groundWeapon().maxRange() * 0.75);
            unit->move(closest_loc_to_permit_attacking);
            return true;
        }
    }

    return false;
}

vector<double> Mobility::getVectorTowardsHome(const Position &pos, const Inventory &inv) const {
    vector<double> return_vector = { 0, 0 };
    int my_spot = inv.getRadialDistanceOutFromHome(pos);
    double temp_x = 0;
    double temp_y = 0;
    double adj_x = 0;
    double adj_y = 0;

    double theta = 0;
    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            double centralize_x = WalkPosition(pos).x + x;
            double centralize_y = WalkPosition(pos).y + y;
            if (!(x == 0 && y == 0) &&
                centralize_x < map_dim.x &&
                centralize_y < map_dim.y &&
                centralize_x > 0 &&
                centralize_y > 0 &&
                centralize_y > 0) // Is the spot acceptable?
            {
                theta = atan2(y, x);

                if (inv.map_veins_[centralize_x][centralize_y] > 1 && // avoid buildings
                    inv.map_veins_out_from_main_[centralize_x][centralize_y] < my_spot) // go directly to my base.
                {
                    temp_x += cos(theta);
                    temp_y += sin(theta);
                }
                //else if (inv.map_veins_out_from_main_[centralize_x][centralize_y] < 1) // repulse from unwalkable.
                //{
                //    x > y ? temp_x -= cos(theta) : temp_y -= sin(theta); // make the smallest most direct avoidence of this obstacle.
                //                                                         //adj_x -= cos(theta);
                //                                                         //adj_y -= sin(theta);
                //}
            }
        }
    }

    if (temp_y != 0 || temp_x != 0) {
        theta = atan2(temp_y + adj_y, temp_x + adj_x);
        return_vector[0] = cos(theta);
        return_vector[1] = sin(theta);
    }

    return  return_vector;
}

vector<double> Mobility::getVectorTowardsEnemy(const Position &pos, const Inventory &inv) const {
    vector<double> return_vector = { 0, 0 };
    int my_spot = inv.getRadialDistanceOutFromEnemy(pos);
    double temp_x = 0;
    double temp_y = 0;
    double theta = 0;
    double adj_x = 0;
    double adj_y = 0;

    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            double centralize_x = WalkPosition(pos).x + x;
            double centralize_y = WalkPosition(pos).y + y;
            if (!(x == 0 && y == 0) &&
                centralize_x < map_dim.x &&
                centralize_y < map_dim.y &&
                centralize_x > 0 &&
                centralize_y > 0) // Is the spot acceptable?
            {
                theta = atan2(y, x);

                if (inv.map_veins_[centralize_x][centralize_y] > 1 && // avoid buildings
                    inv.map_veins_out_from_enemy_[centralize_x][centralize_y] < my_spot) // go directly to their base.
                {
                    temp_x += cos(theta);
                    temp_y += sin(theta);
                }
                //else if (inv.map_veins_out_from_enemy_[centralize_x][centralize_y] < 1) // repulse from unwalkable.
                //{
                //    x > y ? temp_x -= cos(theta) : temp_y -= sin(theta); // make the smallest most direct avoidence of this obstacle.
                //                                                         //adj_x -= cos(theta);
                //                                                         //adj_y -= sin(theta);
                //}
            }
        }
    }

    if (temp_y != 0 || temp_x != 0) {
        theta = atan2(temp_y + adj_y, temp_x + adj_x);
        return_vector[0] = cos(theta);
        return_vector[1] = sin(theta);
    }

    return  return_vector;
}