#pragma once

# include "Source\MeatAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a boids manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Boids::Boids_Movement( const Unit &unit, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inventory, const bool &army_starved, const bool &potential_fears) {

        Position pos = unit->getPosition();
        if (pos) {
            bool healthy = unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints();
            bool ready_to_fight = ei.stock_total_ <= ui.stock_total_ || !potential_fears;
            bool enemy_scouted = ei.getMeanBuildingLocation() != Position(0, 0);
            Unit_Inventory local_neighborhood = MeatAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 500);

            // Units should scout when there is a large gap in our knowledge.
            if (!enemy_scouted && healthy && !inventory.cleared_all_start_positions_) { // check his bases first.
                //scoutEnemyBase(unit, pos, inventory); 
                setAttractionEnemy(unit, pos, ei, inventory, potential_fears);
            }
            else if (!enemy_scouted && healthy && inventory.cleared_all_start_positions_) { // then wander about searching.
                setSeperationScout(unit, pos, local_neighborhood); //This is triggering too often and your army is scattering, not everything else.  
            }
            else {
                if (!unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge) {
                    setSeperation(unit, pos, local_neighborhood);
                } // closure: flyers
            }

            // Units should go to the enemy when it's time to pick a fight, home otherwise.
            if (enemy_scouted && healthy && ready_to_fight && !army_starved) {
                setAttractionEnemy(unit, pos, ei, inventory, potential_fears); 
            }
            else if (enemy_scouted && (!healthy || !ready_to_fight || army_starved)) {
                setAttractionHome(unit, pos, ei, inventory);
            }

            if (potential_fears) {
                setCohesion(unit, pos, ui);
            }
            else {
                setCohesion(unit, pos, local_neighborhood);
            }
            // Units should stick together if we're not scouting.
            //if (spread_forces) {
            //    setCohesion(unit, pos, ui);
            //    //setCohesion(unit, pos, local_neighborhood);
            //}
            //else {
            //    setCohesion(unit, pos, local_neighborhood);
            //    //setStutter(unit, n);
            //}

            //Alignment is part of boids proper but only gets me into trouble!
            //setAlignment(unit, local_neighborhood);

            // lurkers should move when we need them to scout.
            if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && MeatAIModule::spamGuard(unit) && !MeatAIModule::getClosestThreatOrTargetStored(ei, unit, max(UnitTypes::Zerg_Lurker.groundWeapon().maxRange(), ei.max_range_))) {
                unit->unburrow();
                return;
            }

            //Move to the final position.
            int vector_x = x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_;
            int vector_y = y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_;
            int dist = MeatAIModule::getProperSpeed(unit) * 48;
            int norm_x = cos(vector_x) * dist;
            int norm_y = sin(vector_y) * dist;
            Position brownian_pos = { (int)(pos.x + vector_x),
                                      (int)(pos.y + vector_y) };


            unit->move(brownian_pos);

            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + x_stutter_)        , (int)(pos.y + y_stutter_) }, inventory.screen_position_, Colors::Black);//Stutter
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, inventory.screen_position_, Colors::Green);//Alignment
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, inventory.screen_position_, Colors::Blue); // Centraliziation.
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, inventory.screen_position_, Colors::Purple); // Cohesion
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, inventory.screen_position_, Colors::Red); //Attraction towards attackable enemies.
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
            MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different regions. May tilt to become parallel with obstructions to get around them.
        }
};

