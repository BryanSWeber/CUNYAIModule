#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include "Source\LearningManager.h"
# include "Source\Diagnostics.h"
# include "Source\Build.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>
# include <algorithm>
# include <set>
# include <random> // C++ base random is low quality.
# include <thread>
# include <filesystem>
# include <cmath>


using namespace BWAPI;
using namespace Filter;
using namespace std;


void LearningManager::onStart()
{
    // File extension including our race initial;
    myRaceChar = CUNYAIModule::safeString(Broodwar->self()->getRace().c_str());
    enemyRaceChar = CUNYAIModule::safeString(Broodwar->enemy()->getRace().c_str());
    versionChar = "2022.6.24";
    noStats = " 0 0 ";
    learningExtension = myRaceChar + "v" + enemyRaceChar + " " + Broodwar->enemy()->getName() + " " + versionChar + ".csv";
    gameInfoExtension = myRaceChar + "v" + enemyRaceChar + " " + Broodwar->enemy()->getName() + " " + versionChar + " Info.csv";

    definePremadeBuildOrders();
    Diagnostics::DiagnosticText("Build Orders Defined");
    selectDefaultBuild();
    Diagnostics::DiagnosticText("Selected Build");
    parseLearningFile();
    Diagnostics::DiagnosticText("Parsed LearningFile, size = %d", gameHistory_.size());
    selectBestBuild();
    Diagnostics::DiagnosticText("Trying Build Order: %s", getBuildNameString(currentBuild_.getBuildEnum()).c_str() );
}

void LearningManager::onEnd(bool isWinner)
{
    ifstream readFile(getReadDir() + gameInfoExtension);
    if (readFile)
        copyFile(getReadDir() + gameInfoExtension, getWriteDir() + gameInfoExtension);
    ofstream gameLog(getWriteDir() + gameInfoExtension, std::ios_base::app);
    //gameLog << std::setfill('0') << Strategy::getEnemyBuildTime().minutes << ":" << std::setw(2) << Strategy::getEnemyBuildTime().seconds << ",";

    // Print all relevant model and game characteristics.
    gameLog << getBuildNameString(currentBuild_.getBuildEnum()) << ','
        << CUNYAIModule::safeString(Broodwar->mapFileName().c_str()) << ','
        << isWinner << ','
        << CUNYAIModule::short_delay << ','
        << CUNYAIModule::med_delay << ','
        << CUNYAIModule::long_delay << ','
        << Broodwar->self()->getBuildingScore() << ','
        << Broodwar->self()->getKillScore() << ','
        << Broodwar->self()->getRazingScore() << ','
        << Broodwar->self()->getUnitScore() << ','
        << CUNYAIModule::enemy_player_model.getFirstAirSeen() << ','
        << CUNYAIModule::enemy_player_model.getFirstDetectorSeen() << ','
        << Broodwar->elapsedTime() << ','
        << endl;

    //If your Build Order didn't work and the bot is broken, let's write that specifically.
    if (!currentBuild_.isEmptyBuildOrder()) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open("..\\write\\BuildOrderFailures.txt", ios_base::app);
        string print_value = "";

        print_value += currentBuild_.getNext().getResearch().c_str();
        print_value += currentBuild_.getNext().getUnit().c_str();
        print_value += currentBuild_.getNext().getUpgrade().c_str();

        output << "Couldn't build: " << print_value << endl;
        output << "Hatches Left?:" << CUNYAIModule::basemanager.getBaseCount() << endl;
        output << "Win:" << isWinner << endl;
        output.close();
    };

    // If training in some enviorments, I need to manually move the read to write to mimic tournament play.
    if constexpr (MOVE_OUTPUT_BACK_TO_READ) {
        ifstream readFile(getWriteDir() + gameInfoExtension);
        if (readFile)
            copyFile(getWriteDir() + gameInfoExtension, getReadDir() + gameInfoExtension);
    }
}

