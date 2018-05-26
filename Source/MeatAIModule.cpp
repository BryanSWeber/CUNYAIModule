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
// build order selection
// Transition to air?

// reserve locations for buildings.
// workers are pulled back to closest ? in the middle of a transfer.
// geyser logic is a little wonky. Check fastest map for demonstration.
// rearange units perpendicular to opponents for instant concaves.
// units may die from burning down, extractors, or mutations. may cause confusion in inventory system.
// add concept of base?
// Marek Kadek, Opprimobot, Roman Denalis // can beat Lukas Mor
// reduce switching to weak targets. very problematic in melee firefights.
// build drones at hatch that HAS MINERALS AROUND IT FIRST.
// rally buildings.
// disable latency compensation? http://www.teamliquid.net/blogs/519872-towards-a-good-sc-bot-p56-latency

//Quick fix problems.
// overlords don't need to get directly on top of target. I thought I fixed it and it did not improve combat for anyone but terran? Test again?
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

    // Enable the UserInput flag, which allows us to control the bot and type messages. Also needed to get the screen position.
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

    //Initialize model variables. 
    GeneticHistory gene_history = GeneticHistory( ".\\bwapi-data\\read\\output.txt" );

    delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
    gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

                                            //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
    alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
    alpha_vis = gene_history.a_vis_out_mutate_; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant. Currently defunct.
    alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
    alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
    adaptation_rate = gene_history.r_out_mutate_; //rate of worker growth.

    alpha_army_temp = alpha_army; // temp, will be overridden as we meet our enemy and scout.
    alpha_econ_temp = alpha_econ;
    alpha_tech_temp = alpha_tech;

    win_rate = (1 - gene_history.loss_rate_);

    //get initial build order.
    buildorder.getInitialBuildOrder( gene_history.build_order_ );

    //update Map Grids
    inventory.updateUnit_Counts(friendly_inventory);
    inventory.updateBuildablePos();
    inventory.updateUnwalkable();
    inventory.updateSmoothPos();
    inventory.updateMapVeins();
    inventory.updateMapVeinsOutFromMain( Position(Broodwar->self()->getStartLocation()) );
    inventory.updateMapChokes();
    inventory.updateBaseLoc( land_inventory );
    inventory.getStartPositions();

    //update timers.
    short_delay = 0;
    med_delay = 0;
    long_delay = 0;
    my_reservation = Reservation();

}

void MeatAIModule::onEnd( bool isWinner )
{// Called when the game ends

    if (_MOVE_OUTPUT_BACK_TO_READ || _SSCAIT_OR_DOCKER) { // don't write to the read folder. But we want the full read contents ready for us to write in.
        rename(".\\bwapi-data\\read\\output.txt", ".\\bwapi-data\\write\\output.txt");  // Furthermore, rename will fail if there is already an existing file. 
    }

    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open( ".\\bwapi-data\\write\\output.txt", ios_base::app );
    string opponent_name = Broodwar->enemy()->getName().c_str();
    output << delta << "," << gamma << ',' << alpha_army << ',' << alpha_econ << ',' << alpha_tech << ',' << adaptation_rate << ',' << Broodwar->enemy()->getRace().c_str() << "," << isWinner << ',' << short_delay << ',' << med_delay << ',' << long_delay << ',' << opponent_name << ',' << Broodwar->mapFileName().c_str() << ',' << buildorder.initial_building_gene_ << endl;
    output.close();

    if (_MOVE_OUTPUT_BACK_TO_READ) {
        rename(".\\bwapi-data\\write\\output.txt", ".\\bwapi-data\\read\\output.txt"); // Furthermore, rename will fail if there is already an existing file. 
    }
}

void MeatAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if ( Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self() )
        return;

    // Performance Qeuery Timer
    // http://www.decompile.com/cpp/faq/windows_timer_api.htm
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
    bool has_built_this_frame = false;

    //Update enemy units
    enemy_inventory.updateUnitsControlledByOthers();
    enemy_inventory.purgeBrokenUnits();
    enemy_inventory.drawAllHitPoints(inventory);
    enemy_inventory.drawAllLocations(inventory);
    
    //Update neutral units
    neutral_inventory.updateUnitsControlledByOthers();
    neutral_inventory.purgeBrokenUnits();
    neutral_inventory.drawAllHitPoints(inventory);
    neutral_inventory.drawAllLocations(inventory);

    //Update friendly unit inventory.
    if ( friendly_inventory.unit_inventory_.size() == 0 ) {
        friendly_inventory = Unit_Inventory( Broodwar->self()->getUnits() ); // if you only do this you will lose track of all of your locked minerals. 
    }
    else {
        friendly_inventory.updateUnitInventory( Broodwar->self()->getUnits() ); // safe for locked minerals.
    }
    // Purge unwanted friendly inventory units. If I can't see it or it doesn't exist, it's broken and I should purge it.
    friendly_inventory.purgeBrokenUnits();
    friendly_inventory.purgeUnseenUnits();
    //friendly_inventory.drawAllVelocities(inventory);
    friendly_inventory.drawAllHitPoints(inventory);
    friendly_inventory.drawAllSpamGuards(inventory); 
    friendly_inventory.drawAllWorkerLocks(inventory, land_inventory);

    //Update posessed minerals. Erase those that are mined out.
    land_inventory.updateResourceInventory(friendly_inventory, enemy_inventory, inventory);
    land_inventory.drawMineralRemaining(inventory);

    if ((starting_enemy_race == Races::Random || starting_enemy_race == Races::Unknown) && Broodwar->enemy()->getRace() != starting_enemy_race) {
        //Initialize model variables. 
        GeneticHistory gene_history = GeneticHistory( ".\\bwapi-data\\read\\output.txt" );

        delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
        gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

        //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
        alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
        alpha_vis = gene_history.a_vis_out_mutate_; // vision starved parameter. Note the very large scale for vision, vision comes in groups of thousands. Since this is not scale free, the delta must be larger or else it will always be considered irrelevant. Currently defunct.
        alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
        alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
        adaptation_rate = gene_history.r_out_mutate_; //rate of worker growth.
        win_rate = (1 - gene_history.loss_rate_);
        Broodwar->sendText( "WHOA! %s is broken. That's a good random.", Broodwar->enemy()->getRace().c_str() );
    }

    //Update important variables.  Enemy stock has a lot of dependencies, updated above.
    inventory.updateUnit_Counts( friendly_inventory );
    inventory.updateLn_Army_Stock( friendly_inventory );
    inventory.updateLn_Tech_Stock( friendly_inventory );
    inventory.updateLn_Worker_Stock();
    inventory.updateVision_Count();

    inventory.updateLn_Supply_Remain();
    inventory.updateLn_Supply_Total();

    inventory.updateLn_Gas_Total();
    inventory.updateLn_Min_Total();

    inventory.updateGas_Workers();
    inventory.updateMin_Workers();

    inventory.updateMin_Possessed(land_inventory);
    inventory.updateHatcheries();  // macro variables, not every unit I have.
    inventory.updateWorkersClearing(friendly_inventory, land_inventory);
    inventory.my_portion_of_the_map_ = sqrt(pow(Broodwar->mapHeight() * 32, 2) + pow(Broodwar->mapWidth() * 32, 2)) / (double)Broodwar->getStartLocations().size();
    inventory.updateStartPositions(enemy_inventory);
    inventory.updateScreen_Position();

    
    if (t_game == 0) {
        //update local resources
        inventory.updateMapVeinsOutFromFoe(inventory.start_positions_[0]);
        Resource_Inventory mineral_inventory = Resource_Inventory(Broodwar->getStaticMinerals());
        Resource_Inventory geyser_inventory = Resource_Inventory(Broodwar->getStaticGeysers());
        land_inventory = mineral_inventory + geyser_inventory; // for first initialization.
        inventory.updateBaseLoc(land_inventory);
        inventory.getExpoPositions(); // prime this once on game start.
    }

    inventory.updateEnemyBasePosition(friendly_inventory, enemy_inventory, land_inventory, neutral_inventory);
    inventory.drawExpoPositions();
    inventory.drawBasePositions();

    land_inventory.updateGasCollectors();
    land_inventory.updateMiners();

    if ( buildorder.building_gene_.empty() ) {
        buildorder.ever_clear_ = true;
    }
    else {
        bool need_gas_now = false;
        if (buildorder.building_gene_.front().getResearch()) {
            if (buildorder.building_gene_.front().getResearch().gasPrice()) {
                buildorder.building_gene_.front().getResearch().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        } else if (buildorder.building_gene_.front().getUnit()) {
            if (buildorder.building_gene_.front().getUnit().gasPrice()) {
                buildorder.building_gene_.front().getUnit().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        }
        else if (buildorder.building_gene_.front().getUpgrade()) {
            if (buildorder.building_gene_.front().getUpgrade().gasPrice()) {
                buildorder.building_gene_.front().getUpgrade().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        }

        bool no_extractor = Count_Units(UnitTypes::Zerg_Extractor, inventory) == 0;
        if (need_gas_now && no_extractor) {
            buildorder.clearRemainingBuildOrder();
            Broodwar->sendText("Uh oh, something's went wrong with building an extractor!");
        }
    }

    my_reservation.decrementReserveTimer();
    my_reservation.confirmOngoingReservations( friendly_inventory );

    bool build_check_this_frame = false;
    bool upgrade_check_this_frame = false;
    Position mutating_creep_colony_position = Position{0,0}; // this is a simply practical check that saves a TON of resources.
    UnitType mutating_creep_colony_type = UnitTypes::Zerg_Creep_Colony;
    bool mutating_creep_this_frame = false;

    //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.

    //Knee-jerk states: gas, supply.
    gas_starved = (inventory.getLn_Gas_Ratio() < delta && Gas_Outlet()) ||
        (!buildorder.building_gene_.empty() && my_reservation.getExcessGas() > 0) ||// you need gas for a required build order item.
        (tech_starved && Tech_Avail() && Broodwar->self()->gas() < 200); // you need gas because you are tech starved.
    supply_starved = (inventory.getLn_Supply_Ratio()  < gamma  &&   //If your supply is disproportionately low, then you are supply starved, unless
        Broodwar->self()->supplyTotal() <= 400); // you have hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

    //Discontinuities -Cutoff if critically full, or suddenly progress towards one macro goal or another is impossible, or if their army is critically larger than ours.
    bool not_enough_miners = (inventory.min_workers_ <= inventory.min_fields_ * 2);
    bool not_enough_workers = Count_Units(UnitTypes::Zerg_Drone, inventory) < 85;
    bool econ_possible =  not_enough_miners && not_enough_workers; // econ is only a possible problem if undersaturated or less than 62 patches, and worker count less than 90.
    //bool vision_possible = true; // no vision cutoff ATM.
    bool army_possible = ((Broodwar->self()->supplyUsed() < 400 && exp( inventory.ln_army_stock_ ) / exp( inventory.ln_worker_stock_ ) < 5 * alpha_army_temp / alpha_econ_temp)) || 
        Count_Units( UnitTypes::Zerg_Spawning_Pool, inventory) - Count_Units_In_Progress(UnitTypes::Zerg_Spawning_Pool, inventory)
        + Count_Units( UnitTypes::Zerg_Hydralisk_Den, inventory) - Count_Units_In_Progress(UnitTypes::Zerg_Hydralisk_Den, inventory)
        + Count_Units( UnitTypes::Zerg_Spire, inventory) - Count_Units_In_Progress(UnitTypes::Zerg_Spire, inventory)
        + Count_Units( UnitTypes::Zerg_Ultralisk_Cavern, inventory) - Count_Units_In_Progress(UnitTypes::Zerg_Ultralisk_Cavern, inventory) <= 0; // can't be army starved if you are maxed out (or close to it), Or if you have a wild K/L ratio. Or if you can't build combat units at all.
    bool tech_possible = Tech_Avail(); // if you have no tech available, you cannot be tech starved.
                                       //Feed alpha values and cuttoff calculations into Cobb Douglas.

    CobbDouglas CD = CobbDouglas( alpha_army_temp, exp( inventory.ln_army_stock_ ), army_possible, alpha_tech_temp, exp( inventory.ln_tech_stock_ ), tech_possible, alpha_econ_temp, exp( inventory.ln_worker_stock_ ), econ_possible );

    if (_TIT_FOR_TAT_ENGAGED) {

        int dead_worker_count = dead_enemy_inventory.unit_inventory_.empty() ? 0 : dead_enemy_inventory.worker_count_;

        if (Broodwar->getFrameCount() == 0) {
            inventory.estimated_enemy_workers_ = 4;
        }
        else {
            //inventory.estimated_enemy_workers_ *= exp(rate_of_worker_growth); // exponential growth.
            inventory.estimated_enemy_workers_ += max(enemy_inventory.resource_depot_count_, 1) * 1 /(double)UnitTypes::Zerg_Drone.buildTime();
            inventory.estimated_enemy_workers_ = min(inventory.estimated_enemy_workers_, 85); // there exists a maximum reasonable number of workers.
        }
        //int approx_worker_count = exp( r * Broodwar->getFrameCount()) - dead_worker_count; //assumes continuous worker building since frame 1 and a 10 min max.

        int est_worker_count = min(max(enemy_inventory.worker_count_, inventory.estimated_enemy_workers_), 85);


        //Update existing CD functions to more closely mirror opponent. Do every 15 sec or so.
        if (Broodwar->elapsedTime() % 15 == 0 && enemy_inventory.stock_fighting_total_ > 0) {
            int worker_value = Stored_Unit(UnitTypes::Zerg_Drone).stock_value_;
            int e_worker_stock = est_worker_count * worker_value;
            CD.enemy_eval(enemy_inventory.stock_fighting_total_, army_possible, 1, tech_possible, e_worker_stock, econ_possible, adaptation_rate);
            alpha_army_temp = CD.alpha_army;
            alpha_econ_temp = CD.alpha_econ;
            alpha_tech_temp = CD.alpha_tech;
            //Broodwar->sendText("Matching expenditures,%4.2f, %4.2f,%4.2f", alpha_econ_temp, alpha_army_temp, alpha_tech_temp);
        }
        else if ( Broodwar->elapsedTime() % 15 == 0 && enemy_inventory.stock_fighting_total_ == 0 ) {
            alpha_army_temp = alpha_army;
            alpha_econ_temp = alpha_econ;
            alpha_tech_temp = alpha_tech;
            //Broodwar->sendText("Reseting expenditures,%4.2f, %4.2f,%4.2f", alpha_econ_temp, alpha_army_temp, alpha_tech_temp);
        }
    }

    tech_starved = CD.tech_starved();
    army_starved = CD.army_starved();
    econ_starved = CD.econ_starved();

    double econ_derivative = CD.econ_derivative;
    double army_derivative = CD.army_derivative;
    double tech_derivative = CD.tech_derivative;

    if (_ANALYSIS_MODE && Broodwar->getFrameCount() % 24 == 0) {
        CD.printModelParameters();
    }

    bool massive_army = (army_derivative != 0 && friendly_inventory.stock_fighting_total_ - Stock_Units(UnitTypes::Zerg_Sunken_Colony, friendly_inventory) - Stock_Units(UnitTypes::Zerg_Spore_Colony, friendly_inventory) >= enemy_inventory.stock_fighting_total_ * 3);

    //Unitset enemy_set = getEnemy_Set(enemy_inventory);
    enemy_inventory.updateUnitInventorySummary();
    friendly_inventory.updateUnitInventorySummary();
    land_inventory.updateMiners();
    land_inventory.updateGasCollectors();

    inventory.est_enemy_stock_ = (int)enemy_inventory.stock_fighting_total_ ; // just a raw count of their stuff.

    // Display the game status indicators at the top of the screen	
    if ( _ANALYSIS_MODE ) {

        //Print_Unit_Inventory( 0, 50, friendly_inventory );
        Print_Universal_Inventory(0, 50, inventory);
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
        Broodwar->drawTextScreen( 0, 20, "Workers (alt): (m%d, g%d)", land_inventory.total_miners_, land_inventory.total_gas_ );  //

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
            Broodwar->drawTextScreen( 250, 30, "Enemy R: %.2g ", adaptation_rate); // 
            Broodwar->drawTextScreen( 250, 40, "Alpha_Econ: %4.2f %%", CD.alpha_econ * 100 );  // As %s
            Broodwar->drawTextScreen( 250, 50, "Alpha_Army: %4.2f %%", CD.alpha_army * 100 ); //
            Broodwar->drawTextScreen( 250, 60, "Alpha_Tech: %4.2f ", CD.alpha_tech * 100 ); // No longer a % with capital-augmenting technology.
            Broodwar->drawTextScreen( 250, 70, "Enemy Worker Est: %d ", inventory.estimated_enemy_workers_ ); // No longer a % with capital-augmenting technology.

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

        if (_ANALYSIS_MODE) {
            for ( auto p = land_inventory.resource_inventory_.begin(); p != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); ++p ) {
                if ( isOnScreen( p->second.pos_, inventory.screen_position_) ) {
                    Broodwar->drawCircleMap( p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan ); // Plot their last known position.
                    Broodwar->drawTextMap( p->second.pos_, "%d", p->second.current_stock_value_ ); // Plot their current value.
                    Broodwar->drawTextMap( p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_ ); // Plot their current value.
                }
            }


            //for ( vector<int>::size_type i = 0; i < inventory.map_veins_.size(); ++i ) {
            //    for ( vector<int>::size_type j = 0; j < inventory.map_veins_[i].size(); ++j ) {
            //        if ( inventory.map_veins_[i][j] > 175 ) {
            //            if (isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_) ) {
            //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
            //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
            //            }
            //        }
            //        else if (inventory.map_veins_[i][j] < 20 && inventory.map_veins_[i][j] > 1 ) { // should only highlight smoothed-out barriers.
            //            if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
            //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
            //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Purple);
            //            }
            //        }
            //        else if ( inventory.map_veins_[i][j] == 1 ) { // should only highlight smoothed-out barriers.
            //            if (isOnScreen( { (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_) ) {
            //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
            //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
            //            }
            //        }
            //    }
            //} // Pretty to look at!


            for (vector<int>::size_type i = 0; i < inventory.map_veins_out_from_main_.size(); ++i) {
                for (vector<int>::size_type j = 0; j < inventory.map_veins_out_from_main_[i].size(); ++j) {
                    if (inventory.map_veins_out_from_main_[i][j] % 100 == 0 && inventory.map_veins_out_from_main_[i][j] > 1 ) { 
                        if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
                            Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_out_from_main_[i][j] );
                            //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
                        }
                    }
                }
            } // Pretty to look at!

            //for (vector<int>::size_type i = 0; i < inventory.map_veins_out_from_enemy_.size(); ++i) {
            //    for (vector<int>::size_type j = 0; j < inventory.map_veins_out_from_enemy_[i].size(); ++j) {
            //        if (inventory.map_veins_out_from_enemy_[i][j] % 100 == 0 && inventory.map_veins_out_from_enemy_[i][j] > 1) {
            //            if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
            //                Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_out_from_enemy_[i][j]);
            //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
            //            }
            //        }
            //    }
            //} // Pretty to look at!

            //for (vector<int>::size_type i = 0; i < inventory.smoothed_barriers_.size(); ++i) {
            //    for (vector<int>::size_type j = 0; j < inventory.smoothed_barriers_[i].size(); ++j) {
            //        if ( inventory.smoothed_barriers_[i][j] > 0) {
            //            if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
            //                //Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j]);
            //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
            //            }
            //        }
            //    }
            //} // Pretty to look at!

            //for ( auto &u : Broodwar->self()->getUnits() ) {
            //    if ( u->getLastCommand().getType() != UnitCommandTypes::Attack_Move && u_type != UnitTypes::Zerg_Extractor && u->getLastCommand().getType() != UnitCommandTypes::Attack_Unit ) {
            //        Broodwar->drawTextMap( u->getPosition(), u->getLastCommand().getType().c_str() );
            //    }
            //}
        }

    }// close analysis mode



    // Prevent spamming by only running our onFrame once every number of latency frames.
    // Latency frames are the number of frames before commands are processed.
    auto end_preamble = std::chrono::high_resolution_clock::now();
    preamble_time = end_preamble - start_preamble;

    if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0) {
        return;
    }

    // Iterate through all the units that we own
    for (auto &u : Broodwar->self()->getUnits())
    {
        // Ignore the unit if it no longer exists
        // Make sure to include this block when handling any Unit pointer!
        if (!u || !u->exists())
            continue;
        // Ignore the unit if it has one of the following status ailments
        if (u->isLockedDown() ||
            u->isMaelstrommed() ||
            u->isStasised())
            continue;
        // Ignore the unit if it is in one of the following states
        if (u->isLoaded() ||
            !u->isPowered() /*|| u->isStuck()*/)
            continue;
        // Ignore the unit if it is incomplete or busy constructing
        if (!u->isCompleted() ||
            u->isConstructing())
            continue;

        if (!spamGuard(u)) {
            continue;
        }
        UnitType u_type = u->getType();

        // Finally make the unit do some stuff!
        // Unit creation & Hatchery management loop
        auto start_larva = std::chrono::high_resolution_clock::now();
        if ( !has_built_this_frame && u_type == UnitTypes::Zerg_Larva || (u_type == UnitTypes::Zerg_Hydralisk && !u->isUnderAttack()) ) // A resource depot is a Command Center, Nexus, or Hatchery.
        {
            // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition.
            has_built_this_frame = Reactive_Build(u, inventory, friendly_inventory, enemy_inventory);
        }
        auto end_larva = std::chrono::high_resolution_clock::now();

        //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
        auto start_combat = std::chrono::high_resolution_clock::now();
        if (((u_type != UnitTypes::Zerg_Larva && u_type.canAttack()) || u_type == UnitTypes::Zerg_Overlord))
        {
            Stored_Unit* e_closest = getClosestThreatOrTargetStored(enemy_inventory, u, 999999);
            if (u_type == UnitTypes::Zerg_Drone || u_type == UnitTypes::Zerg_Overlord) {
                e_closest = getClosestThreatOrTargetStored(enemy_inventory, u, 256);
            }

            if (e_closest) { // if there are bad guys, search for friends within that area. 
                //e_closest->pos_.x += e_closest->bwapi_unit_->getVelocityX();
                //e_closest->pos_.y += e_closest->bwapi_unit_->getVelocityY();  //short run position forecast.

                int distance_to_foe = e_closest->pos_.getDistance(u->getPosition());
                int chargable_distance_net = MeatAIModule::getChargableDistance(u, enemy_inventory) + MeatAIModule::getChargableDistance(e_closest->bwapi_unit_, friendly_inventory); // how far can you get before he shoots?
                int search_radius = max(max(chargable_distance_net + 64, enemy_inventory.max_range_ + 64), 128);
                //Broodwar->sendText("%s, range:%d, spd:%d,max_cd:%d, charge:%d", u_type.c_str(), MeatAIModule::getProperRange(u), (int)MeatAIModule::getProperSpeed(u), enemy_inventory.max_cooldown_, chargable_distance_net);
                Boids boids;

                Unit_Inventory enemy_loc_around_target = getUnitInventoryInRadius(enemy_inventory, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory enemy_loc_around_self = getUnitInventoryInRadius(enemy_inventory, u->getPosition(), distance_to_foe + search_radius);
                //Unit_Inventory enemy_loc_out_of_reach = getUnitsOutOfReach(enemy_inventory, u);
                Unit_Inventory enemy_loc = (enemy_loc_around_target + enemy_loc_around_self);

                Unit_Inventory friend_loc_around_target = getUnitInventoryInRadius(friendly_inventory, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory friend_loc_around_me = getUnitInventoryInRadius(friendly_inventory, u->getPosition(), distance_to_foe + search_radius);
                //Unit_Inventory friend_loc_out_of_reach = getUnitsOutOfReach(friendly_inventory, u);
                Unit_Inventory friend_loc = (friend_loc_around_target + friend_loc_around_me);

                //Unit_Inventory friend_loc = getUnitInventoryInRadius(friendly_inventory, e_closest->pos_, distance_to_foe + search_radius);

                //enemy_loc.updateUnitInventorySummary();
                //friend_loc.updateUnitInventorySummary(); /// need to update if we do not + the two inventories.

                //int e_count = enemy_loc.unit_inventory_.size();

                //int helpless_e = u->isFlying() ? enemy_loc.stock_fighting_total_ - enemy_loc.stock_shoots_up_ : enemy_loc.stock_fighting_total_ - enemy_loc.stock_shoots_down_;
                //int helpful_e = u->isFlying() ? enemy_loc.stock_shoots_up_ : enemy_loc.stock_shoots_down_; // both forget value of psi units.
                //int helpful_e = friend_loc.stock_fliers_ / (double)(friend_loc.stock_fighting_total_ + 1) * enemy_loc.stock_shoots_up_ + friend_loc.stock_ground_units_ / (double)(friend_loc.stock_fighting_total_ + 1)* enemy_loc.stock_shoots_down_; // permits a noncombat enemy to be seen as worthless, 0 stock.
                //int helpful_u = enemy_loc.stock_fliers_ / (double)(enemy_loc.stock_fighting_total_ + 1) * friend_loc.stock_shoots_up_ + enemy_loc.stock_ground_units_ / (double)(enemy_loc.stock_fighting_total_ + 1)  * friend_loc.stock_shoots_down_;

                vector<int> useful_stocks = MeatAIModule::getUsefulStocks(friend_loc, enemy_loc);
                int helpful_u = useful_stocks[0];
                int helpful_e = useful_stocks[1]; // both forget value of psi units.
                int targetable_stocks = getTargetableStocks(u, enemy_loc);
                int threatening_stocks = getThreateningStocks(u, enemy_loc);

                if (e_closest->valid_pos_) {  // Must have a valid postion on record to attack.

                    //double minimum_enemy_surface = 2 * 3.1416 * sqrt( (double)enemy_loc.volume_ / 3.1414 );
                    //double minimum_friendly_surface = 2 * 3.1416 * sqrt( (double)friend_loc.volume_ / 3.1414 );
                    //double unusable_surface_area_f = max( (minimum_friendly_surface - minimum_enemy_surface) / minimum_friendly_surface, 0.0 );
                    //double unusable_surface_area_e = max( (minimum_enemy_surface - minimum_friendly_surface) / minimum_enemy_surface, 0.0 );
                    //double portion_blocked = min(pow(minimum_occupied_radius / search_radius, 2), 1.0); // the volume ratio (equation reduced by cancelation of 2*pi )

                    bool neccessary_attack =
                        (targetable_stocks > 0 || threatening_stocks == 0) && (
                            helpful_e <= helpful_u * 0.95 || // attack if you outclass them and your boys are ready to fight. Equality for odd moments of matching 0,0 helpful forces. 
                            massive_army || army_derivative == 0 ||
                            inventory.home_base_.getDistance(e_closest->pos_) < search_radius || // Force fight at home base.
                            //inventory.est_enemy_stock_ < 0.75 * exp( inventory.ln_army_stock_ ) || // attack you have a global advantage (very very rare, global army strength is vastly overestimated for them).
                                                                                                   //!army_starved || // fight your army is appropriately sized.
                            (friend_loc.worker_count_ > 0 && u_type != UnitTypes::Zerg_Drone) || //Don't run if drones are present.
                            (Count_Units(UnitTypes::Zerg_Sunken_Colony, friend_loc) > 0 && enemy_loc.stock_ground_units_ > 0) || // Don't run if static d is present.
                            //(!IsFightingUnit(e_closest->bwapi_unit_) && 64 > enemy_loc.max_range_) || // Don't run from noncombat junk.
                            threatening_stocks == 0 ||
                            (u_type == UnitTypes::Zerg_Scourge && distance_to_foe < enemy_loc.max_range_ + 2 * chargable_distance_net) || // the only sucide unit should not be prevented from suiciding.
                        //( 32 > enemy_loc.max_range_ && friend_loc.max_range_ > 32 && helpful_e * (1 - unusable_surface_area_e) < 0.75 * helpful_u)  || Note: a hydra and a ling have the same surface area. But 1 hydra can be touched by 9 or so lings.  So this needs to be reconsidered.
                        (distance_to_foe < 32 && getProperRange(u) < 32) );// don't run if they're in range and you're done for. Melee is <32, not 0. Hugely benifits against terran, hurts terribly against zerg. Lurkers vs tanks?; Just added this., hugely impactful. Not inherently in a good way, either. 
                        //  bool retreat = u->canMove() && ( // one of the following conditions are true:
                        //(u_type.isFlyer() && enemy_loc.stock_shoots_up_ > 0.25 * friend_loc.stock_fliers_) || //  Run if fliers face more than token resistance.


                    bool force_retreat = 
                        ( targetable_stocks == 0 && threatening_stocks > 0 ) ||
                        (u_type == UnitTypes::Zerg_Overlord && threatening_stocks > 0 ) ||
                        (u_type.isFlyer() && u_type != UnitTypes::Zerg_Scourge && ((u->isUnderAttack() && u->getHitPoints() < 0.5 * u->getInitialHitPoints()) || enemy_loc.stock_shoots_up_ > friend_loc.stock_fliers_ )) || // run if you are flying (like a muta) and cannot be practical.
                        (e_closest->bwapi_unit_ && !e_closest->bwapi_unit_->isDetected()) ||  // Run if they are cloaked. Must be visible to know if they are cloaked. Might cause problems with bwapiunits.
                        //helpful_u < helpful_e * 0.50 || // Run if they have local advantage on you
                        ( !getUnitInventoryInRadius(friend_loc, UnitTypes::Zerg_Sunken_Colony, u->getPosition(), 7 * 32 + search_radius).unit_inventory_.empty() && getUnitInventoryInRadius(friend_loc, UnitTypes::Zerg_Sunken_Colony, e_closest->pos_, 7 * 32).unit_inventory_.empty() && enemy_loc.max_range_ < 7 * 32 ) ||
                        //(friend_loc.max_range_ >= enemy_loc.max_range_ && friend_loc.max_range_> 32 && getUnitInventoryInRadius(friend_loc, e_closest->pos_, friend_loc.max_range_ - 32).max_range_ && getUnitInventoryInRadius(friend_loc, e_closest->pos_, friend_loc.max_range_ - 32).max_range_ < friend_loc.max_range_ ) || // retreat if sunken is nearby but not in range.
                        //(friend_loc.max_range_ < enemy_loc.max_range_ || 32 > friend_loc.max_range_ ) && (1 - unusable_surface_area_f) * 0.75 * helpful_u < helpful_e || // trying to do something with these surface areas.
                        (u_type == UnitTypes::Zerg_Overlord && (u->isUnderAttack() || (supply_starved && enemy_loc.stock_shoots_up_ > 0))) || //overlords should be cowardly not suicidal.
                        (u_type == UnitTypes::Zerg_Drone && (!army_starved || u->getHitPoints() < 0.50 *  u_type.maxHitPoints()) ); // Run if drone and (we have forces elsewhere or the drone is injured).  Drones don't have shields.
                        //(helpful_u == 0 && helpful_e > 0); // run if this is pointless. Should not happen because of search for attackable units? Should be redudnent in necessary_attack line one.

                    bool drone_problem = u_type == UnitTypes::Zerg_Drone && enemy_loc.worker_count_ > 0;

                    bool is_spelled = u->isUnderStorm() || u->isUnderDisruptionWeb() || u->isUnderDarkSwarm() || u->isIrradiated(); // Run if spelled.

                    bool cooldown = u->getGroundWeaponCooldown() > 0 || u->getAirWeaponCooldown() > 0;
                    bool kite = cooldown && distance_to_foe < 64 && getProperRange(u) > 64 && getProperRange(e_closest->bwapi_unit_) < 64 && !u->isBurrowed() && Can_Fight(*e_closest, u); //kiting?- /*&& getProperSpeed(e_closest->bwapi_unit_) <= getProperSpeed(u)*/

                    if (neccessary_attack && !force_retreat && !is_spelled && !drone_problem && !kite) {

                        if (u_type.isWorker()) {
                            friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                        }

                        boids.Tactical_Logic(u, enemy_loc, friend_loc, inventory, Colors::Orange); // move towards enemy untill tactical logic takes hold at about 150 range.


                        if (_ANALYSIS_MODE) {
                            if (isOnScreen(u->getPosition(), inventory.screen_position_)) {
                                Broodwar->drawTextMap(u->getPosition().x, u->getPosition().y, "%d", helpful_u);
                            }
                            Position mean_loc = enemy_loc.getMeanLocation();
                            if (isOnScreen(mean_loc, inventory.screen_position_)) {
                                Broodwar->drawTextMap(mean_loc.x, mean_loc.y, "%d", helpful_e);
                            }
                        }

                    }
                    else if (is_spelled) {
                        if (u_type.isWorker()) {
                            friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                        }
                        Stored_Unit* closest = getClosestThreatOrTargetStored(friendly_inventory, u, 128);
                        if (closest) {
                            boids.Retreat_Logic(u, *closest, enemy_inventory, friendly_inventory, inventory, Colors::Blue); // this is not actually getting out of storm. It is simply scattering.
                        }

                    }
                    else if (drone_problem) {
                        if (Count_Units_Doing(UnitTypes::Zerg_Drone, UnitCommandTypes::Attack_Unit, friend_loc) <= enemy_loc.worker_count_ &&
                            friend_loc.getMeanBuildingLocation() != Position(0, 0) &&
                            u->getLastCommand().getType() != UnitCommandTypes::Morph &&
                            u->getHitPoints() / (double)(u_type.maxHitPoints() + u_type.maxShields()) >= e_closest->current_hp_ / (double)(e_closest->type_.maxHitPoints() + e_closest->type_.maxShields()) &&
                            u->getHitPoints() > (double)(u_type.maxHitPoints() + u_type.maxShields()) * 0.25) {
                            friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                            boids.Tactical_Logic(u, enemy_loc, friend_loc, inventory, Colors::Orange); // move towards enemy untill tactical logic takes hold at about 150 range.
                        }
                    }
                    else {

                        if (u_type.isWorker()) {
                            friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                        }

                        boids.Retreat_Logic(u, *e_closest, enemy_inventory, friendly_inventory, inventory, Colors::White);

                        if (!buildorder.ever_clear_ && ((!e_closest->type_.isWorker() && e_closest->type_.canAttack()) || enemy_loc.worker_count_ > 2) && (!u_type.canAttack() || u_type == UnitTypes::Zerg_Drone || friend_loc.getMeanBuildingLocation() != Position(0, 0))) {
                            if (u_type == UnitTypes::Zerg_Overlord) {
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
        auto end_combat = std::chrono::high_resolution_clock::now();
    

        //Scouting/vision loop. Intially just brownian motion, now a fully implemented boids-type algorithm.
        auto start_scout = std::chrono::high_resolution_clock::now();

        bool acceptable_ovi_scout = u_type != UnitTypes::Zerg_Overlord ||
            (u_type == UnitTypes::Zerg_Overlord && enemy_inventory.stock_shoots_up_ == 0 && enemy_inventory.cloaker_count_ == 0 && Broodwar->enemy()->getRace() != Races::Terran) || 
            (u_type == UnitTypes::Zerg_Overlord && massive_army && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Pneumatized_Carapace) > 0);

        if ( spamGuard(u) /*&& acceptable_ovi_scout*/ && u_type != UnitTypes::Zerg_Drone && u_type != UnitTypes::Zerg_Larva && !u_type.isBuilding() ) { //Scout if you're not a drone or larva and can move. Spamguard here prevents double ordering of combat units.
            Boids boids;
            bool potential_fears = !massive_army; // just a name change.
            boids.Boids_Movement(u, friendly_inventory, enemy_inventory, inventory, army_starved, potential_fears);
        } // If it is a combat unit, then use it to attack the enemy.
        auto end_scout = std::chrono::high_resolution_clock::now();

        // Detectors are called for cloaked units. Only if you're not supply starved, because we only have overlords for detectors.
        auto start_detector = std::chrono::high_resolution_clock::now();
        Position c; // holder for cloaked unit position.
        bool call_detector = false;
        if ( !supply_starved && u_type != UnitTypes::Zerg_Overlord && checkOccupiedArea(enemy_inventory, u->getPosition(), u_type.sightRange()) ) {
            Unit_Inventory e_neighbors = getUnitInventoryInRadius(enemy_inventory, u->getPosition(), u_type.sightRange());
            for (auto e = e_neighbors.unit_inventory_.begin(); e != e_neighbors.unit_inventory_.end() && !e_neighbors.unit_inventory_.empty(); e++) {
                if ((*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || (*e).second.type_.isBurrowable()) {
                    c = (*e).second.pos_; // then we may to send in some vision.
                    //Unit_Inventory friend_loc = getUnitInventoryInRadius(friendly_inventory, c, e->second.type_.sightRange()); // we check this cloaker has any friendly units nearby.
                    //if (!friend_loc.unit_inventory_.empty() && friend_loc.detector_count_ == 0) {
                     call_detector = true;
                     break;
                } //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
            }
            if (call_detector) {
                int dist = 999999;
                int dist_temp = 0;
                bool detector_found = false;
                Unit detector_of_choice;
                for (auto d : friendly_inventory.unit_inventory_) {
                    if (d.second.type_ == UnitTypes::Zerg_Overlord &&
                        d.second.bwapi_unit_ &&
                        !d.second.bwapi_unit_->isUnderAttack() &&
                        d.second.current_hp_ > 0.25 * d.second.type_.maxHitPoints() ) { // overlords don't have shields.
                        dist_temp = d.second.bwapi_unit_->getDistance(c);
                        if (dist_temp < dist) {
                            dist = dist_temp;
                            detector_of_choice = d.second.bwapi_unit_;
                            detector_found = true;
                        }
                    }
                }
                if (detector_found /*&& spamGuard(detector_of_choice)*/ ) {
                    Position detector_pos = detector_of_choice->getPosition();
                    double theta = atan2(c.y - detector_pos.y, c.x - detector_pos.x);
                    Position closest_loc_to_c_that_gives_vision = Position(c.x + cos(theta) * SightRange(detector_of_choice) * 0.75, c.y + sin(theta) * SightRange(detector_of_choice)) * 0.75;
                    if (closest_loc_to_c_that_gives_vision.isValid() && closest_loc_to_c_that_gives_vision != Position(0, 0)) {
                        detector_of_choice->move(closest_loc_to_c_that_gives_vision);
                        if (_ANALYSIS_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice->getPosition(), closest_loc_to_c_that_gives_vision, inventory.screen_position_, Colors::Cyan);
                        }
                    }
                    else {
                        detector_of_choice->move(c);
                        if (_ANALYSIS_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice->getPosition(), inventory.screen_position_, c, Colors::Cyan);
                        }
                    }

                }
            }
        }
        auto end_detector = std::chrono::high_resolution_clock::now();
        detector_time += end_detector - start_detector;

        // Worker Loop - moved after combat to prevent mining from overriding worker defense..
        auto start_worker = std::chrono::high_resolution_clock::now();
        if (u_type.isWorker()) {

            bool want_gas = gas_starved && inventory.gas_workers_ < 3 * (Count_Units(UnitTypes::Zerg_Extractor, inventory) - Count_Units_In_Progress(UnitTypes::Zerg_Extractor, inventory));  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  Does not count worker IN extractor.
            bool too_much_gas = 1 - inventory.getLn_Gas_Ratio() > delta;

            if (Broodwar->getFrameCount() == 0) {
                u->stop();
                continue; // fixes the fact that drones auto-lock to something on game start. Now we don't triple-stack part of our initial drones.
            }

            Stored_Unit& miner = friendly_inventory.unit_inventory_.find(u)->second;
            if ( !isRecentCombatant(miner.bwapi_unit_) && !miner.isAssignedClearing(land_inventory) && !miner.isAssignedBuilding() && spamGuard(miner.bwapi_unit_) ) { //Do not disturb fighting workers or workers assigned to clear a position. Do not spam. Allow them to remain locked on their task. 
                if (!IsCarryingGas(u) && !IsCarryingMinerals(u) && miner.isAssignedMining(land_inventory) && !miner.isAssignedBuilding() && my_reservation.last_builder_sent_ < t_game - Broodwar->getLatencyFrames() -  15 * 24 && !build_check_this_frame) { //only get those that are in line or gathering minerals, but not carrying them. This always irked me.
                    build_check_this_frame = true;
                    inventory.getExpoPositions();
                    //friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                    Building_Begin(u, inventory, enemy_inventory, friendly_inventory);
                    if ( miner.isAssignedBuilding() ) { //Don't purge the building relations here - we just established them!
                        continue;
                    }
                } // Close Build loop

               //Workers need to clear empty patches.
                bool time_to_start_clearing_a_path = inventory.hatches_ >= 2 && Nearby_Blocking_Minerals(u, friendly_inventory);
                if (time_to_start_clearing_a_path && inventory.workers_clearing_ == 0 && !IsCarryingMinerals(u) && !IsCarryingGas(u)) {
                    friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                    Worker_Clear(u, friendly_inventory);
                    if (miner.isAssignedClearing(land_inventory)) {
                        inventory.updateWorkersClearing(friendly_inventory, land_inventory);
                        continue;
                    }
                } // clear those empty mineral patches that block paths.

                  // Lock all loose workers down. Maintain gas/mineral balance. 
                  //bool gas_flooded = Broodwar->self()->gas() * delta > Broodwar->self()->minerals(); // Consider you might have too much gas.
                if ( !miner.isAssignedMining(land_inventory) || ((want_gas || too_much_gas) && inventory.last_gas_check_ < t_game - 5 * 24) && spamGuard(miner.bwapi_unit_, 94)) { //if this is your first worker of the frame consider resetting him.
                    friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation);
                    inventory.last_gas_check_ = t_game;
                    if (want_gas) {
                        Worker_Gather(u, UnitTypes::Zerg_Extractor, friendly_inventory);
                        if (miner.isAssignedMining(land_inventory)) {
                            continue;
                        }
                        else { // do SOMETHING.
                            Worker_Gather(u, UnitTypes::Resource_Mineral_Field, friendly_inventory);
                            if (miner.isAssignedMining(land_inventory)) {
                                continue;
                            }
                        }
                    }
                    else if (!want_gas || too_much_gas) {
                        Worker_Gather(u, UnitTypes::Resource_Mineral_Field, friendly_inventory);
                        if (miner.isAssignedMining(land_inventory)) {
                            continue;
                        }
                        else { // do SOMETHING.
                            Worker_Gather(u, UnitTypes::Zerg_Extractor, friendly_inventory);
                            if (miner.isAssignedMining(land_inventory)) {
                               continue;
                            }
                        }
                    }
                }
            }

            // let's leave units in full-mine alone.
            if ( (miner.bwapi_unit_->isCarryingMinerals() || miner.bwapi_unit_->isCarryingGas()) && miner.isAssignedMining(land_inventory) /*|| miner.bwapi_unit_->getOrderTarget() == NULL*/) {
                continue;
            }

            // Maintain the locks.
            if ( (miner.isAssignedClearing(land_inventory) || miner.isAssignedMining(land_inventory)) && miner.isBrokenLock() ) { //spamguard amount found by trial and error.
                if (!miner.bwapi_unit_->gather(miner.locked_mine_) ) { // reassign him back to work.
                    friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                }
                continue;
            } else if ((miner.isAssignedClearing(land_inventory) || miner.isAssignedMining(land_inventory)) && miner.isLongRangeLock() ) { //spamguard amount found by trial and error.
                if (!miner.bwapi_unit_->move(miner.getMine(land_inventory)->pos_)) { // reassign him back to work.
                    friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                }
                continue;
            } else if ((miner.isAssignedClearing(land_inventory) || miner.isAssignedMining(land_inventory)) && miner.isMovingLock()) { //spamguard amount found by trial and error.
                if (!miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                    friendly_inventory.purgeWorkerRelations(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                }
                continue;
            }


        } // Close Worker management loop
        auto end_worker = std::chrono::high_resolution_clock::now();

        //Upgrade loop:
        auto start_upgrade = std::chrono::high_resolution_clock::now();
        if ( isIdleEmpty( u ) && !u->canAttack() && u_type != UnitTypes::Zerg_Larva && !upgrade_check_this_frame && // no trying to morph hydras anymore.
            (u->canUpgrade() || u->canResearch() || u->canMorph()) ) { // this will need to be revaluated once I buy units that cost gas.

             upgrade_check_this_frame = Tech_Begin( u, friendly_inventory , inventory);

            //PrintError_Unit( u );
        }
        auto end_upgrade = std::chrono::high_resolution_clock::now();

        //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
        auto start_creepcolony = std::chrono::high_resolution_clock::now();

        if ( u_type == UnitTypes::Zerg_Creep_Colony ) {
            if (u->getDistance(mutating_creep_colony_position) < UnitTypes::Zerg_Sunken_Colony.sightRange() && mutating_creep_colony_type == UnitTypes::Zerg_Sunken_Colony ) {
                Check_N_Build(UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true);
                mutating_creep_colony_position = u->getPosition();
                mutating_creep_colony_type = UnitTypes::Zerg_Sunken_Colony;
                mutating_creep_this_frame = true;
            }
            else if (u->getDistance(mutating_creep_colony_position) < UnitTypes::Zerg_Spore_Colony.sightRange() && mutating_creep_colony_type == UnitTypes::Zerg_Spore_Colony) {
                Check_N_Build(UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true);
                mutating_creep_colony_position = u->getPosition();
                mutating_creep_colony_type = UnitTypes::Zerg_Spore_Colony;
                mutating_creep_this_frame = true;
            }
            else if (!mutating_creep_this_frame){
                Unit_Inventory local_e = getUnitInventoryInRadius(enemy_inventory, u->getPosition(), inventory.my_portion_of_the_map_);
                local_e.updateUnitInventorySummary();
                bool can_sunken = Count_Units(UnitTypes::Zerg_Spawning_Pool, friendly_inventory) > 0;
                bool can_spore = Count_Units(UnitTypes::Zerg_Evolution_Chamber, friendly_inventory) > 0;
                bool need_static_d = buildorder.checkBuilding_Desired(UnitTypes::Zerg_Spore_Colony) || buildorder.checkBuilding_Desired(UnitTypes::Zerg_Sunken_Colony);
                bool want_static_d = (army_starved || local_e.stock_fighting_total_ > 0) && (can_sunken || can_spore);
                bool u_relatively_weak_against_air = checkWeakAgainstAir(friendly_inventory, enemy_inventory) && local_e.stock_fliers_ > 0; // div by zero concern. Derivative of the above equation 
                if (need_static_d || want_static_d) {
                    //Unit_Inventory incoming_e_threat = getUnitInventoryInRadius( enemy_inventory, u->getPosition(), ( sqrt( pow( map_x , 2 ) + pow( map_y , 2 ) ) * 32 ) / Broodwar->getStartLocations().size() ); 
                    bool cloak_nearby = local_e.cloaker_count_ > 0;
                    bool local_air_problem = local_e.stock_fliers_ > 0;
                    bool global_air_problem = u_relatively_weak_against_air;
                    buildorder.checkBuilding_Desired(UnitTypes::Zerg_Sunken_Colony);
                    buildorder.checkBuilding_Desired(UnitTypes::Zerg_Spore_Colony);
                    if (can_sunken && can_spore) {
                        if (local_air_problem || global_air_problem || cloak_nearby) { // if they have a flyer (that can attack), get spores.
                            Check_N_Build(UnitTypes::Zerg_Spore_Colony, u, friendly_inventory, true);
                            mutating_creep_colony_position = u->getPosition();
                            mutating_creep_colony_type = UnitTypes::Zerg_Spore_Colony;
                            mutating_creep_this_frame = true;
                        }
                        else {
                            Check_N_Build(UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true);
                            mutating_creep_colony_position = u->getPosition();
                            mutating_creep_colony_type = UnitTypes::Zerg_Sunken_Colony;
                            mutating_creep_this_frame = true;
                        }
                    } // build one of the two colonies based on the presence of closest units.
                    else if (can_sunken && !can_spore && !local_air_problem && !global_air_problem && !cloak_nearby) {
                        Check_N_Build(UnitTypes::Zerg_Sunken_Colony, u, friendly_inventory, true);
                        mutating_creep_colony_position = u->getPosition();
                        mutating_creep_colony_type = UnitTypes::Zerg_Sunken_Colony;
                        mutating_creep_this_frame = true;

                    } // build sunkens if you only have that
                    else if (can_spore && !can_sunken) {
                        mutating_creep_colony_position = u->getPosition();
                        mutating_creep_colony_type = UnitTypes::Zerg_Spore_Colony;
                        mutating_creep_this_frame = true;
                    } // build spores if you only have that.
                } // closure: Creep colony loop
            }
        }

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
    if ( (short_delay > 320 || Broodwar->elapsedTime() > 90 * 60 || Count_Units(UnitTypes::Zerg_Drone, friendly_inventory) == 0 ) /* enemy_inventory.stock_fighting_total_> friendly_inventory.stock_fighting_total_ * 2*/ && _RESIGN_MODE ) //if game times out or lags out, end game with resignation.
    {
        Broodwar->leaveGame();
    }
    if ( _ANALYSIS_MODE ) {
        int n;
            n = sprintf(delay_string,           "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count());
            n = sprintf(preamble_string,        "Preamble:      %3.f%%,%3.fms ", preamble_time.count() / (double)total_frame_time.count() * 100, preamble_time.count());
            n = sprintf(larva_string,           "Larva:         %3.f%%,%3.fms", larva_time.count() / (double)total_frame_time.count() * 100, larva_time.count());
            n = sprintf(worker_string,          "Workers:       %3.f%%,%3.fms", worker_time.count() / (double)total_frame_time.count() * 100, worker_time.count());
            n = sprintf(scouting_string,        "Scouting:      %3.f%%,%3.fms", scout_time.count() / (double)total_frame_time.count() * 100, scout_time.count());
            n = sprintf(combat_string,          "Combat:        %3.f%%,%3.fms", combat_time.count() / (double)total_frame_time.count() * 100, combat_time.count());
            n = sprintf(detection_string,       "Detection:     %3.f%%,%3.fms", detector_time.count() / (double)total_frame_time.count() * 100, detector_time.count());
            n = sprintf(upgrade_string,         "Upgrades:      %3.f%%,%3.fms", upgrade_time.count() / (double)total_frame_time.count() * 100, upgrade_time.count());
            n = sprintf(creep_colony_string,    "CreepColonies: %3.f%%,%3.fms", creepcolony_time.count() / (double)total_frame_time.count() * 100, creepcolony_time.count());
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
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if ( unit->getPlayer()->isEnemy( Broodwar->self() ) && !unit->isInvincible() ) { // safety check.
                                                                                             //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit eu = Stored_Unit( unit );

        if ( enemy_inventory.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
                                                                               //Broodwar->sendText( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else { // the insertion must have failed
               //Broodwar->sendText( "%s is already at address %p.", eu.type_.c_str(), enemy_inventory.unit_inventory_.find( unit ) ) ;
        }

        if (unit->getType().isBuilding() && unit->getPlayer()->getRace() == Races::Zerg) {
            inventory.estimated_enemy_workers_--;
        }
    }

    if ( unit->getPlayer()->isNeutral() && !unit->isInvincible() ) { // safety check.
                                                                                 //Broodwar->sendText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit nu = Stored_Unit(unit);
        neutral_inventory.addStored_Unit(nu);

    }

    if (unit->getPlayer()->isNeutral() && unit->getType().isResourceContainer() ) { // safety check.
        Stored_Resource* ru = &Stored_Resource(unit);
        ru->max_stock_value_ = ru->current_stock_value_; // its value is what it has now, since it was somehow missing at game start. Must be passed by refrence or it will be forgotten.
        land_inventory.addStored_Resource(*ru);
        //inventory.updateBaseLoc(land_inventory); // this line breaks the expos? How? Is this even plausible?
        inventory.unwalkable_needs_updating = true;
    }

    //update maps, requires up-to date enemy inventories.
    if ( unit->getType().isBuilding() ) {
        inventory.unwalkable_needs_updating = true;
        //if (unit->getPlayer() == Broodwar->enemy()) {
        //    //update maps, requires up-to date enemy inventories.
        //    inventory.veins_out_need_updating = true;
        //}
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
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

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

    if ( unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone && unit->getPlayer() == Broodwar->self()) {
        my_reservation.removeReserveSystem(unit->getType());
    }

    if (unit->getType().isWorker()) {
        friendly_inventory.purgeWorkerRelations(unit, land_inventory, inventory, my_reservation);
    }
}


void MeatAIModule::onUnitDestroy( BWAPI::Unit unit ) // something mods Unit to 0xf inside here!
{
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if ( unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = enemy_inventory.unit_inventory_.find( unit );
        if ( found_ptr != enemy_inventory.unit_inventory_.end() ) {
            enemy_inventory.unit_inventory_.erase( unit );
            dead_enemy_inventory.addStored_Unit(unit);
            inventory.estimated_enemy_workers_--;
            //Broodwar->sendText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else {
            //Broodwar->sendText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_inventory.unit_inventory_.size() );
        }
    }

    if ( IsMineralField( unit ) ) { // If the unit is a mineral field we have to detach all those poor workers.

        for ( auto potential_miner = friendly_inventory.unit_inventory_.begin(); potential_miner != friendly_inventory.unit_inventory_.end() && !friendly_inventory.unit_inventory_.empty(); potential_miner++ ) {
 
            if (potential_miner->second.locked_mine_ == unit) {
                Unit miner_unit = potential_miner->second.bwapi_unit_;

                bool was_clearing = potential_miner->second.isAssignedClearing(land_inventory); // Was the mine being cleared with intent?

                //if (miner_unit) {
                //    miner_unit->stop();
                //} // Do NOT tell the miner to stop here. He will end with a mineral in his "mouth" and not return it to base!

                friendly_inventory.purgeWorkerRelations(potential_miner->first, land_inventory, inventory, my_reservation); // reset the worker
                
                auto found_mineral_ptr = land_inventory.resource_inventory_.find(unit); // erase the now-gone mine.
                if (found_mineral_ptr != land_inventory.resource_inventory_.end()) {
                    land_inventory.resource_inventory_.erase(unit); //Clear that mine from the resource inventory.
                    //inventory.updateBaseLoc( land_inventory );
                }

                if (_ANALYSIS_MODE) {
                    Broodwar->sendText("A mine is dead!");
                }

                if ( was_clearing ) {
                    MeatAIModule::Worker_Clear(miner_unit, friendly_inventory); // reassign clearing workers again.
                    if (potential_miner->second.isAssignedClearing(land_inventory)) {
                        inventory.updateWorkersClearing(friendly_inventory, land_inventory);
                    }
                }
            }
        }

        // clear it just in case. 
        auto found_mineral_ptr = land_inventory.resource_inventory_.find(unit);
        if (found_mineral_ptr != land_inventory.resource_inventory_.end()) {
            land_inventory.resource_inventory_.erase(unit); //Clear that mine from the resource inventory.
                                                            //inventory.updateBaseLoc( land_inventory );
        }

    }

    if ( unit->getPlayer()->isNeutral() && !IsMineralField(unit) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = neutral_inventory.unit_inventory_.find(unit);
        if (found_ptr != neutral_inventory.unit_inventory_.end()) {
            neutral_inventory.unit_inventory_.erase(unit);
            //Broodwar->sendText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_inventory.unit_inventory_.size() );
        }
        else {
            //Broodwar->sendText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_inventory.unit_inventory_.size() );
        }
    }

    if (unit->getType().isBuilding()) {
        inventory.unwalkable_needs_updating = true;
    }

    if (unit->getPlayer() == Broodwar->self()) {
        if (unit->getType().isWorker()) {
            friendly_inventory.purgeWorkerRelations(unit, land_inventory, inventory, my_reservation);
        }

        if (!buildorder.ever_clear_) {
            if (unit->getType() == UnitTypes::Zerg_Overlord) { // overlords do not restart the build order.
                buildorder.building_gene_.insert(buildorder.building_gene_.begin(), Build_Order_Object(UnitTypes::Zerg_Overlord));
            }
            else {
                buildorder.clearRemainingBuildOrder();
            }
        }
    }
}

void MeatAIModule::onUnitMorph( BWAPI::Unit unit )
{
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

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

    if ( unit->getType().isWorker() || unit->getBuildType().isBuilding() ) {
        friendly_inventory.purgeWorkerRelations(unit, land_inventory, inventory, my_reservation);
    }

    if ( unit->getBuildType().isBuilding() ) {
        buildorder.updateRemainingBuildOrder(unit->getBuildType());
    }

    if ( unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone ) {
        //inventory.unwalkable_needs_updating = true;
        my_reservation.removeReserveSystem( unit->getType() );
    }

}

void MeatAIModule::onUnitRenegade( BWAPI::Unit unit ) // Should be a line-for-line copy of onUnitDestroy.
{
    onUnitDestroy(unit);
}

void MeatAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void MeatAIModule::onUnitComplete( BWAPI::Unit unit )
{
}
