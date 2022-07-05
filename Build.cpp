#pragma once

// Contains Build Objects - tracks things that are important in openings and basic plans.

#include <BWAPI.h>
#include "Source\CUNYAIModule.h"
#include "Source\Build.h"
#include "Source\Diagnostics.h"
#include <string>


void Build::updateRemainingBuildOrder(const UnitType &ut) {
    if (!queueBuild_.empty()) {
        if (queueBuild_.front().getUnit() == ut) {
            queueBuild_.erase(queueBuild_.begin());
        }
    }
    updateCumulativeResources();
}

void Build::updateRemainingBuildOrder(const UpgradeType &ups) {
    if (!queueBuild_.empty()) {
        if (queueBuild_.front().getUpgrade() == ups) {
            queueBuild_.erase(queueBuild_.begin());
        }
    }
    updateCumulativeResources();
}

void Build::updateRemainingBuildOrder(const TechType &research) {
    if (!queueBuild_.empty()) {
        if (queueBuild_.front().getResearch() == research) {
            queueBuild_.erase(queueBuild_.begin());
        }
    }
    updateCumulativeResources();
}

const void Build::announceBuildingAttempt(UnitType ut) {
    if (ut.isBuilding()) {
        Diagnostics::DiagnosticWrite("Building a %s", ut.c_str());
    }
}

const bool Build::checkIfNextInBuild(UnitType ut) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return getNext().getUnit() == ut;
}

const int Build::countTimesInBuildQueue(UnitType ut) {
    int count = 0;
    for (auto g : queueBuild_) {
        if (g.getUnit() == ut) count++;
    }
    return count;
}


const bool Build::checkIfNextInBuild(UpgradeType upgrade) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return getNext().getUpgrade() == upgrade;
}

const bool Build::checkIfNextInBuild(TechType research) {
    // A building is not wanted at that moment if we have active builders or the timer is nonzero.
    return getNext().getResearch() == research;
}

const bool Build::isEmptyBuildOrder() {
    return queueBuild_.empty();
}

void Build::addBuildOrderElement(const UpgradeType & ups)
{
    queueBuild_.push_back(BuildOrderElement(ups));
    updateCumulativeResources();
}

void Build::addBuildOrderElement(const TechType & research)
{
    queueBuild_.push_back(BuildOrderElement(research));
    updateCumulativeResources();
}

void Build::addBuildOrderElement(const UnitType & ut)
{
    queueBuild_.push_back(BuildOrderElement(ut));
    updateCumulativeResources();
}

void Build::retryBuildOrderElement(const UnitType & ut)
{
    queueBuild_.insert(queueBuild_.begin(), BuildOrderElement(ut));
    updateCumulativeResources();
}
void Build::retryBuildOrderElement(const UpgradeType & up)
{
    queueBuild_.insert(queueBuild_.begin(), BuildOrderElement(up));
    updateCumulativeResources();
}

void Build::updateCumulativeResources()
{
    cumulative_gas_ = 0;
    cumulative_minerals_ = 0;
    for (auto u : queueBuild_) {
        cumulative_gas_ += u.getResearch().gasPrice() + u.getUnit().gasPrice() + u.getUpgrade().gasPrice();
        cumulative_minerals_ += u.getResearch().mineralPrice() + u.getUnit().mineralPrice() + u.getUpgrade().mineralPrice();
    }
}

const double Build::getParameter(BuildParameterNames b)
{
    return parameterValues_[b];
}

int Build::getParameterCount()
{
    return distance(begin(parameterValues_), end(parameterValues_));
}

const BuildEnums Build::getBuildEnum()
{
    return buildName_;
}

