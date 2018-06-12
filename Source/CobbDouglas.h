#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

using namespace std;

struct CobbDouglas
{

public:
    CobbDouglas( double a_army, double army_ct, bool army_possible, double a_tech, double tech_ct, bool tech_possible, double a_econ, double wk_ct, bool econ_possible );

    double alpha_army;
    double alpha_tech;
    double alpha_econ;

    double worker_stock;
    double army_stock;
    double tech_stock;
    
    double econ_derivative;
    double army_derivative;
    double tech_derivative;

    double getlny();
    double getlnY();

    double getPriority();

    bool army_starved();
    bool econ_starved();
    bool tech_starved();

    void enemy_eval(int e_army_stock, bool army_possible, int e_tech_stock, bool tech_possible, int e_worker_stock, bool econ_possible, double adaptation_rate);

    // prints progress of economy over time every few seconds.  Gets large quickly.
    void printModelParameters();

    double enemy_alpha_army;
    double enemy_alpha_tech;
    double enemy_alpha_econ;

};




