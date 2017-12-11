#pragma once

#include "MeatAIModule.h"
#include "CobbDouglas.h"
#include "InventoryManager.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"
#include "GeneticHistoryManager.h"
#include "Fight_MovementManager.h"
#include "AssemblyManager.h"
#include <iostream> 
#include <fstream> // for file read/writing
#include <chrono> // for in-game frame clock.
#include <stdio.h>  //for removal of files.

// MeatAI V1.00. Current V goal-> defeat GARMBOT regularly.
// Mineral Locking.
// build order selection
// Transition to air?

// reserve locations for buildings.
// workers are pulled back to closest ? in the middle of a transfer.
// geyser logic is a little wonky. Check fastest map for demonstration.
// rearange units perpendicular to opponents for instant concaves.
// units may die from burning down, extractors, or mutations. may cause confusion in inventory system.
// units sometimes full attack to an unusually large value at max X max Y.
// add concept of base?
// Marek Kadek, Opprimobot, Roman Denalis // can beat Lukas Mor
// reduce switching to weak targets. very problematic in melee firefights.
// build drones at hatch that HAS MINERALS AROUND IT FIRST.
// get neutrals into a neutral inventory.
// rally buildings.
//unit->getLastCommandFrame()    jaj22 : If that's older than the latency then getOrder is valid.
// disable latency compensation? http://www.teamliquid.net/blogs/519872-towards-a-good-sc-bot-p56-latency

//Quick fix problems.
// update unit speeds with upgrades. All approximately 1.5 times faster.
// sometimes long distance mine when you have no mines at all.
// overlords don't need to get directly on top of target. I thought I fixed it and it did not improve combat for anyone but terran? Test again?
// don't build evo chamber if existing one is idle?
// Revisit spores?  Forcebot does Muta rush.
// overlord feeding.


//Conceptual changes.
// drone when enemy units are far away, troop when they are close? Marian devka aka killerbot.  
// Unit composition switching. Killerbot. Antiga.
// lurkers, guardians and remaining tech units.
// put vision as part of knee-jerk responses?


// Bugs and goals.
// extractor trick.

using namespace BWAPI;
using namespace Filter;
using namespace std;


void MeatAIModule::onStart()
{

    // Hello World!
    Broodwar->sendText( "Good luck, have fun!" );

    // Print the map name.
    // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
    Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

    // Enable the UserInput flag, which allows us to control the bot and type messages.
    Broodwar->enableFlag( Flag::UserInput );

    // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
    //Broodwar->enableFlag(Flag::CompleteMapInformation);

    // Set the command optimization level so that common commands can be grouped
    // and reduce the bot's APM (Actions Per Minute).
    Broodwar->setCommandOptimizationLevel( 2 );

    // Check if this is a replay
    if ( Broodwar->isReplay() )
    {
        // Announce the players in the replay
        Broodwar << "The following players are in this replay:" << std::endl;

        // Iterate all the players in the game using a std:: iterator
        Playerset players = Broodwar->getPlayers();
        for ( auto p : players )
        {
            // Only print the player if they are not an observer
            if ( !p->isObserver() )
                Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        }

    }
    else // if this is not a replay
    {
        // Retrieve you and your enemy's races. enemy() will just return the first enemy.
        // If you wish to deal with multiple enemies then you must use enemies().
        if ( Broodwar->enemy() ) // First make sure there is an enemy
            Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
    }

    //Initialize state variables
    gas_starved = false;
    army_starved = false;
    supply_starved = false;
    vision_starved = false;
    econ_starved = true;
    tech_starved = false;

    last_enemy_race = Broodwar->enemy()->getRace();

    //Initialize model variables. 
    GeneticHistory gene_history = GeneticHistory( ".\\bwapi-data\\read\\output.txt" );
    if ( _AT_HOME_MODE ) {
        gene_history = GeneticHistory( ".\\bwapi-data\\write\\output.txt" );
    }

    delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
    gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

                                            //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
    alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
    alpha_vis = gene_history.a_vis_out_mutate_; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant. Currently defunct.
    alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
    alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
    win_rate = (1 - gene_history.loss_rate_);

    //get initial build order.
    buildorder.getInitialBuildOrder( gene_history.build_order_ );

    //update local resources
    Resource_Inventory neutral_inventory; // for first initialization.

    //update Map Grids
    inventory.updateBuildablePos();
    inventory.updateUnwalkable();
    inventory.updateSmoothPos();
    inventory.updateMapVeins();
    inventory.updateMapVeinsOutFromMain( Position(Broodwar->self()->getStartLocation()) );
    //inventory.updateMapChokes();
    inventory.updateBaseLoc( neutral_inventory );
    inventory.getStartPositions();

    //update timers.
    short_delay = 0;
    med_delay = 0;
    long_delay = 0;
    my_reservation = Reservation();
}

void MeatAIModule::onEnd( bool isWinner )
{// Called when the game ends

    if ( !_AT_HOME_MODE ) {
        rename( ".\\bwapi-data\\read\\output.txt", ".\\bwapi-data\\write\\output.txt" );
    }// scope block.  Copy genetic files over at game start.

    ofstream output; // Prints to brood war file proper.
    output.open( ".\\bwapi-data\\write\\output.txt", ios_base::app );
    //output << "delta (gas)" << "," << "gamma (supply)" << ',' << "alpha_army" << ',' << "alpha_vis" << ',' << "alpha_econ" << ',' << "alpha_tech" << ',' << "Race" << "," << "Won" << "Seed" << endl;
    string opponent_name = Broodwar->enemy()->getName().c_str();
    output << delta << "," << gamma << ',' << alpha_army << ',' << alpha_econ << ',' << alpha_tech << ',' << Broodwar->enemy()->getRace().c_str() << "," << isWinner << ',' << short_delay << ',' << med_delay << ',' << long_delay << ',' << opponent_name << ',' << Broodwar->mapFileName().c_str() << ',' << buildorder.initial_building_gene_ << endl;
    output.close();
}

void MeatAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
        return;

    // Start Game clock.
    // Performance Qeuery Timer
    // http://www.decompile.com/cpp/faq/windows_timer_api.htm

    // Assess enemy stock and general positions.
    std::chrono::duration<double, std::milli> preamble_time;
    std::chrono::duration<double, std::milli> larva_time;
    std::chrono::duration<double, std::milli> worker_time;
    std::chrono::duration<double, std::milli> scout_time;
    std::chrono::duration<double, std::milli> combat_time;
    std::chrono::duration<double, std::milli> detector_time;
    std::chrono::duration<double, std::milli> upgrade_time;
    std::chrono::duration<double, std::milli> creepcolony_time;
    std::chrono::duration<double, std::milli> total_frame_time; //will use preamble start time.

    auto start_preamble = std::chrono::high_resolution_clock::now();


    // Game time;
    int t_game = Broodwar->getFrameCount(); // still need this for mining script.

                                            // Let us see what is stored in each unit_inventory and update it. Invalidate unwanted units. Most notably, geysers become extractors on death.

    for ( auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); e++ ) {
        if ( (*e).second.bwapi_unit_ && (*e).second.bwapi_unit_->exists() ) { // If the unit is visible now, update its position.
            (*e).second.pos_ = (*e).second.bwapi_unit_->getPosition();
            (*e).second.type_ = (*e).second.bwapi_unit_->getType();
            (*e).second.current_hp_ = (*e).second.bwapi_unit_->getHitPoints();
            (*e).second.valid_pos_ = true;
            //Broodwar->sendText( "Relocated a %s.", (*e).second.type_.c_str() );
        }
        else if ( Broodwar->isVisible( TilePosition( e->second.pos_ ) ) /*&& e->second.type_.canMove()*/ ) {  // if you can see the tile it SHOULD be at Burned down buildings will pose a problem in future.

            bool present = false;

            Unitset enemies_tile = Broodwar->getUnitsOnTile( TilePosition( e->second.pos_ ), IsEnemy || IsNeutral );  // Confirm it is present.  Addons convert to neutral if their main base disappears.
            for ( auto et = enemies_tile.begin(); et != enemies_tile.end(); ++et ) {
                present = (*et)->getID() == e->second.unit_ID_ /*|| (*et)->isCloaked() || (*et)->isBurrowed()*/;
                if ( present ) {
                    break;
                }
            }
            if ( !present || enemies_tile.empty() ) { // If the last known position is visible, and the unit is not there, then they have an unknown position.  Note a variety of calls to e->first cause crashes here. 
                e->second.valid_pos_ = false;
                //Broodwar->sendText( "Lost track of a %s.", e->second.type_.c_str() );
            }
        }

        if ( e->second.type_ == UnitTypes::Resource_Vespene_Geyser ) { // Destroyed refineries revert to geyers, requiring the manual catch 
            e->second.valid_pos_ = false;
        }

        if ( _ANALYSIS_MODE && e->second.valid_pos_ == true ) {
            if ( isOnScreen( e->second.pos_ ) && e->second.valid_pos_) {
                Broodwar->drawCircleMap( e->second.pos_, (e->second.type_.dimensionUp() + e->second.type_.dimensionLeft()) / 2, Colors::Red ); // Plot their last known position.
            }
        }
    }

    for ( auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); ) {
        if ( e->second.type_ == UnitTypes::Resource_Vespene_Geyser || // Destroyed refineries revert to geyers, requiring the manual catc.
            e->second.type_ == UnitTypes::None ) { // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            e = enemy_inventory.unit_inventory_.erase( e ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++e;
        }
    }

    //Unitset enemy_set_all = getUnit_Set( enemy_inventory, { 0,0 }, 999999 ); // for allin mode.

                                                                             // easy to update friendly unit inventory.
    if ( friendly_inventory.unit_inventory_.size() == 0 ) {
        friendly_inventory = Unit_Inventory( Broodwar->self()->getUnits() );
    }
    else {
        friendly_inventory.updateUnitInventory( Broodwar->self()->getUnits() );
    }


    for ( auto f = friendly_inventory.unit_inventory_.begin(); f != friendly_inventory.unit_inventory_.end() && !friendly_inventory.unit_inventory_.empty();) {
        if ( f->second.type_ == UnitTypes::Resource_Vespene_Geyser || // Destroyed refineries revert to geyers, requiring the manual catc.
            f->second.type_ == UnitTypes::None || // sometimes they have a "none" in inventory. This isn't very reasonable, either.
            !f->second.bwapi_unit_ || !f->second.bwapi_unit_->exists() ) {
            f = friendly_inventory.unit_inventory_.erase( f ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
        }
        else {
            ++f;
        }
    }

    //Update posessed minerals. Erase those that are mined out.
    for ( auto r = neutral_inventory.resource_inventory_.begin(); r != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty();) {
        TilePosition resource_pos = TilePosition( r->second.pos_ );
        bool erasure_sentinel = false;

        if ( r->second.bwapi_unit_ && r->second.bwapi_unit_->exists() ) {
            r->second.current_stock_value_ = r->second.bwapi_unit_->getResources();
            r->second.valid_pos_ = true;
            r->second.type_ = r->second.bwapi_unit_->getType();
            r->second.occupied_natural_ = !r->second.bwapi_unit_->getUnitsInRadius( 256, IsResourceDepot && IsOwned  ).empty() || !getUnitInventoryInRadius(friendly_inventory, UnitTypes::Zerg_Lair, r->second.pos_, 256).unit_inventory_.empty() || !getUnitInventoryInRadius( friendly_inventory, UnitTypes::Zerg_Hive, r->second.pos_, 256 ).unit_inventory_.empty(); // is there a resource depot in 250 of it?
           //r->second.full_resource_ = r->second.number_of_miners_ >= 2 ; // not used at this time. Inproperly initialized so I am leaving it as null to help identify when there is a problem faster.
        }

        if ( Broodwar->isVisible( resource_pos ) ) {
            Unitset resource_tile = Broodwar->getUnitsOnTile( resource_pos, IsMineralField || IsResourceContainer || IsRefinery );  // Confirm it is present.
            if ( resource_tile.empty() ) {
                r = neutral_inventory.resource_inventory_.erase( r ); // get rid of these. Don't iterate if this occurs or we will (at best) end the loop with an invalid iterator.
                erasure_sentinel = true;
            }
        }

        if ( !erasure_sentinel ) {
            r++;
        }
    }

    if ( last_enemy_race != Broodwar->enemy()->getRace() ) {
        //Initialize model variables. 
        GeneticHistory gene_history = GeneticHistory( ".\\bwapi-data\\read\\output.txt" );
        if ( _AT_HOME_MODE ) {
            gene_history = GeneticHistory( ".\\bwapi-data\\write\\output.txt" );
        }

        delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
        gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

                                                //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
        alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
        alpha_vis = gene_history.a_vis_out_mutate_; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant. Currently defunct.
        alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
        alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
        win_rate = (1 - gene_history.loss_rate_);
        last_enemy_race = Broodwar->enemy()->getRace();
        Broodwar->sendText( "WHOA! %s is broken. That's a good random.", last_enemy_race.c_str() );
    }

    //Update important variables.  Enemy stock has a lot of dependencies, updated above.
    inventory.updateLn_Army_Stock( friendly_inventory );
    inventory.updateLn_Tech_Stock( friendly_inventory );
    inventory.updateLn_Worker_Stock();
    inventory.updateVision_Count();

    inventory.updateLn_Supply_Remain( friendly_inventory );
    inventory.updateLn_Supply_Total();

    inventory.updateLn_Gas_Total();
    inventory.updateLn_Min_Total();

    inventory.updateGas_Workers();
    inventory.updateMin_Workers();

    inventory.updateMin_Possessed();
    inventory.updateHatcheries( friendly_inventory );  // macro variables, not every unit I have.
    inventory.updateWorkersClearing(friendly_inventory, neutral_inventory);

    inventory.updateStartPositions();
    if ( t_game == 0 ) {
        inventory.getExpoPositions(); // prime this once on game start.
    }

    if ( buildorder.building_gene_.empty() ) {
        buildorder.ever_clear_ = true;
    }

    my_reservation.decrementReserveTimer();
    my_reservation.confirmOngoingReservations( friendly_inventory );

    bool build_check_this_frame = false;
    bool upgrade_check_this_frame = false;

    //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.

    //Knee-jerk states: gas, supply.
    gas_starved = (inventory.getLn_Gas_Ratio() < delta && Gas_Outlet()) ||
        (!buildorder.building_gene_.empty() && (buildorder.building_gene_.begin()->getUnit().gasPrice() > Broodwar->self()->gas() || buildorder.building_gene_.begin()->getUpgrade().gasPrice() > Broodwar->self()->gas() || buildorder.building_gene_.begin()->getResearch().gasPrice() > Broodwar->self()->gas())) ||// you need gas for a required build order item.
        (tech_starved && Tech_Avail() && Broodwar->self()->gas() < 200); // you need gas because you are tech starved.
    supply_starved = (inventory.getLn_Supply_Ratio()  < gamma &&   //If your supply is disproportionately low, then you are gas starved, unless
        Broodwar->self()->supplyTotal() <= 400); // you have not hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

                                                 //Discontinuities (Cutoff if critically full, or suddenly progress towards one macro goal or another is impossible. 
    bool econ_possible = inventory.min_workers_ <= inventory.min_fields_ * 2 && (Count_Units( UnitTypes::Zerg_Drone, friendly_inventory ) < 85); // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
    bool vision_possible = true; // no vision cutoff ATM.
    bool army_possible = Broodwar->self()->supplyUsed() < 375 && exp( inventory.ln_army_stock_ ) / exp( inventory.ln_worker_stock_ ) < 2 * alpha_army / alpha_econ || Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) + Count_Units( UnitTypes::Zerg_Hydralisk_Den, friendly_inventory ) + Count_Units( UnitTypes::Zerg_Spire, friendly_inventory ) + Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, friendly_inventory ) - Broodwar->self()->incompleteUnitCount( UnitTypes::Zerg_Spawning_Pool ) <= 0; // can't be army starved if you are maxed out (or close to it), Or if you have a wild K/L ratio. Or if you can't build combat units at all.
    bool tech_possible = Tech_Avail(); // if you have no tech available, you cannot be tech starved.

                                       //Feed alpha values and cuttoff calculations into Cobb Douglas.
    CobbDouglas CD = CobbDouglas( alpha_army, exp( inventory.ln_army_stock_ ), army_possible, alpha_tech, exp( inventory.ln_tech_stock_ ), tech_possible, alpha_econ, exp( inventory.ln_worker_stock_ ), econ_possible );

    tech_starved = CD.tech_starved();
    army_starved = CD.army_starved();
    econ_starved = CD.econ_starved();

    double econ_derivative = CD.econ_derivative;
    double army_derivative = CD.army_derivative;
    double tech_derivative = CD.tech_derivative;

    //Unitset enemy_set = getEnemy_Set(enemy_inventory);
    enemy_inventory.updateUnitInventorySummary();
    friendly_inventory.updateUnitInventorySummary();
    inventory.est_enemy_stock_ = (int)(enemy_inventory.stock_total_ * (1 + 1 - inventory.vision_tile_count_ / (double)map_area)); //assumes enemy stuff is uniformly distributed. Bad assumption.
    
    // Display the game status indicators at the top of the screen	
    if ( _ANALYSIS_MODE ) {

        Print_Unit_Inventory( 0, 50, friendly_inventory );
        Print_Upgrade_Inventory( 375, 80 );
        Print_Reservations( 250, 170, my_reservation );
        if ( buildorder.checkEmptyBuildOrder() ) {
            Print_Unit_Inventory( 500, 170, enemy_inventory );
        }
        else {
            Print_Build_Order_Remaining( 500, 170, buildorder );
        }

        Broodwar->drawTextScreen( 0, 0, "Reached Min Fields: %d", inventory.min_fields_ );
        Broodwar->drawTextScreen( 0, 10, "Active Workers: %d", inventory.gas_workers_ + inventory.min_workers_ );
        Broodwar->drawTextScreen( 0, 20, "Workers (alt): (m%d, g%d)", miner_count_, gas_count_ );  //
        miner_count_ = 0; // just after the fact.
        gas_count_ = 0;
        Broodwar->drawTextScreen( 0, 30, "Active Miners: %d", inventory.min_workers_ );
        Broodwar->drawTextScreen( 0, 40, "Active Gas Miners: %d", inventory.gas_workers_ );

        Broodwar->drawTextScreen( 125, 0, "Econ Starved: %s", CD.econ_starved() ? "TRUE" : "FALSE" );  //
        Broodwar->drawTextScreen( 125, 10, "Army Starved: %s", CD.army_starved() ? "TRUE" : "FALSE" );  //
        Broodwar->drawTextScreen( 125, 20, "Tech Starved: %s", CD.tech_starved() ? "TRUE" : "FALSE" );  //

        Broodwar->drawTextScreen( 125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE" );
        Broodwar->drawTextScreen( 125, 50, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE" );
        Broodwar->drawTextScreen( 125, 60, "Gas Outlet: %s", Gas_Outlet() ? "TRUE" : "FALSE" );  //

        if ( _COBB_DOUGLASS_REVEALED ) {
            Broodwar->drawTextScreen( 125, 80, "Ln Y/L: %4.2f", CD.getlny() ); //
            Broodwar->drawTextScreen( 125, 90, "Ln Y: %4.2f", CD.getlnY() ); //
        }
        Broodwar->drawTextScreen( 125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60 ); //
        Broodwar->drawTextScreen( 125, 110, "Win Rate: %1.2f", win_rate ); //
        Broodwar->drawTextScreen( 125, 120, "Race: %s", Broodwar->enemy()->getRace().c_str() );
        Broodwar->drawTextScreen( 125, 130, "Opponent: %s", Broodwar->enemy()->getName().c_str() ); //
        Broodwar->drawTextScreen( 125, 140, "Map: %s", Broodwar->mapFileName().c_str()); //

        if ( _COBB_DOUGLASS_REVEALED ) {
            Broodwar->drawTextScreen( 250, 0, "Econ Gradient: %.2g", CD.econ_derivative );  //
            Broodwar->drawTextScreen( 250, 10, "Army Gradient: %.2g", CD.army_derivative ); //
            Broodwar->drawTextScreen( 250, 20, "Tech Gradient: %.2g", CD.tech_derivative ); //

            Broodwar->drawTextScreen( 250, 40, "Alpha_Econ: %4.2f %%", CD.alpha_econ * 100 );  // As %s
            Broodwar->drawTextScreen( 250, 50, "Alpha_Army: %4.2f %%", CD.alpha_army * 100 ); //
            Broodwar->drawTextScreen( 250, 60, "Alpha_Tech: %4.2f ", CD.alpha_tech * 100 ); // No longer a % with capital-augmenting technology.

            Broodwar->drawTextScreen( 250, 80, "Delta_gas: %4.2f", delta ); //
            Broodwar->drawTextScreen( 250, 90, "Gamma_supply: %4.2f", gamma ); //
        }

        Broodwar->drawTextScreen( 250, 100, "Time to Completion: %d", my_reservation.building_timer_ ); //
        Broodwar->drawTextScreen( 250, 110, "Freestyling: %s", buildorder.checkEmptyBuildOrder() ? "TRUE" : "FALSE" ); //
        Broodwar->drawTextScreen( 250, 120, "Last Builder Sent: %d", my_reservation.last_builder_sent_ );
        Broodwar->drawTextScreen( 250, 130, "Last Building: %s", buildorder.last_build_order.c_str() ); //
        Broodwar->drawTextScreen( 250, 140, "Next Expo Loc: (%d , %d)", inventory.next_expo_.x, inventory.next_expo_.y ); //
        if ( buildorder.checkEmptyBuildOrder() ) {
            Broodwar->drawTextScreen( 250, 160, "Total Reservations: Min: %d, Gas: %d", my_reservation.min_reserve_, my_reservation.gas_reserve_ );
        }
        else {
            Broodwar->drawTextScreen( 250, 160, "Top in Build Order: Min: %d, Gas: %d", buildorder.building_gene_.begin()->getUnit().mineralPrice(), buildorder.building_gene_.begin()->getUnit().gasPrice() );
        }

        for ( auto &p : inventory.expo_positions_ ) {
            Broodwar->drawCircleMap( Position( p ), 25, Colors::Green, TRUE );
        }
        Broodwar->drawCircleMap( Position( inventory.next_expo_ ), 10, Colors::Red, TRUE );

        //vision belongs here.

        Broodwar->drawTextScreen( 375, 20, "Enemy Stock(Est.): %d", inventory.est_enemy_stock_ );
        Broodwar->drawTextScreen( 375, 30, "Army Stock: %d", (int)exp( inventory.ln_army_stock_ ) ); //
        Broodwar->drawTextScreen( 375, 40, "Gas (Pct. Ln.): %4.2f", inventory.getLn_Gas_Ratio() );
        Broodwar->drawTextScreen( 375, 50, "Vision (Pct.): %4.2f", inventory.vision_tile_count_ / (double)map_area );  //
        Broodwar->drawTextScreen( 375, 60, "Unexplored Starts: %d", (int)inventory.start_positions_.size() );  //

        //Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %4.2f", inventory.getLn_Supply_Ratio() );  //
        //Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d",  inventory.vision_tile_count_ );  //
        //Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

        Broodwar->drawTextScreen( 500, 20, "Performance:" );  // 
        Broodwar->drawTextScreen( 500, 30, "APM: %d", Broodwar->getAPM() );  // 
        Broodwar->drawTextScreen( 500, 40, "APF: %4.2f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS() );  // 
        Broodwar->drawTextScreen( 500, 50, "FPS: %4.2f", Broodwar->getAverageFPS() );  // 
        Broodwar->drawTextScreen( 500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames() );  //

        Broodwar->drawTextScreen( 500, 70, delay_string ); // Flickers. Annoying.
        Broodwar->drawTextScreen( 500, 80, preamble_string );
        Broodwar->drawTextScreen( 500, 90, larva_string );
        Broodwar->drawTextScreen( 500, 100, worker_string );
        Broodwar->drawTextScreen( 500, 110, scouting_string );
        Broodwar->drawTextScreen( 500, 120, combat_string );
        Broodwar->drawTextScreen( 500, 130, detection_string );
        Broodwar->drawTextScreen( 500, 140, upgrade_string );
        Broodwar->drawTextScreen( 500, 150, creep_colony_string );

        for ( auto p = neutral_inventory.resource_inventory_.begin(); p != neutral_inventory.resource_inventory_.end() && !neutral_inventory.resource_inventory_.empty(); ++p ) {
            if ( isOnScreen( p->second.pos_ ) ) {
                Broodwar->drawCircleMap( p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan ); // Plot their last known position.
                Broodwar->drawTextMap( p->second.pos_, "%d", p->second.current_stock_value_ ); // Plot their current value.
                Broodwar->drawTextMap( p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_ ); // Plot their current value.
            }
        }

        //for ( vector<int>::size_type i = 0; i != inventory.buildable_positions_.size(); ++i ) {
        //    for ( vector<int>::size_type j = 0; j != inventory.buildable_positions_[i].size(); ++j ) {
        //        if ( inventory.buildable_positions_[i][j] == false ) {
        //            if ( isOnScreen( { (int)i * 32 + 16, (int)j * 32 + 16 } ) ) {
        //                Broodwar->drawCircleMap( i * 32 + 16, j * 32 + 16, 1, Colors::Yellow );
        //            }
        //        }
        //    }
        //} // both of these structures are on the same tile system.

        //for ( vector<int>::size_type i = 0; i != inventory.base_values_.size(); ++i ) {
        //    for ( vector<int>::size_type j = 0; j != inventory.base_values_[i].size(); ++j ) {
        //        if ( inventory.base_values_[i][j] > 1 ) {
        //            Broodwar->drawTextMap( i * 32 + 16, j * 32 + 16, "%d", inventory.base_values_[i][j] );
        //        }
        //    };
        //} // not that pretty to look at.

        for ( vector<int>::size_type i = 0; i < inventory.smoothed_barriers_.size(); ++i ) {
            for ( vector<int>::size_type j = 0; j < inventory.smoothed_barriers_[i].size(); ++j ) {
                if ( inventory.smoothed_barriers_[i][j] == 0 ) {
                    if ( isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 } ) ) {
                        //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j] );
                        //Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
                    }
                }
                else if ( inventory.smoothed_barriers_[i][j] > 0 ) {
                    if ( isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 } ) ) {
                        //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j] );
                        Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
                    }
                }

            };
        } // Pretty to look at!

        if ( _COBB_DOUGLASS_REVEALED ) {
            for ( vector<int>::size_type i = 0; i < inventory.map_veins_.size(); ++i ) {
                for ( vector<int>::size_type j = 0; j < inventory.map_veins_[i].size(); ++j ) {
                    if ( inventory.map_veins_[i][j] > 100 ) {
                        if ( isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 } ) ) {
                            //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
                            Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
                        }
                    }
                    if ( inventory.map_veins_[i][j] == 1 ) { // should only highlight smoothed-out barriers.
                        if ( isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 } ) ) {
                            //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
                            Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
                        }
                    }
                }
            } // Pretty to look at!

            //if ( !inventory.map_veins_out_.empty() ) {
            //    for ( vector<int>::size_type i = 0; i < inventory.map_veins_out_.size(); ++i ) {
            //        for ( vector<int>::size_type j = 0; j < inventory.map_veins_out_[i].size(); ++j ) {
            //            //if ( inventory.map_veins_[i][j] > 175 ) {
            //            if ( isOnScreen( Position( i * 8 + 4, j * 8 + 4 ) ) && inventory.map_veins_[i][j] > 175 ) {
            //                Broodwar->drawTextMap( i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_out_[i][j] );
            //            }
            //        }
            //    } // Pretty to look at!
            //}
        }

        //for ( vector<int>::size_type i = 0; i < inventory.map_chokes_.size(); ++i ) {
        //    for ( vector<int>::size_type j = 0; j < inventory.map_chokes_[i].size(); ++j ) {
        //        if ( inventory.map_chokes_[i][j] > 1 ) {
        //            if ( isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 } ) ) {
        //                Broodwar->drawTextMap( i * 8 + 4, j * 8 + 4, "%d", inventory.map_chokes_[i][j] );
        //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, inventory.map_chokes_[i][j]*8, Colors::Cyan );
        //            }
        //        }
        //    }
        //} // Pretty to look at!

        if ( _ANALYSIS_MODE ) {
            for ( auto &u : Broodwar->self()->getUnits() ) {
                if ( u->getLastCommand().getType() != UnitCommandTypes::Attack_Move && u->getType() != UnitTypes::Zerg_Extractor && u->getLastCommand().getType() != UnitCommandTypes::Attack_Unit ) {
                    Broodwar->drawTextMap( u->getPosition(), u->getLastCommand().getType().c_str() );
                }
            }
        }

    }// close analysis mode

    auto end_preamble = std::chrono::high_resolution_clock::now();
    preamble_time = end_preamble - start_preamble;

    // Prevent spamming by only running our onFrame once every number of latency frames.
    // Latency frames are the number of frames before commands are processed.
    if ( Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0 )
        return;

    // Iterate through all the units that we own
    for ( auto &u : Broodwar->self()->getUnits() )
    {
        // Ignore the unit if it no longer exists
        // Make sure to include this block when handling any Unit pointer!
        if ( !u || !u->exists() )
            continue;
        // Ignore the unit if it has one of the following status ailments
        if ( u->isLockedDown() ||
            u->isMaelstrommed() ||
            u->isStasised() )
            continue;
        // Ignore the unit if it is in one of the following states
        if ( u->isLoaded() ||
            !u->isPowered() ||
            u->isStuck() )
            continue;
        // Ignore the unit if it is incomplete or busy constructing
        if ( !u->isCompleted() ||
            u->isConstructing() )
            continue;

        // Finally make the unit do some stuff!
        // Unit creation & Hatchery management loop
        auto start_larva = std::chrono::high_resolution_clock::now();
        if ( u->getType() == UnitTypes::Zerg_Larva || (u->getType() == UnitTypes::Zerg_Hydralisk && !u->isUnderAttack() ) ) // A resource depot is a Command Center, Nexus, or Hatchery.
        {
            // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition.
            Reactive_Build( u, inventory, friendly_inventory, enemy_inventory );
        }
        auto end_larva = std::chrono::high_resolution_clock::now();

        // Worker Loop
        auto start_worker = std::chrono::high_resolution_clock::now();
        if ( u->getType().isWorker() && !isRecentCombatant( u ) )
        {
            bool want_gas = gas_starved && inventory.gas_workers_ < 3 * (Count_Units(UnitTypes::Zerg_Extractor, friendly_inventory) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor));  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  Does not count worker IN extractor.
            bool too_much_gas = Broodwar->self()->gas() > Broodwar->self()->minerals() * delta;
            if (Broodwar->getFrameCount() == 0) {
                u->stop();
                continue; // fixes the fact that drones auto-lock to something on game start. Now we don't triple-stack part of our initial drones.
            }

            Stored_Unit& miner = friendly_inventory.unit_inventory_.find( u )->second;

            //bool gas_flooded = Broodwar->self()->gas() * delta > Broodwar->self()->minerals(); // Consider you might have too much gas.


            if ( miner.locked_mine_ ) {
                Diagnostic_Line(miner.pos_, miner.locked_mine_->getPosition(), Colors::Green);
            }

            //// Building subloop.
            //if (miner.isClearing(neutral_inventory)) {
            //    continue;
            //}

            if ( !IsCarryingGas( u ) && !IsCarryingMinerals( u ) && my_reservation.last_builder_sent_ < t_game - Broodwar->getLatencyFrames() - 5 && !build_check_this_frame ){ //only get those that are in line or gathering minerals, but not carrying them. This always irked me.
                build_check_this_frame = true;
                inventory.getExpoPositions();
                if ( Building_Begin( u, inventory, enemy_inventory ) ) {
                    miner.stopMine(neutral_inventory);
                    continue;
                }
            } // Close Build loop

            if ( (my_reservation.reservation_map_.find(UnitTypes::Zerg_Hatchery) != my_reservation.reservation_map_.end()  || Broodwar->self()->minerals() > 150) && inventory.hatches_ >= 2 && Nearby_Blocking_Minerals( u, friendly_inventory) && !inventory.workers_are_clearing_ ) {
                //my_reservation.removeReserveSystem( UnitTypes::Zerg_Hatchery );
                miner.stopMine(neutral_inventory);
                Worker_Clear(u, friendly_inventory);
                if (miner.locked_mine_) {
                    inventory.updateWorkersClearing(friendly_inventory, neutral_inventory);
                    continue;
                }
            }

            // Lock all loose workers down. Maintain gas/mineral balance. 
            if ( isIdleEmpty( miner.bwapi_unit_ ) || ((want_gas || too_much_gas) && !miner.isClearing(neutral_inventory) && inventory.last_gas_check_ < t_game - 5 * 24) ) { //if this is your first worker of the frame consider resetting him.
                miner.stopMine( neutral_inventory );
                inventory.last_gas_check_ = t_game;
                if ( want_gas ) {
                    Worker_Gas( u, friendly_inventory );
                    if ( miner.locked_mine_ ) {
                        continue;
                    }
                    else { // do SOMETHING.
                        Worker_Mine(u, friendly_inventory);
                        if (miner.locked_mine_) {
                            continue;
                        }
                    }
                }
                else if ( !want_gas || too_much_gas ) {
                    Worker_Mine( u, friendly_inventory );
                    if ( miner.locked_mine_ ) {
                        continue;
                    }
                    else { // do SOMETHING.
                        Worker_Gas(u, friendly_inventory);
                        if (miner.locked_mine_) {
                            continue;
                        }
                    }
                }
            }

            if ( miner.bwapi_unit_->isCarryingMinerals() || miner.bwapi_unit_->isCarryingGas() || miner.bwapi_unit_->getOrderTarget() == NULL ) {
                continue;
            }

            if (miner.locked_mine_ && miner.locked_mine_->getID() != miner.bwapi_unit_->getOrderTarget()->getID() ) {

                //if ( miner.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Morph || miner.bwapi_unit_->getLastCommand().getType() == UnitCommandTypes::Build || miner.bwapi_unit_->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
                //    my_reservation.removeReserveSystem( miner.bwapi_unit_->getBuildType() );
                //}

                //if ( !miner.getMine(neutral_inventory) ) {
                //    miner.stopMine( neutral_inventory ); //Hey! If you can't get back to work something's wrong with you and we're resetting you.
                //}
                if (!miner.bwapi_unit_->gather(miner.locked_mine_)) {
                    miner.stopMine(neutral_inventory); //Hey! If you can't get back to work something's wrong with you and we're resetting you.
                }
                //if (miner.getMine(neutral_inventory) && !miner.bwapi_unit_->gather(miner.locked_mine_) && miner.bwapi_unit_->getLastCommand().getTargetPosition() != Position(miner.getMine(neutral_inventory)->pos_)) {
                //    miner.stopMine(neutral_inventory); //Hey! If you can't get back to work something's wrong with you and we're resetting you.
                //}
            }


        } // Close Worker management loop
        auto end_worker = std::chrono::high_resolution_clock::now();

        //Scouting/vision loop. Intially just brownian motion, now a fully implemented boids-type algorithm.
        auto start_scout = std::chrono::high_resolution_clock::now();
        if ( (isIdleEmpty( u ) && !u->isAttacking() && !u->isUnderAttack() /*&& u->getType() != UnitTypes::Zerg_Overlord*/ && u->getType() != UnitTypes::Zerg_Drone &&  u->getType() != UnitTypes::Zerg_Larva && (u->canMove() || u->isBurrowed()) && u->getLastCommandFrame() < t_game - 24 ) )
        { //Scout if you're not a drone or larva and can move.
            Boids boids;
            bool enemy_found = enemy_inventory.getMeanBuildingLocation() != Position( 0, 0 ) && enemy_inventory.stock_total_ > 0; //(u->getType() == UnitTypes::Zerg_Overlord && !supply_starved)
            if ( (!enemy_found && inventory.start_positions_.empty()) && (!army_starved || army_derivative == 0) ) {
                boids.Boids_Movement( u, 12, friendly_inventory, enemy_inventory, inventory, false); // not army starved in this case.
            }
            else {
                boids.Boids_Movement( u, 1, friendly_inventory, enemy_inventory, inventory, true ); // keep this because otherwise they clump up very heavily, like mutas. Don't want to lose every overlord to one AOE.
            }
        } // If it is a combat unit, then use it to attack the enemy.
        auto end_scout = std::chrono::high_resolution_clock::now();


        //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
        auto start_combat = std::chrono::high_resolution_clock::now();
        if ( ( (u->getType() != UnitTypes::Zerg_Larva && u->getType().canAttack()) || u->getType() == UnitTypes::Zerg_Overlord ) && u->getLastCommandFrame() < Broodwar->getFrameCount() - 12 )
        {

            Stored_Unit* e_closest = getClosestThreatOrTargetStored( enemy_inventory, u->getType(), u->getPosition(), 999999 );
            if ( u->getType() == UnitTypes::Zerg_Drone || u->getType() == UnitTypes::Zerg_Overlord ) {
                e_closest = getClosestThreatOrTargetStored( enemy_inventory, u->getType(), u->getPosition(), 256 );
            }


            if ( e_closest ) { // if there are bad guys, search for friends within that area. 
                e_closest->pos_.x += e_closest->bwapi_unit_->getVelocityX();
                e_closest->pos_.y += e_closest->bwapi_unit_->getVelocityY();  //short run position forecast.

                int distance_to_foe = e_closest->pos_.getDistance( u->getPosition() );
                int appropriate_range = u->isFlying() ? e_closest->type_.airWeapon().maxRange() : e_closest->type_.groundWeapon().maxRange() ;
                int chargable_distance_net = (getProperSpeed(u) + e_closest->type_.topSpeed()) * enemy_inventory.max_cooldown_ ; // how far can you get before he shoots?

                int search_radius = max(chargable_distance_net + appropriate_range + 64, enemy_inventory.max_range_ + 64);
                Unit_Inventory enemy_loc_around_target = getUnitInventoryInRadius( enemy_inventory, e_closest->pos_, distance_to_foe + search_radius );
                Unit_Inventory enemy_loc_around_self = getUnitInventoryInRadius(enemy_inventory, u->getPosition(), distance_to_foe + search_radius);
                Unit_Inventory enemy_loc = enemy_loc_around_target + enemy_loc_around_self;

                Boids boids;

                if ( army_derivative > 0 || u->getType() == UnitTypes::Zerg_Drone ) { //In normal, non-massive army scenarioes...  

                    Unit_Inventory friend_loc_around_target = getUnitInventoryInRadius( friendly_inventory, e_closest->pos_, distance_to_foe + search_radius );
                    Unit_Inventory friend_loc_around_me = getUnitInventoryInRadius(friendly_inventory, u->getPosition(), distance_to_foe + search_radius);
                    Unit_Inventory friend_loc = friend_loc_around_target + friend_loc_around_me;
                    enemy_loc.updateUnitInventorySummary();

                    if ( !friend_loc.unit_inventory_.empty() ) { // if you exist (implied by friends).

                        friend_loc.updateUnitInventorySummary();

                        //Tally up crucial details about enemy. 
                        int e_count = enemy_loc.unit_inventory_.size();

                        int helpless_e = u->isFlying() ? enemy_loc.stock_total_ - enemy_loc.stock_shoots_up_ : enemy_loc.stock_total_ - enemy_loc.stock_shoots_down_;
                        int helpful_e = u->isFlying() ? enemy_loc.stock_shoots_up_ : enemy_loc.stock_shoots_down_; // both forget value of psi units.

                                                                                                                   //int helpless_u = 0; // filled below.  Need to actually reflect MY inventory.
                        int helpful_u = 0; // filled below.

                        if ( enemy_inventory.stock_fliers_ > 0 ) {
                            helpful_u += friend_loc.stock_shoots_up_; // double-counts hydras and units that attack both air and ground.
                        }
                        if ( enemy_inventory.stock_ground_units_ > 0 ) {
                            helpful_u += friend_loc.stock_shoots_down_; // double-counts hydras and units that attack both air and ground.
                        }
                        if ( enemy_inventory.stock_ground_units_ == 0 && enemy_inventory.stock_fliers_ == 0 ) {
                            helpful_u += friend_loc.stock_total_;
                        } // if you're off the charts, throw everything in.

                          //if ( u->getType().airWeapon() != WeaponTypes::None ) {
                          //    helpless_u += Stock_Units_ShootDown( friend_loc );
                          //}
                          //if ( u->getType().groundWeapon() != WeaponTypes::None ) {
                          //    helpless_u += enemy_loc.stock_ground_units_;
                          //}

                        if ( e_closest->valid_pos_ ) {  // Must have a valid postion on record to attack.

                            double minimum_enemy_surface = 2 * 3.1416 * sqrt( (double)enemy_loc.volume_ / 3.1414 );
                            double minimum_friendly_surface = 2 * 3.1416 * sqrt( (double)friend_loc.volume_ / 3.1414 );
                            double unusable_surface_area_f = max( (minimum_friendly_surface - minimum_enemy_surface) / minimum_friendly_surface, 0.0 );
                            double unusable_surface_area_e = max( (minimum_enemy_surface - minimum_friendly_surface) / minimum_enemy_surface, 0.0 );
                            //double portion_blocked = min(pow(minimum_occupied_radius / search_radius, 2), 1.0); // the volume ratio (equation reduced by cancelation of 2*pi )

                            bool neccessary_attack = helpful_e < helpful_u || // attack if you outclass them and your boys are ready to fight.
                                //inventory.est_enemy_stock_ < 0.75 * exp( inventory.ln_army_stock_ ) || // attack you have a global advantage (very very rare, global army strength is vastly overestimated for them).
                                                                                                       //!army_starved || // fight your army is appropriately sized.
                                //(friend_loc.worker_count_ > 0 && u->getType() != UnitTypes::Zerg_Drone) || //Don't run if drones are present.
                                (!IsFightingUnit(e_closest->bwapi_unit_) && 64 > enemy_loc.max_range_) || // Don't run from noncombat junk.
                                ((friend_loc.max_range_ > enemy_loc.max_range_ || 32 > enemy_loc.max_range_) && helpful_e * (1 - unusable_surface_area_e) < 0.75 * helpful_u)  || // trying to do something with these surface areas.
                                (distance_to_foe < u->getType().groundWeapon().maxRange() && u->getType().groundWeapon().maxRange() > 32 && u->getLastCommandFrame() < Broodwar->getFrameCount() - 24 ) || // a stutterstep component. Should seperate it off.
                                (distance_to_foe < enemy_loc.max_range_ && distance_to_foe < chargable_distance_net && appropriate_range > 64);// don't run if they're in range and you're melee. Melee is <32, not 0. Hugely benifits against terran, hurts terribly against zerg. Lurkers vs tanks?; Just added this., hugely impactful. Not inherently in a good way, either.

//  bool retreat = u->canMove() && ( // one of the following conditions are true:
//(u->getType().isFlyer() && enemy_loc.stock_shoots_up_ > 0.25 * friend_loc.stock_fliers_) || //  Run if fliers face more than token resistance.
//( e_closest->isInWeaponRange( u ) && ( u->getType().airWeapon().maxRange() > e_closest->getType().airWeapon().maxRange() || u->getType().groundWeapon().maxRange() > e_closest->getType().groundWeapon().maxRange() ) ) || // If you outrange them and they are attacking you. Kiting?
//                                  );

                            bool force_retreat = (u->getType().isFlyer() && u->getType() != UnitTypes::Zerg_Scourge && ((u->isUnderAttack() && u->getHitPoints() < 0.5 * u->getInitialHitPoints()) || enemy_loc.stock_shoots_up_ > 0.75 * friend_loc.stock_fliers_)) || // run if you are flying (like a muta) and cannot be practical.
                                //(friend_loc.stock_shoots_up_ == 0 && enemy_loc.stock_fliers_ > 0 && enemy_loc.stock_shoots_down_ > 0 && enemy_loc.stock_ground_units_ == 0) || //run if you're getting picked off from above.
                                !e_closest->bwapi_unit_->isDetected() ||  // Run if they are cloaked. Must be visible to know if they are cloaked.
                                //helpful_u < helpful_e * 0.75 || // Run if they have local advantage on you
                                (distance_to_foe < 64 && e_closest->type_.topSpeed() <= getProperSpeed(u) && u->getType().groundWeapon().maxRange() > enemy_loc.max_range_ && enemy_loc.max_range_ < 64 &&  u->getType().groundWeapon().maxRange() > 64 && !u->isBurrowed() && Can_Fight(*e_closest, u)) || //kiting?
                                //(friend_loc.max_range_ < enemy_loc.max_range_ || 32 > friend_loc.max_range_ ) && (1 - unusable_surface_area_f) * 0.75 * helpful_u < helpful_e || // trying to do something with these surface areas.
                                (u->getType() == UnitTypes::Zerg_Overlord && (u->isUnderAttack() || (supply_starved && enemy_loc.stock_shoots_up_ > 0))) || //overlords should be cowardly not suicidal.
                                (u->getType() == UnitTypes::Zerg_Drone && (!army_starved || u->getHitPoints() < 0.50 * u->getType().maxHitPoints())); // Run if drone and (we have forces elsewhere or the drone is injured).
                                                                                                                                                      //(helpful_u == 0 && helpful_e > 0); // run if this is pointless. Should not happen because of search for attackable units? Should be redudnent in necessary_attack line one.

                            bool only_workers = Stock_Units( UnitTypes::Zerg_Drone, enemy_loc ) == enemy_loc.stock_ground_units_ ||
                                Stock_Units( UnitTypes::Protoss_Probe, enemy_loc ) == enemy_loc.stock_ground_units_ ||
                                Stock_Units( UnitTypes::Terran_SCV, enemy_loc ) == enemy_loc.stock_ground_units_;


                            bool drone_problem = only_workers && u->getType() == UnitTypes::Zerg_Drone;

                            bool is_spelled = u->isUnderStorm() || u->isUnderDisruptionWeb() || u->isUnderDarkSwarm() || u->isIrradiated(); // Run if spelled.

                            if ( neccessary_attack && !force_retreat && !is_spelled && !drone_problem ) {

                                boids.Tactical_Logic( u, enemy_loc, friend_loc, Colors::Orange ); // move towards enemy untill tactical logic takes hold at about 150 range.

                                                                                      //if (u->getType() == UnitTypes::Zerg_Drone && !ignore){
                                                                                      //	friendly_inventory.unit_inventory_.find(u)->second.stopMine(neutral_inventory);
                                                                                      //}

                                if ( _ANALYSIS_MODE ) {
                                    if ( isOnScreen( u->getPosition() ) ) {
                                        Broodwar->drawTextMap( u->getPosition().x, u->getPosition().y, "%d", helpful_u );
                                    }
                                    Position mean_loc = enemy_loc.getMeanLocation();
                                    if ( isOnScreen( mean_loc ) ) {
                                        Broodwar->drawTextMap( mean_loc.x, mean_loc.y, "%d", enemy_loc.stock_ground_units_ + enemy_loc.stock_fliers_ );
                                    }
                                }

                            }
                            else if ( is_spelled ) {
                                Stored_Unit* closest = getClosestThreatOrTargetStored( friendly_inventory, u->getType(), u->getPosition(), 128 );
                                if ( closest ) {
                                    boids.Retreat_Logic( u, *closest, enemy_inventory, friendly_inventory, inventory, Colors::Blue ); // this is not actually getting out of storm. It is simply scattering.
                                }
                            }
                            else if ( drone_problem ) {

                                if ( Count_Units_Doing( UnitTypes::Zerg_Drone, UnitCommandTypes::Attack_Unit, Broodwar->self()->getUnits() ) < enemy_loc.worker_count_ + 1 &&
                                    //friend_loc.getMeanBuildingLocation() != Position(0, 0) &&
                                    u->getHitPoints() > 0.50 * u->getType().maxHitPoints() ) {
                                    boids.Tactical_Logic( u, enemy_loc, friend_loc, Colors::Orange ); // move towards enemy untill tactical logic takes hold at about 150 range.
                                }
                            }
                            else {
                                boids.Retreat_Logic( u, *e_closest, enemy_inventory, friendly_inventory, inventory, Colors::White );

                                if ( !buildorder.ever_clear_ && ((!e_closest->type_.isWorker() && e_closest->type_.canAttack()) || (only_workers && enemy_loc.unit_inventory_.size() > 2)) && (!u->getType().canAttack() || u->getType() == UnitTypes::Zerg_Drone) ) {
                                    if ( u->getType() == UnitTypes::Zerg_Overlord ) {
                                        //see unit destruction case. We will replace this overlord, likely a foolish scout.
                                    }
                                    else {
                                        buildorder.clearRemainingBuildOrder(); // Neutralize the build order if something other than a worker scout is happening.
                                    }
                                }

                            }
                        }
                    } // close local examination.
                }
                else { // who cares what they have if the override is triggered?
                    boids.Tactical_Logic( u, enemy_inventory, friendly_inventory, Colors::Black ); // enemy inventory?
                }
            }
        }
        auto end_combat = std::chrono::high_resolution_clock::now();

        // Detectors are called for cloaked units. Only if you're not supply starved, because we only have overlords for detectors.
        auto start_detector = std::chrono::high_resolution_clock::now();
        Position c; // holder for cloaked unit position.
        bool sentinel_value = false;
        if ( /*(!army_starved || army_derivative == 0) &&*/ !supply_starved) {
            for (auto e = enemy_inventory.unit_inventory_.begin(); e != enemy_inventory.unit_inventory_.end() && !enemy_inventory.unit_inventory_.empty(); e++) {
                if ((*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || (*e).second.type_.isBurrowable()) {
                    c = (*e).second.pos_; // then we may to send in some vision.
                    Unit_Inventory friend_loc = getUnitInventoryInRadius(friendly_inventory, c, e->second.type_.sightRange()); // we check this cloaker has any friendly units nearby.
                    if (!friend_loc.unit_inventory_.empty() && friend_loc.detector_count_ == 0) {
                        sentinel_value = true;
                        break;
                    }
                } //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
            }
            if (sentinel_value) {
                int dist = 999999;
                int dist_temp = 0;
                bool detector_found = false;
                Unit detector_of_choice;
                for (auto d : Broodwar->self()->getUnits()) {
                    if (d->getType() == UnitTypes::Zerg_Overlord &&
                        !d->isUnderAttack() &&
                        d->getHitPoints() > 0.25 * d->getInitialHitPoints() /*&&
                        d->getLastCommandFrame() < Broodwar->getFrameCount() - 12*/) {
                        dist_temp = d->getDistance(c);
                        if (dist_temp < dist) {
                            dist = dist_temp;
                            detector_of_choice = d;
                            detector_found = true;
                        }
                    }
                }
                if (detector_found) {
                    Position detector_pos = detector_of_choice->getPosition();
                    double theta = atan2(c.y - detector_pos.y, c.x - detector_pos.x);
                    Position closest_loc_to_c_that_gives_vision = Position(c.x + cos(theta) * SightRange(detector_of_choice) * 0.75, c.y + sin(theta) * SightRange(detector_of_choice)) * 0.75;
                    if (closest_loc_to_c_that_gives_vision.isValid() && closest_loc_to_c_that_gives_vision != Position(0, 0)) {
                        detector_of_choice->move(closest_loc_to_c_that_gives_vision);
                        if (_ANALYSIS_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice->getPosition(), closest_loc_to_c_that_gives_vision, Colors::Cyan);
                        }
                    }
                    else {
                        detector_of_choice->move(c);
                        if (_ANALYSIS_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice->getPosition(), c, Colors::Cyan);
                        }
                    }

                }
            }
        }
        auto end_detector = std::chrono::high_resolution_clock::now();
        detector_time += end_detector - start_detector;

        //Upgrade loop:
        auto start_upgrade = std::chrono::high_resolution_clock::now();
        if ( isIdleEmpty( u ) && !u->canAttack() && u->getType() != UnitTypes::Zerg_Larva && !upgrade_check_this_frame && // no trying to morph hydras anymore.
            (u->canUpgrade() || u->canResearch() || u->canMorph()) ) { // this will need to be revaluated once I buy units that cost gas.

             upgrade_check_this_frame = Tech_Begin( u, friendly_inventory );

            //PrintError_Unit( u );
        }
        auto end_upgrade = std::chrono::high_resolution_clock::now();

        //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
        auto start_creepcolony = std::chrono::high_resolution_clock::now();
        Unit_Inventory local_e = getUnitInventoryInRadius(enemy_inventory, u->getPosition(), sqrt(pow(map_x * 32, 2) + pow(map_y * 32, 2)) / Broodwar->getStartLocations().size());
        bool can_sunken = Count_Units( UnitTypes::Zerg_Spawning_Pool, friendly_inventory ) > 0;
        bool can_spore = Count_Units( UnitTypes::Zerg_Evolution_Chamber, friendly_inventory ) > 0;
        bool need_static_d = buildorder.checkBuilding_Desired(UnitTypes::Zerg_Spore_Colony) || buildorder.checkBuilding_Desired(UnitTypes::Zerg_Sunken_Colony);
        bool want_static_d = army_starved && local_e.stock_total_ > 0 && (can_sunken || can_spore);

        local_e.updateUnitInventorySummary();

        if ( u->getType() == UnitTypes::Zerg_Creep_Colony && ( need_static_d || want_static_d ) ) {

            //Unit_Inventory incoming_e_threat = getUnitInventoryInRadius( enemy_inventory, u->getPosition(), ( sqrt( pow( map_x , 2 ) + pow( map_y , 2 ) ) * 32 ) / Broodwar->getStartLocations().size() ); 
            bool cloak_nearby = local_e.cloaker_count_ > 0;
            local_e.updateUnitInventorySummary();
            bool local_air_problem = local_e.stock_fliers_ > 0;
            bool global_air_problem = enemy_inventory.stock_fliers_ > friendly_inventory.stock_shoots_up_ * 0.75;
            buildorder.checkBuilding_Desired(UnitTypes::Zerg_Sunken_Colony);
            buildorder.checkBuilding_Desired(UnitTypes::Zerg_Spore_Colony);
            if ( can_sunken && can_spore ) {
                if ( local_air_problem || global_air_problem || cloak_nearby ) { // if they have a flyer (that can attack), get spores.
                    Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true );
                }
                else {
                    Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true );
                }
            } // build one of the two colonies based on the presence of closest units.
            else if ( can_sunken && !can_spore && !local_air_problem && !global_air_problem && !cloak_nearby ) {
                Check_N_Build( UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true );
            } // build sunkens if you only have that
            else if ( can_spore && !can_sunken ) {
                Check_N_Build( UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true );
            } // build spores if you only have that.
        } // closure: Creep colony loop
        auto end_creepcolony = std::chrono::high_resolution_clock::now();

        larva_time += end_larva - start_larva;
        worker_time += end_worker - start_worker;
        scout_time += end_scout - start_scout;
        combat_time += end_combat - start_combat;
        upgrade_time += end_upgrade - start_upgrade;
        creepcolony_time += end_creepcolony - start_creepcolony;
    } // closure: unit iterator


    auto end = std::chrono::high_resolution_clock::now();
    total_frame_time = end - start_preamble;

    preamble_time;
    larva_time;
    worker_time;
    scout_time;
    combat_time;
    detector_time;
    upgrade_time;
    creepcolony_time;
    total_frame_time; //will use preamble start time.

                      //Clock App
    if ( total_frame_time.count() > 55 ) {
        short_delay += 1;
    }
    if ( total_frame_time.count() > 1000 ) {
        med_delay += 1;
    }
    if ( total_frame_time.count() > 10000 ) {
        long_delay += 1;
    }
    if ( (short_delay > 320 || Broodwar->elapsedTime() > 90 * 60) /*|| Count_Units(UnitTypes::Zerg_Drone, friendly_inventory) == 0 || enemy_inventory.stock_total_> friendly_inventory.stock_total_ * 2*/ && _RESIGN_MODE ) //if game times out or lags out, end game with resignation.
    {
        Broodwar->leaveGame();
    }
    if ( _ANALYSIS_MODE ) {
        int n;

        n = sprintf( delay_string, "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count() ); // Is wrong.
        n = sprintf( preamble_string, "Preamble:%3.f%%,%3.fms ", preamble_time.count() / (double)total_frame_time.count() * 100, preamble_time.count() );
        n = sprintf( larva_string, "Larva:%3.f%%,%3.fms", larva_time.count() / (double)total_frame_time.count() * 100, larva_time.count() );
        n = sprintf( worker_string, "Workers:%3.f%%,%3.fms", worker_time.count() / (double)total_frame_time.count() * 100, worker_time.count() );
        n = sprintf( scouting_string, "Scouting:%3.f%%,%3.fms", scout_time.count() / (double)total_frame_time.count() * 100, scout_time.count() );
        n = sprintf( combat_string, "Combat:%3.f%%,%3.fms", combat_time.count() / (double)total_frame_time.count() * 100, combat_time.count() );
        n = sprintf( detection_string, "Detection:%3.f%%,%3.fms", detector_time.count() / (double)total_frame_time.count() * 100, detector_time.count() );
        n = sprintf( upgrade_string, "Upgrades:%3.f%%,%3.fms", upgrade_time.count() / (double)total_frame_time.count() * 100, upgrade_time.count() );
        n = sprintf( creep_colony_string, "CreepColonies:%3.f%%,%3.fms", creepcolony_time.count() / (double)total_frame_time.count() * 100, creepcolony_time.count() );

    }

} // closure: Onframe

