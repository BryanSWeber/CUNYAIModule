#pragma once

# include "Source\MeatAIModule.h"
# include "Source\Fight_MovementManager.h"
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

//Forces a unit to stutter in a boids manner. Size of stutter is unit's (vision range * n ). Will attack if it sees something.  Overlords & lings stop if they can see minerals.
void Boids::Boids_Movement( const Unit &unit, const double &n, const Unit_Inventory &ui, Unit_Inventory &ei, Inventory &inventory, const bool &army_starved ) {

    Position pos = unit->getPosition();
    Unit_Inventory flock = MeatAIModule::getUnitInventoryInRadius( ui, pos, 352 );

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( -1, 1 );

    double rng_direction_ = dis( gen );

    setAlignment( unit, flock );
    setStutter( unit, n );
    setCohesion( unit, pos, flock );
    setAttraction( unit, pos, ei, inventory, army_starved ); // applies to overlords.

    // The following do NOT apply to flying units: Seperation, centralization.
    if ( !unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge ) {
        Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 32 );
        setSeperation( unit, pos, neighbors );
        setCentralize( pos, inventory );
    } // closure: flyers

      //Make sure the end destination is one suitable for you.
    Position brownian_pos = { (int)(pos.x + x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };

    bool walkable_plus = brownian_pos.isValid() &&
        (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
        (MeatAIModule::isClearRayTrace( pos, brownian_pos, inventory ) && //or does it cross an unwalkable position? 
         MeatAIModule::checkOccupiedArea ( ui, brownian_pos, 32 ) ) ); // or does it end on a unit?

    if ( !walkable_plus ) { // if we can't move there for some reason, Push from unwalkability, tilted 1/4 pi, 45 degrees, we'll have to go around the obstruction.
        setObjectAvoid( unit, pos, inventory );
        brownian_pos = { (int)(pos.x + x_stutter_ + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_), (int)(pos.y + y_stutter_ + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_) };// redefine this to be a walkable one.
    }

    if ( unit->canAttack( brownian_pos ) ) {
        unit->attack( brownian_pos );
    }
    else {
        unit->move( brownian_pos );
    }

    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + x_stutter_)        , (int)(pos.y + y_stutter_) }, Colors::Black );//Stutter
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attune_dx_)        , (int)(pos.y + attune_dy_) }, Colors::Green );//Alignment
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + centralization_dx_), (int)(pos.y + centralization_dy_) }, Colors::Blue ); // Centraliziation.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + cohesion_dx_)      , (int)(pos.y + cohesion_dy_) }, Colors::Purple ); // Cohesion
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x + attract_dx_)       , (int)(pos.y + attract_dy_) }, Colors::Red ); //Attraction towards attackable enemies.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - seperation_dx_)    , (int)(pos.y - seperation_dy_) }, Colors::Orange ); // Seperation, does not apply to fliers.
    MeatAIModule::Diagnostic_Line( unit->getPosition(), { (int)(pos.x - walkability_dx_)   , (int)(pos.y - walkability_dy_) }, Colors::Cyan ); // Push from unwalkability, different regions. May tilt to become parallel with obstructions to get around them.

};

