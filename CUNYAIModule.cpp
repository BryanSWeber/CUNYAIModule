#pragma once

#include "Source\CUNYAIModule.h"
#include "Source\CobbDouglas.h"
#include "Source\MapInventory.h"
#include "Source\UnitInventory.h"
#include "Source\ResourceInventory.h"
#include "Source\ResearchInventory.h"
#include "Source\LearningManager.h"
#include "Source\AssemblyManager.h"
#include "Source\CombatManager.h"
#include "Source\TechManager.h"
#include "Source\Diagnostics.h"
#include "Source\FAP\FAP\include\FAP.hpp" // could add to include path but this is more explicit.
#include "Source\BWEB\BWEB.h"
#include "Source\BaseManager.h"
#include <bwem.h>
#include <iostream>
#include <fstream> // for file read/writing
#include <numeric> // std::accumulate
#include <algorithm>    // std::min_element, std::max_element
#include <chrono> // for in-game frame clock.
#include <stdio.h>  //for removal of files.
#include <filesystem>


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

double supply_ratio = 0; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth. Created in a ratio ln(supply remaining)/ln(supply used).
double gas_proportion = 0; // for gas levels. Gas is critical for spending and will be mined in a proportion of gas/(gas+min).
double CUNYAIModule::adaptation_rate = 0; //Adaptation rate to opponent.
double CUNYAIModule::alpha_army_original = 0;
double CUNYAIModule::alpha_tech_original = 0;
double CUNYAIModule::alpha_econ_original = 0;
double CUNYAIModule::supply_ratio; // for supply levels.  Supply is an inhibition on growth rather than a resource to spend.  Cost of growth.
double CUNYAIModule::gas_proportion; // for gas levels. Gas is critical for spending but will be matched with supply.
PlayerModel CUNYAIModule::friendly_player_model;
PlayerModel CUNYAIModule::enemy_player_model;
PlayerModel CUNYAIModule::neutral_player_model;
ResourceInventory CUNYAIModule::land_inventory; // resources.
MapInventory CUNYAIModule::currentMapInventory;  // macro variables, not every unit I have.
CombatManager CUNYAIModule::combat_manager;
FAP::FastAPproximation<StoredUnit*> CUNYAIModule::MCfap; // integrating FAP into combat with a produrbation.
TechManager CUNYAIModule::techmanager;
AssemblyManager CUNYAIModule::assemblymanager;
BuildingGene CUNYAIModule::buildorder; //
Reservation CUNYAIModule::my_reservation;
LearningManager CUNYAIModule::learned_plan;
WorkerManager CUNYAIModule::workermanager;
BaseManager CUNYAIModule::basemanager;