void MeatAIModule::onSendText( std::string text )
{

    // Send the text to the game if it is not being processed.
    Broodwar->sendText( "%s", text.c_str() );

    // Make sure to use %s and pass the text as a parameter,
    // otherwise you may run into problems when you use the %(percent) character!

}

void MeatAIModule::onReceiveText( BWAPI::Player player, std::string text )
{
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void MeatAIModule::onPlayerLeft( BWAPI::Player player )
{
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText( "That was a good game. I'll remember this! %s!", player->getName().c_str() );
}

void MeatAIModule::onNukeDetect( BWAPI::Position target )
{

    // Check if the target is a valid position
    if ( target )
    {
        // if so, print the location of the nuclear strike target
        Broodwar << "Have you no shame? My sources say there's nuclear launch at " << target << std::endl;
    }
    else
    {
        // Otherwise, ask other players where the nuke is!
Broodwar->sendText( "Where's the nuke?" );
    }

    // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void MeatAIModule::onUnitDiscover( BWAPI::Unit unit )
{
    bool initially_empty = enemy_inventory.getMeanBuildingLocation() == Position( 0, 0 );

    if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) && !unit->isInvincible() ) { // safety check.
                                                                                             //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit eu = Stored_Unit( unit );

        if ( enemy_inventory.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
                                                                               //Broodwar->sendText( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else { // the insertion must have failed
               //Broodwar->sendText( "%s is already at address %p.", eu.type_.c_str(), enemy_inventory.unit_inventory_.find( unit ) ) ;
        }
    }

    //update maps, requires up-to date enemy inventories.
    if ( unit && unit->getType().isBuilding() ) {
        inventory.updateLiveMapVeins( unit, friendly_inventory, enemy_inventory, neutral_inventory );
        if ( unit->getPlayer() == Broodwar->enemy() ) {
            //update maps, requires up-to date enemy inventories.
            if ( enemy_inventory.getMeanBuildingLocation() != Position( 0, 0 ) ) {
                Stored_Unit* center_unit = getClosestStored( enemy_inventory, enemy_inventory.getMeanBuildingLocation(), 999999 ); // If the mean location is over water, nothing will be updated.
                if ( center_unit ) {
                    inventory.updateMapVeinsOutFromFoe( center_unit->pos_ );
                }

            }
        }
    }


}

