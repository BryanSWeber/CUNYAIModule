#pragma once

#include "CUNYAIModule.h"
#include "CobbDouglas.h"
#include "Map_Inventory.h"
#include "Unit_Inventory.h"
#include "Resource_Inventory.h"
#include "Research_Inventory.h"
#include "GeneticHistoryManager.h"
#include "Fight_MovementManager.h"
#include "AssemblyManager.h"
#include "FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include <iostream> 
#include <fstream> // for file read/writing
#include <numeric> // std::accumulate
#include <algorithm>    // std::min_element, std::max_element
#include <chrono> // for in-game frame clock.
#include <stdio.h>  //for removal of files.

// CUNYAI V1.00

using namespace BWAPI;
using namespace Filter;
using namespace std;

//Declare universally shared inventories.
Player_Model CUNYAIModule::friendly_player_model;
Player_Model CUNYAIModule::enemy_player_model;
Player_Model CUNYAIModule::neutral_player_model;
//Unit_Inventory CUNYAIModule::enemy_inventory;
Resource_Inventory CUNYAIModule::land_inventory;
//Research_Inventory CUNYAIModule::research_inventory;
Map_Inventory CUNYAIModule::current_map_inventory;
FAP::FastAPproximation<Stored_Unit*> CUNYAIModule::MCfap;
FAP::FastAPproximation<Stored_Unit*>  CUNYAIModule::buildfap;

bool CUNYAIModule::army_starved;
bool CUNYAIModule::econ_starved;
bool CUNYAIModule::tech_starved;
double CUNYAIModule::adaptation_rate; //Adaptation rate to opponent.
double CUNYAIModule::alpha_army_original;
double CUNYAIModule::alpha_tech_original;
double CUNYAIModule::alpha_econ_original;

void CUNYAIModule::onStart()
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
    econ_starved = true;
    tech_starved = false;

    //Initialize model variables. 
    GeneticHistory gene_history = GeneticHistory( ".\\bwapi-data\\read\\output.txt" );

    delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
    gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

    //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
    alpha_army_original = friendly_player_model.spending_model_.alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
    alpha_econ_original = friendly_player_model.spending_model_.alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
    alpha_tech_original = friendly_player_model.spending_model_.alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
    adaptation_rate = gene_history.r_out_mutate_; //rate of worker growth.

    win_rate = (1 - gene_history.loss_rate_);

    //get initial build order.
    buildorder.getInitialBuildOrder( gene_history.build_order_ );

    //update Map Grids
    current_map_inventory.updateBuildablePos();
    current_map_inventory.updateUnwalkable();
    //inventory.updateSmoothPos();
    current_map_inventory.updateMapVeins();
    current_map_inventory.updateMapVeinsOut( Position(Broodwar->self()->getStartLocation()), current_map_inventory.home_base_, current_map_inventory.map_out_from_home_ );
    //inventory.updateMapChokes();
    current_map_inventory.updateBaseLoc( land_inventory );
    current_map_inventory.getStartPositions();

    //update timers.
    short_delay = 0;
    med_delay = 0;
    long_delay = 0;
    my_reservation = Reservation();

}

void CUNYAIModule::onEnd( bool isWinner )
{// Called when the game ends

    if constexpr (MOVE_OUTPUT_BACK_TO_READ || SSCAIT_OR_DOCKER) { // don't write to the read folder. But we want the full read contents ready for us to write in.
        rename(".\\bwapi-data\\read\\output.txt", ".\\bwapi-data\\write\\output.txt");  // Furthermore, rename will fail if there is already an existing file. 
    }

    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open( ".\\bwapi-data\\write\\output.txt", ios_base::app );
    string opponent_name = Broodwar->enemy()->getName().c_str();
    output << delta << "," 
        << gamma << ',' 
        << alpha_army_original << ',' 
        << alpha_econ_original << ',' 
        << alpha_tech_original << ',' 
        << adaptation_rate << ',' 
        << Broodwar->enemy()->getRace().c_str() << "," 
        << isWinner << ',' 
        << short_delay << ',' 
        << med_delay << ',' 
        << long_delay << ',' 
        << opponent_name << ',' 
        << Broodwar->mapFileName().c_str() 
        << ',' << buildorder.initial_building_gene_ 
        << endl;
    output.close();

    if constexpr (MOVE_OUTPUT_BACK_TO_READ) {
        rename(".\\bwapi-data\\write\\output.txt", ".\\bwapi-data\\read\\output.txt"); // Furthermore, rename will fail if there is already an existing file. 
    }
}