// This is basic combat logic for nonspellcasting units.
void Boids::Tactical_Logic( const Unit &unit, const Unit_Inventory &ei, const Color &color = Colors::White )
{
    UnitType u_type = unit->getType();
    Stored_Unit target;
    int priority = 0;

    //std::random_device rd;  //Will be used to obtain a seed for the random number engine
    //std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    //std::uniform_real_distribution<double> dis( -1, 1 );

    //rng_direction_ = dis( gen );

    int range_radius = u_type.airWeapon().maxRange() > u_type.groundWeapon().maxRange() ? u_type.airWeapon().maxRange() : u_type.groundWeapon().maxRange();
    int dist = range_radius + MeatAIModule::getProperSpeed(unit) * 24;
    double limit_units_diving = (log( ei.stock_total_ ) == 0 ? 1 : log( ei.stock_total_ ));
    int max_dist = ei.max_range_/limit_units_diving + dist;
    int max_dist_no_priority = 9999999;
    bool target_sentinel = false;
    bool target_sentinel_poor_target_atk = false;
    bool visible_target_atk = false;

    for ( auto e = ei.unit_inventory_.begin(); e != ei.unit_inventory_.end() && !ei.unit_inventory_.empty(); ++e ) {
        if ( e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() ) {
            UnitType e_type = e->second.type_;
            int e_priority = 0;

            if ( MeatAIModule::Can_Fight( unit, e->second ) ) { // if we can fight this enemy and there is a clear path to them:
                int dist_to_enemy = unit->getDistance( e->second.pos_ );
                bool critical_target = e_type.groundWeapon().innerSplashRadius() > 0 ||
                    e->second.current_hp_ < 0.25 * e_type.maxHitPoints() && MeatAIModule::Can_Fight( e->second, unit ) ||
                    e_type == UnitTypes::Protoss_Reaver; // Prioritise these guys: Splash, crippled combat units

                if ( critical_target ) {
                    e_priority = 3;
                }
                else if ( MeatAIModule::Can_Fight( e->second, unit ) || e_type.spaceProvided() > 0 || (e_type.isSpellcaster() && !e_type.isBuilding()) || e_type == UnitTypes::Protoss_Carrier || 
                    (e->second.bwapi_unit_ && e->second.bwapi_unit_->exists() && (e->second.bwapi_unit_->isAttacking() || e->second.bwapi_unit_->isRepairing()) ) ) { // if they can fight us, carry troops, or cast spells.
                    e_priority = 2;
                }
				else if (e->second.type_.mineralPrice() > 1 && e->second.type_ != UnitTypes::Zerg_Egg && e->second.type_ != UnitTypes::Zerg_Larva) {
                    e_priority = 1; // or if they cant fight back we'll get those last.
                }
                else {
                    e_priority = 0; // should leave stuff like larvae and eggs in here. Low, low priority.
                }

                if ( (e_priority == priority && dist_to_enemy <= dist) || e_priority > priority && dist_to_enemy < max_dist ) { // closest target of equal priority, or target of higher priority. Don't hop to enemies across the map when there are undefended things to destroy here.
                    target_sentinel = true;
                    visible_target_atk = true;
                    priority = e_priority;
                    dist = dist_to_enemy; // now that we have one within range, let's tighten our existing range.
                    target = e->second;
                }
                else if ( !target_sentinel && dist_to_enemy >= max_dist && dist_to_enemy < max_dist_no_priority ) {
                    target_sentinel_poor_target_atk = true;
                    visible_target_atk = true;
                    dist = dist_to_enemy; // if nothing is within range, let's take any old target. We do not look for priority among these, merely closeness. helps melee units lock onto target instead of diving continually into enemy lines.
                    max_dist_no_priority = dist_to_enemy; // then we will get the closest of these.
                    target = e->second;
                }
            }
        }
    }

    if ( (target_sentinel || target_sentinel_poor_target_atk) ) {
        if ( target.bwapi_unit_ && target.bwapi_unit_->exists() ) {
            unit->attack( target.bwapi_unit_ );
        }
        else {
            unit->attack( target.pos_ );
        }
        MeatAIModule::Diagnostic_Line( unit->getPosition(), target.pos_, color );
    }

}