// This is basic combat logic for nonspellcasting units.
void Boids::Tactical_Logic( const Unit &unit, const Unit_Inventory &ei, const Unit_Inventory &ui, const Inventory &inv, const Color &color = Colors::White )
{
        UnitType u_type = unit->getType();
        Stored_Unit target;
        int priority = 0;
        bool u_flyer = unit->isFlying();
        int chargeable_dist = MeatAIModule::getChargableDistance(unit, ei);
        int helpless_e = u_flyer ? ei.stock_total_ - ei.stock_shoots_up_ : ei.stock_total_ - ei.stock_shoots_down_;
        int helpful_e = u_flyer ? ei.stock_shoots_up_ : ui.stock_shoots_down_; // both forget value of psi units.
        int helpful_u = u_flyer ? ui.stock_fliers_ : ui.stock_ground_units_;
        bool weak_enemy_or_small_armies = (helpful_e < helpful_u || helpful_e < 750);
        double limit_units_diving = weak_enemy_or_small_armies ? 2 : 2 * log( helpful_e - helpful_u);
        int max_dist = (ei.max_range_ + chargeable_dist);
        int max_chargable_dist = max_dist/ (double)limit_units_diving;
        int max_dist_no_priority = 9999999;
        bool target_sentinel = false;
        bool target_sentinel_poor_target_atk = false;
        Unit last_target = unit->getLastCommand().getTarget();

        for (auto e = ei.unit_inventory_.begin(); e != ei.unit_inventory_.end() && !ei.unit_inventory_.empty(); ++e) {
            if (e->second.valid_pos_) {
                UnitType e_type = e->second.type_;
                int e_priority = 0;

                if (MeatAIModule::Can_Fight(unit, e->second)) { // if we can fight this enemy 
                    int dist_to_enemy = unit->getDistance(e->second.pos_);

                    bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                        (e_type.isSpellcaster() && !e_type.isBuilding()) ||
                        (e_type.isDetector() && ui.cloaker_count_ > ei.detector_count_) ||
                        e_type == UnitTypes::Protoss_Carrier ||
                        (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && e->second.bwapi_unit_->isRepairing()) ||
                        //(e->second.current_hp_ < 0.25 * e_type.maxHitPoints() && MeatAIModule::Can_Fight( e->second, unit )) ||
                        e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units
                    bool lurkers_diving = u_type == UnitTypes::Zerg_Lurker && dist_to_enemy > UnitTypes::Zerg_Lurker.groundWeapon().maxRange();

                    if (critical_target && dist_to_enemy <= max_chargable_dist && !lurkers_diving) {
                        e_priority = 6;
                    }
                    else if (e->second.bwapi_unit_ && MeatAIModule::Can_Fight(e->second, unit) && dist_to_enemy < min(chargeable_dist, 32) && last_target &&
                        (last_target == e->second.bwapi_unit_ || (e->second.type_ == last_target->getType() && e->second.current_hp_ < last_target->getHitPoints()))) {
                        e_priority = 5;
                    }
                    else if (MeatAIModule::Can_Fight(e->second, unit)) {
                        e_priority = 4;
                    }
                    else if (e_type.isWorker()) {
                        e_priority = 3;
                    }
                    else if (MeatAIModule::IsFightingUnit(e->second) || e_type.spaceProvided() > 0) {
                        e_priority = 2;
                    }
                    else if (e->second.type_.mineralPrice() > 25 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) {
                        e_priority = 1; // or if they cant fight back we'll get those last.
                    }
                    else {
                        e_priority = 0; // should leave stuff like larvae and eggs in here. Low, low priority.
                    }


                    if (e_priority >= priority && e_priority >= 4 && dist_to_enemy <= max_dist) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                        target_sentinel = true;
                        priority = e_priority;
                        max_dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                        target = e->second;
                    }
                    else if (dist_to_enemy < max_dist_no_priority && target_sentinel == false) {
                        target_sentinel_poor_target_atk = true;
                        max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                        target = e->second;
                    }
                }
            }
        }

        if ((target_sentinel || target_sentinel_poor_target_atk) && unit->hasPath(target.pos_)) {
            if (target.bwapi_unit_ && target.bwapi_unit_->exists()) {
                if (adjust_lurker_burrow(unit, ui, ei, target.pos_)) {
                    //
                }
                else {
                    unit->attack(target.bwapi_unit_);
                    MeatAIModule::Diagnostic_Line(unit->getPosition(), target.pos_, inv.screen_position_, color);
                }
            }
            else if (target.valid_pos_) {
                if (adjust_lurker_burrow(unit, ui, ei, target.pos_)) {
                    //
                }
                else {
                    unit->attack(target.pos_);
                    MeatAIModule::Diagnostic_Line(unit->getPosition(), target.pos_, inv.screen_position_, color);
                }
            }
        }
}