void CUNYAIModule::onFrame()
{ // Called once every game frame

  // Return if the game is a replay or is paused
    if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
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
    bool attempted_morph_larva_this_frame = false;
    bool attempted_morph_lurker_this_frame = false;
    bool attempted_morph_guardian_this_frame = false;

    // Update enemy player model. Draw all associated units.
    enemy_player_model.updateOtherOnFrame(Broodwar->enemy());
    enemy_player_model.units_.drawAllHitPoints(current_map_inventory);
    enemy_player_model.units_.drawAllLocations(current_map_inventory);

    //Update neutral units
    Player* neutral_player;
    for (auto p : Broodwar->getPlayers()) {
        if (p->isNeutral()) neutral_player = &p;
    }
    neutral_player_model.updateOtherOnFrame(*neutral_player);
    neutral_player_model.units_.drawAllHitPoints(current_map_inventory);
    neutral_player_model.units_.drawAllLocations(current_map_inventory);

    friendly_player_model.updateSelfOnFrame(enemy_player_model); // So far, mimics the only other enemy player. 

    //friendly_player_model.units_.drawAllVelocities(inventory);
    friendly_player_model.units_.drawAllHitPoints(current_map_inventory);
    friendly_player_model.units_.drawAllSpamGuards(current_map_inventory);
    friendly_player_model.units_.drawAllWorkerTasks(current_map_inventory, land_inventory);

    // Update FAPS with units.
    MCfap.clear();
    buildfap.clear();

    enemy_player_model.units_.addToMCFAP(MCfap, false, enemy_player_model.researches_);
    enemy_player_model.units_.addToBuildFAP(buildfap, false, enemy_player_model.researches_);

    friendly_player_model.units_.addToMCFAP(MCfap, true, friendly_player_model.researches_);
    friendly_player_model.units_.addToBuildFAP(buildfap, true, friendly_player_model.researches_);
    //friendly_player_model.units_.drawAllMAFAPaverages(inventory);

    // Let us estimate FAP values.
    MCfap.simulate(); // 96 frames of simulation for us.
    int friendly_fap_score = getFAPScore(MCfap, true);
    int enemy_fap_score = getFAPScore(MCfap, false);
    friendly_player_model.units_.pullFromFAP(*MCfap.getState().first);
    enemy_player_model.units_.pullFromFAP(*MCfap.getState().second);

    writePlayerModel(friendly_player_model, "friendly");
    writePlayerModel(enemy_player_model, "enemy");

    //Update posessed minerals. Erase those that are mined out.
    land_inventory.updateResourceInventory(friendly_player_model.units_, enemy_player_model.units_, current_map_inventory);
    land_inventory.drawMineralRemaining(current_map_inventory);

    if ((starting_enemy_race == Races::Random || starting_enemy_race == Races::Unknown) && Broodwar->enemy()->getRace() != starting_enemy_race) {
        //Initialize model variables. 
        GeneticHistory gene_history = GeneticHistory(".\\bwapi-data\\read\\output.txt");

        delta = gene_history.delta_out_mutate_; //gas starved parameter. Triggers state if: ln_gas/(ln_min + ln_gas) < delta;  Higher is more gas.
        gamma = gene_history.gamma_out_mutate_; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < gamma; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.  

        //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
        friendly_player_model.spending_model_.alpha_army = gene_history.a_army_out_mutate_; // army starved parameter. 
        friendly_player_model.spending_model_.alpha_econ = gene_history.a_econ_out_mutate_; // econ starved parameter. 
        friendly_player_model.spending_model_.alpha_tech = gene_history.a_tech_out_mutate_; // tech starved parameter. 
        adaptation_rate = gene_history.r_out_mutate_; //rate of worker growth.
        win_rate = (1 - gene_history.loss_rate_);
        Broodwar->sendText("WHOA! %s is broken. That's a good random.", Broodwar->enemy()->getRace().c_str());
    }

    //Update important variables.  Enemy stock has a lot of dependencies, updated above.
    current_map_inventory.updateVision_Count();

    current_map_inventory.updateLn_Supply_Remain();
    current_map_inventory.updateLn_Supply_Total();

    current_map_inventory.updateLn_Gas_Total();
    current_map_inventory.updateLn_Min_Total();

    current_map_inventory.updateGas_Workers();
    current_map_inventory.updateMin_Workers();

    current_map_inventory.updateMin_Possessed(land_inventory);
    current_map_inventory.updateHatcheries();  // macro variables, not every unit I have.
    current_map_inventory.updateWorkersClearing(friendly_player_model.units_, land_inventory);
    current_map_inventory.updateWorkersLongDistanceMining(friendly_player_model.units_, land_inventory);
    current_map_inventory.my_portion_of_the_map_ = (int)(sqrt(pow(Broodwar->mapHeight() * 32, 2) + pow(Broodwar->mapWidth() * 32, 2)) / (double)Broodwar->getStartLocations().size());
    current_map_inventory.expo_portion_of_the_map_ = (int)(sqrt(pow(Broodwar->mapHeight() * 32, 2) + pow(Broodwar->mapWidth() * 32, 2)) / (double)current_map_inventory.expo_positions_complete_.size());
    current_map_inventory.updateStartPositions(enemy_player_model.units_);
    current_map_inventory.updateScreen_Position();

    if (t_game == 0) {
        //update local resources
        current_map_inventory.updateMapVeinsOut( current_map_inventory.start_positions_[0], current_map_inventory.enemy_base_ground_, current_map_inventory.map_out_from_enemy_ground_);
        Resource_Inventory mineral_inventory = Resource_Inventory(Broodwar->getStaticMinerals());
        Resource_Inventory geyser_inventory = Resource_Inventory(Broodwar->getStaticGeysers());
        land_inventory = mineral_inventory + geyser_inventory; // for first initialization.
        current_map_inventory.updateBaseLoc(land_inventory);
        current_map_inventory.getExpoPositions(); // prime this once on game start.
    }

    current_map_inventory.updateBasePositions(friendly_player_model.units_, enemy_player_model.units_, land_inventory, neutral_player_model.units_, friendly_player_model.casualties_);
    current_map_inventory.drawExpoPositions();
    current_map_inventory.drawBasePositions();

    land_inventory.updateGasCollectors();
    land_inventory.updateMiners();

    if (buildorder.building_gene_.empty()) {
        buildorder.ever_clear_ = true;
    }
    else {
        bool need_gas_now = false;
        if (buildorder.building_gene_.front().getResearch()) {
            if (buildorder.building_gene_.front().getResearch().gasPrice()) {
                buildorder.building_gene_.front().getResearch().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        }
        else if (buildorder.building_gene_.front().getUnit()) {
            if (buildorder.building_gene_.front().getUnit().gasPrice()) {
                buildorder.building_gene_.front().getUnit().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        }
        else if (buildorder.building_gene_.front().getUpgrade()) {
            if (buildorder.building_gene_.front().getUpgrade().gasPrice()) {
                buildorder.building_gene_.front().getUpgrade().gasPrice() > 0 ? need_gas_now = true : need_gas_now = false;
            }
        }

        bool no_extractor = Count_Units(UnitTypes::Zerg_Extractor) == 0;
        if (need_gas_now && no_extractor) {
            buildorder.clearRemainingBuildOrder();
            CUNYAIModule::DiagnosticText("Uh oh, something's went wrong with building an extractor!");
        }
    }

    my_reservation.decrementReserveTimer();
    my_reservation.confirmOngoingReservations(friendly_player_model.units_);

    bool build_check_this_frame = false;
    vector<UnitType> types_of_units_checked_for_upgrades_this_frame = {};// starts empty.
    Position mutating_creep_colony_position = Position{ 0,0 }; // this is a simply practical check that saves a TON of resources.
    UnitType mutating_creep_colony_type = UnitTypes::Zerg_Creep_Colony;
    bool mutating_creep_this_frame = false;

    //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.

    //Knee-jerk states: gas, supply.
    gas_starved = (current_map_inventory.getLn_Gas_Ratio() < delta && Gas_Outlet()) ||
        (Gas_Outlet() && Broodwar->self()->gas() < 125) || // you need gas to buy things you have already invested in.
        (!buildorder.building_gene_.empty() && my_reservation.getExcessGas() > 0) ||// you need gas for a required build order item.
        (tech_starved && Tech_Avail() && Broodwar->self()->gas() < 200); // you need gas because you are tech starved.
    supply_starved = (current_map_inventory.getLn_Supply_Ratio() < gamma  &&   //If your supply is disproportionately low, then you are supply starved, unless
        Broodwar->self()->supplyTotal() < 400); // you have hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

    //This command has passed its diagnostic usefullness.
    //if constexpr (ANALYSIS_MODE) {
    //    if (t_game % (24*10) == 0) {
    //        friendly_player_model.spending_model_.printModelParameters();
    //    }
    //}

    bool massive_army = friendly_player_model.spending_model_.army_derivative == 0 || (friendly_player_model.units_.stock_fighting_total_ - Stock_Units(UnitTypes::Zerg_Sunken_Colony, friendly_player_model.units_) - Stock_Units(UnitTypes::Zerg_Spore_Colony, friendly_player_model.units_) >= enemy_player_model.units_.stock_fighting_total_ * 3);

    //Unitset enemy_set = getEnemy_Set(enemy_player_model.units_);
    enemy_player_model.units_.updateUnitInventorySummary();
    //friendly_player_model.units_.updateUnitInventorySummary(); Redundant with //updateUnitInventory.
    land_inventory.updateMiners();
    land_inventory.updateGasCollectors();

    current_map_inventory.est_enemy_stock_ = (int)enemy_player_model.units_.stock_fighting_total_; // just a raw count of their stuff.

    
    // Display the game status indicators at the top of the screen    
    if constexpr(DRAWING_MODE) {

        //Print_Unit_Inventory( 0, 50, friendly_player_model.units_ );
        Print_Cached_Inventory(0, 50);
        //Print_Test_Case(0, 50);
        Print_Upgrade_Inventory(375, 80);
        Print_Reservations(250, 170, my_reservation);
        if (buildorder.isEmptyBuildOrder()) {
            //Print_Unit_Inventory(500, 170, enemy_player_model.units_);
            Print_Research_Inventory(500, 170, enemy_player_model.researches_);
        }
        else {
            Print_Build_Order_Remaining(500, 170, buildorder);
        }

        Broodwar->drawTextScreen(0, 0, "Reached Min Fields: %d", current_map_inventory.min_fields_);
        Broodwar->drawTextScreen(0, 10, "Active Workers: %d", current_map_inventory.gas_workers_ + current_map_inventory.min_workers_);
        Broodwar->drawTextScreen(0, 20, "Workers (alt): (m%d, g%d)", land_inventory.total_miners_, land_inventory.total_gas_);  //

        Broodwar->drawTextScreen(0, 30, "Active Miners: %d", current_map_inventory.min_workers_);
        Broodwar->drawTextScreen(0, 40, "Active Gas Miners: %d", current_map_inventory.gas_workers_);

        Broodwar->drawTextScreen(125, 0, "Econ Starved: %s", friendly_player_model.spending_model_.econ_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 10, "Army Starved: %s", friendly_player_model.spending_model_.army_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 20, "Tech Starved: %s", friendly_player_model.spending_model_.tech_starved() ? "TRUE" : "FALSE");  //

        Broodwar->drawTextScreen(125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 50, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 60, "Gas Outlet: %s", Gas_Outlet() ? "TRUE" : "FALSE");  //


        Broodwar->drawTextScreen(125, 80, "Ln Y/L: %4.2f", friendly_player_model.spending_model_.getlny()); //
        Broodwar->drawTextScreen(125, 90, "Ln Y: %4.2f", friendly_player_model.spending_model_.getlnY()); //

        Broodwar->drawTextScreen(125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60); //
        Broodwar->drawTextScreen(125, 110, "Win Rate: %1.2f", win_rate); //
        Broodwar->drawTextScreen(125, 120, "Race: %s", Broodwar->enemy()->getRace().c_str());
        Broodwar->drawTextScreen(125, 130, "Opponent: %s", Broodwar->enemy()->getName().c_str()); //
        Broodwar->drawTextScreen(125, 140, "Map: %s", Broodwar->mapFileName().c_str()); //

        Broodwar->drawTextScreen(250, 0, "Econ Gradient: %.2g", friendly_player_model.spending_model_.econ_derivative);  //
        Broodwar->drawTextScreen(250, 10, "Army Gradient: %.2g", friendly_player_model.spending_model_.army_derivative); //
        Broodwar->drawTextScreen(250, 20, "Tech Gradient: %.2g", friendly_player_model.spending_model_.tech_derivative); //
        Broodwar->drawTextScreen(250, 30, "Enemy R: %.2g ", adaptation_rate); // 
        Broodwar->drawTextScreen(250, 40, "Alpha_Econ: %4.2f %%", friendly_player_model.spending_model_.alpha_econ * 100);  // As %s
        Broodwar->drawTextScreen(250, 50, "Alpha_Army: %4.2f %%", friendly_player_model.spending_model_.alpha_army * 100); //
        Broodwar->drawTextScreen(250, 60, "Alpha_Tech: %4.2f ", friendly_player_model.spending_model_.alpha_tech * 100); // No longer a % with capital-augmenting technology.
        Broodwar->drawTextScreen(250, 70, "Enemy Worker Est: %d ", current_map_inventory.estimated_enemy_workers_); // No longer a % with capital-augmenting technology.
        Broodwar->drawTextScreen(250, 80, "Delta_gas: %4.2f", delta); //
        Broodwar->drawTextScreen(250, 90, "Gamma_supply: %4.2f", gamma); //
        Broodwar->drawTextScreen(250, 100, "Time to Completion: %d", my_reservation.building_timer_); //
        Broodwar->drawTextScreen(250, 110, "Freestyling: %s", buildorder.isEmptyBuildOrder() ? "TRUE" : "FALSE"); //
        Broodwar->drawTextScreen(250, 120, "Last Builder Sent: %d", my_reservation.last_builder_sent_);
        Broodwar->drawTextScreen(250, 130, "Last Building: %s", buildorder.last_build_order.c_str()); //
        Broodwar->drawTextScreen(250, 140, "Next Expo Loc: (%d , %d)", current_map_inventory.next_expo_.x, current_map_inventory.next_expo_.y); //
        if (buildorder.isEmptyBuildOrder()) {
            Broodwar->drawTextScreen(250, 160, "Total Reservations: Min: %d, Gas: %d", my_reservation.min_reserve_, my_reservation.gas_reserve_);
        }
        else {
            Broodwar->drawTextScreen(250, 160, "Top in Build Order: Min: %d, Gas: %d", buildorder.building_gene_.begin()->getUnit().mineralPrice(), buildorder.building_gene_.begin()->getUnit().gasPrice());
        }

        //Broodwar->drawTextScreen(250, 150, "FAPP comparison: (%d , %d)", friendly_fap_score, enemy_fap_score); //
        Broodwar->drawTextScreen(250, 150, "FAPP: (%d , %d)", friendly_player_model.units_.moving_average_fap_stock_, enemy_player_model.units_.moving_average_fap_stock_); //

        //vision belongs here.
        Broodwar->drawTextScreen(375, 20, "Foe Stock(Est.): %d", current_map_inventory.est_enemy_stock_);
        Broodwar->drawTextScreen(375, 30, "Foe Army Stock: %d", enemy_player_model.units_.stock_fighting_total_); //
        Broodwar->drawTextScreen(375, 40, "Foe T Stock(Est.): %d", enemy_player_model.researches_.research_stock_);
        Broodwar->drawTextScreen(375, 50, "Gas (Pct. Ln.): %4.2f", current_map_inventory.getLn_Gas_Ratio());
        Broodwar->drawTextScreen(375, 60, "Vision (Pct.): %4.2f", current_map_inventory.vision_tile_count_ / (double)map_area);  //
        Broodwar->drawTextScreen(375, 70, "Unexplored Starts: %d", (int)current_map_inventory.start_positions_.size());  //

        //Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %4.2f", inventory.getLn_Supply_Ratio() );  //
        //Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d",  inventory.vision_tile_count_ );  //
        //Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

        Broodwar->drawTextScreen(500, 20, "Performance:");  // 
        Broodwar->drawTextScreen(500, 30, "APM: %d", Broodwar->getAPM());  // 
        Broodwar->drawTextScreen(500, 40, "APF: %4.2f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS());  // 
        Broodwar->drawTextScreen(500, 50, "FPS: %4.2f", Broodwar->getAverageFPS());  // 
        Broodwar->drawTextScreen(500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames());  //

        Broodwar->drawTextScreen(500, 70, delay_string);
        Broodwar->drawTextScreen(500, 80, preamble_string);
        Broodwar->drawTextScreen(500, 90, larva_string);
        Broodwar->drawTextScreen(500, 100, worker_string);
        Broodwar->drawTextScreen(500, 110, scouting_string);
        Broodwar->drawTextScreen(500, 120, combat_string);
        Broodwar->drawTextScreen(500, 130, detection_string);
        Broodwar->drawTextScreen(500, 140, upgrade_string);
        Broodwar->drawTextScreen(500, 150, creep_colony_string);

        for (auto p = land_inventory.resource_inventory_.begin(); p != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); ++p) {
            if (isOnScreen(p->second.pos_, current_map_inventory.screen_position_)) {
                Broodwar->drawCircleMap(p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan); // Plot their last known position.
                Broodwar->drawTextMap(p->second.pos_, "%d", p->second.current_stock_value_); // Plot their current value.
                Broodwar->drawTextMap(p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_); // Plot their current value.
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


        //for (vector<int>::size_type i = 0; i < inventory.map_out_from_home_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < inventory.map_out_from_home_[i].size(); ++j) {
        //        if (inventory.map_out_from_home_[i][j] % 100 == 0 /*&& inventory.map_out_from_home_[i][j] <= 1*/ ) { 
        //            if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
        //                Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_out_from_home_[i][j] );
        //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
        //            }
        //        }
        //    }
        //} // Pretty to look at!

        //for (vector<int>::size_type i = 0; i < inventory.map_out_from_enemy_ground_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < inventory.map_out_from_enemy_ground_[i].size(); ++j) {
        //        if (inventory.map_out_from_enemy_ground_[i][j] % 25 == 0 && inventory.map_out_from_enemy_ground_[i][j] > 1) {
        //            if (isOnScreen({ (int)i * 8 + 4, (int)j * 8 + 4 }, inventory.screen_position_)) {
        //                Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", inventory.map_out_from_enemy_ground_[i][j]);
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

        //for (auto &u : Broodwar->self()->getUnits()) {
        //    if (u->getLastCommand().getType() != UnitCommandTypes::Attack_Move /*&& u_type != UnitTypes::Zerg_Extractor && u->getLastCommand().getType() != UnitCommandTypes::Attack_Unit*/) {
        //        Broodwar->drawTextMap(u->getPosition(), u->getLastCommand().getType().c_str());
        //    }
        //}

        for (auto & j : friendly_player_model.units_.unit_inventory_) {
            CUNYAIModule::DiagnosticPhase(j.second,current_map_inventory.screen_position_);
        }


    }// close analysis mode



    // Prevent spamming by only running our onFrame once every number of latency frames.
    // Latency frames are the number of frames before commands are processed.
    auto end_preamble = std::chrono::high_resolution_clock::now();
    preamble_time = end_preamble - start_preamble;

    if (t_game % Broodwar->getLatencyFrames() != 0) {
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
        auto start_unit_morphs = std::chrono::high_resolution_clock::now();

        if (last_frame_of_unit_morph_command < t_game - 12) {

            //Only morph one larva this frame.
            if (!attempted_morph_larva_this_frame && u_type == UnitTypes::Zerg_Larva )
            {
                // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition.
                attempted_morph_larva_this_frame = true;
                if (Reactive_Build(u, current_map_inventory, friendly_player_model.units_, enemy_player_model.units_)) {
                    last_frame_of_unit_morph_command = t_game;
                }
                continue;
            }

            // Only ONE morph this frame. Potential adverse conflict with previous  Reactive_Build calls.
            if (!attempted_morph_lurker_this_frame && u_type == UnitTypes::Zerg_Hydralisk && !u->isUnderAttack() && Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect))
            {
                // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition. Updates if something is found.
                attempted_morph_lurker_this_frame = true;
                if (Reactive_BuildFAP(u, current_map_inventory, friendly_player_model.units_, enemy_player_model.units_)) {
                    last_frame_of_unit_morph_command = t_game;
                }
                continue;
            }

            // Only ONE morph this frame. Potential adverse conflict with previous  Reactive_Build calls.
            if (!attempted_morph_guardian_this_frame && u_type == UnitTypes::Zerg_Mutalisk && !u->isUnderAttack() && Count_Units(UnitTypes::Zerg_Greater_Spire) - Count_Units_In_Progress(UnitTypes::Zerg_Greater_Spire) > 0)
            {
                // Build appropriate units. Check for suppply block, rudimentary checks for enemy composition. Updates if something is found.
                attempted_morph_guardian_this_frame = true;
                if (Reactive_BuildFAP(u, current_map_inventory, friendly_player_model.units_, enemy_player_model.units_)) {
                    last_frame_of_unit_morph_command = t_game;
                }
                continue;
            }
        }

        auto end_unit_morphs = std::chrono::high_resolution_clock::now();


        // Detectors are called for cloaked units. Only if you're not supply starved, because we only have overlords for detectors.  Should happen before combat script or else the units will be 'continued' past;
        auto start_detector = std::chrono::high_resolution_clock::now();
        Position c; // holder for cloaked unit position.
        bool call_detector = false;
        if (!supply_starved && u_type != UnitTypes::Zerg_Overlord && checkOccupiedArea(enemy_player_model.units_, u->getPosition(), u_type.sightRange())) {
            Unit_Inventory e_neighbors = getUnitInventoryInRadius(enemy_player_model.units_, u->getPosition(), u_type.sightRange());
            for (auto e = e_neighbors.unit_inventory_.begin(); e != e_neighbors.unit_inventory_.end() && !e_neighbors.unit_inventory_.empty(); e++) {
                if ((*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || (*e).second.type_.isBurrowable()) {
                    c = (*e).second.pos_; // then we may to send in some vision.
                    call_detector = true;
                    break;
                } //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
            }
            if (call_detector) {
                int dist = 999999;
                int dist_temp = 0;
                bool detector_found = false;
                Stored_Unit detector_of_choice;
                for (auto d : friendly_player_model.units_.unit_inventory_) {
                    if (d.second.type_ == UnitTypes::Zerg_Overlord &&
                        d.second.bwapi_unit_ &&
                        !d.second.bwapi_unit_->isUnderAttack() &&
                        d.second.current_hp_ > 0.25 * d.second.type_.maxHitPoints()) { // overlords don't have shields.
                        dist_temp = d.second.bwapi_unit_->getDistance(c);
                        if (dist_temp < dist) {
                            dist = dist_temp;
                            detector_of_choice = d.second;
                            detector_found = true;
                        }
                    }
                }
                if (detector_found /*&& spamGuard(detector_of_choice)*/) {
                    double theta = atan2(detector_of_choice.pos_.y- c.y, detector_of_choice.pos_.x - c.x);
                    Position closest_loc_to_c_that_gives_vision = Position(c.x + cos(theta) * detector_of_choice.type_.sightRange() * 0.75, c.y + sin(theta) * detector_of_choice.type_.sightRange() * 0.75);
                    if (closest_loc_to_c_that_gives_vision.isValid() && closest_loc_to_c_that_gives_vision != Positions::Origin) {
                        detector_of_choice.bwapi_unit_->move(closest_loc_to_c_that_gives_vision);
                        if constexpr (DRAWING_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice.pos_, closest_loc_to_c_that_gives_vision, current_map_inventory.screen_position_, Colors::Cyan);
                        }
                        detector_of_choice.updateStoredUnit(detector_of_choice.bwapi_unit_);
                    }
                    else {
                        detector_of_choice.bwapi_unit_->move(c);
                        if constexpr (DRAWING_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice.pos_, current_map_inventory.screen_position_, c, Colors::Cyan);
                        }
                        detector_of_choice.updateStoredUnit(detector_of_choice.bwapi_unit_);
                    }

                }
            }
        }
        auto end_detector = std::chrono::high_resolution_clock::now();

        //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground. 
        auto start_combat = std::chrono::high_resolution_clock::now();
        bool foe_within_radius = false;

        if (((u_type != UnitTypes::Zerg_Larva && u_type.canAttack()) || u_type == UnitTypes::Zerg_Overlord) && spamGuard(u))
        {
            Mobility mobility;

            Stored_Unit* e_closest = getClosestThreatOrTargetStored(enemy_player_model.units_, u, 3200);
            if (u_type == UnitTypes::Zerg_Drone || u_type == UnitTypes::Zerg_Overlord) {
                e_closest = getClosestThreatOrTargetStored(enemy_player_model.units_, u, 256);
            }

            if (e_closest) { // if there are bad guys, search for friends within that area. 

                int distance_to_foe = (int)e_closest->pos_.getDistance(u->getPosition());
                int chargable_distance_self = CUNYAIModule::getChargableDistance(u, enemy_player_model.units_);
                int chargable_distance_enemy = CUNYAIModule::getChargableDistance(e_closest->bwapi_unit_, friendly_player_model.units_);
                int chargable_distance_net = chargable_distance_self + chargable_distance_enemy; // how far can you get before he shoots?
                int search_radius = max(max(chargable_distance_net + 64, enemy_player_model.units_.max_range_ + 64), 256 ); // expanded radius because of units intermittently suiciding against static D.
                //CUNYAIModule::DiagnosticText("%s, range:%d, spd:%d,max_cd:%d, charge:%d", u_type.c_str(), CUNYAIModule::getProperRange(u), (int)CUNYAIModule::getProperSpeed(u), enemy_player_model.units_.max_cooldown_, chargable_distance_net);

                Unit_Inventory enemy_loc_around_target = getUnitInventoryInRadius(enemy_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory enemy_loc_around_self = getUnitInventoryInRadius(enemy_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                //Unit_Inventory enemy_loc_out_of_reach = getUnitsOutOfReach(enemy_player_model.units_, u);
                Unit_Inventory enemy_loc = (enemy_loc_around_target + enemy_loc_around_self);

                Unit_Inventory friend_loc_around_target = getUnitInventoryInRadius(friendly_player_model.units_, e_closest->pos_, distance_to_foe + search_radius);
                Unit_Inventory friend_loc_around_me = getUnitInventoryInRadius(friendly_player_model.units_, u->getPosition(), distance_to_foe + search_radius);
                //Unit_Inventory friend_loc_out_of_reach = getUnitsOutOfReach(friendly_player_model.units_, u);
                Unit_Inventory friend_loc = (friend_loc_around_target + friend_loc_around_me);

                //vector<int> useful_stocks = CUNYAIModule::getUsefulStocks(friend_loc, enemy_loc);
                //int helpful_u = useful_stocks[0];
                //int helpful_e = useful_stocks[1]; // both forget value of psi units.
                int targetable_stocks = getTargetableStocks(u, enemy_loc);
                int threatening_stocks = getThreateningStocks(u, enemy_loc);

                bool unit_death_in_1_second = !Stored_Unit::unitAliveinFuture(friend_loc.unit_inventory_.at(u),48);
                bool they_take_a_fap_beating = checkSuperiorFAPForecast(friend_loc, enemy_loc);

                //bool we_take_a_fap_beating = (friendly_player_model.units_.stock_total_ - friendly_player_model.units_.future_fap_stock_) * enemy_player_model.units_.stock_total_ > (enemy_player_model.units_.stock_total_ - enemy_player_model.units_.future_fap_stock_) * friendly_player_model.units_.stock_total_; // attempt to see if unit stuttering is a result of this. 
                //bool we_take_a_fap_beating = false;

                foe_within_radius = distance_to_foe < search_radius;

                if (e_closest->valid_pos_ && foe_within_radius ) {  // Must have a valid postion on record to attack.
                                              //double minimum_enemy_surface = 2 * 3.1416 * sqrt( (double)enemy_loc.volume_ / 3.1414 );
                                              //double minimum_friendly_surface = 2 * 3.1416 * sqrt( (double)friend_loc.volume_ / 3.1414 );
                                              //double unusable_surface_area_f = max( (minimum_friendly_surface - minimum_enemy_surface) / minimum_friendly_surface, 0.0 );
                                              //double unusable_surface_area_e = max( (minimum_enemy_surface - minimum_friendly_surface) / minimum_enemy_surface, 0.0 );
                                              //double portion_blocked = min(pow(minimum_occupied_radius / search_radius, 2), 1.0); // the volume ratio (equation reduced by cancelation of 2*pi )

                    bool neccessary_attack =
                        (targetable_stocks > 0 || threatening_stocks == 0 || they_take_a_fap_beating) && (
                            //helpful_e <= helpful_u * 0.95 || // attack if you outclass them and your boys are ready to fight. Equality for odd moments of matching 0,0 helpful forces. 
                            //massive_army ||
                            //friend_loc.is_attacking_ > (friend_loc.unit_inventory_.size() / 2) || // attack by vote. Will cause herd problems.
                            threatening_stocks == 0 || they_take_a_fap_beating ||
                            current_map_inventory.home_base_.getDistance(e_closest->pos_) < search_radius || // Force fight at home base.
                            current_map_inventory.safe_base_.getDistance(e_closest->pos_) < search_radius || // Force fight at safe base.
                            //inventory.est_enemy_stock_ < 0.75 * exp( inventory.ln_army_stock_ ) || // attack you have a global advantage (very very rare, global army strength is vastly overestimated for them).
                            //!army_starved || // fight your army is appropriately sized.
                            (friend_loc.worker_count_ > 0 && u_type != UnitTypes::Zerg_Drone) //Don't run if drones are present.
                            //(Count_Units(UnitTypes::Zerg_Sunken_Colony, friend_loc) > 0 && enemy_loc.stock_ground_units_ > 0) || // Don't run if static d is present.
                                //(!IsFightingUnit(e_closest->bwapi_unit_) && 64 > enemy_loc.max_range_) || // Don't run from noncombat junk.
                            //threatening_stocks == 0 ||
                            //( 32 > enemy_loc.max_range_ && friend_loc.max_range_ > 32 && helpful_e * (1 - unusable_surface_area_e) < 0.75 * helpful_u)  || Note: a hydra and a ling have the same surface area. But 1 hydra can be touched by 9 or so lings.  So this needs to be reconsidered.
                            );// don't run if they're in range and you're done for. Melee is <32, not 0. Hugely benifits against terran, hurts terribly against zerg. Lurkers vs tanks?; Just added this., hugely impactful. Not inherently in a good way, either. 
                                                   //  bool retreat = u->canMove() && ( // one of the following conditions are true:
                                                   //(u_type.isFlyer() && enemy_loc.stock_shoots_up_ > 0.25 * friend_loc.stock_fliers_) || //  Run if fliers face more than token resistance.


                    bool force_retreat =
                        (!they_take_a_fap_beating) ||
                        //!unit_likes_forecast || // don't run just because you're going to die. Silly units, that's what you're here for.
                        //(targetable_stocks == 0 && threatening_stocks > 0 && !grim_distance_trigger) ||
                        //(u_type == UnitTypes::Zerg_Overlord && threatening_stocks > 0) ||
                        //(u_type.isFlyer() && u_type != UnitTypes::Zerg_Scourge && ((u->isUnderAttack() && u->getHitPoints() < 0.5 * u->getInitialHitPoints()) || enemy_loc.stock_shoots_up_ > friend_loc.stock_fliers_)) || // run if you are flying (like a muta) and cannot be practical.
                        //(e_closest->bwapi_unit_ && !e_closest->bwapi_unit_->isDetected()) ||  // Run if they are cloaked. Must be visible to know if they are cloaked. Might cause problems with bwapiunits.
                        //                                                                      //helpful_u < helpful_e * 0.50 || // Run if they have local advantage on you
                        //(!getUnitInventoryInRadius(friend_loc, UnitTypes::Zerg_Sunken_Colony, u->getPosition(), 7 * 32 + search_radius).unit_inventory_.empty() && getUnitInventoryInRadius(friend_loc, UnitTypes::Zerg_Sunken_Colony, e_closest->pos_, 7 * 32).unit_inventory_.empty() && enemy_loc.max_range_ < 7 * 32) ||
                        //(friend_loc.max_range_ >= enemy_loc.max_range_ && friend_loc.max_range_> 32 && getUnitInventoryInRadius(friend_loc, e_closest->pos_, friend_loc.max_range_ - 32).max_range_ && getUnitInventoryInRadius(friend_loc, e_closest->pos_, friend_loc.max_range_ - 32).max_range_ < friend_loc.max_range_ ) || // retreat if sunken is nearby but not in range.
                        //(friend_loc.max_range_ < enemy_loc.max_range_ || 32 > friend_loc.max_range_ ) && (1 - unusable_surface_area_f) * 0.75 * helpful_u < helpful_e || // trying to do something with these surface areas.
                        (u_type == UnitTypes::Zerg_Overlord || //overlords should be cowardly not suicidal.
                        (u_type == UnitTypes::Zerg_Drone && unit_death_in_1_second)); // Run if drone and (we have forces elsewhere or the drone is injured).  Drones don't have shields.
                                                                                                                                      //(helpful_u == 0 && helpful_e > 0); // run if this is pointless. Should not happen because of search for attackable units? Should be redudnent in necessary_attack line one.

                    bool drone_problem = u_type == UnitTypes::Zerg_Drone && enemy_loc.worker_count_ > 0;

                    bool is_spelled = u->isUnderStorm() || u->isUnderDisruptionWeb() || u->isUnderDarkSwarm() || u->isIrradiated(); // Run if spelled.
                    //bool too_far_away_from_front_line = (inventory.getRadialDistanceOutFromEnemy(u->getPosition()) >(inventory.closest_radial_distance_enemy_ground_ + 3 * 24 * CUNYAIModule::getProperSpeed(u) / 4));
                    //bool safe_distance_away = distance_to_foe > chargable_distance_enemy;

                    bool cooldown = u->getGroundWeaponCooldown() > 0 || u->getAirWeaponCooldown() > 0;
                    bool kite = cooldown && distance_to_foe < 64 && getProperRange(u) > 64 && getProperRange(e_closest->bwapi_unit_) < 64 && !u->isBurrowed() && Can_Fight(*e_closest, u); //kiting?- /*&& getProperSpeed(e_closest->bwapi_unit_) <= getProperSpeed(u)*/
                    
                    if (neccessary_attack && !force_retreat && !is_spelled && !drone_problem && !kite) {
                        mobility.Tactical_Logic(u, enemy_loc, friend_loc, search_radius, current_map_inventory, Colors::Orange); 
                    }
                    else if (is_spelled) {
                        Stored_Unit* closest = getClosestThreatOrTargetStored(friendly_player_model.units_, u, 128);
                        if (closest) {
                            mobility.Retreat_Logic(u, *closest, friend_loc, enemy_loc, enemy_player_model.units_, friendly_player_model.units_, search_radius, current_map_inventory, Colors::Blue); // this is not explicitly getting out of storm. It is simply scattering.
                        }

                    }
                    else if (drone_problem) {
                        if (Count_Units_Doing(UnitTypes::Zerg_Drone, UnitCommandTypes::Attack_Unit, friend_loc) <= enemy_loc.worker_count_ &&
                            friend_loc.getMeanBuildingLocation() != Positions::Origin &&
                            u->getLastCommand().getType() != UnitCommandTypes::Morph &&
                            !unit_death_in_1_second){
                            friendly_player_model.units_.purgeWorkerRelations(u, land_inventory, current_map_inventory, my_reservation);
                            mobility.Tactical_Logic(u, enemy_loc, friend_loc, search_radius, current_map_inventory, Colors::Orange); // move towards enemy untill tactical logic takes hold at about 150 range.
                        }
                    }
                    else{
                        if (!buildorder.ever_clear_ && ((!e_closest->type_.isWorker() && e_closest->type_.canAttack()) || enemy_loc.worker_count_ > 2) && (!u_type.canAttack() || u_type == UnitTypes::Zerg_Drone || friend_loc.getMeanBuildingLocation() != Positions::Origin)) {
                            if (u_type == UnitTypes::Zerg_Overlord) {
                                //see unit destruction case. We will replace this overlord, likely a foolish scout.
                            }
                            else {
                                buildorder.clearRemainingBuildOrder(); // Neutralize the build order if something other than a worker scout is happening.
                                CUNYAIModule::DiagnosticText("Clearing Build Order, board state is dangerous.");
                            }
                        }
                            mobility.Retreat_Logic(u, *e_closest, friend_loc, enemy_loc, enemy_player_model.units_, friendly_player_model.units_, search_radius, current_map_inventory, Colors::White);
                    }


                    // workers tasks should be reset.
                    if (u_type.isWorker()) {
                        friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation);
                    }

                    continue; // this unit is finished.
                }

            } // close local examination.
            
            if (u_type != UnitTypes::Zerg_Drone && u_type != UnitTypes::Zerg_Larva && !u_type.isBuilding()){ // if there is nothing to fight, psudo-boids.
                mobility.Pathing_Movement(u, friendly_player_model.units_, enemy_player_model.units_, current_map_inventory);
            }
        }
        auto end_combat = std::chrono::high_resolution_clock::now();


        // Worker Loop - moved after combat to prevent mining from overriding worker defense..
        auto start_worker = std::chrono::high_resolution_clock::now();
        if (u_type.isWorker()) {
            Stored_Unit& miner = *friendly_player_model.units_.getStoredUnit(u);

            bool want_gas = gas_starved && (Count_Units(UnitTypes::Zerg_Extractor) - Count_Units_In_Progress(UnitTypes::Zerg_Extractor)) > 0;  // enough gas if (many critera), incomplete extractor, or not enough gas workers for your extractors.  
            bool too_much_gas = 1 - current_map_inventory.getLn_Gas_Ratio() > delta;
            bool no_recent_worker_alteration = miner.time_of_last_purge_ < t_game - 12 && miner.time_since_last_command_ > 12;

            // Identify old mineral task. If there's no new better job, put them back on this without disturbing them.
            bool was_gas = miner.isAssignedGas(land_inventory);
            bool was_mineral = miner.isAssignedMining(land_inventory);
            bool was_long_mine = miner.isLongRangeLock(land_inventory);
            Unit old_mineral_patch = nullptr;
            if ((was_mineral || was_gas) && !was_long_mine) {
                old_mineral_patch = miner.locked_mine_;
            }

            if (!isRecentCombatant(miner.bwapi_unit_) && !miner.isAssignedClearing(land_inventory) && !miner.isAssignedBuilding(land_inventory) && spamGuard(miner.bwapi_unit_)) { //Do not disturb fighting workers or workers assigned to clear a position. Do not spam. Allow them to remain locked on their task. 

                // Each mineral-related subtask does the following:
                // Checks if it is doing a task of lower priority.
                // It clears the worker. 
                // It tries to assign the worker to the new task.
                // If it is successfully assigned, continue. On the next frame you will be caught by "Maintain the locks" step.
                // If it is not successfully assigned, return to old task.

                //BUILD-RELATED TASKS:
                if (isEmptyWorker(u) && miner.isAssignedResource(land_inventory) && !miner.isAssignedGas(land_inventory) && !miner.isAssignedBuilding(land_inventory) && my_reservation.last_builder_sent_ < t_game - Broodwar->getLatencyFrames() - 15 * 24 && !build_check_this_frame) { //only get those that are in line or gathering minerals, but not carrying them or harvesting gas. This always irked me.
                    build_check_this_frame = true;
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation); //Must be disabled or else under some conditions, we "stun" a worker every frame. Usually the exact same one, essentially killing it.
                    Building_Begin(u, current_map_inventory, enemy_player_model.units_); // something's funny here. I would like to put it in the next line conditional but it seems to cause a crash when no major buildings are left to build.
                    if (miner.isAssignedBuilding(land_inventory)) { //Don't purge the building relations here - we just established them!
                        miner.stopMine(land_inventory);
                        continue;
                    }
                    else if (old_mineral_patch) {
                        attachToParticularMine(old_mineral_patch, land_inventory, miner); // go back to your old job. Updated unit.
                        //continue;
                    }
                } // Close Build loop

                //Workers at their end build location should build there!
                if (miner.isAssignedBuilding(land_inventory) && TilePosition(miner.pos_) == current_map_inventory.next_expo_ && my_reservation.reservation_map_.at(miner.bwapi_unit_->getBuildType())) {
                    clearBuildingObstuctions(friendly_player_model.units_, current_map_inventory, miner.bwapi_unit_);
                    if (miner.bwapi_unit_->build(UnitTypes::Zerg_Hatchery, current_map_inventory.next_expo_)) {
                        my_reservation.removeReserveSystem(miner.bwapi_unit_->getBuildType());
                    }
                    continue;
                }

                //MINERAL-RELATED TASKS
                //Workers need to clear empty patches.
                bool time_to_start_clearing_a_path = current_map_inventory.hatches_ >= 2 && Nearby_Blocking_Minerals(u, friendly_player_model.units_);
                if (time_to_start_clearing_a_path && current_map_inventory.workers_clearing_ == 0 && isEmptyWorker(u)) {
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation);
                    Worker_Clear(u, friendly_player_model.units_);
                    if (miner.isAssignedClearing(land_inventory)) {
                        current_map_inventory.updateWorkersClearing(friendly_player_model.units_, land_inventory);
                        //continue;
                    }
                    else if (old_mineral_patch) {
                        attachToParticularMine(old_mineral_patch, land_inventory, miner); // go back to your old job. Updated unit.
                        //continue;
                    }
                } // clear those empty mineral patches that block paths.

                //Shall we assign them to gas?
                land_inventory.countViableMines();
                bool could_use_another_gas = land_inventory.local_gas_collectors_ * 2 <= land_inventory.local_refineries_ && land_inventory.local_refineries_ > 0 && want_gas;
                bool worker_bad_gas = (want_gas && miner.isAssignedMining(land_inventory) && could_use_another_gas);
                bool worker_bad_mine = ((!want_gas || too_much_gas) && miner.isAssignedGas(land_inventory));
                bool unassigned_worker = !miner.isAssignedResource(land_inventory) && !miner.isAssignedBuilding(land_inventory) && !miner.isLongRangeLock(land_inventory) && !miner.isAssignedClearing(land_inventory);
                // If we need gas, get gas!
                if (could_use_another_gas && ( unassigned_worker || (worker_bad_gas && current_map_inventory.last_gas_check_ < t_game - 5 * 24 && isEmptyWorker(u))) ) { //if this is your first worker of the frame consider resetting him.
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation);
                    current_map_inventory.last_gas_check_ = t_game;
                    if (could_use_another_gas /*&& !was_gas*/) { // don't reassign from gas into gas.
                        Worker_Gather(u, UnitTypes::Zerg_Extractor, friendly_player_model.units_); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
                        if (miner.isAssignedGas(land_inventory)) {
                            //continue;
                        }
                        else { // default to gathering minerals.
                            Worker_Gather(u, UnitTypes::Resource_Mineral_Field, friendly_player_model.units_); //assign a worker (minerals)
                            if (miner.isAssignedMining(land_inventory)) {
                                //continue;
                            }
                        }
                    }
                }

                //Otherwise, we should put them on minerals.
                if (!could_use_another_gas && ( unassigned_worker || (worker_bad_mine && current_map_inventory.last_gas_check_ < t_game - 5 * 24 && isEmptyWorker(u))) ) { //if this is your first worker of the frame consider resetting him.
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation);
                    current_map_inventory.last_gas_check_ = t_game;
                    Worker_Gather(u, UnitTypes::Resource_Mineral_Field, friendly_player_model.units_); // assign a worker a mine (gas). Will return null if no viable refinery exists. Might be a bug source.
                }
            }

            // return minerals manually if you have them.
            if (!isEmptyWorker(u) && u->isIdle() && no_recent_worker_alteration) {
                //friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                miner.bwapi_unit_->returnCargo();
                //if (old_mineral_patch) {
                //    attachToParticularMine(old_mineral_patch, land_inventory, miner); // go back to your old job. Updated unit.
                //}
                //else {
                //    miner.updateStoredUnit(u);
                //}
                //continue;
            }

            // If idle get a job.
            if (u->isIdle() && no_recent_worker_alteration) {
                friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation);
                Worker_Gather(u, UnitTypes::Resource_Mineral_Field, friendly_player_model.units_);
                if (!miner.isAssignedMining(land_inventory)) {
                    Worker_Gather(u, UnitTypes::Resource_Vespene_Geyser, friendly_player_model.units_);
                }
                miner.updateStoredUnit(u);
            }

            // let's leave units in full-mine alone. Miners will be automatically assigned a "return cargo task" by BW upon collecting a mineral from the mine.
            if (miner.isAssignedResource(land_inventory) && !isEmptyWorker(u) && !u->isIdle()) {
                continue;
            }

            // Maintain the locks by assigning the worker to their intended mine!
            bool worker_has_lockable_task = miner.isAssignedClearing(land_inventory) || miner.isAssignedResource(land_inventory);

            if (worker_has_lockable_task && !isEmptyWorker(u) && !u->isIdle()) {
                continue;
            }

            if (worker_has_lockable_task && ((miner.isBrokenLock(land_inventory) && miner.time_since_last_command_ > 12) || t_game < 5 + Broodwar->getLatencyFrames() || (u->isIdle() && no_recent_worker_alteration))) { //5 frame pause needed on gamestart or else the workers derp out. Can't go to 3.
                if (!miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                }
                miner.updateStoredUnit(u);
                continue;
            }
            else if (worker_has_lockable_task && miner.isLongRangeLock(land_inventory)) {
                if (!miner.bwapi_unit_->move(miner.getMine(land_inventory)->pos_)) { // reassign him back to work.
                    friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, current_map_inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
                }
                miner.updateStoredUnit(u);
                continue;
            }

            //else if (worker_has_lockable_task && miner.isMovingLock(land_inventory)) {
            //    if (!miner.bwapi_unit_->gather(miner.locked_mine_)) { // reassign him back to work.
            //        friendly_player_model.units_.purgeWorkerRelationsNoStop(u, land_inventory, inventory, my_reservation); //If he can't get back to work something's wrong with you and we're resetting you.
            //    }
            //    miner.updateStoredUnit(u);
            //    continue;
            //}

        } // Close Worker management loop
        auto end_worker = std::chrono::high_resolution_clock::now();

        //Upgrade loop:
        auto start_upgrade = std::chrono::high_resolution_clock::now();

        bool unconsidered_unit_type = std::find(types_of_units_checked_for_upgrades_this_frame.begin(), types_of_units_checked_for_upgrades_this_frame.end(), u_type) == types_of_units_checked_for_upgrades_this_frame.end();

        if (isIdleEmpty(u) && !u->canAttack() && u_type != UnitTypes::Zerg_Larva && u_type != UnitTypes::Zerg_Drone && unconsidered_unit_type && spamGuard(u) &&
            (u->canUpgrade() || u->canResearch() || u->canMorph())) { // this will need to be revaluated once I buy units that cost gas.
            Tech_Begin(u, friendly_player_model.units_, current_map_inventory);
            types_of_units_checked_for_upgrades_this_frame.push_back(u_type); // only check each type once.
            //PrintError_Unit( u );
        }
        auto end_upgrade = std::chrono::high_resolution_clock::now();

        //Creep Colony upgrade loop.  We are more willing to upgrade them than to build them, since the units themselves are useless in the base state.
        auto start_creepcolony = std::chrono::high_resolution_clock::now();

        if (u_type == UnitTypes::Zerg_Creep_Colony && spamGuard(u)) {
                   buildStaticDefence(u); // checks globally but not bad, info is mostly already there.
        }// closure: Creep colony loop

        auto end_creepcolony = std::chrono::high_resolution_clock::now();

        detector_time += end_detector - start_detector;
        larva_time += end_unit_morphs - start_unit_morphs;
        worker_time += end_worker - start_worker;
        //scout_time += end_scout - start_scout;
        combat_time += end_combat - start_combat;
        upgrade_time += end_upgrade - start_upgrade;
        creepcolony_time += end_creepcolony - start_creepcolony;
    } // closure: unit iterator


    auto end = std::chrono::high_resolution_clock::now();
    total_frame_time = end - start_preamble;

    //Clock App
    if (total_frame_time.count() > 55) {
        short_delay += 1;
    }
    if (total_frame_time.count() > 1000) {
        med_delay += 1;
    }
    if (total_frame_time.count() > 10000) {
        long_delay += 1;
    }
    if constexpr (RESIGN_MODE) {
        if ((short_delay > 320 || med_delay > 10 || long_delay > 1 || Broodwar->elapsedTime() > 90 * 60 || Count_Units(UnitTypes::Zerg_Drone, friendly_player_model.units_) == 0)) //if game times out or lags out, end game with resignation.
        {
            Broodwar->leaveGame();
        }
    }

    if constexpr (DRAWING_MODE) {
        int n;
        n = sprintf(delay_string, "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count());
        n = sprintf(preamble_string, "Preamble:      %3.f%%,%3.fms ", preamble_time.count() / (double)total_frame_time.count() * 100, preamble_time.count());
        n = sprintf(larva_string, "Larva:         %3.f%%,%3.fms", larva_time.count() / (double)total_frame_time.count() * 100, larva_time.count());
        n = sprintf(worker_string, "Workers:       %3.f%%,%3.fms", worker_time.count() / (double)total_frame_time.count() * 100, worker_time.count());
        n = sprintf(scouting_string, "Scouting:      %3.f%%,%3.fms", scout_time.count() / (double)total_frame_time.count() * 100, scout_time.count());
        n = sprintf(combat_string, "Combat:        %3.f%%,%3.fms", combat_time.count() / (double)total_frame_time.count() * 100, combat_time.count());
        n = sprintf(detection_string, "Detection:     %3.f%%,%3.fms", detector_time.count() / (double)total_frame_time.count() * 100, detector_time.count());
        n = sprintf(upgrade_string, "Upgrades:      %3.f%%,%3.fms", upgrade_time.count() / (double)total_frame_time.count() * 100, upgrade_time.count());
        n = sprintf(creep_colony_string, "CreepColonies: %3.f%%,%3.fms", creepcolony_time.count() / (double)total_frame_time.count() * 100, creepcolony_time.count());
    }

} // closure: Onframe