//void Build::setInitialBuildOrder(string s) {
//
//    queueBuild_.clear();
//
//    initial_building_gene_ = s;
//
//    std::stringstream ss(s);
//    std::istream_iterator<std::string> begin(ss);
//    std::istream_iterator<std::string> end;
//    std::vector<std::string> build_string(begin, end);
//
//    BuildOrderQueue hatch = BuildOrderQueue(UnitTypes::Zerg_Hatchery);
//    BuildOrderQueue extract = BuildOrderQueue(UnitTypes::Zerg_Extractor);
//    BuildOrderQueue drone = BuildOrderQueue(UnitTypes::Zerg_Drone);
//    BuildOrderQueue ovi = BuildOrderQueue(UnitTypes::Zerg_Overlord);
//    BuildOrderQueue pool = BuildOrderQueue(UnitTypes::Zerg_Spawning_Pool);
//    BuildOrderQueue evo = BuildOrderQueue(UnitTypes::Zerg_Evolution_Chamber);
//    BuildOrderQueue speed = BuildOrderQueue(UpgradeTypes::Metabolic_Boost);
//    BuildOrderQueue ling = BuildOrderQueue(UnitTypes::Zerg_Zergling);
//    BuildOrderQueue creep = BuildOrderQueue(UnitTypes::Zerg_Creep_Colony);
//    BuildOrderQueue sunken = BuildOrderQueue(UnitTypes::Zerg_Sunken_Colony);
//    BuildOrderQueue spore = BuildOrderQueue(UnitTypes::Zerg_Spore_Colony);
//    BuildOrderQueue lair = BuildOrderQueue(UnitTypes::Zerg_Lair);
//    BuildOrderQueue hive = BuildOrderQueue(UnitTypes::Zerg_Hive);
//    BuildOrderQueue spire = BuildOrderQueue(UnitTypes::Zerg_Spire);
//    BuildOrderQueue greater_spire = BuildOrderQueue(UnitTypes::Zerg_Greater_Spire);
//    BuildOrderQueue devourer = BuildOrderQueue(UnitTypes::Zerg_Devourer);
//    BuildOrderQueue muta = BuildOrderQueue(UnitTypes::Zerg_Mutalisk);
//    BuildOrderQueue hydra = BuildOrderQueue(UnitTypes::Zerg_Hydralisk);
//    BuildOrderQueue lurker = BuildOrderQueue(UnitTypes::Zerg_Lurker);
//    BuildOrderQueue hydra_den = BuildOrderQueue(UnitTypes::Zerg_Hydralisk_Den);
//    BuildOrderQueue queens_nest = BuildOrderQueue(UnitTypes::Zerg_Queens_Nest);
//    BuildOrderQueue lurker_tech = BuildOrderQueue(TechTypes::Lurker_Aspect);
//    BuildOrderQueue grooved_spines = BuildOrderQueue(UpgradeTypes::Grooved_Spines);
//    BuildOrderQueue muscular_augments = BuildOrderQueue(UpgradeTypes::Muscular_Augments);
//
//    for (auto &build : build_string) {
//        if (build == "hatch") {
//            queueBuild_.push_back(hatch);
//        }
//        else if (build == "extract") {
//            queueBuild_.push_back(extract);
//        }
//        else if (build == "drone") {
//            queueBuild_.push_back(drone);
//        }
//        else if (build == "ovi") {
//            queueBuild_.push_back(ovi);
//        }
//        else if (build == "overlord") {
//            queueBuild_.push_back(ovi);
//        }
//        else if (build == "pool") {
//            queueBuild_.push_back(pool);
//        }
//        else if (build == "evo") {
//            queueBuild_.push_back(evo);
//        }
//        else if (build == "speed") {
//            queueBuild_.push_back(speed);
//        }
//        else if (build == "ling") {
//            queueBuild_.push_back(ling);
//        }
//        else if (build == "creep") {
//            queueBuild_.push_back(creep);
//        }
//        else if (build == "sunken") {
//            queueBuild_.push_back(sunken);
//        }
//        else if (build == "spore") {
//            queueBuild_.push_back(spore);
//        }
//        else if (build == "lair") {
//            queueBuild_.push_back(lair);
//        }
//        else if (build == "hive") {
//            queueBuild_.push_back(hive);
//        }
//        else if (build == "spire") {
//            queueBuild_.push_back(spire);
//        }
//        else if (build == "greater_spire") {
//            queueBuild_.push_back(greater_spire);
//        }
//        else if (build == "devourer") {
//            queueBuild_.push_back(devourer);
//        }
//        else if (build == "muta") {
//            queueBuild_.push_back(muta);
//        }
//        else if (build == "lurker_tech") {
//            queueBuild_.push_back(lurker_tech);
//        }
//        else if (build == "hydra") {
//            queueBuild_.push_back(hydra);
//        }
//        else if (build == "lurker") {
//            queueBuild_.push_back(lurker);
//        }
//        else if (build == "hydra_den") {
//            queueBuild_.push_back(hydra_den);
//        }
//        else if (build == "queens_nest") {
//            queueBuild_.push_back(queens_nest);
//        }
//        else if (build == "grooved_spines") {
//            queueBuild_.push_back(grooved_spines);
//        }
//        else if (build == "muscular_augments") {
//            queueBuild_.push_back(muscular_augments);
//        }
//        else if (build == "5pool") { //shortcuts.
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(pool);
//        }
//        else if (build == "7pool") {
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(pool);
//        }
//        else if (build == "9pool") {
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(pool);
//        }
//        else if (build == "12pool") {
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(ovi);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(pool);
//        }
//        else if (build == "12hatch") {
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(ovi);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(drone);
//            queueBuild_.push_back(hatch);
//        }
//    }
//    updateCumulativeResources();
//}