void LearningManager::definePremadeBuildOrders()
{
    // 12H muta build order!
    vector<BuildOrderElement> MutaList = { BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Hatchery),
       BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
       BuildOrderElement(UnitTypes::Zerg_Extractor),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Lair),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UpgradeTypes::Metabolic_Boost),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Spire),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Extractor),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Creep_Colony),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Creep_Colony),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Sunken_Colony),
       BuildOrderElement(UnitTypes::Zerg_Sunken_Colony),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk),
       BuildOrderElement(UnitTypes::Zerg_Mutalisk)
    };

    double mutaParams[6] = { 0.517767817, 1.238421617 , 0.48223217, 0.439303835, 0.717060969, 0.373843463 };

    // 12pool Lurker
    vector<BuildOrderElement> lurkerList = { BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Overlord),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
        BuildOrderElement(UnitTypes::Zerg_Extractor),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Hatchery),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Lair),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UpgradeTypes::Metabolic_Boost),
        BuildOrderElement(UnitTypes::Zerg_Hydralisk_Den),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(TechTypes::Lurker_Aspect),
        BuildOrderElement(UnitTypes::Zerg_Overlord),
        BuildOrderElement(UnitTypes::Zerg_Hydralisk),
        BuildOrderElement(UnitTypes::Zerg_Hydralisk),
        BuildOrderElement(UnitTypes::Zerg_Hydralisk),
        BuildOrderElement(UnitTypes::Zerg_Hydralisk),
        BuildOrderElement(UnitTypes::Zerg_Extractor),
        BuildOrderElement(UnitTypes::Zerg_Lurker),
        BuildOrderElement(UnitTypes::Zerg_Lurker),
        BuildOrderElement(UnitTypes::Zerg_Lurker),
        BuildOrderElement(UnitTypes::Zerg_Lurker)
    };
    double lurkerParams[6] = { 0.460984384, 1.217642114, 0.539015569, 0.543615491, 0.73221256, 0.432943104 };

    // 5pool ling
    vector<BuildOrderElement> fivePoolList = { BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Drone),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Overlord),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling),
        BuildOrderElement(UnitTypes::Zerg_Zergling)
    };

    double fivePoolParams[6] = { 0.464593318, 1.145650349, 0.535406679, 0.409437865, 0.669303145, 0.428709651 };

   // // 3 Hatch Muta: https://liquipedia.net/starcraft/3_Hatch_Muta_(vs._Terran) In progress.
   // vector<BuildOrderElement> threeHatchMutaList = { BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Overlord),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Hatchery),
   //    BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Hatchery),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Drone),
   //    BuildOrderElement(UnitTypes::Zerg_Extractor)
   //}
   // double threeHatchMutaParams[6] = { 0.517767817, 1.238421617 , 0.48223217, 0.439303835, 0.717060969, 0.373843463 };

   // 9 Pool Spire
    vector<BuildOrderElement> OneBaseSpireList = { BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
    BuildOrderElement(UnitTypes::Zerg_Creep_Colony),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Extractor),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Sunken_Colony),
    BuildOrderElement(UnitTypes::Zerg_Overlord),
    BuildOrderElement(UnitTypes::Zerg_Zergling),
    BuildOrderElement(UnitTypes::Zerg_Zergling),
    BuildOrderElement(UnitTypes::Zerg_Zergling), //Each ling is "double counted" in BASIL
    BuildOrderElement(UnitTypes::Zerg_Lair),
    BuildOrderElement(UnitTypes::Zerg_Zergling),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Drone),
    BuildOrderElement(UnitTypes::Zerg_Spire),
    BuildOrderElement(UnitTypes::Zerg_Overlord),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk),
    BuildOrderElement(UnitTypes::Zerg_Mutalisk)
    };
    double OneBaseSpireParams[6] = { 0.517767817, 1.238421617 , 0.48223217, 0.439303835, 0.717060969, 0.373843463 };

    // 4H Macro Before Gas https://liquipedia.net/starcraft/4_Hatch_before_Gas_(vs._Protoss)
    vector<BuildOrderElement> FourHatchList = { BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Overlord),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Hatchery),
       BuildOrderElement(UnitTypes::Zerg_Spawning_Pool),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Hatchery),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Hatchery), // 16–18 — Hatchery @ Natural or 3rd Base. MacroHatch.
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Extractor),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Evolution_Chamber), //100% Extractor — Evolution Chamber (See note)
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone), 
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UpgradeTypes::Zerg_Carapace), // @100% Evolution Chamber — +1 Zerg Carapace, (see note)
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Hatchery), // @300 Minerals — Hatchery (preferably used to narrow a chokepoint)
       BuildOrderElement(UnitTypes::Zerg_Lair), // @100 Gas — Lair
       BuildOrderElement(UpgradeTypes::Metabolic_Boost), // @100 Gas — Metabolic Boost
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Zergling),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Drone),
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Overlord), //Not listed in BO, difficult to time.
       BuildOrderElement(UnitTypes::Zerg_Spire)
    };

    double FourHatchParams[6] = { 0.458350597, 1.293531827 , 0.541649398, 0.33578132, 0.697611437, 0.319556148 };
    
    //Hardcoded build orders below.
    BuildOrderSetup MutaSetup = BuildOrderSetup(MutaList, mutaParams, BuildEnums::TwoBaseMuta);
    BuildOrderSetup OneBaseSpireSetup = BuildOrderSetup(OneBaseSpireList, OneBaseSpireParams, BuildEnums::OneBaseSpire);
    BuildOrderSetup LurkerSetup = BuildOrderSetup(lurkerList, lurkerParams, BuildEnums::Lurker);
    BuildOrderSetup fivePoolSetup = BuildOrderSetup(fivePoolList, fivePoolParams, BuildEnums::FivePool);
    BuildOrderSetup FourHatchSetup = BuildOrderSetup(FourHatchList, FourHatchParams, BuildEnums::FourHatch);

    myBuilds_.push_back(MutaSetup);
    myBuilds_.push_back(OneBaseSpireSetup);
    myBuilds_.push_back(LurkerSetup);
    myBuilds_.push_back(fivePoolSetup);
    myBuilds_.push_back(FourHatchSetup);
}