void CUNYAIModule::onSendText( std::string text )
{

    // Send the text to the game if it is not being processed.
    Broodwar->sendText( "%s", text.c_str() );

    // Make sure to use %s and pass the text as a parameter,
    // otherwise you may run into problems when you use the %(percent) character!

}

void CUNYAIModule::onReceiveText( BWAPI::Player player, std::string text )
{
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void CUNYAIModule::onPlayerLeft( BWAPI::Player player )
{
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText( "That was a good game. I'll remember this! %s!", player->getName().c_str() );
}

void CUNYAIModule::onNukeDetect( BWAPI::Position target )
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

void CUNYAIModule::onUnitDiscover( BWAPI::Unit unit )
{
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if ( unit->getPlayer()->isEnemy( Broodwar->self() ) && !unit->isInvincible() ) { // safety check.
                                                                                             //CUNYAIModule::DiagnosticText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit eu = Stored_Unit( unit );

        if ( enemy_player_model.units_.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
                                                                               //CUNYAIModule::DiagnosticText( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else { // the insertion must have failed
               //CUNYAIModule::DiagnosticText( "%s is already at address %p.", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.find( unit ) ) ;
        }

        if (unit->getType().isBuilding() && unit->getPlayer()->getRace() == Races::Zerg) {
            current_map_inventory.estimated_enemy_workers_--;
        }
    }

    if ( unit->getPlayer()->isNeutral() && !unit->isInvincible() ) { // safety check.
                                                                                 //CUNYAIModule::DiagnosticText( "I just gained vision of a %s", unit->getType().c_str() );
        Stored_Unit nu = Stored_Unit(unit);
        neutral_player_model.units_.addStored_Unit(nu);

    }

    if (unit->getPlayer()->isNeutral() && unit->getType().isResourceContainer() ) { // safety check.
        Stored_Resource* ru = &Stored_Resource(unit);
        ru->max_stock_value_ = ru->current_stock_value_; // its value is what it has now, since it was somehow missing at game start. Must be passed by refrence or it will be forgotten.
        land_inventory.addStored_Resource(*ru);
        //inventory.updateBaseLoc(land_inventory); // this line breaks the expos? How? Is this even plausible?
    }

    //update maps, requires up-to date enemy inventories.
    if ( unit->getType().isBuilding() ) {
        //if (unit->getPlayer() == Broodwar->enemy()) {
        //    //update maps, requires up-to date enemy inventories.
        //    inventory.veins_out_need_updating = true;
        //}
    }


}

void CUNYAIModule::onUnitEvade( BWAPI::Unit unit )
{
    //if ( unit && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check.
    //                                                                //CUNYAIModule::DiagnosticText( "I just gained vision of a %s", unit->getType().c_str() );
    //    Stored_Unit eu = Stored_Unit( unit );

    //    if ( enemy_player_model.units_.unit_inventory_.insert( { unit, eu } ).second ) { // if the insertion succeeded
    //        CUNYAIModule::DiagnosticText( "A %s just evaded me. Added to hiddent unit inventory, size %d", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
    //    }
    //    else { // the insertion must have failed
    //        CUNYAIModule::DiagnosticText( "Insertion of %s failed.", eu.type_.c_str() );
    //    }
    //}
}

void CUNYAIModule::onUnitShow( BWAPI::Unit unit )
{
    //if ( unit && unit->exists() && unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.. (old comment?)
    //    Stored_Unit eu = Stored_Unit( unit );
    //    auto found_ptr = enemy_player_model.units_.unit_inventory_.find( unit );
    //    if ( found_ptr != enemy_player_model.units_.unit_inventory_.end() ) {
    //        enemy_player_model.units_.unit_inventory_.erase( unit );
    //        CUNYAIModule::DiagnosticText( "Redscovered a %s, hidden unit inventory is now %d.", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
    //    }
    //    else {
    //        CUNYAIModule::DiagnosticText( "Discovered a %s.", unit->getType().c_str() );
    //    }
    //}
}

void CUNYAIModule::onUnitHide( BWAPI::Unit unit )
{


}

void CUNYAIModule::onUnitCreate( BWAPI::Unit unit )
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
            CUNYAIModule::DiagnosticText( "%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str() );
        }
    }

    if ( unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone && unit->getPlayer() == Broodwar->self()) {
        my_reservation.removeReserveSystem(unit->getType());
    }

    if (unit->getType().isWorker()) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit, land_inventory, current_map_inventory, my_reservation);
    }
}