void MeatAIModule::onUnitEvade( BWAPI::Unit unit )
{
    //if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check.
    //                                                                //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
    //    Stored_Unit eu = Stored_Unit( unit );

    //    if ( enemy_inventory.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
    //        Broodwar->sendText( "A %s just evaded me. Added to hiddent unit inventory, size %d", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
    //    }
    //    else { // the insertion must have failed
    //        Broodwar->sendText( "Insertion of %s failed.", eu.type_.c_str() );
    //    }
    //}
}

void MeatAIModule::onUnitShow( BWAPI::Unit unit )
{
    //if ( unit && unit->exists() && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.. (old comment?)
    //    Stored_Unit eu = Stored_Unit( unit );
    //    auto found_ptr = enemy_inventory.unit_inventory_.find( unit );
    //    if ( found_ptr != enemy_inventory.unit_inventory_.end() ) {
    //        enemy_inventory.unit_inventory_.erase( unit );
    //        Broodwar->sendText( "Redscovered a %s, hidden unit inventory is now %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
    //    }
    //    else {
    //        Broodwar->sendText( "Discovered a %s.", unit->getType().c_str() );
    //    }
    //}
}

void MeatAIModule::onUnitHide( BWAPI::Unit unit )
{


}

void MeatAIModule::onUnitCreate( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() && !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }

    if ( unit && unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone ) {
        my_reservation.removeReserveSystem( unit->getType() );
    }

    //if ( Broodwar->getFrameCount() > 1 ) {
    //    buildorder.updateRemainingBuildOrder( unit );
    //}

    //if (unit && unit->getType().isWorker()) {
    //    Stored_Unit& miner = friendly_inventory.unit_inventory_.find(unit)->second;
    //    bool want_gas = gas_starved && inventory.gas_workers_ < 3 * (Count_Units(UnitTypes::Zerg_Extractor, friendly_inventory) - Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor));  // enough gas if (many critera), incomplete 

    //    if (miner.bwapi_unit_) {
    //        miner.stopMine(neutral_inventory);
    //        if (want_gas) {
    //            Worker_Gas(unit, friendly_inventory);
    //            if (miner.locked_mine_) {
    //            }
    //            else { // do SOMETHING.
    //                Worker_Mine(unit, friendly_inventory);
    //                if (miner.locked_mine_) {
    //                }
    //            }
    //        }
    //        else if (!want_gas) {
    //            Worker_Mine(unit, friendly_inventory);
    //            if (miner.locked_mine_) {
    //            }
    //            else { // do SOMETHING.
    //                Worker_Gas(unit, friendly_inventory);
    //                if (miner.locked_mine_) {
    //                }
    //            }
    //        }
    //    }
    //}
}