// Basic retreat logic, range = enemy range
void Boids::Retreat_Logic( const Unit &unit, const Stored_Unit &e_unit, const Unit_Inventory &ui, const Inventory &inventory, const Color &color = Colors::White ) {

    int dist = unit->getDistance( e_unit.pos_ );
    int air_range = e_unit.type_.airWeapon().maxRange();
    int ground_range = e_unit.type_.groundWeapon().maxRange();
	int chargable_distance_net = ( MeatAIModule::getProperSpeed(unit) + e_unit.type_.topSpeed()) * unit->isFlying() ? e_unit.type_.airWeapon().damageCooldown() : e_unit.type_.groundWeapon().damageCooldown();
	int range = unit->isFlying() ? air_range : ground_range ;
	if ( dist < range + chargable_distance_net ) { //  Run if you're a noncombat unit or army starved. +3 tiles for safety. Retreat function now accounts for walkability.

        Position pos = unit->getPosition();
        Unit_Inventory flock = MeatAIModule::getUnitInventoryInRadius( ui, pos, 352 );

        std::random_device rd;  //Will be used to obtain a seed for the random number engine
        std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_real_distribution<double> dis( -1, 1 );

        rng_direction_ = dis( gen );

        Broodwar->drawCircleMap( e_unit.pos_, range, Colors::Red );

        //initial retreat spot from enemy.
        int dist_x = e_unit.pos_.x - pos.x;
        int dist_y = e_unit.pos_.y - pos.y;
        double theta = atan2( dist_y, dist_x ); // att_y/att_x = tan (theta).
        double retreat_dx = -cos( theta ) * ( range + chargable_distance_net - dist );
        double retreat_dy = -sin( theta ) * ( range + chargable_distance_net - dist ); // get -range- outside of their range.  Should be safe.

        setAlignment( unit, ui );
        setCohesion( unit, pos, ui );

        // The following do NOT apply to flying units: Seperation.
		if (!unit->getType().isFlyer() || unit->getType() == UnitTypes::Zerg_Scourge ) {
            Unit_Inventory neighbors = MeatAIModule::getUnitInventoryInRadius( ui, pos, 32 );
            setSeperation( unit, pos, neighbors );
            setCentralize( pos, inventory );
        } // closure: flyers


          //Make sure the end destination is one suitable for you.
        Position retreat_spot = { (int)(pos.x + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_ + retreat_dx), (int)(pos.y + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_ + retreat_dy) }; //attract is zero when it's not set.

        bool walkable_plus = retreat_spot.isValid() &&
            (unit->isFlying() || // can I fly, rendering the idea of walkablity moot?
            (MeatAIModule::isClearRayTrace( pos, retreat_spot, inventory ) && //or does it cross an unwalkable position? 
             MeatAIModule::checkOccupiedArea( ui, retreat_spot, 32 ) ) ); // or does it end on a unit?

        if ( !walkable_plus ) { // if we can't move there for some reason, Push from unwalkability, tilted 1/4 pi, 45 degrees, we'll have to go around the obstruction.
            setObjectAvoid( unit, pos, inventory );
            retreat_spot = { (int)(pos.x + cohesion_dx_ - seperation_dx_ + attune_dx_ - walkability_dx_ + attract_dx_ + centralization_dx_ + retreat_dx), (int)(pos.y + cohesion_dy_ - seperation_dy_ + attune_dy_ - walkability_dy_ + attract_dy_ + centralization_dy_ + retreat_dy) };
            // redefine this to be a walkable one.
        }

        if ( retreat_spot && retreat_spot.isValid() ) {
            unit->move( retreat_spot ); //identify vector between yourself and e.  go 350 pixels away in the quadrant furthest from them.
            MeatAIModule::Diagnostic_Line( pos, retreat_spot, color );
        }
	}
	else if (unit->isAttacking() && !unit->isAttackFrame() ){
		unit->stop();
	}
}


//Brownian Stuttering, causes unit to move about randomly.
void Boids::setStutter( const Unit &unit, const double &n ) {

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( -1, 1 );

    x_stutter_ = n * dis( gen ) * unit->getType().sightRange();
    y_stutter_ = n * dis( gen ) * unit->getType().sightRange(); // The naieve approach of rand()%3 - 1 is "distressingly non-random in lower order bits" according to stackoverflow and other sources.
}

