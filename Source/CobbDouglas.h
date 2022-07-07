#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"
#include "LearningManager.h"
#include <signal.h>


using namespace std;

class PlayerModel;

class CobbDouglas
{
private:

    double alpha_army;
    double alpha_tech;
    double alpha_econ;
    double adoptionRate;

    double worker_stock;
    double army_stock;
    double tech_stock;

    double econ_derivative;
    double army_derivative;
    double tech_derivative;

    void setModelParameters(double armyAlpha, double econAlpha, double techAlpha, double adopRate);
    double getPriority() const;

public:
    CobbDouglas() {}; // default constructor.

    void onStartSelf(LearningManager l);

    void setCD(double army_ct, double tech_ct, double wk_ct); //Estimates a CD with perfect information.
    void setEnemyCD(int e_army_stock, int e_tech_stock, int e_worker_stock); //Estimates a CD with some ad-hoc limits on values.

    double getlnYPerCapita() const; //Per capita wealth, not really needed here but part of the formal model in economics.
    double getlnY() const; // Total overall wealth.
    double getlnYusing(const double alpha_army, const double alpha_tech) const;


    bool army_starved() const;
    bool econ_starved() const;
    bool tech_starved() const;

    double getParameter(BuildParameterNames b) const;
    double getDeriviative(BuildParameterNames b) const;
    double getStock(BuildParameterNames b) const;

    //void estimateUnknownCD(int e_army_stock, int e_tech_stock, int e_worker_stock); //Estimates a CD within some ad-hoc bounds. For enemies.
    //void setStockObserved(int e_army_stock, int e_tech_stock, int e_worker_stock); //Stores Stocks for later calculations. 

    void enemy_mimic(const PlayerModel &enemy);

    // prints progress of economy over time every few seconds.  Gets large quickly.
    void printModelParameters() const;

    //Discontinuities -Cutoff if critically full, or suddenly progress towards one macro goal or another is impossible, or if their army is critically larger than ours.
    bool evalArmyPossible() const;
    bool evalEconPossible() const;
    bool evalTechPossible() const;

};


//The CD model is not made for 0's. But zeros happen in SC. 
template <class T>
double safeDiv(T &lhs, T &rhs) {
    if (lhs == 0 && rhs != 0) return DBL_MIN;
    else if (lhs > 0 && rhs == 0)  return DBL_MAX;
    else if (lhs < 0 && rhs == 0)  return -DBL_MAX;
    else if (lhs == 0 && rhs == 0) return DBL_MIN;
    else return static_cast<double>(lhs) / static_cast<double>(rhs); //(lhs != 0 && rhs != 0)
}