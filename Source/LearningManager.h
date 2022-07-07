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

    string myRaceChar_;
    string enemyRaceChar_;
    string versionChar_;
    string noStats_;
    string learningExtension_;
    string gameInfoExtension_;

public:

    // Key Operations
    void onStart();
    void onEnd(bool isWinner);

    string getBuildNameString(const BuildEnums b) const;

    Build inspectCurrentBuild() const; //Returns a copy, does not allow touching the class. Good for various inspections that may be wanted.
    Build* modifyCurrentBuild(); //Only do this if you're ready to modify the build.

    string getReadDir() const;
    string getWriteDir() const;

    void definePremadeBuildOrders(); // Creates the builds and stuffs them into myBuilds_.
    void parseLearningFile(); // Reads a learning file appropriate for this game and stores the info in gameHistory_.
    void selectDefaultBuild(); //Set Build if no opponent is recognized or no history exists.
    void selectBestBuild(); //Using history, determine superior build.
};