void CUNYAIModule::onStart()
{
    Broodwar << "Map initialization..." << std::endl;

    //Initialize BWEM, must be done FIRST.
    bwemMap.Initialize(BWAPI::BroodwarPtr);
    bwemMap.EnableAutomaticPathAnalysis();
    bool startingLocationsOK = bwemMap.FindBasesForStartingLocations();

    BWEB::Map::onStart();
    BWEB::Stations::findStations();
    BWEB::Walls::createZSimCity();
    //assemblymanager.getDefensiveWalls(); //needs work.
    BWEB::Blocks::findBlocks();

    // Hello World!
    Broodwar->sendText("Good luck, have fun!");

    // Print the map name.
    // BWAPI returns std::string when retrieving a string, don't forget to add .c_str() when printing!
    Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

    // Enable the UserInput flag, which allows us to control the bot and type messages. Also needed to get the screen position.
    Broodwar->enableFlag(Flag::UserInput);

    // Uncomment the following line and the bot will know about everything through the fog of war (cheat).
    //Broodwar->enableFlag(Flag::CompleteMapInformation);

    // Set the command optimization level so that common commands can be grouped
    // and reduce the bot's APM (Actions Per Minute).
    Broodwar->setCommandOptimizationLevel(2);

    // Check if this is a replay
    if (Broodwar->isReplay())
    {
        // Announce the players in the replay
        Broodwar << "The following players are in this replay:" << std::endl;

        // Iterate all the players in the game using a std:: iterator
        Playerset players = Broodwar->getPlayers();
        for (auto p : players)
        {
            // Only print the player if they are not an observer
            if (!p->isObserver())
                Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        }

    }
    else // if this is not a replay
    {
        // Retrieve you and your enemy's races. enemy() will just return the first enemy.
        // If you wish to deal with multiple enemies then you must use enemies().
        if (Broodwar->enemy()) // First make sure there is an enemy
            Diagnostics::DiagnosticWrite(string(string("The matchup is ") + string(Broodwar->self()->getRace().c_str()) + string(" vs ") + string(Broodwar->enemy()->getRace().c_str())).c_str()); // this is pretty ugly.
    }

    //Initialize state variables
    gas_starved = false;
    army_starved = false;
    supply_starved = false;
    econ_starved = true;
    tech_starved = false;

    //Initialize model variables.
    learned_plan = LearningManager();
    learned_plan.confirmLearningFilesPresent();

    //if (PY_RF_LEARNING) {
    //    learned_plan.initializeRFLearning();
    //}
    if (GENETIC_HISTORY) {
        learned_plan.initializeGeneticLearning();
    }
    if (RANDOM_PLAN) {
        learned_plan.initializeRandomStart();
    }
    if (TEST_MODE) {
        learned_plan.initializeTestStart();
    }
    if (UNIT_WEIGHTING) {
        learned_plan.initializeGAUnitWeighting(); // in progress.
    }
    //if (PY_UNIT_WEIGHTING) {
    //    learned_plan.initializeCMAESUnitWeighting(); // in progress.
    //}

    gas_proportion = learned_plan.gas_proportion_t0; //gas starved parameter. Triggers state if: gas/(min + gas) < gas_proportion;  Higher is more gas.
    supply_ratio = learned_plan.supply_ratio_t0; //supply starved parameter. Triggers state if: ln_supply_remain/ln_supply_total < supply_ratio; Current best is 0.70. Some good indicators that this is reasonable: ln(4)/ln(9) is around 0.63, ln(3)/ln(9) is around 0.73, so we will build our first overlord at 7/9 supply. ln(18)/ln(100) is also around 0.63, so we will have a nice buffer for midgame.
    //Cobb-Douglas Production exponents.  Can be normalized to sum to 1.
    alpha_army_original = friendly_player_model.spending_model_.alpha_army = learned_plan.a_army_t0; // army starved parameter.
    alpha_econ_original = friendly_player_model.spending_model_.alpha_econ = learned_plan.a_econ_t0; // econ starved parameter.
    alpha_tech_original = friendly_player_model.spending_model_.alpha_tech = learned_plan.a_tech_t0; // tech starved parameter.
    adaptation_rate = learned_plan.r_out_t0; //rate of worker growth.

    buildorder.getInitialBuildOrder(learned_plan.build_order_t0);  //get initial build order.
    Diagnostics::DiagnosticWrite(string("The build order is: " + learned_plan.build_order_t0).c_str());

    //update Map Grids
    currentMapInventory.updateBuildablePos();
    currentMapInventory.updateUnwalkable();
    //inventory.updateSmoothPos();
    currentMapInventory.updateMapVeins();
    //current_MapInventory.updateMapVeinsOut(Position(Broodwar->self()->getStartLocation()) + Position(UnitTypes::Zerg_Hatchery.dimensionLeft(), UnitTypes::Zerg_Hatchery.dimensionUp()), current_MapInventory.front_line_base_, current_MapInventory.map_out_from_home_);
    //inventory.updateMapChokes();

    //update timers.
    short_delay = 0;
    med_delay = 0;
    long_delay = 0;
    my_reservation = Reservation();

    //friendly_player_model.setLockedOpeningValues();

    // Testing Build Order content intenstively.
    ofstream output; // Prints to brood war file while in the WRITE file.
    output.open( (learned_plan.writeDirectory + "BuildOrderFailures.txt").c_str(), ios_base::app);
    string print_value = "";
    print_value += learned_plan.build_order_t0;
    output << "Trying Build Order" << print_value << endl;
    output.close();


    if (RIP_REPLAY) {
        string src = learned_plan.readDirectory + Broodwar->enemy()->getName() + ".txt";
        string dst = learned_plan.writeDirectory + Broodwar->enemy()->getName() + ".txt";
        rename(src.c_str(), dst.c_str());

        src = learned_plan.readDirectory + Broodwar->enemy()->getName() + "casualties" + ".txt";
        dst = learned_plan.writeDirectory + Broodwar->enemy()->getName() + "casualties" + ".txt";
        rename(src.c_str(), dst.c_str());

        src = learned_plan.readDirectory + Broodwar->self()->getName() + ".txt";
        dst = learned_plan.writeDirectory + Broodwar->self()->getName() + ".txt";
        rename(src.c_str(), dst.c_str());

        src = learned_plan.readDirectory + Broodwar->self()->getName() + "casualties" + ".txt";
        dst = learned_plan.writeDirectory + Broodwar->self()->getName() + "casualties" + ".txt";
        rename(src.c_str(), dst.c_str());

        if (std::filesystem::exists(learned_plan.readDirectory))
            Diagnostics::DiagnosticWrite( "We found a READ folder");
        if (std::filesystem::exists(learned_plan.writeDirectory))
            Diagnostics::DiagnosticWrite( "We found a WRITE folder");

    }

}