void CUNYAIModule::onUnitDestroy( BWAPI::Unit unit ) // something mods Unit to 0xf inside here!
{
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if (unit->getPlayer() == Broodwar->self()) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.
        auto found_ptr = friendly_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            friendly_player_model.units_.unit_inventory_.erase(unit);
            friendly_player_model.casualties_.addStored_Unit(unit);
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
    }

    if ( unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.
        auto found_ptr = enemy_player_model.units_.getStoredUnit(unit);
        if ( found_ptr ) {
            enemy_player_model.units_.unit_inventory_.erase( unit );
            enemy_player_model.casualties_.addStored_Unit(unit);
            if(found_ptr->type_.isWorker()) enemy_player_model.estimated_workers_--;
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
    }

    if ( IsMineralField( unit ) ) { // If the unit is a mineral field we have to detach all those poor workers.

        // Check for miners who may have been digging at that patch.
        for ( auto potential_miner = friendly_player_model.units_.unit_inventory_.begin(); potential_miner != friendly_player_model.units_.unit_inventory_.end() && !friendly_player_model.units_.unit_inventory_.empty(); potential_miner++ ) {
 
            if (potential_miner->second.locked_mine_ == unit) {
                Unit miner_unit = potential_miner->second.bwapi_unit_;

                bool was_clearing = potential_miner->second.isAssignedClearing(land_inventory); // Was the mine being cleared with intent?

                // Do NOT tell the miner to stop here. He will end with a mineral in his "mouth" and not return it to base!
                friendly_player_model.units_.purgeWorkerRelationsNoStop(miner_unit, land_inventory, current_map_inventory, my_reservation); // reset the worker
                if ( was_clearing ) {

                    auto found_mineral_ptr = land_inventory.resource_inventory_.find(unit); // erase the now-gone mine.
                    if (found_mineral_ptr != land_inventory.resource_inventory_.end()) {
                        land_inventory.resource_inventory_.erase(unit); //Clear that mine from the resource inventory.
                        //inventory.updateBaseLoc( land_inventory );
                    }

                    CUNYAIModule::Worker_Clear(miner_unit, friendly_player_model.units_); // reassign clearing workers again.
                    if (potential_miner->second.isAssignedClearing(land_inventory)) {
                        current_map_inventory.updateWorkersClearing(friendly_player_model.units_, land_inventory);
                    }
                }
                else {
                    miner_unit->stop();
                }
            }
        }

        CUNYAIModule::DiagnosticText("A mine is dead!");


        // clear it just in case. 
        auto found_mineral_ptr = land_inventory.resource_inventory_.find(unit);
        if (found_mineral_ptr != land_inventory.resource_inventory_.end()) {
            land_inventory.resource_inventory_.erase(unit); //Clear that mine from the resource inventory.
                                                            //inventory.updateBaseLoc( land_inventory );
        }

    }

    if ( unit->getPlayer()->isNeutral() && !IsMineralField(unit) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = neutral_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            neutral_player_model.units_.unit_inventory_.erase(unit);
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
    }

    if (unit->getType().isBuilding()) {
    }

    if (unit->getPlayer() == Broodwar->self()) {
        if (unit->getType().isWorker()) {
            friendly_player_model.units_.purgeWorkerRelations(unit, land_inventory, current_map_inventory, my_reservation);
        }

        if (!buildorder.ever_clear_) {
            if (unit->getType() == UnitTypes::Zerg_Overlord) { // overlords do not restart the build order.
                buildorder.building_gene_.insert(buildorder.building_gene_.begin(), Build_Order_Object(UnitTypes::Zerg_Overlord));
            }
            else if (unit->getOrder() == Orders::ZergBuildingMorph && unit->isMorphing()) {
                buildorder.clearRemainingBuildOrder(); 
            }
        }
    }
}

void CUNYAIModule::onUnitMorph( BWAPI::Unit unit )
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

    if ( unit->getType().isWorker() ) {
        friendly_player_model.units_.purgeWorkerRelations(unit, land_inventory, current_map_inventory, my_reservation);
    }


    if ( unit->getBuildType().isBuilding() ) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit, land_inventory, current_map_inventory, my_reservation);
        buildorder.updateRemainingBuildOrder(unit->getBuildType());
    }

    if ( unit->getType().isBuilding() && unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone ) {
        my_reservation.removeReserveSystem( unit->getType() );
    }

}

void CUNYAIModule::onUnitRenegade( BWAPI::Unit unit ) // Should be a line-for-line copy of onUnitDestroy.
{
    onUnitDestroy(unit);
}

void CUNYAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void CUNYAIModule::onUnitComplete( BWAPI::Unit unit )
{
}