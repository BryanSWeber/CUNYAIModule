#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\CobbDouglas.h"
#include "Source\Map_Inventory.h"
#include "Source\Unit_Inventory.h"
#include "Source\Resource_Inventory.h"
#include "Source\Research_Inventory.h"
#include "Source\GeneticHistoryManager.h"
#include "Source\MobilityManager.h"
#include "Source\AssemblyManager.h"
#include "Source\\CombatManager.h"
#include "Source\TechManager.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\BWEB\BWEB.h"
#include <bwem.h>
#include <iostream>
#include <fstream> // for file read/writing
#include <numeric> // std::accumulate
#include <algorithm>    // std::min_element, std::max_element
#include <chrono> // for in-game frame clock.
#include <stdio.h>  //for removal of files.

// CUNYAI V2.00

using namespace BWAPI;
using namespace Filter;
using namespace std;
namespace { auto & bwemMap = BWEM::Map::Instance(); }

//Declare static variables for access in other modules.
    bool CUNYAIModule::army_starved = false;
    bool CUNYAIModule::econ_starved = false;
    bool CUNYAIModule::tech_starved = false;
    bool CUNYAIModule::supply_starved = false;
    bool CUNYAIModule::gas_starved = false;
    bool CUNYAIModule::larva_starved = false;
    double gamma = 0; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
    double delta = 0; // for gas levels. Gas is critical for spending but will be matched with supply.
    double CUNYAIModule::adaptation_rate = 0; //Adaptation rate to opponent.
    double CUNYAIModule::alpha_army_original = 0;
    double CUNYAIModule::alpha_tech_original = 0;
    double CUNYAIModule::alpha_econ_original = 0;
    double CUNYAIModule::gamma; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
    double CUNYAIModule::delta; // for gas levels. Gas is critical for spending but will be matched with supply.
    Player_Model CUNYAIModule::friendly_player_model;
    Player_Model CUNYAIModule::enemy_player_model;
    Player_Model CUNYAIModule::neutral_player_model;
    Resource_Inventory CUNYAIModule::land_inventory; // resources.
    Map_Inventory CUNYAIModule::current_map_inventory;  // macro variables, not every unit I have.
    CombatManager CUNYAIModule::combat_manager;
    FAP::FastAPproximation<Stored_Unit*> CUNYAIModule::MCfap; // integrating FAP into combat with a produrbation.
    TechManager CUNYAIModule::techmanager;
    AssemblyManager CUNYAIModule::assemblymanager;
    Building_Gene CUNYAIModule::buildorder; //
    Reservation CUNYAIModule::my_reservation;
    GeneticHistory CUNYAIModule::gene_history;
    WorkerManager CUNYAIModule::workermanager;

void CUNYAIModule::onStart()
{

    //Initialize BWEM, must be done FIRST.
    Broodwar << "Map initialization..." << std::endl;

    bwemMap.Initialize(BWAPI::BroodwarPtr);
    bwemMap.EnableAutomaticPathAnalysis();
    bool startingLocationsOK = bwemMap.FindBasesForStartingLocations();

    BWEB::Map::onStart();
    BWEB::Stations::findStations();
    //BWEB::Walls::createZSimCity();
    //assemblymanager.getDefensiveWalls(); //needs work.
    BWEB::Blocks::findBlocks();

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
    gene_history = GeneticHistory();
    gene_history.initializeHistory();

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
    current_map_inventory.updateMapVeinsOut( Position(Broodwar->self()->getStartLocation()) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), current_map_inventory.front_line_base, current_map_inventory.map_out_from_home_ );
    //inventory.updateMapChokes();
    current_map_inventory.getStartPositions();

    //update timers.
    short_delay = 0;
    med_delay = 0;
    long_delay = 0;
    my_reservation = Reservation();

    //friendly_player_model.setLockedOpeningValues();

    // Testing Build Order content intenstively.
    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open(".\\bwapi-data\\write\\BuildOrderFailures.txt", ios_base::app);
    string print_value = "";
    print_value += gene_history.build_order_;
    output << "Trying Build Order" << print_value << endl;
    output.close();


}

