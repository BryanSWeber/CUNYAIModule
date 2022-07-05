#pragma once

//Controls most ML processes, including Genetic algorithms, random forest.  Only point of interaction with python. Used for learning between games. Does not control any learning or adaptation within games.

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Build.h"
#include <string>

using namespace std;
using namespace BWAPI;

struct History {
    string buildName;
    string mapName;
    bool isWinner;
    int shortDelay;
    int medDelay;
    int longDelay;
    int scoreBuilding;
    int scoreKill;
    int scoreRaze;
    int scoreUnit;
    int firstAirFrame;
    int firstDetectFrame;
    int gameDuration;
};


class LearningManager {

private:
    //const string readDirectory_ = "..//read//"; // If you are launched by the shell exe (in the at-home test enviorment) and are inside the AI folder, you start from your particular position and search down.
    //const string writeDirectory_ = "..//write//";
    const string readDirectory_ = "bwapi-data/read/"; // If you are launched by BWAPI you start from starcraft.exe
    const string writeDirectory_ = "bwapi-data/write/";


    Build currentBuild_;
    vector<History> gameHistory_;
    double getUpperConfidenceBound(int win, int lose); // Calculates average reward + confidence interval, the UCB.

    const int countLines(string fileIn); //Count lines in File
    const void copyFile(string source, string destination); // Moves file, important since the tournaments copy write dir into read dir at the end of each round.
    string cleanBuildName(string messyString); //takes care of leading newline characters, nothing else.
    vector<BuildOrderSetup> myBuilds_; //Predefined BO's
    static map<string, BuildEnums> BuildStringsTable_; //Table for build orders to translate human-readable strings.
    BuildOrderSetup findMatchingBO(BuildEnums b); //searchs MyBuilds for the correct matching type. Returns an empty BO otherwise.

public:

    int flierFrame; // Threatening Fliers First Seen
    int detectorFrame; // Detectors First Seen

    // Key Operations
    void onStart();
    void onEnd(bool isWinner);


    bool mapLearning;

    string myRaceChar;
    string enemyRaceChar;
    string versionChar;
    string noStats;
    string learningExtension;
    string gameInfoExtension;
    const string getBuildNameString(BuildEnums b);

    Build inspectCurrentBuild();
    Build* modifyCurrentBuild();

    const string getReadDir();
    const string getWriteDir();

    void definePremadeBuildOrders();
    void parseLearningFile();
    void selectDefaultBuild(); //Set Build if no opponent is recognized or no history exists.
    void selectBestBuild(); //Using history, determine superior build.

};