void MeatAIModule::onUnitDestroy( BWAPI::Unit unit )
{
    if ( unit && !unit->getPlayer()->isAlly( Broodwar->self() ) && !unit->isInvincible() ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = enemy_inventory.unit_inventory_.find( unit );
        if ( found_ptr != enemy_inventory.unit_inventory_.end() ) {
            enemy_inventory.unit_inventory_.erase( unit );
            //Broodwar->sendText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else {
            //Broodwar->sendText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_inventory.unit_inventory_.size() );
        }
    }

    if ( unit && IsMineralField( unit ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        for ( auto potential_miner = friendly_inventory.unit_inventory_.begin(); potential_miner != friendly_inventory.unit_inventory_.end() && !friendly_inventory.unit_inventory_.empty(); potential_miner++ ) {
            if ( potential_miner->second.locked_mine_ == unit ) {
                potential_miner->second.stopMine( neutral_inventory ); // Find that particular worker stored_unit in map using the unit index. Tell him to stop mining
            }

        }
        auto found_mineral_ptr = neutral_inventory.resource_inventory_.find( unit );
        if ( found_mineral_ptr != neutral_inventory.resource_inventory_.end() ) {
            neutral_inventory.resource_inventory_.erase( unit ); //Clear that mine from the resource inventory.
            //inventory.updateBaseLoc( neutral_inventory );
        }
        else {
            //then nothing.
        }
    }

    if ( !buildorder.ever_clear_ && unit->getType() == UnitTypes::Zerg_Overlord ) {
        buildorder.building_gene_.insert( buildorder.building_gene_.begin(), Build_Order_Object( UnitTypes::Zerg_Overlord ) );
    }

    if ( unit && unit->getType().isBuilding() ) {
        inventory.updateLiveMapVeins( unit, friendly_inventory, enemy_inventory, neutral_inventory );
        if ( unit->getPlayer() == Broodwar->self() ) {
            Position current_home;
            if ( unit->getClosestUnit( IsOwned && IsResourceDepot ) && unit->getClosestUnit( IsOwned && IsResourceDepot )->exists()) {
                Position current_home = unit->getClosestUnit( IsOwned && IsResourceDepot )->getPosition();
                inventory.updateMapVeinsOutFromMain( current_home );
            }
        }
        if ( unit->getPlayer() == Broodwar->enemy() ) {
            //update maps, requires up-to date enemy inventories.
            if ( enemy_inventory.getMeanBuildingLocation() != Position( 0, 0 ) ) {
                Stored_Unit* center_unit = getClosestStored( enemy_inventory, enemy_inventory.getMeanBuildingLocation(), 999999 ); // If the mean location is over water, nothing will be updated.
                if ( center_unit ) {
                    inventory.updateMapVeinsOutFromFoe(center_unit->pos_);
                }

            }
        }
    }

    if ( unit && unit->getType().isWorker() ) {
        map<Unit, Stored_Unit>::iterator iter = friendly_inventory.unit_inventory_.find( unit );
        if ( iter != friendly_inventory.unit_inventory_.end() ) {
            Stored_Unit& miner = iter->second;
            miner.stopMine( neutral_inventory );
        }
        if ( unit->getLastCommand().getType() == UnitCommandTypes::Morph || unit->getLastCommand().getType() == UnitCommandTypes::Build || unit->getLastCommand().getTargetPosition() == Position( inventory.next_expo_ ) ) {
            my_reservation.removeReserveSystem( unit->getBuildType() );
        }
    }
}

void MeatAIModule::onUnitMorph( BWAPI::Unit unit )
{
    if ( Broodwar->isReplay() )
    {
        // if we are in a replay, then we will print out the build order of the structures
        if ( unit->getType().isBuilding() &&
            !unit->getPlayer()->isNeutral() )
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText( "%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }

    if ( unit->getBuildType().isBuilding() ) {
        buildorder.updateRemainingBuildOrder(unit->getBuildType());
    }

    if ( unit && unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone ) {
        inventory.updateLiveMapVeins( unit, friendly_inventory, enemy_inventory, neutral_inventory );
        my_reservation.removeReserveSystem( unit->getType() );
    }

    if ( unit && unit->getType().isBuilding() && unit->getPlayer() == BWAPI::Broodwar->self() ) {
        map<Unit, Stored_Unit>::iterator iter = friendly_inventory.unit_inventory_.find( unit );
        if ( iter != friendly_inventory.unit_inventory_.end() ) {
            Stored_Unit& miner = iter->second;
            miner.stopMine( neutral_inventory );
        }
    }

}

void MeatAIModule::onUnitRenegade( BWAPI::Unit unit )
{
}

void MeatAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void MeatAIModule::onUnitComplete( BWAPI::Unit unit )
{
}