void CUNYAIModule::onEnd( bool isWinner )
{// Called when the game ends

    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open( ".\\bwapi-data\\write\\history.txt", ios_base::app );
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
        << Broodwar->mapFileName().c_str() << ','
        << round(enemy_player_model.average_army_ * 1000000) / 1000000 << ','
        << round(enemy_player_model.average_econ_ * 1000000) / 1000000 << ','
        << round(enemy_player_model.average_tech_ * 1000000) / 1000000 << ','
        << buildorder.initial_building_gene_ << ","
        << Broodwar->self()->getBuildingScore() << ','
        << Broodwar->self()->getKillScore() << ','
        << Broodwar->self()->getRazingScore() << ','
        << Broodwar->self()->getUnitScore() << ','
        << enemy_player_model.casualties_.detector_count_ + enemy_player_model.units_.detector_count_ << ','
        << enemy_player_model.casualties_.stock_fliers_ + enemy_player_model.units_.stock_fliers_ << ','
        << Broodwar->elapsedTime()
        << endl;
        ;
    output.close();

    if constexpr (MOVE_OUTPUT_BACK_TO_READ) {
        rename(".\\bwapi-data\\write\\history.txt", ".\\bwapi-data\\read\\history.txt"); // Furthermore, rename will fail if there is already an existing file.
    }

    if (!buildorder.isEmptyBuildOrder()) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open(".\\bwapi-data\\write\\BuildOrderFailures.txt", ios_base::app);
        string print_value = "";

        print_value += buildorder.building_gene_.front().getResearch().c_str();
        print_value += buildorder.building_gene_.front().getUnit().c_str();
        print_value += buildorder.building_gene_.front().getUpgrade().c_str();

        output << "Couldn't build: " << print_value << endl;
        output << "Hatches Left?:" << current_map_inventory.hatches_ << endl;
        output << "Win:" << isWinner << endl;
        output.close();
    }; // testing build order stuff intensively.

}