// Basic retreat logic, range = enemy range
void Boids::Retreat_Logic( const Unit &unit, const Stored_Unit &e_unit, Unit_Inventory &ei, const Unit_Inventory &ui, Inventory &inventory, const Color &color = Colors::White ) {

    int dist = unit->getDistance(e_unit.pos_);
    int air_range = e_unit.type_.airWeapon().maxRange();
    int ground_range = e_unit.type_.groundWeapon().maxRange();
    int chargable_distance_net = MeatAIModule::getChargableDistance(unit, ei); // seems to have been abandoned in favor of the spamguard as the main time unit.
    int range = unit->isFlying() ? air_range : ground_range;
    Position pos = unit->getPosition();
    Unit_Inventory local_neighborhood = MeatAIModule::getUnitInventoryInRadius(ui, unit->getPosition(), 1250);
    Position e_mean = ei.getMeanArmyLocation();
    bool order_sent = false;
    if (dist < range + chargable_distance_net + 96 ) { //  Run if you're a noncombat unit or army starved. +3 tiles for safety. Retreat function now accounts for walkability.

        Broodwar->drawCircleMap(e_unit.pos_, range + chargable_distance_net + 96, Colors::Red);

        //initial retreat spot from enemy.

        //setDirectRetreat(pos, e_unit.pos_, unit->getType(), ei);
        setAttractionHome(unit, pos, ei, inventory);

        //setAlignment( unit, ui );
        //setAlignment( unit, local_neighborhood);
        setCohesion( unit, pos, local_neighborhood);

        // Flying units to not need to seperate.
        if ( !unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge ) {
            Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 96 );
            setSeperation( unit, pos, neighbors );
        } // closure: flyers

        if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && unit->isDetected() && ei.stock_ground_units_ == 0 ) {
            unit->unburrow();
            return;
        }
        else if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && ei.stock_ground_units_ > 0 && ei.detector_count_ == 0 ) {
            unit->burrow();
            return;
        }
        
        int vector_x = attract_dx_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + centralization_dx_ + retreat_dx_;
        int vector_y = attract_dy_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + centralization_dy_ + retreat_dy_;
        int dist = MeatAIModule::getProperSpeed(unit) * 48;
        int norm_x = cos(vector_x) * dist;
        int norm_y = sin(vector_y) * dist;

          //Make sure the end destination is one suitable for you.
        Position retreat_spot = { (int)(pos.x + vector_x),
                                  (int)(pos.y + vector_y) }; //attract is zero when it's not set.

        bool walkable_plus = retreat_spot.isValid() &&
            (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            MeatAIModule::isClearRayTrace(pos, retreat_spot, inventory)); //&& //or does it cross an unwalkable position? 
                //MeatAIModule::checkOccupiedArea( ui, retreat_spot, 32 ))); // or does it end on a unit?

        if (retreat_spot && !unit->isBurrowed() && walkable_plus ) {
            unit->move( retreat_spot ); //run away.
            order_sent = true;
        }
        else { // if that spot will not work for you, then instead check along that vector.

            int velocity_x = attract_dx_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + centralization_dx_ + retreat_dx_;
            int velocity_y = attract_dy_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + centralization_dy_ + retreat_dy_;
            int x = 1;

            Position potential_running_spot = retreat_spot;
            while (!walkable_plus && x < 3) {
                potential_running_spot = Position(retreat_spot.x + velocity_x * (3-x)/(double)3, retreat_spot.y + velocity_y * (3-x)/(double)3);
                walkable_plus = potential_running_spot.isValid() &&
                    (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
                    MeatAIModule::isClearRayTrace(pos, potential_running_spot, inventory)); //&& //or does it cross an unwalkable position? 
                     //MeatAIModule::checkOccupiedArea(ui, potential_running_spot, 32))); // or does it end on a unit?
                x++;
            }
            if (walkable_plus) {
                unit->move(potential_running_spot); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
                order_sent = true;
            }
            else {
                if ( unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() ) {
                    unit->burrow();
                    order_sent = true;
                    return;
                }
                else {
                    unit->attack(retreat_spot);
                    order_sent = true;
                }
            }
        }
    }
    //else if ((unit->getLastCommand().getType() == UnitCommandTypes::Attack_Move) || (unit->getLastCommand().getType() == UnitCommandTypes::Attack_Unit) && MeatAIModule::spamGuard(unit) ) {
    //    unit->stop();
    //}

    if (order_sent) {
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + retreat_dx_)       , (int)(pos.y + retreat_dy_) }, inventory.screen_position_, Colors::White);//Run directly away
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, inventory.screen_position_, Colors::Green);//Alignment
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, inventory.screen_position_, Colors::Blue); // Centraliziation.
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, inventory.screen_position_, Colors::Purple); // Cohesion
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, inventory.screen_position_, Colors::Red); //Attraction towards attackable enemies or home base.
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, inventory.screen_position_, Colors::Orange); // Seperation, does not apply to fliers.
        MeatAIModule::Diagnostic_Line(unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, inventory.screen_position_, Colors::Cyan); // Push from unwalkability, different 
    }
}