//Alignment. Convinces all units in unit inventory to move at similar velocities.
void Boids::setAlignment( const Unit &unit, const Unit_Inventory &ui ) {
    int temp_tot_x = 0;
    int temp_tot_y = 0;

    int flock_count = 0;
    if ( !ui.unit_inventory_.empty() ) {
        for ( auto i = ui.unit_inventory_.begin(); i != ui.unit_inventory_.end() && !ui.unit_inventory_.empty(); ++i ) {
            if ( i->second.type_ != UnitTypes::Zerg_Drone && i->second.type_ != UnitTypes::Zerg_Overlord && i->second.type_ != UnitTypes::Buildings ) {
                temp_tot_x += (int)i->second.bwapi_unit_->getVelocityX() ; //get the horiz element.
                temp_tot_y += (int)i->second.bwapi_unit_->getVelocityY() ; // get the vertical element. Averaging angles was trickier than I thought. 
                flock_count++;
            }
        }
        //double theta = atan2( temp_tot_y - unit->getVelocityY() , temp_tot_x - unit->getVelocityX() );  // subtract out the unit's personal heading.

        if ( flock_count > 1 ) {
            attune_dx_ = ( ( temp_tot_x - unit->getVelocityX() ) / (flock_count - 1) + unit->getVelocityX() ) * 24 ;
            attune_dy_ = ( ( temp_tot_y - unit->getVelocityY() ) / (flock_count - 1) + unit->getVelocityY() ) * 24 ; // think the velocity is per frame, I'd prefer it per second so its scale is sensical.
        }
        else {
            attune_dx_ = (unit->getVelocityX() ) * 24 ;
            attune_dy_ = (unit->getVelocityY() ) * 24 ;
        }
    }
}