void CUNYAIModule::onFrame()
{ // Called once every game frame


  // Return if the game is a replay or is paused

    if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
        return;

    // Performance Qeuery Timer
    // http://www.decompile.com/cpp/faq/windows_timer_api.htm
    std::chrono::duration<double, std::milli> map_time;
    std::chrono::duration<double, std::milli> playermodel_time;
    std::chrono::duration<double, std::milli> larva_time;
    std::chrono::duration<double, std::milli> worker_time;
    std::chrono::duration<double, std::milli> scout_time;
    std::chrono::duration<double, std::milli> combat_time;
    std::chrono::duration<double, std::milli> detector_time;
    std::chrono::duration<double, std::milli> upgrade_time;
    std::chrono::duration<double, std::milli> total_frame_time; //will use preamble start time.

    auto start_playermodel = std::chrono::high_resolution_clock::now();

    // Game time;
    int t_game = Broodwar->getFrameCount(); // still need this for mining script.
    bool attempted_morph_larva_this_frame = false;
    bool attempted_morph_lurker_this_frame = false;
    bool attempted_morph_guardian_this_frame = false;

    // Update enemy player model. Draw all associated units.
    enemy_player_model.updateOtherOnFrame(Broodwar->enemy());
    //enemy_player_model.units_.drawAllHitPoints(current_map_inventory);
    enemy_player_model.units_.drawAllLocations();

    //Update neutral units
    Player* neutral_player;
    for (auto p : Broodwar->getPlayers()) {
        if (p->isNeutral()) neutral_player = &p;
    }
    neutral_player_model.updateOtherOnFrame(*neutral_player);
    neutral_player_model.units_.drawAllHitPoints();
    neutral_player_model.units_.drawAllLocations();

    friendly_player_model.updateSelfOnFrame(); // So far, mimics the only other enemy player.
    //friendly_player_model.units_.drawAllVelocities(inventory);
    //friendly_player_model.units_.drawAllHitPoints(inventory);
    friendly_player_model.units_.drawAllSpamGuards();
    //friendly_player_model.units_.drawAllWorkerTasks(current_map_inventory, land_inventory);
    //friendly_player_model.units_.drawAllMisplacedGroundUnits(current_map_inventory);
    //land_inventory.drawUnreachablePatch(current_map_inventory);
    // Update FAPS with units.
    MCfap.clear();
    enemy_player_model.units_.addToMCFAP(MCfap, false, enemy_player_model.researches_);
    //enemy_player_model.units_.drawAllMAFAPaverages();
    enemy_player_model.units_.drawAllFutureDeaths();

    friendly_player_model.units_.addToMCFAP(MCfap, true, friendly_player_model.researches_);
    //friendly_player_model.units_.drawAllMAFAPaverages();
    friendly_player_model.units_.drawAllFutureDeaths();

    // Let us estimate FAP values.
    MCfap.simulate(MOVING_AVERAGE_DURATION);
    int friendly_fap_score = getFAPScore(MCfap, true);
    int enemy_fap_score = getFAPScore(MCfap, false);
    friendly_player_model.units_.pullFromFAP(*MCfap.getState().first);
    enemy_player_model.units_.pullFromFAP(*MCfap.getState().second);

    writePlayerModel(friendly_player_model, "friendly");
    writePlayerModel(enemy_player_model, "enemy");

    if ((starting_enemy_race == Races::Random || starting_enemy_race == Races::Unknown) && Broodwar->enemy()->getRace() != starting_enemy_race) {
        //Initialize model variables.
        gene_history = GeneticHistory();
        gene_history.initializeHistory();

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

    //Knee-jerk states: gas, supply.
    gas_starved = (current_map_inventory.getGasRatio() < delta && workermanager.checkGasOutlet()) ||
        (workermanager.checkGasOutlet() && Broodwar->self()->gas() < max({ Count_Units(UnitTypes::Zerg_Extractor) * 100, CUNYAIModule::techmanager.getMaxGas(), 100 })) || // you need gas to buy things.
        (!buildorder.building_gene_.empty() && my_reservation.getExcessGas() <= 0);// you need gas for a required build order item.

    supply_starved = (current_map_inventory.getLn_Supply_Ratio() < gamma  &&   //If your supply is disproportionately low, then you are supply starved, unless
        Broodwar->self()->supplyTotal() < 399); // you have hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

    bool massive_army = friendly_player_model.spending_model_.army_derivative == 0 || (friendly_player_model.units_.stock_fighting_total_ - Stock_Units(UnitTypes::Zerg_Sunken_Colony, friendly_player_model.units_) - Stock_Units(UnitTypes::Zerg_Spore_Colony, friendly_player_model.units_) >= enemy_player_model.units_.stock_fighting_total_ * 3);


    current_map_inventory.est_enemy_stock_ = enemy_player_model.units_.stock_fighting_total_; // just a raw count of their stuff.
    combat_manager.updateReadiness();

    auto end_playermodel = std::chrono::high_resolution_clock::now();
    playermodel_time = end_playermodel - start_playermodel;


    auto start_map = std::chrono::high_resolution_clock::now();

    //Update posessed minerals. Erase those that are mined out.
    land_inventory.updateResourceInventory(friendly_player_model.units_, enemy_player_model.units_, current_map_inventory);
    land_inventory.drawMineralRemaining();

    //Update important variables.  Enemy stock has a lot of dependencies, updated above.
    current_map_inventory.updateVision_Count();

    current_map_inventory.updateLn_Supply_Remain();
    current_map_inventory.updateLn_Supply_Total();

    workermanager.updateGas_Workers();
    workermanager.updateMin_Workers();
    workermanager.updateWorkersClearing();
    workermanager.updateWorkersLongDistanceMining();
    workermanager.updateExcessCapacity();

    current_map_inventory.updateHatcheries();  // macro variables, not every unit I have.
    current_map_inventory.my_portion_of_the_map_ = static_cast<int>(sqrt(pow(Broodwar->mapHeight() * 32, 2) + pow(Broodwar->mapWidth() * 32, 2)) / static_cast<double>(Broodwar->getStartLocations().size()));
    current_map_inventory.expo_portion_of_the_map_ = static_cast<int>(sqrt(pow(Broodwar->mapHeight() * 32, 2) + pow(Broodwar->mapWidth() * 32, 2)) / static_cast<double>(current_map_inventory.expo_positions_complete_.size()));
    current_map_inventory.updateStartPositions(enemy_player_model.units_);
    current_map_inventory.updateScreen_Position();

    if (t_game == 0) {
        //update local resources
        current_map_inventory.updateMapVeinsOut( current_map_inventory.start_positions_[0], current_map_inventory.enemy_base_ground_, current_map_inventory.map_out_from_enemy_ground_);
        Resource_Inventory mineral_inventory = Resource_Inventory(Broodwar->getStaticMinerals());
        Resource_Inventory geyser_inventory = Resource_Inventory(Broodwar->getStaticGeysers());
        land_inventory = mineral_inventory + geyser_inventory; // for first initialization.
        current_map_inventory.getExpoPositions(); // prime this once on game start.
    }

    current_map_inventory.updateBasePositions();
    current_map_inventory.drawExpoPositions();
    current_map_inventory.drawBasePositions();

    techmanager.updateTech_Avail();
    assemblymanager.updateOptimalCombatUnit();
    assemblymanager.updatePotentialBuilders();
    if (t_game % MOVING_AVERAGE_DURATION == 0) {
        techmanager.clearSimulationHistory();
        assemblymanager.clearSimulationHistory();
    }// every X seconds reset the simulations.

    larva_starved = CUNYAIModule::Count_Units(UnitTypes::Zerg_Larva) <= CUNYAIModule::Count_Units(UnitTypes::Zerg_Hatchery)/2;

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
            //buildorder.clearRemainingBuildOrder();
            CUNYAIModule::DiagnosticText("Uh oh, something's went wrong with building an extractor!");
        }
    }

    my_reservation.decrementReserveTimer();
    my_reservation.confirmOngoingReservations();
    DiagnosticReservations(my_reservation, current_map_inventory.screen_position_);

    vector<UnitType> types_of_units_checked_for_upgrades_this_frame = {};// starts empty.

    //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.

   // if (Broodwar->mapWidth() && Broodwar->mapHeight()) {
    //current_map_inventory.createThreatField(enemy_player_model);
    //current_map_inventory.createAttractField(enemy_player_model);
        current_map_inventory.createExploreField();
        current_map_inventory.createAAField(enemy_player_model);
   // }

    //current_map_inventory.DiagnosticField(current_map_inventory.pf_explore_);
    //current_map_inventory.DiagnosticTile();


    auto end_map = std::chrono::high_resolution_clock::now();
    map_time = end_map - start_map;

    // Display the game status indicators at the top of the screen
    if constexpr(DRAWING_MODE) {

        //bwemMap.Draw(BWAPI::BroodwarPtr);
        BWEB::Map::draw();

        Print_Unit_Inventory( 0, 50, friendly_player_model.units_ );
        //Print_Cached_Inventory(0, 50);
        //assemblymanager.Print_Assembly_FAP_Cycle(0, 50);
        //Print_Test_Case(0, 50);
        Print_Upgrade_Inventory(375, 90);
        Print_Reservations(250, 190, my_reservation);
        //enemy_player_model.Print_Average_CD(500, 170);
        //techmanager.Print_Upgrade_FAP_Cycle(500, 170);
        if (buildorder.isEmptyBuildOrder()) {
        //    techmanager.Print_Upgrade_FAP_Cycle(500, 170);
            //Print_Unit_Inventory(500, 170, enemy_player_model.units_); // actual units on ground.
            Print_Research_Inventory(500, 170, enemy_player_model.researches_); // tech stuff
        }
        else {
            Print_Build_Order_Remaining(500, 170, buildorder);
        }

        Broodwar->drawTextScreen(0, 0, "Reached Min Fields: %d", land_inventory.getLocalMinPatches());
        Broodwar->drawTextScreen(0, 20, "Workers (alt): (m%d, g%d)", workermanager.min_workers_, workermanager.gas_workers_);  //
        Broodwar->drawTextScreen(0, 30, "Miners: %d vs %d", workermanager.min_workers_, land_inventory.getLocalMiners()); // This a misuse of local miners.
        Broodwar->drawTextScreen(0, 40, "Gas-ers: %d vs %d", workermanager.gas_workers_, land_inventory.getLocalGasCollectors()); // this is a misuse of local gas.

        Broodwar->drawTextScreen(125, 0, "Econ Starved: %s", friendly_player_model.spending_model_.econ_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 10, "Army Starved: %s", friendly_player_model.spending_model_.army_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 20, "Tech Starved: %s", friendly_player_model.spending_model_.tech_starved() ? "TRUE" : "FALSE");  //

        //Broodwar->drawTextScreen(125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 50, "Gas Starved: %s", gas_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 60, "Gas Outlet: %s", workermanager.checkGasOutlet() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 70, "Xtra Gas Avail: %s", workermanager.excess_gas_capacity_ ? "TRUE" : "FALSE");  //


        //Broodwar->drawTextScreen(125, 80, "Ln Y/L: %4.2f", friendly_player_model.spending_model_.getlny()); //
        //Broodwar->drawTextScreen(125, 90, "Ln Y: %4.2f", friendly_player_model.spending_model_.getlnY()); //

        //Broodwar->drawTextScreen(125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60); //
        //Broodwar->drawTextScreen(125, 110, "Win Rate: %1.2f", win_rate); //
        //Broodwar->drawTextScreen(125, 120, "Race: %s", Broodwar->enemy()->getRace().c_str());
        //Broodwar->drawTextScreen(125, 130, "Opponent: %s", Broodwar->enemy()->getName().c_str()); //
        //Broodwar->drawTextScreen(125, 140, "Map: %s", Broodwar->mapFileName().c_str()); //
        //Broodwar->drawTextScreen(125, 150, "Min Reserved: %d", my_reservation.min_reserve_); //
        //Broodwar->drawTextScreen(125, 160, "Gas Reserved: %d", my_reservation.gas_reserve_); //

        Broodwar->drawTextScreen(250, 0, "Econ Gradient: %.2g", friendly_player_model.spending_model_.econ_derivative);  //
        Broodwar->drawTextScreen(250, 10, "Army Gradient: %.2g", friendly_player_model.spending_model_.army_derivative); //
        Broodwar->drawTextScreen(250, 20, "Tech Gradient: %.2g", friendly_player_model.spending_model_.tech_derivative); //
        Broodwar->drawTextScreen(250, 30, "Enemy R: %.2g ", adaptation_rate); //
        Broodwar->drawTextScreen(250, 40, "Alpha_Econ: %4.2f %%", friendly_player_model.spending_model_.alpha_econ * 100);  // As %s
        Broodwar->drawTextScreen(250, 50, "Alpha_Army: %4.2f %%", friendly_player_model.spending_model_.alpha_army * 100); //
        Broodwar->drawTextScreen(250, 60, "Alpha_Tech: %4.2f ", friendly_player_model.spending_model_.alpha_tech * 100); // No longer a % with capital-augmenting technology.
        Broodwar->drawTextScreen(250, 70, "Delta_gas: %4.2f", delta); //
        Broodwar->drawTextScreen(250, 80, "Gamma_supply: %4.2f", gamma); //
        //Broodwar->drawTextScreen(250, 90, "Time to Completion: %d", my_reservation.building_timer_); //
        //Broodwar->drawTextScreen(250, 100, "Freestyling: %s", buildorder.isEmptyBuildOrder() ? "TRUE" : "FALSE"); //
        //Broodwar->drawTextScreen(250, 110, "Last Builder Sent: %d", my_reservation.last_builder_sent_);
        //Broodwar->drawTextScreen(250, 120, "Last Building: %s", buildorder.last_build_order.c_str()); //
        //Broodwar->drawTextScreen(250, 130, "Next Expo Loc: (%d , %d)", current_map_inventory.next_expo_.x, current_map_inventory.next_expo_.y); //
        //Broodwar->drawTextScreen(250, 140, "FAPP: (%d , %d)", friendly_player_model.units_.moving_average_fap_stock_, enemy_player_model.units_.moving_average_fap_stock_); //

        //if (buildorder.isEmptyBuildOrder()) {
        //    Broodwar->drawTextScreen(250, 160, "Total Reservations: Min: %d, Gas: %d", my_reservation.min_reserve_, my_reservation.gas_reserve_);
        //}
        //else {
        //    Broodwar->drawTextScreen(250, 160, "Top in Build Order: Min: %d, Gas: %d", buildorder.building_gene_.begin()->getUnit().mineralPrice(), buildorder.building_gene_.begin()->getUnit().gasPrice());
        //}

        ////Broodwar->drawTextScreen(250, 150, "FAPP comparison: (%d , %d)", friendly_fap_score, enemy_fap_score); //
        //Broodwar->drawTextScreen(250, 170, "Air Weakness: %s", friendly_player_model.u_have_active_air_problem_ ? "TRUE" : "FALSE"); //
        //Broodwar->drawTextScreen(250, 180, "Foe Air Weakness: %s", friendly_player_model.e_has_air_vunerability_ ? "TRUE" : "FALSE"); //

        ////vision belongs here.
        //Broodwar->drawTextScreen(375, 20, "Foe Stock(Est.): %d", current_map_inventory.est_enemy_stock_);
        //Broodwar->drawTextScreen(375, 30, "Foe Army Stock: %d", enemy_player_model.units_.stock_fighting_total_); //
        //Broodwar->drawTextScreen(375, 40, "Foe Tech Stock(Est.): %d", enemy_player_model.researches_.research_stock_);
        Broodwar->drawTextScreen(375, 50, "Foe Workers (Est.): %d", static_cast<int>(enemy_player_model.estimated_workers_));
        //Broodwar->drawTextScreen(375, 60, "Gas (Pct. Ln.): %4.2f", current_map_inventory.getGasRatio());
        Broodwar->drawTextScreen(375, 70, "Net lnY (est.): E:%4.2f, F:%4.2f", enemy_player_model.spending_model_.getlnYusing(friendly_player_model.spending_model_.alpha_army, friendly_player_model.spending_model_.alpha_tech), friendly_player_model.spending_model_.getlnY());  //
        //Broodwar->drawTextScreen(375, 80, "Unexplored Starts: %d", static_cast<int>(current_map_inventory.start_positions_.size()));  //

        ////Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %4.2f", inventory.getLn_Supply_Ratio() );  //
        ////Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d",  inventory.vision_tile_count_ );  //
        ////Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

        //Broodwar->drawTextScreen(500, 20, "Performance:");  //
        //Broodwar->drawTextScreen(500, 30, "APM: %d", Broodwar->getAPM());  //
        //Broodwar->drawTextScreen(500, 40, "APF: %4.2f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS());  //
        //Broodwar->drawTextScreen(500, 50, "FPS: %4.2f", Broodwar->getAverageFPS());  //
        //Broodwar->drawTextScreen(500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames());  //

        //Broodwar->drawTextScreen(500, 70, delay_string);
        //Broodwar->drawTextScreen(500, 80, playermodel_string);
        //Broodwar->drawTextScreen(500, 90, map_string);
        //Broodwar->drawTextScreen(500, 100, larva_string);
        //Broodwar->drawTextScreen(500, 110, worker_string);
        //Broodwar->drawTextScreen(500, 120, scouting_string);
        //Broodwar->drawTextScreen(500, 130, combat_string);
        //Broodwar->drawTextScreen(500, 140, detection_string);
        //Broodwar->drawTextScreen(500, 150, upgrade_string);
        //Broodwar->drawTextScreen(500, 160, creep_colony_string);

        //for (auto p = land_inventory.resource_inventory_.begin(); p != land_inventory.resource_inventory_.end() && !land_inventory.resource_inventory_.empty(); ++p) {
        //    if (isOnScreen(p->second.pos_, current_map_inventory.screen_position_)) {
        //        Broodwar->drawCircleMap(p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan); // Plot their last known position.
        //        Broodwar->drawTextMap(p->second.pos_, "%d", p->second.current_stock_value_); // Plot their current value.
        //        Broodwar->drawTextMap(p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_); // Plot their current value.
        //    }
        //}

        //for ( vector<int>::size_type i = 0; i < current_map_inventory.map_veins_.size(); ++i ) {
        //    for ( vector<int>::size_type j = 0; j < current_map_inventory.map_veins_[i].size(); ++j ) {
        //        if (current_map_inventory.map_veins_[i][j] > 175 ) {
        //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_) ) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
        //            }
        //        }
        //        else if (current_map_inventory.map_veins_[i][j] <= 2 && current_map_inventory.map_veins_[i][j] > 1 ) { // should only highlight smoothed-out barriers.
        //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_)) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Purple);
        //            }
        //        }
        //        else if (current_map_inventory.map_veins_[i][j] == 1 ) { // should only highlight smoothed-out barriers.
        //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_) ) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
        //            }
        //        }
        //    }
        //} // Pretty to look at!


        //for (vector<int>::size_type i = 0; i < current_map_inventory.map_out_from_home_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < current_map_inventory.map_out_from_home_[i].size(); ++j) {
        //        if (current_map_inventory.map_out_from_home_[i][j] <= 5 /*&& current_map_inventory.map_out_from_home_[i][j] <= 1*/ ) {
        //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_)) {
        //                Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", current_map_inventory.map_out_from_home_[i][j] );
        //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
        //            }
        //        }
        //    }
        //} // Pretty to look at!

        //for (vector<int>::size_type i = 0; i < inventory.map_out_from_enemy_ground_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < inventory.map_out_from_enemy_ground_[i].size(); ++j) {
        //        if (inventory.map_out_from_enemy_ground_[i][j] % 25 == 0 && inventory.map_out_from_enemy_ground_[i][j] > 1) {
        //            if (isOnScreen({ static_cast<int>i * 8 + 4, static_cast<int>j * 8 + 4 }, inventory.screen_position_)) {
        //                Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", inventory.map_out_from_enemy_ground_[i][j]);
        //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
        //            }
        //        }
        //    }
        //} // Pretty to look at!

        //for (vector<int>::size_type i = 0; i < inventory.smoothed_barriers_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < inventory.smoothed_barriers_[i].size(); ++j) {
        //        if ( inventory.smoothed_barriers_[i][j] > 0) {
        //            if (isOnScreen({ static_cast<int>i * 8 + 4, static_cast<int>j * 8 + 4 }, inventory.screen_position_)) {
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

        for (auto & j : friendly_player_model.units_.unit_map_) {
            CUNYAIModule::DiagnosticPhase(j.second,current_map_inventory.screen_position_);
        }

        //Diagnostic_Tiles(current_map_inventory.screen_position_, Colors::White);
        Diagnostic_Destination(friendly_player_model.units_, current_map_inventory.screen_position_, Colors::Grey);
        //Diagnostic_Watch_Expos();
    }// close analysis mode



    // Prevent spamming by only running our onFrame once every number of latency frames.
    // Latency frames are the number of frames before commands are processed.
    if (t_game % Broodwar->getLatencyFrames() != 0) {
        return;
    }

    auto start_unit_morphs = std::chrono::high_resolution_clock::now();
        assemblymanager.assignUnitAssembly();
    auto end_unit_morphs = std::chrono::high_resolution_clock::now();
    larva_time = end_unit_morphs - start_unit_morphs;


    // Iterate through all the units that we own
    for (auto &u : Broodwar->self()->getUnits())
    {
        if (!checkUnitTouchable(u)) continue; // can we mess with it at all?

        UnitType u_type = u->getType();

        // Finally make the unit do some stuff!

        // Detectors are called for cloaked units. Only if you're not supply starved, because we only have overlords for detectors.  Should happen before combat script or else the units will be 'continued' past;
        auto start_detector = std::chrono::high_resolution_clock::now();
        Position c; // holder for cloaked unit position.
        bool call_detector = false;
        if (!supply_starved && u_type != UnitTypes::Zerg_Overlord && checkOccupiedArea(enemy_player_model.units_, u->getPosition())) {
            Unit_Inventory e_neighbors = getUnitInventoryInRadius(enemy_player_model.units_, u->getPosition(), u_type.sightRange());
            for (auto e = e_neighbors.unit_map_.begin(); e != e_neighbors.unit_map_.end() && !e_neighbors.unit_map_.empty(); e++) {
                if ((*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || (*e).second.type_.isBurrowable() && CUNYAIModule::enemy_player_model.researches_.tech_[TechTypes::Burrowing]) {
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
                for (auto d : friendly_player_model.units_.unit_map_) {
                    if (d.second.type_ == UnitTypes::Zerg_Overlord &&
                        d.second.bwapi_unit_ &&
                        !static_cast<bool>(d.second.time_since_last_dmg_ < MOVING_AVERAGE_DURATION) &&
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
                    Position closest_loc_to_c_that_gives_vision = Position(c.x + static_cast<int>(cos(theta) * 0.75) * detector_of_choice.type_.sightRange(), c.y + static_cast<int>(sin(theta) * 0.75) * detector_of_choice.type_.sightRange() );
                    if (closest_loc_to_c_that_gives_vision.isValid() && closest_loc_to_c_that_gives_vision != Positions::Origin) {
                        detector_of_choice.bwapi_unit_->move(closest_loc_to_c_that_gives_vision);
                        if constexpr (DRAWING_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice.pos_, closest_loc_to_c_that_gives_vision, current_map_inventory.screen_position_, Colors::Cyan);
                        }
                        CUNYAIModule::updateUnitPhase(detector_of_choice.bwapi_unit_, Stored_Unit::Phase::Detecting); // Update the detector not the calling unit.
                    }
                    else {
                        detector_of_choice.bwapi_unit_->move(c);
                        if constexpr (DRAWING_MODE) {
                            Broodwar->drawCircleMap(c, 25, Colors::Cyan);
                            Diagnostic_Line(detector_of_choice.pos_, current_map_inventory.screen_position_, c, Colors::Cyan);
                        }
                        CUNYAIModule::updateUnitPhase(detector_of_choice.bwapi_unit_, Stored_Unit::Phase::Detecting);  // Update the detector not the calling unit.
                    }

                }
            }
        }
        auto end_detector = std::chrono::high_resolution_clock::now();

        //Combat Logic. Has some sophistication at this time. Makes retreat/attack decision.  Only retreat if your army is not up to snuff. Only combat units retreat. Only retreat if the enemy is near. Lings only attack ground.
        auto start_combat = std::chrono::high_resolution_clock::now();
        combat_manager.grandStrategyScript(u);
        auto end_combat = std::chrono::high_resolution_clock::now();


        // Worker Loop - moved after combat to prevent mining from overriding worker defense..
        auto start_worker = std::chrono::high_resolution_clock::now();
           if(u->getType().isWorker()) workermanager.workerWork(u);
        auto end_worker = std::chrono::high_resolution_clock::now();

        //Upgrade loop:
        auto start_upgrade = std::chrono::high_resolution_clock::now();

        bool unconsidered_unit_type = std::find(types_of_units_checked_for_upgrades_this_frame.begin(), types_of_units_checked_for_upgrades_this_frame.end(), u_type) == types_of_units_checked_for_upgrades_this_frame.end();

        if (isIdleEmpty(u) && !u_type.canAttack() && u_type != UnitTypes::Zerg_Drone && unconsidered_unit_type && spamGuard(u) &&
            (u->canUpgrade() || u->canResearch() || u->canMorph())) { // this will need to be revaluated once I buy units that cost gas.
            techmanager.Tech_BeginBuildFAP(u, friendly_player_model.units_, current_map_inventory);
            types_of_units_checked_for_upgrades_this_frame.push_back(u_type); // only check each type once.
            //PrintError_Unit( u );
        }
        auto end_upgrade = std::chrono::high_resolution_clock::now();

        detector_time += end_detector - start_detector;
        worker_time += end_worker - start_worker;
        combat_time += end_combat - start_combat;
        upgrade_time += end_upgrade - start_upgrade;
    } // closure: unit iterator


    auto end = std::chrono::high_resolution_clock::now();
    total_frame_time = end - start_playermodel;

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
        n = sprintf_s(delay_string,       "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count());
        n = sprintf_s(playermodel_string, "Players:       %3.f%%,%3.fms ", playermodel_time.count() / static_cast<double>(total_frame_time.count()) * 100, playermodel_time.count());
        n = sprintf_s(map_string,         "Maps:          %3.f%%,%3.fms ", map_time.count() / static_cast<double>(total_frame_time.count()) * 100, map_time.count());
        n = sprintf_s(larva_string,       "Larva:         %3.f%%,%3.fms", larva_time.count() / static_cast<double>(total_frame_time.count()) * 100, larva_time.count());
        n = sprintf_s(worker_string,      "Workers:       %3.f%%,%3.fms", worker_time.count() / static_cast<double>(total_frame_time.count()) * 100, worker_time.count());
        n = sprintf_s(scouting_string,    "Scouting:      %3.f%%,%3.fms", scout_time.count() / static_cast<double>(total_frame_time.count()) * 100, scout_time.count());
        n = sprintf_s(combat_string,      "Combat:        %3.f%%,%3.fms", combat_time.count() / static_cast<double>(total_frame_time.count()) * 100, combat_time.count());
        n = sprintf_s(detection_string,   "Detection:     %3.f%%,%3.fms", detector_time.count() / static_cast<double>(total_frame_time.count()) * 100, detector_time.count());
        n = sprintf_s(upgrade_string,     "Upgrades:      %3.f%%,%3.fms", upgrade_time.count() / static_cast<double>(total_frame_time.count()) * 100, upgrade_time.count());
    }

    //if (buildorder.isEmptyBuildOrder())  Broodwar->leaveGame(); // Test Opening Game intensively.

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

        if ( enemy_player_model.units_.unit_map_.insert( { unit, eu } ).second ) { // if the insertion succeeded
                                                                               //CUNYAIModule::DiagnosticText( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else { // the insertion must have failed
               //CUNYAIModule::DiagnosticText( "%s is already at address %p.", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.find( unit ) ) ;
        }

        if (unit->getType().isBuilding() && unit->getPlayer()->getRace() == Races::Zerg) {
            enemy_player_model.estimated_workers_--;
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
    }

    //update maps, requires up-to date enemy inventories.
    if ( unit->getType().isBuilding() ) {
        //if (unit->getPlayer() == Broodwar->enemy()) {
        //    //update maps, requires up-to date enemy inventories.
        //    inventory.veins_out_need_updating = true;
        //}
    }

    BWEB::Map::onUnitDiscover(unit);

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
        my_reservation.removeReserveSystem(TilePosition(unit->getOrderTargetPosition()), unit->getBuildType(), false);
    }

    if (unit->getType().isWorker()) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
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
            friendly_player_model.units_.unit_map_.erase(unit);
            friendly_player_model.casualties_.addStored_Unit(unit);
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        combat_manager.removeScout(unit);
    }

    if ( unit->getPlayer()->isEnemy( Broodwar->self() ) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.
        auto found_ptr = enemy_player_model.units_.getStoredUnit(unit);
        if ( found_ptr ) {
            if (found_ptr->type_.isWorker()) enemy_player_model.estimated_workers_--;
            enemy_player_model.units_.unit_map_.erase( unit );
            enemy_player_model.casualties_.addStored_Unit(unit);
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
    }

    if ( IsMineralField( unit ) ) { // If the unit is a mineral field we have to detach all those poor workers.

        // Check for miners who may have been digging at that patch.
        for ( auto potential_miner = friendly_player_model.units_.unit_map_.begin(); potential_miner != friendly_player_model.units_.unit_map_.end() && !friendly_player_model.units_.unit_map_.empty(); potential_miner++ ) {

            if (potential_miner->second.locked_mine_ == unit) {
                Unit miner_unit = potential_miner->second.bwapi_unit_;

                bool was_clearing = potential_miner->second.isAssignedClearing(); // Was the mine being cleared with intent?

                // Do NOT tell the miner to stop here. He will end with a mineral in his "mouth" and not return it to base!
                friendly_player_model.units_.purgeWorkerRelationsNoStop(miner_unit); // reset the worker
                if ( was_clearing ) {

                    auto found_mineral_ptr = land_inventory.resource_inventory_.find(unit); // erase the now-gone mine.
                    if (found_mineral_ptr != land_inventory.resource_inventory_.end()) {
                        land_inventory.resource_inventory_.erase(unit); //Clear that mine from the resource inventory.
                    }

                    workermanager.assignClear(miner_unit); // reassign clearing workers again.
                    if (potential_miner->second.isAssignedClearing()) {
                        workermanager.updateWorkersClearing();
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
        }

    }

    if ( unit->getPlayer()->isNeutral() && !IsMineralField(unit) ) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = neutral_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            neutral_player_model.units_.unit_map_.erase(unit);
            //CUNYAIModule::DiagnosticText( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
        else {
            //CUNYAIModule::DiagnosticText( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.unit_inventory_.size() );
        }
    }

    if (unit->getPlayer() == Broodwar->self()) {
        if (unit->getType().isWorker()) {
            friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
        }

        if (!buildorder.ever_clear_) {
            if (unit->getType() == UnitTypes::Zerg_Overlord) { // overlords do not restart the build order.
                buildorder.building_gene_.insert(buildorder.building_gene_.begin(), Build_Order_Object(UnitTypes::Zerg_Overlord));
            }
            else if ( unit->getOrder() == Orders::ZergBuildingMorph && unit->isMorphing() ) {
                //buildorder.clearRemainingBuildOrder( false );
            }
        }
    }

    BWEB::Map::onUnitDestroy(unit);

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
        friendly_player_model.units_.purgeWorkerRelationsStop(unit);
    }

    if ( unit->getType() == UnitTypes::Zerg_Egg || unit->getType() == UnitTypes::Zerg_Cocoon || unit->getType() == UnitTypes::Zerg_Lurker_Egg ) {
        buildorder.updateRemainingBuildOrder(unit->getBuildType()); // Shouldn't be a problem if unit isn't in buildorder.  Don't have to worry about double-built units (lings) since the second one is not morphed as per BWAPI rules.
    }

    if ( unit->getBuildType().isBuilding() ) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
        //buildorder.updateRemainingBuildOrder(unit->getBuildType()); // Should be caught on RESERVATION ONLY, this might double catch them...

        if (unit->getType().whatBuilds().first == UnitTypes::Zerg_Drone) {
            my_reservation.removeReserveSystem(unit->getTilePosition(), unit->getBuildType(), false);
        }
        else {
            buildorder.updateRemainingBuildOrder(unit->getBuildType()); // Upgrading building morphs are not reserved... (ex greater spire)
        }
    }

}

void CUNYAIModule::onUnitRenegade( BWAPI::Unit unit ) // Should be a line-for-line copy of onUnitDestroy.
{
    onUnitDestroy(unit);
    onUnitDiscover(unit);
}

void CUNYAIModule::onSaveGame( std::string gameName )
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void CUNYAIModule::onUnitComplete( BWAPI::Unit unit )
{
}
