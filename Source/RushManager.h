#pragma once

//Controls most ML processes, including Genetic algorithms, random forest.  Only point of interaction with python. Used for learning between games. Does not control any learning or adaptation within games.

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "LearningManager.h"

using namespace std;
using namespace BWAPI;

class RushManager {
private:
    int clockToFrames(int min, int sec); //convenience, since nearly all my work is in frames, but HRF is min/sec.
    bool rushDetected;
    bool rushResponded;
    map <UnitType, pair<int, int> > rushRules; // unit, <count, time in frames>
    void doRushResponse();
    void updateRushDetected();
public:
    RushManager::RushManager();
    bool getRushDetected();
    bool getRushResponded();
    void OnFrame();
};