//Centralization, all units prefer sitting along map veins to edges.
void Boids::setCentralize( const Position &pos, const Inventory &inventory ) {
    double temp_centralization_dx_ = 0;
    double temp_centralization_dy_ = 0;
    WalkPosition map_dim = WalkPosition(TilePosition( {Broodwar->mapWidth(), Broodwar->mapHeight()} ) );
    for ( int x = -5; x <= 5; ++x ) {
        for ( int y = -5; y <= 5; ++y ) {
            int mini_x = WalkPosition( pos ).x;
            int mini_y = WalkPosition( pos ).y;
            double centralize_x = mini_x + x;
            double centralize_y = mini_y + y;
            if ( !(x == 0 && y == 0) &&
                centralize_x < map_dim.x && 
                centralize_y < map_dim.y &&
                centralize_x > 0 && 
                centralize_y > 0 &&
                ( inventory.map_veins_[centralize_x][centralize_y] > inventory.map_veins_[mini_x][mini_y] || inventory.map_veins_[centralize_x][centralize_y] > 175 )  )
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

    const Position loc_center = ui.getMeanCombatLocation();
    if ( loc_center != Position( 0, 0 ) ) {
        double cohesion_x = loc_center.x - pos.x;
        double cohesion_y = loc_center.y - pos.y;
        double theta = atan2( cohesion_y, cohesion_x );
        cohesion_dx_ = cos( theta ) * 0.25 * unit->getDistance( loc_center );
        cohesion_dy_ = sin( theta ) * 0.25 * unit->getDistance( loc_center );
    }
}

//Attraction, pull towards enemy units that we can attack. Requires some macro variables to be in place.
void Boids::setAttraction( const Unit &unit, const Position &pos, Unit_Inventory &ei, Inventory &inv, const bool &army_starved ) {

	bool armed = unit->getType().airWeapon() != WeaponTypes::None || unit->getType().groundWeapon() != WeaponTypes::None;
	bool healthy = unit->getHitPoints() > 0.25 * unit->getType().maxHitPoints();
	bool ready_to_fight = !army_starved || ei.stock_total_ <= 0.75 * exp(inv.ln_army_stock_);
	bool enemy_scouted = ei.getMeanBuildingLocation() != Position(0, 0) || inv.start_positions_.empty();

	if ( armed && healthy && ready_to_fight && enemy_scouted) { // and your army is relatively large.
		int dist = 999999;
		bool visible_unit_found = false;
		if (!ei.unit_inventory_.empty()) { // if there isn't a visible targetable enemy, but we have an inventory of them...

            Stored_Unit* e = MeatAIModule::getClosestAttackableStored( ei , unit->getType(), unit->getPosition(), dist );
            
			if (e && e->pos_) {
				if (MeatAIModule::isClearRayTrace(pos, e->pos_, inv)) { // go to it if the path is clear,
					int dist = unit->getDistance(e->pos_);
					int dist_x = e->pos_.x - pos.x;
					int dist_y = e->pos_.y - pos.y;
					double theta = atan2(dist_y, dist_x);
					attract_dx_ = cos(theta) * (dist * 0.02 + 64); // run 5% towards them, plus 2 tiles.
					attract_dy_ = sin(theta) * (dist * 0.02 + 64);
				}
				else { // tilt around it
					int dist = unit->getDistance(e->pos_);
					int dist_x = e->pos_.x - pos.x;
					int dist_y = e->pos_.y - pos.y;
					double theta = atan2(dist_y, dist_x);

					double tilt = rng_direction_ * 0.75 * 3.1415; // random number -1..1 times 0.75 * pi, should rotate it 45 degrees away from directly backwards. 
					attract_dx_ = cos(theta + tilt) * (dist * 0.02 + 64); // run 5% towards them, plus 2 tiles.
					attract_dy_ = sin(theta + tilt) * (dist * 0.02 + 64);
				}
			}
		}
	}
	else if ( !enemy_scouted && healthy && ready_to_fight) {
		int randomIndex = rand() % inv.start_positions_.size();
		if (inv.start_positions_[randomIndex]) {
			Position possible_base = inv.start_positions_[randomIndex];
			int dist = unit->getDistance(possible_base);
			int dist_x = possible_base.x - pos.x;
			int dist_y = possible_base.y - pos.y;
			double theta = atan2(dist_y, dist_x);
			attract_dx_ = cos(theta) * dist; // run 100% towards them.
			attract_dy_ = sin(theta) * dist;
			inv.start_positions_.erase(inv.start_positions_.begin() + randomIndex);
		}
	}
}

//Seperation from nearby units, search very local neighborhood of 2 tiles.
void Boids::setSeperation( const Unit &unit, const Position &pos, const Unit_Inventory &ui ) {
    Position seperation = ui.getMeanLocation();
    if ( seperation != pos ) { // don't seperate from yourself, that would be a disaster.
        double seperation_x = seperation.x - pos.x;
        double seperation_y = seperation.y - pos.y;
        double theta = atan2( seperation_y, seperation_x );
        seperation_dx_ = cos( theta ) * 64; // run 2 tiles away from everyone. Should help avoid being stuck in those wonky spots.
        seperation_dy_ = sin( theta ) * 64;
    }
}

void Boids::setObjectAvoid( const Unit &unit, const Position &pos, const Inventory &inventory ) {

    double temp_walkability_dx_ = 0;
    double temp_walkability_dy_ = 0;
    if ( !unit->isFlying() ) {
        for ( int x = -5; x <= 5; ++x ) {
            for ( int y = -5; y <= 5; ++y ) {
                double walkability_x = TilePosition( pos ).x + x;
                double walkability_y = TilePosition( pos ).y + y;
                if ( !(x == 0 && y == 0) ) {
                    if ( walkability_x >= Broodwar->mapWidth() * 32 || walkability_y >= Broodwar->mapHeight() * 32 || // out of bounds by above map value.
                        walkability_x < 0 || walkability_y < 0 ||  // out of bounds below map,  0.
                        Broodwar->getGroundHeight( TilePosition(walkability_x , walkability_y) ) != Broodwar->getGroundHeight( unit->getTilePosition() ) ||  //If a position is on a different level, it's essentially unwalkable.
                        !MeatAIModule::isClearRayTrace(pos, Position(walkability_x,walkability_y), inventory ) ) { // or if the path to the target is relatively unwalkable.
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


        double tilt =  rng_direction_ * 0.75 * 3.1415; // random number -1..1 times 0.75 * pi, should rotate it 45 degrees away from directly backwards. 
        walkability_dx_ = cos( theta + tilt ) * temp_dist * 0.25;
        walkability_dy_ = sin( theta + tilt ) * temp_dist * 0.25;
    }
}