const BuildOrderElement Build::getNext()
{
    if (!isEmptyBuildOrder())
        return queueBuild_.front();
    else
        return BuildOrderElement(UnitTypes::None);
}

const vector<BuildOrderElement> Build::getQueue()
{
    return queueBuild_;
}

const int Build::getRemainingGas()
{
    return cumulative_gas_;
}

const int Build::getRemainingMinerals()
{
    return cumulative_minerals_;
}

const int Build::getNextGasCost()
{
    return getNext().getResearch().gasPrice() + getNext().getUnit().gasPrice() + getNext().getUpgrade().gasPrice();
}

const int Build::getNextMinCost()
{
    return getNext().getResearch().mineralPrice() + getNext().getUnit().mineralPrice() + getNext().getUpgrade().mineralPrice();
}

void Build::initializeBuildOrder(BuildOrderSetup b)
{
    for (int i = 0; i < 5; i++) {
        this->parameterValues_[i] = b.getCoreParameters(i);
    }
    this->queueBuild_ = b.getSetupQueue();
    this->buildName_ = b.getBuildEnum();

    this->updateCumulativeResources(); // Run after setting or modifying build order.
}

void Build::clearRemainingBuildOrder(const bool diagnostic) {
    if constexpr (ANALYSIS_MODE) {
        if (!queueBuild_.empty() && diagnostic) {

            if (queueBuild_.front().getUnit().supplyRequired() > Broodwar->self()->supplyTotal() - Broodwar->self()->supplyTotal()) {
                ofstream output; // Prints to brood war file while in the WRITE file.
                output.open("..\\write\\BuildOrderFailures.txt", ios_base::app);
                string print_value = "";

                //print_value += building_gene_.front().getResearch().c_str();
                print_value += queueBuild_.front().getUnit().c_str();
                //print_value += building_gene_.front().getUpgrade().c_str();

                output << "Supply blocked: " << print_value << endl;
                output.close();
                Broodwar->sendText("A %s was canceled.", print_value);
            }
            else {
                ofstream output; // Prints to brood war file while in the WRITE file.
                output.open("..\\write\\BuildOrderFailures.txt", ios_base::app);
                string print_value = "";

                print_value += queueBuild_.front().getResearch().c_str();
                print_value += queueBuild_.front().getUnit().c_str();
                print_value += queueBuild_.front().getUpgrade().c_str();

                output << "Couldn't build: " << print_value << endl;
                output.close();
                Broodwar->sendText("A %s was canceled.", print_value);
            }
        }
    }
    queueBuild_.clear();
    updateCumulativeResources();
};


BuildOrderSetup::BuildOrderSetup()
{
}

BuildOrderSetup::BuildOrderSetup(vector<BuildOrderElement> q, double parameterArray[6], BuildEnums n)
{
    queueBuild_ = q;  // What do we need to make?
    for (int i = 0; i < 5; i++) {
        parameterValues_[i] = parameterArray[i];
    }
    buildName_ = n;
}

vector<BuildOrderElement> BuildOrderSetup::getSetupQueue()
{
    return queueBuild_;
}

double BuildOrderSetup::getCoreParameters(int i)
{
    return parameterValues_[i];
}

BuildEnums BuildOrderSetup::getBuildEnum()
{
    return buildName_;
}

int BuildOrderSetup::getParameterCount()
{
    return distance(begin(parameterValues_), end(parameterValues_));
}


