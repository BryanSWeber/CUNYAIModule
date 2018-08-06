#pragma once

# include "Source\CUNYAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.
# include <numeric>


#define DISTANCE_METRIC (int)(CUNYAIModule::getProperSpeed(unit) * 24);

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a Mobility manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Mobility::Mobility_Movement(const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, const Inventory &inv) {
    Position pos = unit->getPosition();
    distance_metric = DISTANCE_METRIC;
    //double normalization = pos.getDistance(inv.home_base_) / (double)inv.my_portion_of_the_map_; // It is a boids type algorithm.
    Unit_Inventory local_neighborhood = CUNYAIModule::getUnitInventoryInRadius(ui, pos, 250);
    local_neighborhood.updateUnitInventorySummary();
    Unit_Inventory e_neighborhood = CUNYAIModule::getUnitInventoryInRadius(ei, pos, 250);

    UnitType u_type = unit->getType();

    bool healthy = unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints();
    bool ready_to_fight = CUNYAIModule::checkSuperiorFAPForecast(ui, ei);
    bool enemy_scouted = ei.getMeanBuildingLocation() != Positions::Origin;
    bool scouting_returned_nothing = inv.checked_all_expo_positions_ && !enemy_scouted;
    bool in_my_base = local_neighborhood.getMeanBuildingLocation() != Positions::Origin;

    if (u_type != UnitTypes::Zerg_Overlord) {
        // Units should head towards enemies when there is a large gap in our knowledge, OR when it's time to pick a fight.
        if (healthy && ready_to_fight ) {
            if(u_type.airWeapon() != WeaponTypes::None) setAttraction(unit, pos, inv, inv.map_out_from_enemy_air_, inv.enemy_base_air_);
            else setAttraction(unit, pos, inv, inv.map_out_from_enemy_ground_, inv.enemy_base_ground_);
        }
        else { // Otherwise, return to home.
            setAttraction(unit, pos, inv, inv.map_out_from_home_, inv.home_base_);
        }
        
        if (healthy && scouting_returned_nothing) { // If they don't exist, then wander about searching. 
            setSeperationScout(unit, pos, local_neighborhood); //This is triggering too often and your army is scattering, not everything else.  
        }

        int average_side = ui.unit_inventory_.find(unit)->second.circumference_/4;
        Unit_Inventory neighbors = CUNYAIModule::getUnitInventoryInRadius(local_neighborhood, pos, 32 + average_side * 2);
        if(u_type != UnitTypes::Zerg_Mutalisk) setSeperation(unit, pos, neighbors);
        setCohesion(unit, pos, local_neighborhood);

    }
    else { //If you are an overlord, follow an abbreviated version of this.

        if (!ready_to_fight) { // Otherwise, return to safety.
            setAttraction(unit, pos, inv, inv.map_out_from_safety_, inv.safe_base_); 
        }
        else {
            if (e_neighborhood.stock_shoots_up_ == 0) {
                setSeperationScout(unit, pos, local_neighborhood); //This is triggering too often and your army is scattering, not everything else. 
            } 
            else {
                setSeperationScout(unit, pos, e_neighborhood); //This is triggering too often and your army is scattering, not everything else. 
            }
        }

    }

    //Avoidance vector:
    int avoidance_vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int avoidance_vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position avoidance_pos = { (int)(pos.x + avoidance_vector_x ), (int)(pos.y + avoidance_vector_y ) };

    //Which way should we avoid objects?
    if (healthy && ready_to_fight && u_type.airWeapon() != WeaponTypes::None) setObjectAvoid(unit, pos, avoidance_pos, inv, inv.map_out_from_enemy_air_);
    else if (healthy && ready_to_fight) setObjectAvoid(unit, pos, avoidance_pos, inv, inv.map_out_from_enemy_ground_);
    else setObjectAvoid(unit, pos, avoidance_pos, inv, inv.map_out_from_home_);

    //Move to the final position.
    int vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position brownian_pos = { (int)(pos.x + vector_x ), (int)(pos.y + vector_y ) };
    
    if (brownian_pos != pos) {

        // lurkers should move when we need them to scout.
        if (u_type == UnitTypes::Zerg_Lurker && unit->isBurrowed() && !CUNYAIModule::getClosestThreatOrTargetStored(ei, unit, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), ei.max_range_))) {
            unit->unburrow();
            Stored_Unit& changing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
            changing_unit.updateStoredUnit(unit);
            return;
        }

        unit->move(brownian_pos);

        Position retreat_pos = Position(retreat_dx_, retreat_dy_);
        Position attune_pos = Position(attune_dx_, attune_dy_);
        Position centralize_pos = Position(centralization_dx_, centralization_dy_);
        Position cohesion_pos = Position(cohesion_dx_, cohesion_dy_);
        Position attract_pos = Position(attract_dx_, attract_dy_);
        Position seperate_pos = Position(seperation_dx_, seperation_dy_);
        Position walkablity_pos = Position(walkability_dx_, walkability_dy_);

        CUNYAIModule::Diagnostic_Line(pos, pos + retreat_pos, inv.screen_position_, Colors::White);//Run directly away
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos, pos + retreat_pos + attune_pos, inv.screen_position_, Colors::Red);//Alignment
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos, pos + retreat_pos + attune_pos + centralize_pos, inv.screen_position_, Colors::Blue); // Centraliziation.
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos, inv.screen_position_, Colors::Purple); // Cohesion
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos, inv.screen_position_, Colors::Green); //Attraction towards attackable enemies or home base.
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos, inv.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
        CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos - walkablity_pos, inv.screen_position_, Colors::Cyan); // Push from unwalkability, different 
    }


    Stored_Unit& changing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    changing_unit.updateStoredUnit(unit);
    changing_unit.phase_ = "Pathing";
};
// This is basic combat logic for nonspellcasting units.
void Mobility::Tactical_Logic(const Unit &unit, Unit_Inventory &ei, const Unit_Inventory &ui, const int passed_distance, const Inventory &inv, const Color &color = Colors::White)
{
    UnitType u_type = unit->getType();
    Stored_Unit* target;
    vector<int> useful_stocks = CUNYAIModule::getUsefulStocks(ui, ei);
    Unit last_target = unit->getLastCommand().getTarget();

    int widest_dim = max(u_type.height(), u_type.width());
    int priority = 0;
    int chargeable_dist = CUNYAIModule::getChargableDistance(unit, ei);
    int helpful_u = useful_stocks[0];
    int helpful_e = useful_stocks[1]; // both forget value of psi units.
    int max_dist_no_priority = INT_MAX;
    int max_dist = passed_distance; // copy, to be modified later.
    bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 150);
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    bool melee = CUNYAIModule::getProperRange(unit) < 32;
    double limit_units_diving = weak_enemy_or_small_armies ? 2 : 2 * log(helpful_e - helpful_u);
    double max_diveable_dist = passed_distance / (double)limit_units_diving;

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

                if (critical_target && dist_to_enemy <= max_diveable_dist && !lurkers_diving) {
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
                else if (e_priority >= priority && e_priority < 3 && dist_to_enemy < max_dist_no_priority && target_sentinel == false) {
                    target_sentinel_poor_target_atk = true;
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = &e->second;
                }

            }
        }
    }

    bool attack_order_issued = false;
    //bool target_is_on_wrong_plane = target && target->bwapi_unit_ && target->elevation_ != ui.getStoredUnitValue(unit).elevation_ && melee && !ui.getStoredUnitValue(unit).is_flying_; // keeps crashing from nullptr target.

    if ((target_sentinel || target_sentinel_poor_target_atk) && unit->hasPath(target->pos_) ){
        if (target->bwapi_unit_ && target->bwapi_unit_->exists()) {
            if (adjust_lurker_burrow(unit, ui, ei, target->pos_)) {
                //
            }
            else {
                unit->attack(target->bwapi_unit_);
                if (melee) target->circumference_remaining_ -= widest_dim;
                CUNYAIModule::Diagnostic_Line(unit->getPosition(), target->pos_, inv.screen_position_, color);
            }
            attack_order_issued = true;
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
            attack_order_issued = true;
        }
    }

    Stored_Unit& changing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
    changing_unit.updateStoredUnit(unit);
    changing_unit.phase_ = "Attacking";

    if(!attack_order_issued) Mobility_Movement(unit, ui, ei, inv);
}
// Basic retreat logic, range = enemy range
void Mobility::Retreat_Logic(const Unit &unit, const Stored_Unit &e_unit, const Unit_Inventory &u_squad, Unit_Inventory &e_squad, Unit_Inventory &ei, const Unit_Inventory &ui, const int passed_distance, Inventory &inventory, const Color &color = Colors::White) {

    int dist = unit->getDistance(e_unit.pos_);
    //int air_range = e_unit.type_.airWeapon().maxRange();
    //int ground_range = e_unit.type_.groundWeapon().maxRange();
    distance_metric = DISTANCE_METRIC; // retreating must be done very fast.
   
    int e_range = ei.max_range_;
    int f_range = ui.max_range_;

    Position pos = unit->getPosition();
    bool order_sent = false;

    if constexpr (ANALYSIS_MODE) {
        Broodwar->drawCircleMap(e_unit.pos_, e_range, Colors::Red);
        Broodwar->drawCircleMap(e_unit.pos_, passed_distance, Colors::Green);
    }

    if (CUNYAIModule::getThreateningStocks(unit, e_squad) > 0) {
        setSeperation(unit, pos, e_squad); // might return false positives.
        if (e_unit.is_flying_) setRepulsion(unit, pos, inventory, inventory.map_out_from_enemy_air_, inventory.enemy_base_air_);
        else setRepulsion(unit, pos, inventory, inventory.map_out_from_enemy_ground_, inventory.enemy_base_ground_);
    }
    else {
        setAttraction(unit, pos, inventory, inventory.map_out_from_safety_, inventory.safe_base_); // otherwise a flying unit will be saticated by simply not having a dangerous weapon directly under them.
    }

    //setAlignment( unit, ui );
    //setAlignment( unit, local_neighborhood);
    //setCohesion( unit, pos, local_neighborhood);
    //setCentralize(pos, inventory); // causes problems with kiting.
    //setDirectRetreat(pos, e_unit.pos_, unit->getType());//might need this to solve scourge problem?

    //Avoidance vector:
    int avoidance_vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
    int avoidance_vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
    Position avoidance_pos = { (int)(pos.x + avoidance_vector_x), (int)(pos.y + avoidance_vector_y) };
    setObjectAvoid(unit, pos, avoidance_pos, inventory, inventory.map_out_from_safety_);

    //final vector
    int vector_x = x_stutter_ + attract_dx_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + centralization_dx_ + retreat_dx_;
    int vector_y = y_stutter_ + attract_dy_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + centralization_dy_ + retreat_dy_;

    //Make sure the end destination is one suitable for you.
    Position retreat_spot = { (int)(pos.x + vector_x), (int)(pos.y + vector_y) }; //attract is zero when it's not set.

    bool clear_walkable = retreat_spot.isValid() &&
        (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            CUNYAIModule::isClearRayTrace(pos, retreat_spot, inventory.unwalkable_barriers_with_buildings_, 1)); //or does it cross an unwalkable position? Includes buildings.
    bool cooldown = unit->getGroundWeaponCooldown() > 0 || unit->getAirWeaponCooldown() > 0;
    bool kiting = !cooldown && dist < 64 && CUNYAIModule::getProperRange(unit) > 64 && CUNYAIModule::getProperRange(e_unit.bwapi_unit_) < 64 && CUNYAIModule::Can_Fight(e_unit, unit); // only kite if he's in range,

    bool scourge_retreating = unit->getType() == UnitTypes::Zerg_Scourge && dist < e_range;
    bool unit_death_in_1_second = Stored_Unit::unitAliveinFuture(ui.unit_inventory_.at(unit), 48);
    bool squad_death_in_1_second = u_squad.squadAliveinFuture(48);
    bool never_suicide = unit->getType() == UnitTypes::Zerg_Mutalisk || unit->getType() == UnitTypes::Zerg_Overlord || unit->getType() == UnitTypes::Zerg_Drone;
    bool melee_fight = CUNYAIModule::getProperRange(unit) < 64 && e_squad.max_range_ < 64;

    if ( retreat_spot && !kiting && !(unit_death_in_1_second && squad_death_in_1_second && clear_walkable && melee_fight)) {
        if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->isDetected() && ei.stock_ground_units_ == 0) {
            unit->unburrow();
        }
        else {
            unit->move(retreat_spot); //run away.
            Position retreat_pos = Position(retreat_dx_, retreat_dy_);
            Position attune_pos = Position(attune_dx_, attune_dy_);
            Position centralize_pos = Position(centralization_dx_, centralization_dy_);
            Position cohesion_pos = Position(cohesion_dx_, cohesion_dy_);
            Position attract_pos = Position(attract_dx_, attract_dy_);
            Position seperate_pos = Position(seperation_dx_, seperation_dy_);
            Position walkablity_pos = Position(walkability_dx_, walkability_dy_);

            CUNYAIModule::Diagnostic_Line(pos, pos+retreat_pos, inventory.screen_position_, Colors::White);//Run directly away
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos, pos + retreat_pos + attune_pos, inventory.screen_position_, Colors::Red);//Alignment
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos, pos + retreat_pos + attune_pos + centralize_pos, inventory.screen_position_, Colors::Blue); // Centraliziation.
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos, inventory.screen_position_, Colors::Purple); // Cohesion
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos, inventory.screen_position_, Colors::Green); //Attraction towards attackable enemies or home base.
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos, inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
            CUNYAIModule::Diagnostic_Line(pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos, pos + retreat_pos + attune_pos + centralize_pos + cohesion_pos + attract_pos - seperate_pos - walkablity_pos, inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different 
        }
        Stored_Unit& changing_unit = CUNYAIModule::friendly_inventory.unit_inventory_.find(unit)->second;
        changing_unit.updateStoredUnit(unit);
        changing_unit.phase_ = "Retreating";
    }
    else { // if that spot will not work for you, prep to die.
        // if your death is immenent fight back.
        Tactical_Logic(unit, e_squad, u_squad, passed_distance, inventory);
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
                temp_tot_x += i->second.bwapi_unit_->getVelocityX(); //get the horiz element.
                temp_tot_y += i->second.bwapi_unit_->getVelocityY(); // get the vertical element. Averaging angles was trickier than I thought. 

                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if (flock_count > 1) {
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
    if (loc_center != Positions::Origin) {
        double cohesion_x = loc_center.x - pos.x;
        double cohesion_y = loc_center.y - pos.y;
        double theta = atan2(cohesion_y, cohesion_x);
        cohesion_dx_ = cos(theta) * 0.25 * distance_metric;
        cohesion_dy_ = sin(theta) * 0.25 * distance_metric;
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


//Attraction, pull towards map center.
void Mobility::setAttraction(const Unit &unit, const Position &pos, const Inventory &inv, const vector<vector<int>> &map, const Position &map_center) {

        if (map.empty() || unit->isFlying()) {
            int dist_x = map_center.x - pos.x;
            int dist_y = map_center.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * distance_metric; // run to (map)!
            attract_dy_ = sin(theta) * distance_metric;
        }
        else {
            WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
            vector<double> direction = getVectorTowardsMap(unit->getPosition(), inv, map);
            attract_dx_ = direction[0] * distance_metric;
            attract_dy_ = direction[1] * distance_metric;
        }
}

//Repulsion, pull away from map center. Literally just a negative of the previous.
void Mobility::setRepulsion(const Unit &unit, const Position &pos, const Inventory &inv, const vector<vector<int>> &map, const Position &map_center) {

    if (map.empty() || unit->isFlying()) {
        int dist_x = map_center.x - pos.x;
        int dist_y = map_center.y - pos.y;
        double theta = atan2(dist_y, dist_x);
        attract_dx_ = -cos(theta) * distance_metric; // run to (map)!
        attract_dy_ = -sin(theta) * distance_metric;
    }
    else {
        WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
        vector<double> direction = getVectorTowardsMap(unit->getPosition(), inv, map);
        attract_dx_ = -direction[0] * distance_metric;
        attract_dy_ = -direction[1] * distance_metric;
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

void Mobility::setObjectAvoid(const Unit &unit, const Position &current_pos, const Position &future_pos, const Inventory &inventory, const vector<vector<int>> &map) {
        double temp_walkability_dx_ = 0;
        double temp_walkability_dy_ = 0;
        double theta = 0;
        WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
        //Position avoidance_pos = { 2 * future_pos.x - current_pos.x, 2 * future_pos.y - current_pos.y};
        vector<Position> trial_positions = { current_pos, future_pos };

        bool unwalkable_tiles = false;
        pair<int, int> min_tile = { INT_MAX, 0 }; // Thie stile is the LEAST walkable.
        pair<int, int> max_tile = { INT_MIN, 0 }; //this tile is the MOST walkable.
        pair<int,int> temp_int = {0,0};
        vector<WalkPosition> min_max_minitiles = { WalkPosition(future_pos), WalkPosition(future_pos) };

        if (!unit->isFlying()) {
            for (auto considered_pos : trial_positions) {
                for (int x = -8; x <= 8; ++x) {
                    for (int y = -8; y <= 8; ++y) {
                        double centralize_x = WalkPosition(considered_pos).x + x;
                        double centralize_y = WalkPosition(considered_pos).y + y;
                        if (!(x == 0 && y == 0) &&
                            centralize_x < map_dim.x &&
                            centralize_y < map_dim.y &&
                            centralize_x > 0 &&
                            centralize_y > 0 &&
                            centralize_y > 0) // Is the spot acceptable?
                        {
                            temp_int = { inventory.map_veins_[centralize_x][centralize_y], map[centralize_x][centralize_y] };
                            if (temp_int.first <= 1 && !unwalkable_tiles) // repulse from unwalkable.
                            {
                                unwalkable_tiles = true;
                            }
                            if (temp_int.first < min_tile.first || (temp_int.first == min_tile.first && temp_int.second < min_tile.second)) {
                                min_tile = temp_int;
                                min_max_minitiles[0] = WalkPosition(centralize_x, centralize_y);
                            }
                            if (temp_int.first > max_tile.first || (temp_int.first == min_tile.first && temp_int.second < min_tile.second)) {
                                max_tile = temp_int;
                                min_max_minitiles[1] = WalkPosition(centralize_x, centralize_y);
                            }
                        }
                    }
                }
                if (unwalkable_tiles) {
                    double vector_push_x = min_max_minitiles[0].x - min_max_minitiles[1].x;
                    double vector_push_y = min_max_minitiles[0].y - min_max_minitiles[1].y;

                    double theta = atan2(future_pos.x, future_pos.y);
                    //int when_did_we_stop = std::distance(trial_positions.begin(), std::find(trial_positions.begin(), trial_positions.end(), considered_pos)); // should go to 0,1,2.
                    walkability_dx_ += cos(theta) * vector_push_x * 4 /** (3 - when_did_we_stop) / trial_positions.size()*/;
                    walkability_dy_ += sin(theta) * vector_push_y * 4 /** (3 - when_did_we_stop) / trial_positions.size()*/;
                    return;
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

vector<double> Mobility::getVectorTowardsMap(const Position &pos, const Inventory &inv, const vector<vector<int>> &map) const {
    vector<double> return_vector = { 0, 0 };
    int my_spot = inv.getMapValue(pos, map);
    double temp_x = 0;
    double temp_y = 0;
    double adj_x = 0;
    double adj_y = 0;

    double theta = 0;
    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
    for (int x = -3; x <= 3; ++x) {
        for (int y = -3; y <= 3; ++y) {
            if (x != 3 && y != 3 && x != -3 && y != -3) continue;
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
                    map[centralize_x][centralize_y] < my_spot) // go directly to my base.
                {
                    temp_x += cos(theta);
                    temp_y += sin(theta);
                }
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