void LearningManager::parseLearningFile()
{
    //Required Elements
    gameHistory_.clear();
    int nRows = countLines(getReadDir() + gameInfoExtension);

    //Grab File
    ifstream myFile;
    myFile.open(getReadDir() + gameInfoExtension);
    if (!myFile.is_open()) {
        Diagnostics::DiagnosticText("No file to read in to ParseCSV");
    }

    string rowHolder;
    for (int i = 0; i < nRows; i++) {
        History row;
        getline(myFile, rowHolder, ','); //First element is a string to enum problem.
        row.buildName = cleanBuildName(rowHolder);
        getline(myFile, rowHolder, ',');
        row.mapName = rowHolder;
        getline(myFile, rowHolder, ',');
        row.isWinner = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.shortDelay = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.medDelay = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.longDelay = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.scoreBuilding = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.scoreKill = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.scoreRaze = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.scoreUnit = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.firstAirFrame = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.firstDetectFrame = std::stoi(rowHolder);
        getline(myFile, rowHolder, ',');
        row.gameDuration = std::stoi(rowHolder);
        //Now that all files have been added, append this to the output vector.
        gameHistory_.push_back(row);
    }

    myFile.close();
}


void LearningManager::selectDefaultBuild() {
    switch (Broodwar->enemy()->getRace() ) {
    case Races::Protoss:
        currentBuild_.initializeBuildOrder(findMatchingBO(BuildEnums::TwoBaseMuta));
        break;
    case Races::Terran:
        currentBuild_.initializeBuildOrder(findMatchingBO(BuildEnums::Lurker));
        break;
    case Races::Zerg:
        currentBuild_.initializeBuildOrder(findMatchingBO(BuildEnums::OneBaseSpire));
        break;
    default: //Random or Unknown.
        currentBuild_.initializeBuildOrder(findMatchingBO(BuildEnums::FivePool));
    }
}

