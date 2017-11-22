#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"

using namespace std;

struct CES
{

public:
    CES( double a_army, double army_ct, bool army_possible, double a_tech, double tech_ct, bool tech_possible, double a_econ, double wk_ct, bool econ_possible );

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
};




