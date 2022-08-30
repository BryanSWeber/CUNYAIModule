#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\RushManager.h"
#include "Source\Diagnostics.h"
#include "Source\UnitInventory.h"
#include "Source\LearningManager.h"

int RushManager::clockToFrames(int min, int sec)
{
    return 24 * ( min * 60 + sec );
}

void RushManager::updateRushDetected()
{
    UnitInventory enemyEverythingInventory = CUNYAIModule::enemy_player_model.casualties_ + CUNYAIModule::enemy_player_model.units_;
    for (auto rule : rushRules) {
        bool rushEvidenceFound = CUNYAIModule::countUnits(rule.first, enemyEverythingInventory) >= rule.second.first && rule.second.second >= Broodwar->getFrameCount();
        if(rushEvidenceFound && !rushDetected)
            Diagnostics::DiagnosticText("At %d found %d %s, with a max allowed: %d", rule.second.second, CUNYAIModule::countUnits(rule.first, enemyEverythingInventory), rule.first.c_str(), rule.second.first);
        rushDetected = rushDetected || rushEvidenceFound; //One cannot "unrush" once rushed.
    }
}

RushManager::RushManager()
{
    rushDetected = false;
    rushResponded = false;
    rushRules = {
        { UnitTypes::Zerg_Zergling, { 8, clockToFrames(3,30) } },
        { UnitTypes::Protoss_Zealot, {4, clockToFrames(3,30) } },
        { UnitTypes::Terran_Marine, {4, clockToFrames(3,30) } },
        { UnitTypes::Zerg_Spawning_Pool, {1, clockToFrames(2,15) } },
        { UnitTypes::Protoss_Gateway, {2, clockToFrames(2,15) } },
        { UnitTypes::Terran_Barracks, {2, clockToFrames(3,0) } }
    }; // unit, <count, time in frames>

    // Hints from McRave:
    // 2 gates complete by 2:15 for a 9 / 9 in base
    // 2 zealots arrive by 3 : 25
    // 4 arrive by ~4 : 00
}

bool RushManager::getRushDetected()
{
    return rushDetected;
}

bool RushManager::getRushResponded()
{
    return rushResponded;
}

void RushManager::OnFrame()
{
    updateRushDetected();
    if (getRushDetected() && !getRushResponded()) {
        Diagnostics::DiagnosticText("We are getting rushed, I think...");
        if (CUNYAIModule::countUnitsInProgress(UnitTypes::Zerg_Hatchery) == 0) { //do not build sunkens if you're building a hatchery - wait until you're done.
            doRushResponse();
        }
    }
}

void RushManager::doRushResponse()
{
    switch (Broodwar->enemy()->getRace()) {
        case Races::Protoss:
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony); //Remember, first thing you do is added last. The Stack!
            break;
        case Races::Zerg:
            break;
        case Races::Terran:
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Sunken_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Drone);
            CUNYAIModule::learnedPlan.modifyCurrentBuild()->pushToFrontOfBuildOrder(UnitTypes::Zerg_Creep_Colony);
            break;
        case Races::Random:
        default:
            break;
    }
    rushResponded = true;
}
