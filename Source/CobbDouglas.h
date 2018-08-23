#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "PlayerModelManager.h"


using namespace std;

struct Player_Model;

struct CobbDouglas
{
    CobbDouglas() {}; // default constructor.

    void evaluateCD(double army_ct, bool army_possible, double tech_ct, bool tech_possible, double wk_ct, bool econ_possible );

    double alpha_army;
    double alpha_tech;
    double alpha_econ;

    double worker_stock;
    double army_stock;
    double tech_stock;
    
    double econ_derivative;
    double army_derivative;
    double tech_derivative;

    double getPriority();

    double getlny();
    double getlnY();

    bool army_starved();
    bool econ_starved();
    bool tech_starved();

    void estimateCD(int e_army_stock, int e_tech_stock, int e_worker_stock);
    void enemy_mimic(const Player_Model &enemy, const bool army_possible, const bool tech_possible, const bool econ_possible, const double adaptation_rate);

    // prints progress of economy over time every few seconds.  Gets large quickly.
    void printModelParameters();
};