//Brownian Stuttering, causes unit to move about randomly.
void Boids::setStutter( const Unit &unit, const double &n ) {

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( -1, 1 );

    x_stutter_ = n * dis( gen ) * 32;
    y_stutter_ = n * dis( gen ) * 32; // The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
void Boids::setAlignment( const Unit &unit, const Unit_Inventory &ui ) {
    int temp_tot_x = 0;
    int temp_tot_y = 0;
    int speed = MeatAIModule::getProperSpeed(unit->getType());
    int flock_count = 0;
    if ( !ui.unit_inventory_.empty() ) {
        for ( auto i = ui.unit_inventory_.begin(); i != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); ++i ) {
            if ( i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings ) {
                //temp_tot_x += cos(i->second.bwapi_unit_->getAngle()); //get the horiz element.
                //temp_tot_y += sin(i->second.bwapi_unit_->getAngle()); // get the vertical element. Averaging angles was trickier than I thought. 
                temp_tot_x += i->second.bwapi_unit_->getVelocityX(); //get the horiz element.
                temp_tot_y += i->second.bwapi_unit_->getVelocityY(); // get the vertical element. Averaging angles was trickier than I thought. 

                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if ( flock_count > 1 ) {
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

void Boids::setDirectRetreat(const Position &pos, const Position &e_pos, const UnitType &type, const Unit_Inventory &ei) {
    int dist_x = e_pos.x - pos.x;
    int dist_y = e_pos.y - pos.y;
    double theta = atan2(dist_y, dist_x); // att_y/att_x = tan (theta).
    retreat_dx_ = -cos(theta) * MeatAIModule::getProperSpeed(type) * 24;
    retreat_dy_ = -sin(theta) * MeatAIModule::getProperSpeed(type) * 24; // get -range- outside of their range.  Should be safe.
}

//Centralization, all units prefer sitting along map veins to edges.
void Boids::setCentralize( const Position &pos, const Inventory &inventory ) {
    double temp_centralization_dx_ = 0;
    double temp_centralization_dy_ = 0;
    WalkPosition map_dim = WalkPosition( TilePosition( { Broodwar->mapWidth(), Broodwar->mapHeight() } ) );
    for ( int x = -10; x <= 10; ++x ) {
        for ( int y = -10; y <= 10; ++y ) {
            int mini_x = WalkPosition( pos ).x;
            int mini_y = WalkPosition( pos ).y;
            double centralize_x = mini_x + x;
            double centralize_y = mini_y + y;
            if ( !(x == 0 && y == 0) &&
                centralize_x < map_dim.x &&
                centralize_y < map_dim.y &&
                centralize_x > 0 &&
                centralize_y > 0 &&
                (inventory.map_veins_[centralize_x][centralize_y] > inventory.map_veins_[mini_x][mini_y] || inventory.map_veins_[centralize_x][centralize_y] > 175) )
            {
                double theta = atan2( y, x );
                temp_centralization_dx_ += cos( theta );
                temp_centralization_dy_ += sin( theta );
            }
        }
    }
    if ( temp_centralization_dx_ != 0 && temp_centralization_dy_ != 0 ) {
        double theta = atan2( temp_centralization_dy_, temp_centralization_dx_ );
        int distance_metric = abs( temp_centralization_dy_ * 8 ) + abs( temp_centralization_dx_ * 8 );
        centralization_dx_ = cos( theta ) * distance_metric * 0.25;
        centralization_dy_ = sin( theta ) * distance_metric * 0.25;
    }
}

//Cohesion, all units tend to prefer to be together.
void Boids::setCohesion( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {

    const Position loc_center = ui.getClosestMeanArmyLocation();
    if ( loc_center != Position( 0, 0 ) ) {
        double cohesion_x = loc_center.x - pos.x;
        double cohesion_y = loc_center.y - pos.y;
        double theta = atan2( cohesion_y, cohesion_x );
        cohesion_dx_ = cos( theta ) * 0.10 * unit->getDistance( loc_center );
        cohesion_dy_ = sin( theta ) * 0.10 * unit->getDistance( loc_center );
    }
}

void Boids::scoutEnemyBase(const Unit &unit, const Position &pos, Inventory &inv) {
        if (!inv.start_positions_.empty()) {
            Position possible_base = inv.start_positions_[0];
            int dist = unit->getDistance(possible_base);
            int dist_x = possible_base.x - pos.x;
            int dist_y = possible_base.y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * dist; // run 100% towards them.
            attract_dy_ = sin(theta) * dist;

            std::rotate(inv.start_positions_.begin(), inv.start_positions_.begin() + 1, inv.start_positions_.end());
        }
}

//Attraction, pull towards enemy units that we can attack. Requires some macro variables to be in place. Only sees visible units.
void Boids::setAttractionEnemy(const Unit &unit, const Position &pos, Unit_Inventory &ei, const Inventory &inv, const bool &potential_fears) {

    if (MeatAIModule::IsFightingUnit(unit)) {
        bool safe_distance_away = true;
        bool enemy_found = false;
        int distance_metric = MeatAIModule::getProperSpeed(unit);

        if (!ei.unit_inventory_.empty()) { // if there isn't a visible targetable enemy, but we have an inventory of them...
                                           //Stored_Unit* e = MeatAIModule::getClosestAttackableStored(ei, unit, dist); // only returns valid postitions.
            Stored_Unit* e = MeatAIModule::getMostAdvancedThreatOrTargetStored(ei, unit, inv, 999999); //test replace for above line.
            if (e && e->pos_ && e->valid_pos_) {
                safe_distance_away = e->pos_.getDistance(pos) > (ei.max_range_ + MeatAIModule::getChargableDistance(unit, ei)) * potential_fears;
                enemy_found = true;
            }

            if ((inv.map_veins_out_from_enemy_.empty() || unit->isFlying()) && safe_distance_away && enemy_found) {
                int dist_x = e->pos_.x - pos.x;
                int dist_y = e->pos_.y - pos.y;
                double theta = atan2(dist_y, dist_x);
                attract_dx_ = cos(theta) * distance_metric * 24; // run towards them.
                attract_dy_ = sin(theta) * distance_metric * 24;
                return;
            }
            
        }

        if (ei.unit_inventory_.empty()) { // if there isn't a visible targetable enemy, but we have an inventory of them...

            if ( !unit->isFlying() && safe_distance_away) {
                if (!inv.map_veins_out_from_enemy_.empty()) {
                    double temp_attract_dx_ = 0;
                    double temp_attract_dy_ = 0;
                    WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
                    vector<double> vector_to_enemy = getVectorTowardsEnemy(pos, inv);
                    attract_dx_ = vector_to_enemy[0] * (distance_metric * 24);
                    attract_dy_ = vector_to_enemy[1] * (distance_metric * 24);
                    return;
                }
            }

            if ( !inv.start_positions_.empty() && unit->isFlying() && safe_distance_away && enemy_found) {
                int dist_x = inv.start_positions_[0].x - pos.x;
                int dist_y = inv.start_positions_[0].y - pos.y;
                double theta = atan2(dist_y, dist_x);
                attract_dx_ = cos(theta) * distance_metric * 24; // run towards them.
                attract_dy_ = sin(theta) * distance_metric * 24;
                return;
            }

        }
    }
}

//Attraction, pull towards homes that we can attack. Requires some macro variables to be in place.
void Boids::setAttractionHome(const Unit &unit, const Position &pos, const Unit_Inventory &ui, const Inventory &inv) {
    if ( !ui.unit_inventory_.empty() && !inv.map_veins_out_from_main_.empty() ) { // if there is an existant enemy.
        int distance_metric = MeatAIModule::getProperSpeed(unit);

        if ( !inv.map_veins_out_from_main_.empty() && !unit->isFlying() ) {
            WalkPosition map_dim = WalkPosition(TilePosition({ Broodwar->mapWidth(), Broodwar->mapHeight() }));
            vector<double> direction = getVectorTowardsHome(unit->getPosition(), inv);
            attract_dx_ = direction[0] * (distance_metric * 24);
            attract_dy_ = direction[1] * (distance_metric * 24);
        }
        else {
            int dist_x = ui.getMeanBuildingLocation().x - pos.x;
            int dist_y = ui.getMeanBuildingLocation().y - pos.y;
            double theta = atan2(dist_y, dist_x);
            attract_dx_ = cos(theta) * distance_metric * 24; // run home!
            attract_dy_ = sin(theta) * distance_metric * 24;
        }
    }
}


//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {
    UnitType type = unit->getType();
    int largest_dim = max(type.height(), type.width());
    Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius(ui, pos, 32 + largest_dim);
    int seperation_x = 0;
    int seperation_y = 0;
    for ( auto &u : neighbors.unit_inventory_  ) { // don't seperate from yourself, that would be a disaster.
        seperation_x += u.second.pos_.x - pos.x;
        seperation_y += u.second.pos_.y - pos.y;
    }
    double theta = atan2(seperation_y, seperation_x);
    seperation_dx_ = cos(theta) * 32; // run 1 tiles away from everyone. Should help avoid being stuck in those wonky spots.
    seperation_dy_ = sin(theta) * 32;
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperationScout(const Unit &unit, const Position &pos, const Unit_Inventory &ui) {
    UnitType type = unit->getType();
    int distance = type.sightRange();
    int largest_dim = max(type.height(), type.width());
    Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius(ui, pos, distance + largest_dim);
    int seperation_x = 0;
    int seperation_y = 0;
    for (auto &u : neighbors.unit_inventory_) { // don't seperate from yourself, that would be a disaster.
        seperation_x += u.second.pos_.x - pos.x;
        seperation_y += u.second.pos_.y - pos.y;
    }
    double theta = atan2(seperation_y, seperation_x);
    seperation_dx_ = cos(theta) * distance; // run 2 tiles away from everyone. Should help avoid being stuck in those wonky spots.
    seperation_dy_ = sin(theta) * distance;
}

void Boids::setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory ) {

    double temp_walkability_dx_ = 0;
    double temp_walkability_dy_ = 0;
    if ( !unit->isFlying() ) {
        for ( int x = -10; x <= 10; ++x ) {
            for ( int y = -10; y <= 10; ++y ) {
                double walkability_x = TilePosition( pos ).x + x;
                double walkability_y = TilePosition( pos ).y + y;
                if ( !(x == 0 && y == 0) ) {
                    if ( walkability_x >= Broodwar->mapWidth() * 32 || walkability_y >= Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                        walkability_x < 0 || walkability_y < 0 ||  // out of bounds below map,  0.
                        Broodwar->getGroundHeight( TilePosition( walkability_x, walkability_y ) ) != Broodwar->getGroundHeight( unit->getTilePosition() ) ||  //If a position is on a different level, it's essentially unwalkable.
                        !MeatAIModule::isClearRayTrace( pos, Position( walkability_x, walkability_y ), inventory ) ) { // or if the path to the target is relatively unwalkable.
                                                                                                                       //!Broodwar->getUnitsOnTile( walkability_x / 32, walkability_y / 32, !IsFlying ).empty() ) { // or if the position is occupied.
                        double theta = atan2( y, x );
                        temp_walkability_dx_ += cos( theta );
                        temp_walkability_dy_ += sin( theta );
                    }
                }
            }
        }
    }
    if ( temp_walkability_dx_ != 0 && temp_walkability_dy_ != 0 ) {
        double theta = atan2( temp_walkability_dy_, temp_walkability_dx_ );
        double temp_dist = abs( temp_walkability_dx_ * 8 ) + abs( temp_walkability_dy_ * 8 ); // we should move away based on the number of tiles rejecting us.


        double tilt = rng_direction_ * 0.25 * 3.1415; // random number -1..1 times 0.75 * pi, should rotate it 45 degrees to the left or right of its original target. 
        walkability_dx_ = cos( theta + tilt ) * temp_dist * 0.25;
        walkability_dy_ = sin( theta + tilt ) * temp_dist * 0.25;
    }
}


// returns TRUE if the lurker needed fixing. For Attack.
bool Boids::adjust_lurker_burrow(const Unit &unit, const Unit_Inventory &ui, const Unit_Inventory &ei, const Position position_of_target) {
    int dist_to_threat_or_target = unit->getDistance(position_of_target);
    bool dist_condition = dist_to_threat_or_target < UnitTypes::Zerg_Lurker.groundWeapon().maxRange();
    bool burrow_condition = (/*(dist_to_threat_or_target < ei.max_range_ && ei.detector_count_ <= ui.cloaker_count_ ) ||*/ dist_condition);

    if (MeatAIModule::spamGuard(unit)) {
        if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && burrow_condition) {
            unit->burrow();
            return true;
        }
        else if (unit->getType() == UnitTypes::Zerg_Lurker && unit->isBurrowed() && !burrow_condition) {
            unit->unburrow();
            return true;
        }
        else if (unit->getType() == UnitTypes::Zerg_Lurker && !unit->isBurrowed() && !burrow_condition) {
            double theta = atan2(position_of_target.y - unit->getPosition().y, position_of_target.x - unit->getPosition().x);
            Position closest_loc_to_permit_attacking = Position(position_of_target.x + cos(theta) * unit->getType().groundWeapon().maxRange() * 0.75, position_of_target.y + sin(theta) * unit->getType().groundWeapon().maxRange() * 0.75);
            unit->move(closest_loc_to_permit_attacking);
            return true;
        }
    }
    return false;
}

vector<double> Boids::getVectorTowardsHome(const Position &pos, const Inventory &inv) const{
    vector<double> return_vector = { 0, 0 };
    int my_spot = inv.getRadialDistanceOutFromHome(pos);
    double temp_x = 0;
    double temp_y = 0;
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
                inv.map_veins_out_from_main_[centralize_x][centralize_y] > 0 &&
                inv.map_veins_out_from_main_[centralize_x][centralize_y] < my_spot ) // go directly home to our base if they are outside of us.
            {
                theta = atan2(y, x);
                temp_x += cos(theta);
                temp_y += sin(theta);
            }
        }
    }

    theta = atan2(temp_y, temp_x);
    return_vector[0] = cos(theta);
    return_vector[1] = sin(theta);

    return  return_vector;
}

vector<double> Boids::getVectorTowardsEnemy(const Position &pos, const Inventory &inv) const{
    vector<double> return_vector = { 0, 0 };
    int my_spot = inv.getRadialDistanceOutFromEnemy(pos);
    double temp_x = 0;
    double temp_y = 0;
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
                inv.map_veins_out_from_enemy_[centralize_x][centralize_y] > 0 &&
                inv.map_veins_out_from_enemy_[centralize_x][centralize_y] < my_spot) // go directly home to our base if they are outside of us.
            {
                theta = atan2(y, x);
                temp_x += cos(theta);
                temp_y += sin(theta);
            }
        }
    }

    theta = atan2(temp_y, temp_x);
    return_vector[0] = cos(theta);
    return_vector[1] = sin(theta);

    return  return_vector;
}