void CUNYAIModule::onEnd(bool isWinner)
{// Called when the game ends

    ofstream output; // Prints to brood war file while in the WRITE file.
    if (std::filesystem::exists(learned_plan.writeDirectory + "history.txt")) {
        //std::cout << "Writing to history at game end..." << std::endl;
    }
    output.open(learned_plan.writeDirectory + "history.txt", ios_base::app);
    string opponent_name = Broodwar->enemy()->getName().c_str();
    output << gas_proportion << ","
        << supply_ratio << ','
        << alpha_army_original << ','
        << alpha_econ_original << ','
        << alpha_tech_original << ','
        << adaptation_rate << ','
        << CUNYAIModule::safeString(Broodwar->enemy()->getRace().c_str()) << ","
        << isWinner << ','
        << short_delay << ','
        << med_delay << ','
        << long_delay << ','
        << CUNYAIModule::safeString(opponent_name) << ','
        << CUNYAIModule::safeString(Broodwar->mapFileName().c_str()) << ','
        << round(enemy_player_model.getCumArmy() * 1000000) / 1000000 << ','
        << round(enemy_player_model.getCumEco() * 1000000) / 1000000 << ','
        << round(enemy_player_model.getCumTech() * 1000000) / 1000000 << ','
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
        try {
            std::filesystem::copy(learned_plan.writeDirectory, learned_plan.readDirectory, filesystem::copy_options::update_existing | std::filesystem::copy_options::recursive);
            Diagnostics::DiagnosticWrite("Successfully copied from WRITE to READ folder.");
        }
        catch (...) {
            Diagnostics::DiagnosticWrite("Couldn't copy from WRITE to READ folder.");
        }
    }

    if (!buildorder.isEmptyBuildOrder()) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open("..\\write\\BuildOrderFailures.txt", ios_base::app);
        string print_value = "";

        print_value += buildorder.building_gene_.front().getResearch().c_str();
        print_value += buildorder.building_gene_.front().getUnit().c_str();
        print_value += buildorder.building_gene_.front().getUpgrade().c_str();

        output << "Couldn't build: " << print_value << endl;
        output << "Hatches Left?:" << CUNYAIModule::basemanager.getBaseCount() << endl;
        output << "Win:" << isWinner << endl;
        output.close();
    }; // testing build order stuff intensively.

    if (UNIT_WEIGHTING) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open(learned_plan.writeDirectory + "UnitWeights.txt", ios_base::app);
        for (auto uw : learned_plan.unit_weights) {
            output << uw.second << ",";
        }
        output << learned_plan.getOutcomeScore(isWinner, Broodwar->self()->getBuildingScore(), Broodwar->self()->getKillScore(), Broodwar->self()->getRazingScore(),Broodwar->self()->getBuildingScore()) << endl;
        output.close();
    }
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
    //enemy_player_model.units_.drawAllHitPoints(current_MapInventory);
    enemy_player_model.units_.drawAllLocations();
    enemy_player_model.units_.drawAllLastSeens();

    //Update neutral units
    Player* neutral_player;
    for (auto p : Broodwar->getPlayers()) {
        if (p->isNeutral()) neutral_player = &p;
    }
    neutral_player_model.updateOtherOnFrame(*neutral_player);
    Diagnostics::drawAllHitPoints(neutral_player_model.units_);
    neutral_player_model.units_.drawAllLocations();

    friendly_player_model.updateSelfOnFrame(); // So far, mimics the only other enemy player.
    Diagnostics::drawAllSpamGuards(friendly_player_model.units_);

    // Update FAPS with units.
    MCfap.clear();
    enemy_player_model.units_.addToMCFAP(MCfap, false, enemy_player_model.researches_);
    Diagnostics::drawAllFutureDeaths(enemy_player_model.units_);

    friendly_player_model.units_.addToMCFAP(MCfap, true, friendly_player_model.researches_);
    Diagnostics::drawAllFutureDeaths(friendly_player_model.units_);

    // Let us estimate FAP values.
    MCfap.simulate(FAP_SIM_DURATION);
    int friendly_fap_score = getFAPScore(MCfap, true);
    int enemy_fap_score = getFAPScore(MCfap, false);
    friendly_player_model.units_.pullFromFAP(*MCfap.getState().first);
    enemy_player_model.units_.pullFromFAP(*MCfap.getState().second);

    writePlayerModel(friendly_player_model, "friendly");
    writePlayerModel(enemy_player_model, "enemy");

    buildorder.getCumulativeResources();
    //Knee-jerk states: gas, supply.
    gas_starved = (workermanager.checkGasOutlet() && workermanager.getMinWorkers() > workermanager.getGasWorkers() //You must have more mineral gatherers than gas miners, otherwise you are simply eco starved.
        && (currentMapInventory.getGasRatio() < gas_proportion || Broodwar->self()->gas() < max({ CUNYAIModule::assemblymanager.getMaxGas(), CUNYAIModule::techmanager.getMaxGas()}))) || // you cannot buy something because of gas.
        (!buildorder.building_gene_.empty() && (my_reservation.getExcessGas() <= 0 || buildorder.cumulative_gas_ >= Broodwar->self()->gas()));// you need gas for a required build order item.

    supply_starved = (currentMapInventory.getLn_Supply_Ratio() < supply_ratio  &&   //If your supply is disproportionately low, then you are supply starved, unless
        Broodwar->self()->supplyTotal() < 399); // you have hit your supply limit, in which case you are not supply blocked. The real supply goes from 0-400, since lings are 0.5 observable supply.

    bool massive_army = friendly_player_model.spending_model_.army_derivative == 0 || (friendly_player_model.units_.stock_fighting_total_ - Stock_Units(UnitTypes::Zerg_Sunken_Colony, friendly_player_model.units_) - Stock_Units(UnitTypes::Zerg_Spore_Colony, friendly_player_model.units_) >= enemy_player_model.units_.stock_fighting_total_ * 3);

    combat_manager.updateMacroCombatReadiness();

    auto end_playermodel = std::chrono::high_resolution_clock::now();
    playermodel_time = end_playermodel - start_playermodel;


    auto start_map = std::chrono::high_resolution_clock::now();

    //Update posessed minerals. Erase those that are mined out.
    land_inventory.updateResourceInventory(friendly_player_model.units_, enemy_player_model.units_, currentMapInventory);
    land_inventory.drawMineralRemaining();

    //Update important variables.  Enemy stock has a lot of dependencies, updated above.
    currentMapInventory.updateVision_Count();

    workermanager.updateGas_Workers();
    workermanager.updateMin_Workers();
    workermanager.updateWorkersClearing();
    workermanager.updateWorkersLongDistanceMining();
    workermanager.updateWorkersOverstacked();
    workermanager.updateExcessCapacity();

    currentMapInventory.my_portion_of_the_map_ = CUNYAIModule::convertTileDistanceToPixelDistance( sqrt(pow(Broodwar->mapHeight(), 2) + pow(Broodwar->mapWidth(), 2)) / static_cast<double>(Broodwar->getStartLocations().size()) );
    currentMapInventory.expo_portion_of_the_map_ = CUNYAIModule::convertTileDistanceToPixelDistance( sqrt(pow(Broodwar->mapHeight(), 2) + pow(Broodwar->mapWidth(), 2)) / static_cast<double>(currentMapInventory.getExpoTilePositions().size()) );
    currentMapInventory.updateScreen_Position();
    currentMapInventory.mainCurrentMap();
    //currentMapInventory.createAirThreatField(enemy_player_model);
    //currentMapInventory.createGroundThreatField(enemy_player_model);
    currentMapInventory.createDetectField(enemy_player_model);
    //currentMapInventory.createVisionField(enemy_player_model);
    //currentMapInventory.createBlindField(enemy_player_model);
    currentMapInventory.createThreatField(enemy_player_model);
    currentMapInventory.createThreatBufferField(enemy_player_model);
    currentMapInventory.createExtraWideBufferField(enemy_player_model);
    currentMapInventory.createOccupationField();
    currentMapInventory.createSurroundField(enemy_player_model);
    //currentMapInventory.DiagnosticSurroundTiles();
    currentMapInventory.DiagnosticThreatTiles();

    basemanager.updateBases();

    if (t_game == 0) {
        //update local resources
        //current_MapInventory.updateMapVeinsOut(current_MapInventory.start_positions_[0], current_MapInventory.enemy_base_ground_, current_MapInventory.map_out_from_enemy_ground_);
        ResourceInventory mineral_inventory = ResourceInventory(Broodwar->getStaticMinerals());
        ResourceInventory geyser_inventory = ResourceInventory(Broodwar->getStaticGeysers());
        land_inventory = mineral_inventory + geyser_inventory; // for first initialization.
        currentMapInventory.getExpoTilePositions(); // prime this once on game start.


        if (INF_MONEY) {
            Broodwar->sendText("show me the money");
        }
        if (MAP_REVEAL) {
            Broodwar->sendText("black sheep wall");
        }
        if (NEVER_DIE) {
            Broodwar->sendText("power overwhelming");
        }
        if (INSTANT_WIN) {
            Broodwar->sendText("there is no cow level");
        }

        if (!(INF_MONEY || MAP_REVEAL || NEVER_DIE || INSTANT_WIN)) {
            Broodwar->sendText("Cough Cough: Power Overwhelming! (Please work!)");
        }

        for (auto i : CUNYAIModule::friendly_player_model.getBuildingCartridge())
            Diagnostics::DiagnosticWrite("Our Legal Buildings are: %s", i.first.c_str());

        for (auto i : CUNYAIModule::friendly_player_model.getCombatUnitCartridge())
            Diagnostics::DiagnosticWrite("Our Legal combatants are: %s", i.first.c_str());

        for (auto i : CUNYAIModule::friendly_player_model.getTechCartridge())
            Diagnostics::DiagnosticWrite("Our techs are: %s", i.first.c_str());

        for (auto i : CUNYAIModule::friendly_player_model.getUpgradeCartridge())
            Diagnostics::DiagnosticWrite("Our upgrades are: %s", i.first.c_str());
    }

    if (t_game % (24 * 60) == 0 && RIP_REPLAY) {
        friendly_player_model.units_.printUnitInventory(Broodwar->self());
        friendly_player_model.casualties_.printUnitInventory(Broodwar->self(), "casualties");
        enemy_player_model.units_.printUnitInventory(Broodwar->enemy());
        enemy_player_model.casualties_.printUnitInventory(Broodwar->enemy(), "casualties");
    }

    techmanager.updateCanMakeTechExpenditures();
    techmanager.updateOptimalTech();

    if(army_starved || CUNYAIModule::my_reservation.canReserveWithExcessResource(UnitTypes::Zerg_Zergling))
        assemblymanager.updateOptimalCombatUnit();
    assemblymanager.updatePotentialBuilders();


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

        bool reserved_extractor = false;
        bool no_extractor = countUnits(UnitTypes::Zerg_Extractor) == 0;
        for (auto r : CUNYAIModule::my_reservation.getReservedBuildings()) {
            reserved_extractor = r.second == UnitTypes::Zerg_Extractor || reserved_extractor;
        }
        if (need_gas_now && no_extractor && !reserved_extractor) {
            buildorder.clearRemainingBuildOrder(false);
            Diagnostics::DiagnosticWrite("Uh oh, something's went wrong with building an extractor!");
        }
    }

    my_reservation.confirmOngoingReservations();
    Diagnostics::drawReservations(my_reservation, currentMapInventory.screen_position_);

    vector<UnitType> types_of_units_checked_for_upgrades_this_frame = {};// starts empty.

    //Vision inventory: Map area could be initialized on startup, since maps do not vary once made.
    int map_x = Broodwar->mapWidth();
    int map_y = Broodwar->mapHeight();
    int map_area = map_x * map_y; // map area in tiles.

   // if (Broodwar->mapWidth() && Broodwar->mapHeight()) {
    //current_MapInventory.createThreatField(enemy_player_model);
    //current_MapInventory.createAttractField(enemy_player_model);
    //current_MapInventory.createExploreField();
    //current_MapInventory.createAAField(enemy_player_model);
    // }

     //current_MapInventory.DiagnosticField(current_MapInventory.pf_explore_);
     //current_MapInventory.DiagnosticTile();


    auto end_map = std::chrono::high_resolution_clock::now();
    map_time = end_map - start_map;

    // Display the game status indicators at the top of the screen
    if constexpr (DIAGNOSTIC_MODE) {
        Diagnostics::onFrame();
    }// close analysis mode



    // Prevent spamming by only running our onFrame once every number of latency frames.
    // Latency frames are the number of frames before commands are processed.
    if (t_game % Broodwar->getLatencyFrames() != 0) {
        return;
    }

    // Assemble units when needed.
    auto start_unit_morphs = std::chrono::high_resolution_clock::now();
        assemblymanager.assignAssemblyRole();
        assemblymanager.morphReservedUnits();
        assemblymanager.updateExpoPosition();
        Diagnostics::drawExpo();
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
            UnitInventory e_neighbors = getUnitInventoryInRadius(enemy_player_model.units_, u->getPosition(), u_type.sightRange());
            for (auto e = e_neighbors.unit_map_.begin(); e != e_neighbors.unit_map_.end() && !e_neighbors.unit_map_.empty(); e++) {
                if ((*e).second.type_.isCloakable() || (*e).second.type_ == UnitTypes::Zerg_Lurker || (*e).second.type_.hasPermanentCloak() || ((*e).second.type_.isBurrowable() && CUNYAIModule::enemy_player_model.researches_.hasTech(TechTypes::Burrowing))) {
                    c = (*e).second.pos_; // then we may to send in some vision.
                    call_detector = true;
                    break;
                } //some units, DT, Observers, are not cloakable. They are cloaked though. Recall burrow and cloak are different.
            }
            if (call_detector) {
                int dist = 999999;
                int dist_temp = 0;
                bool detector_found = false;
                StoredUnit detector_of_choice;
                for (auto d : friendly_player_model.units_.unit_map_) {
                    if (d.second.type_ == UnitTypes::Zerg_Overlord &&
                        d.second.bwapi_unit_ &&
                        !static_cast<bool>(d.second.time_since_last_dmg_ < FAP_SIM_DURATION) &&
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
                    double theta = atan2(detector_of_choice.pos_.y - c.y, detector_of_choice.pos_.x - c.x);
                    Position closest_loc_to_c_that_gives_vision = Position(c.x + static_cast<int>(cos(theta) * 0.75) * detector_of_choice.type_.sightRange(), c.y + static_cast<int>(sin(theta) * 0.75) * detector_of_choice.type_.sightRange());
                    if (closest_loc_to_c_that_gives_vision.isValid() && closest_loc_to_c_that_gives_vision != Positions::Origin) {
                        detector_of_choice.bwapi_unit_->move(closest_loc_to_c_that_gives_vision);
                        Diagnostics::drawCircle(c, CUNYAIModule::currentMapInventory.screen_position_, 25, Colors::Cyan);
                        Diagnostics::drawLine(detector_of_choice.pos_, closest_loc_to_c_that_gives_vision, currentMapInventory.screen_position_, Colors::Cyan);
                        CUNYAIModule::updateUnitPhase(detector_of_choice.bwapi_unit_, StoredUnit::Phase::Detecting); // Update the detector not the calling unit.
                    }
                    else {
                        detector_of_choice.bwapi_unit_->move(c);
                        Diagnostics::drawCircle(c, CUNYAIModule::currentMapInventory.screen_position_, 25, Colors::Cyan);
                        Diagnostics::drawLine(detector_of_choice.pos_, currentMapInventory.screen_position_, c, Colors::Cyan);
                        CUNYAIModule::updateUnitPhase(detector_of_choice.bwapi_unit_, StoredUnit::Phase::Detecting);  // Update the detector not the calling unit.
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
        if (u->getType().isWorker()) workermanager.workerWork(u);
        auto end_worker = std::chrono::high_resolution_clock::now();

        detector_time += end_detector - start_detector;
        worker_time += end_worker - start_worker;
        combat_time += end_combat - start_combat;
    } // closure: unit iterator

    //Upgrade loop- happens AFTER buildings.
    for (auto &u : Broodwar->self()->getUnits())
    {
        if (!checkUnitTouchable(u)) continue; // can we mess with it at all?
        UnitType u_type = u->getType();
        auto start_upgrade = std::chrono::high_resolution_clock::now();
        bool unconsidered_unit_type = std::find(types_of_units_checked_for_upgrades_this_frame.begin(), types_of_units_checked_for_upgrades_this_frame.end(), u_type) == types_of_units_checked_for_upgrades_this_frame.end();
        //Upgrades only occur on a specific subtype of units.
        if (isIdleEmpty(u) && !u_type.canAttack() && unconsidered_unit_type && spamGuard(u) && (u->canUpgrade() || u->canResearch() || u->canMorph())) { // this will need to be revaluated once I buy units that cost gas.
            techmanager.tryToTech(u, friendly_player_model.units_, currentMapInventory);
            types_of_units_checked_for_upgrades_this_frame.push_back(u_type); // only check each type once.
            //PrintError_Unit( u );
        }
        auto end_upgrade = std::chrono::high_resolution_clock::now();
        upgrade_time += end_upgrade - start_upgrade;
    }

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
        if ((short_delay > 320 || med_delay > 10 || long_delay > 1 || Broodwar->elapsedTime() > 90 * 60 || countUnits(UnitTypes::Zerg_Drone, friendly_player_model.units_) == 0)) //if game times out or lags out, end game with resignation.
        {
            Broodwar->leaveGame();
        }
    }

    if constexpr (DIAGNOSTIC_MODE) {
        int n;
        n = sprintf_s(delay_string, "Delays:{S:%d,M:%d,L:%d}%3.fms", short_delay, med_delay, long_delay, total_frame_time.count());
        n = sprintf_s(playermodel_string, "Players:       %3.f%%,%3.fms ", playermodel_time.count() / static_cast<double>(total_frame_time.count()) * 100, playermodel_time.count());
        n = sprintf_s(map_string, "Maps:          %3.f%%,%3.fms ", map_time.count() / static_cast<double>(total_frame_time.count()) * 100, map_time.count());
        n = sprintf_s(larva_string, "Larva:         %3.f%%,%3.fms", larva_time.count() / static_cast<double>(total_frame_time.count()) * 100, larva_time.count());
        n = sprintf_s(worker_string, "Workers:       %3.f%%,%3.fms", worker_time.count() / static_cast<double>(total_frame_time.count()) * 100, worker_time.count());
        n = sprintf_s(scouting_string, "Scouting:      %3.f%%,%3.fms", scout_time.count() / static_cast<double>(total_frame_time.count()) * 100, scout_time.count());
        n = sprintf_s(combat_string, "Combat:        %3.f%%,%3.fms", combat_time.count() / static_cast<double>(total_frame_time.count()) * 100, combat_time.count());
        n = sprintf_s(detection_string, "Detection:     %3.f%%,%3.fms", detector_time.count() / static_cast<double>(total_frame_time.count()) * 100, detector_time.count());
        n = sprintf_s(upgrade_string, "Upgrades:      %3.f%%,%3.fms", upgrade_time.count() / static_cast<double>(total_frame_time.count()) * 100, upgrade_time.count());
    }

    //if (buildorder.isEmptyBuildOrder())  Broodwar->leaveGame(); // Test Opening Game intensively.

} // closure: Onframe

void CUNYAIModule::onSendText(std::string text)
{
    if (text == "/show bullets")
    {
        Diagnostics::drawBullets();
    }
    else if (text == "/show players")
    {
        Diagnostics::showPlayers();
    }
    else if (text == "/show forces")
    {
        Diagnostics::showForces();
    }
    else if (text == "/show visibility")
    {
        Diagnostics::drawVisibilityData();
    }
}

void CUNYAIModule::onReceiveText(BWAPI::Player player, std::string text)
{
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void CUNYAIModule::onPlayerLeft(BWAPI::Player player)
{
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText("That was a good game. I'll remember this! %s!", player->getName().c_str());
}

void CUNYAIModule::onNukeDetect(BWAPI::Position target)
{
    // Check if the target is a valid position
    if (target)
    {
        // if so, print the location of the nuclear strike target
        Broodwar << "Have you no shame? My sources say there's nuclear launch at " << target << std::endl;
    }
    else
    {
        // Otherwise, ask other players where the nuke is!
        Broodwar->sendText("Where's the nuke?");
    }

    // You can also retrieve all the nuclear missile targets using Broodwar->getNukeDots()!
}

void CUNYAIModule::onUnitDiscover(BWAPI::Unit unit)
{
    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if (unit->getPlayer()->isEnemy(Broodwar->self()) && !unit->isInvincible()) { // safety check.
                                                                                             //Diagnostics::DiagnosticWrite( "I just gained vision of a %s", unit->getType().c_str() );
        StoredUnit eu = StoredUnit(unit);

        if (enemy_player_model.units_.unit_map_.insert({ unit, eu }).second) { // if the insertion succeeded
                                                                               //Diagnostics::DiagnosticWrite( "A %s just was discovered. Added to unit inventory, size %d", eu.type_.c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
        else { // the insertion must have failed
               //Diagnostics::DiagnosticWrite( "%s is already at address %p.", eu.type_.c_str(), enemy_player_model.units_.UnitInventory_.find( unit ) ) ;
        }

        if (unit->getType().isBuilding() && unit->getPlayer()->getRace() == Races::Zerg) {
            enemy_player_model.decrementUnseenWorkers();
        }

        enemy_player_model.imputeUnits(unit);

    }

    if (unit->getPlayer()->isNeutral() && !unit->isInvincible()) { // safety check.
                                                                                 //Diagnostics::DiagnosticWrite( "I just gained vision of a %s", unit->getType().c_str() );
        StoredUnit nu = StoredUnit(unit);
        neutral_player_model.units_.addStoredUnit(nu);

    }

    if (unit->getPlayer()->isNeutral() && unit->getType().isResourceContainer()) { // safety check.
        Stored_Resource* ru = &Stored_Resource(unit);
        ru->max_stock_value_ = ru->current_stock_value_; // its value is what it has now, since it was somehow missing at game start. Must be passed by refrence or it will be forgotten.
        land_inventory.addStored_Resource(*ru);
    }


    BWEB::Map::onUnitDiscover(unit);

}

void CUNYAIModule::onUnitEvade(BWAPI::Unit unit)
{

}

void CUNYAIModule::onUnitShow(BWAPI::Unit unit)
{

}

void CUNYAIModule::onUnitHide(BWAPI::Unit unit)
{


}

void CUNYAIModule::onUnitCreate(BWAPI::Unit unit)
{

    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if (CUNYAIModule::isFightingUnit(unit)) {
        techmanager.clearSimulationHistory();
        assemblymanager.clearSimulationHistory();
    }

    if (Broodwar->isReplay())
    {
        // if we are in a replay, then we will print out the build order of the structures
        if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Diagnostics::DiagnosticWrite("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
        }
    }

    if (unit->getType().isWorker()) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
    }

    Diagnostics::DiagnosticWrite("Created a %s.", unit->getType().c_str());

}

void CUNYAIModule::onUnitDestroy(BWAPI::Unit unit) // something mods Unit to 0xf inside here!
{

    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if (CUNYAIModule::isFightingUnit(unit)) {
        techmanager.clearSimulationHistory();
        assemblymanager.clearSimulationHistory();
    }

    if (unit->getPlayer() == Broodwar->self()) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.

        if (unit->getType().isWorker()) {
            friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
        }

        if (!buildorder.isEmptyBuildOrder()) {
            if (unit->getType() == UnitTypes::Zerg_Overlord) { // overlords do not restart the build order.
                buildorder.retryBuildOrderElement(UnitTypes::Zerg_Overlord);
            }
            else if (unit->getType() == UnitTypes::Zerg_Drone && unit->getLastCommand().getUnitType() != UnitTypes::Zerg_Extractor) { // The extractor needs to be put seperately because BW-specific unit transitions. Drones making extractors die and the geyser morphs into the extractor.
                buildorder.clearRemainingBuildOrder( false );
                Diagnostics::DiagnosticWrite("Uh oh! A drone has died and this means we need to ditch our build order!");
            }
        }

        auto found_ptr = friendly_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            friendly_player_model.units_.unit_map_.erase(unit);
            friendly_player_model.casualties_.addStoredUnit(unit);
            //Diagnostics::DiagnosticWrite( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
        else {
            //Diagnostics::DiagnosticWrite( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }

        combat_manager.removeScout(unit);
        combat_manager.removeLiablity(unit);
    }

    if (unit->getPlayer()->isEnemy(Broodwar->self())) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead.
        auto found_ptr = enemy_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            if (found_ptr->type_.isWorker()) enemy_player_model.decrementUnseenWorkers();
            enemy_player_model.units_.unit_map_.erase(unit);
            enemy_player_model.casualties_.addStoredUnit(unit);
            //Diagnostics::DiagnosticWrite( "Killed a %s, inventory is now size %d.", found_ptr->second.type_.c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
        else {
            //Diagnostics::DiagnosticWrite( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
    }

    if (IsMineralField(unit)) { // If the unit is a mineral field we have to detach all those poor workers.

        // Check for miners who may have been digging at that patch.
        for (auto potential_miner = friendly_player_model.units_.unit_map_.begin(); potential_miner != friendly_player_model.units_.unit_map_.end() && !friendly_player_model.units_.unit_map_.empty(); potential_miner++) {

            if (potential_miner->second.locked_mine_ == unit) {
                Unit miner_unit = potential_miner->second.bwapi_unit_;

                bool was_clearing = potential_miner->second.isAssignedClearing(); // Was the mine being cleared with intent?

                // Do NOT tell the miner to stop here. He will end with a mineral in his "mouth" and not return it to base!
                friendly_player_model.units_.purgeWorkerRelationsNoStop(miner_unit); // reset the worker
                if (was_clearing) {

                    auto found_mineral_ptr = land_inventory.ResourceInventory_.find(unit); // erase the now-gone mine.
                    if (found_mineral_ptr != land_inventory.ResourceInventory_.end()) {
                        land_inventory.ResourceInventory_.erase(unit); //Clear that mine from the resource inventory.
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

        Diagnostics::DiagnosticWrite("A mine is dead!");


        // clear it just in case.
        auto found_mineral_ptr = land_inventory.ResourceInventory_.find(unit);
        if (found_mineral_ptr != land_inventory.ResourceInventory_.end()) {
            land_inventory.ResourceInventory_.erase(unit); //Clear that mine from the resource inventory.
        }

    }

    if (unit->getPlayer()->isNeutral() && !IsMineralField(unit)) { // safety check for existence doesn't work here, the unit doesn't exist, it's dead..
        auto found_ptr = neutral_player_model.units_.getStoredUnit(unit);
        if (found_ptr) {
            neutral_player_model.units_.unit_map_.erase(unit);
            //Diagnostics::DiagnosticWrite( "Killed a %s, inventory is now size %d.", eu.type_.c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
        else {
            //Diagnostics::DiagnosticWrite( "Killed a %s. But it wasn't in inventory, size %d.", unit->getType().c_str(), enemy_player_model.units_.UnitInventory_.size() );
        }
    }

    BWEB::Map::onUnitDestroy(unit);

}

void CUNYAIModule::onUnitMorph(BWAPI::Unit unit)
{

    if (!unit) {
        return; // safety catch for nullptr dead units. Sometimes is passed.
    }

    if (CUNYAIModule::isFightingUnit(unit)) {
        techmanager.clearSimulationHistory();
        assemblymanager.clearSimulationHistory();
    }

    BWEB::Map::onUnitMorph(unit);

    if (Broodwar->isReplay())
    {
        // if we are in a replay, then we will print out the build order of the structures
        if (unit->getType().isBuilding() &&
            !unit->getPlayer()->isNeutral())
        {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
        }
    }

    if (unit->getType().isWorker()) {
        friendly_player_model.units_.purgeWorkerRelationsStop(unit);
    }

    //if (unit->getType() == UnitTypes::Zerg_Egg || unit->getType() == UnitTypes::Zerg_Cocoon || unit->getType() == UnitTypes::Zerg_Lurker_Egg) {
    //    buildorder.updateRemainingBuildOrder(unit->getBuildType()); // Shouldn't be a problem if unit isn't in buildorder.  Don't have to worry about double-built units (lings) since the second one is not morphed as per BWAPI rules.
    //}

    Diagnostics::DiagnosticWrite("Morphing a %s.", unit->getType().c_str());


    if (unit->getBuildType().isBuilding()) {
        friendly_player_model.units_.purgeWorkerRelationsNoStop(unit);
        my_reservation.removeReserveSystem(unit->getTilePosition(), unit->getBuildType(), false);
    }
}

void CUNYAIModule::onUnitRenegade(BWAPI::Unit unit) // Should be a line-for-line copy of onUnitDestroy.
{
    onUnitDestroy(unit);
    onUnitDiscover(unit);
}

void CUNYAIModule::onSaveGame(std::string gameName)
{
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void CUNYAIModule::onUnitComplete(BWAPI::Unit unit)
{
}