void LearningManager::selectBestBuild()
{
    map<BuildEnums, double> storedUCB;
    Diagnostics::DiagnosticText("Selecting Best Build.");

    for (auto b : BuildStringsTable_) {
        Diagnostics::DiagnosticText("Enumerating Builds Within BuildTableName: %s", b.first.c_str());
    }

    for (auto b : BuildStringsTable_) {
        int win = 0, loss = 0, unmatched = 0;
        Diagnostics::DiagnosticText("On Start: Build, win, loss:", b.first.c_str(), win, loss);
        for (auto &game : gameHistory_) {
            if (game.buildName == b.first) {
                game.isWinner ? win++ : loss++;
            }
            else {
                unmatched++;
            }
        }
        Diagnostics::DiagnosticText("Game that matched: Build, win, loss, unmatched, UCB:", b.first.c_str(), win, loss, unmatched, getUpperConfidenceBound(win, loss));
        storedUCB.insert({ b.second , getUpperConfidenceBound(win, loss) });
        Diagnostics::DiagnosticText("Build: %s is rated %d", b.first.c_str(), getUpperConfidenceBound(win, loss));
    }

    BuildEnums bestBuild = currentBuild_.getBuildEnum(); //Start with the default.
    double bestScore = storedUCB.find(bestBuild)->second;

    for (auto &s : storedUCB) {
        if (s.second > bestScore) {
            bestScore = s.second;
            bestBuild = s.first;
            Diagnostics::DiagnosticText("New best Build:", getBuildNameString(bestBuild) );
            currentBuild_.initializeBuildOrder(findMatchingBO(bestBuild));
        }
    }
    currentBuild_.initializeBuildOrder(findMatchingBO(FourHatch));
}

//https://www.aionlinecourse.com/tutorial/machine-learning/upper-confidence-bound-%28ucb%29
double LearningManager::getUpperConfidenceBound(int win, int lose) {
    if (win + lose == 0) {
        return DBL_MAX;
    }
    return double(win) / double(lose) + sqrt( 1.5 * log(double(gameHistory_.size()))/double(win + lose) );
}

const string LearningManager::getBuildNameString(BuildEnums b)
{
    for (auto i : BuildStringsTable_) {
        if (i.second == b)
            return i.first;
    }
    string errorMsg = "Error: Build Not Found in Build Strings Table";
    return errorMsg;
}

Build LearningManager::inspectCurrentBuild() {
    return currentBuild_;
}

Build* LearningManager::modifyCurrentBuild()
{
    return &currentBuild_;
}

const string LearningManager::getReadDir()
{
    return readDirectory_;
}

const string LearningManager::getWriteDir()
{
    return writeDirectory_;
}


const int LearningManager::countLines(string fileIn) {
    ifstream myFile;
    myFile.open(fileIn);
    if (!myFile.is_open()) {
        Diagnostics::DiagnosticText("No file to read in at: %s", fileIn);
        return 0;
    }
    int n = 0;
    string rowHolder;
    while (getline(myFile, rowHolder)) {
        n++;
    }
    myFile.close();
    return n;
}

const void LearningManager::copyFile(string source, string destination) {
    std::ifstream src(source, std::ios::binary);
    std::ofstream dest(destination, std::ios::binary);
    dest << src.rdbuf();
}

string LearningManager::cleanBuildName(string messyString)
{
    string cleanup = messyString;
    cleanup.erase(std::remove(cleanup.begin(), cleanup.end(), '\n'), cleanup.end());
    return cleanup;
}

BuildOrderSetup LearningManager::findMatchingBO(BuildEnums b)
{
    for(auto i : myBuilds_)
        if(i.getBuildEnum() == b)
            return i;
    return BuildOrderSetup(); // else return an empty BO.
}

map<string, BuildEnums> LearningManager::BuildStringsTable_ ={
    { "MutaTwoBase", BuildEnums::TwoBaseMuta},
    { "Lurker", BuildEnums::Lurker },
    { "PoolFive", BuildEnums::FivePool } ,
    { "MutaOneBase", BuildEnums::OneBaseSpire },
    { "FourHatchMacro", BuildEnums::FourHatch }
};
