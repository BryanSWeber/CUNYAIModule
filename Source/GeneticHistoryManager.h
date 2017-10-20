#pragma once

#include <BWAPI.h>
#include "MeatAIModule.h"

using namespace std;
using namespace BWAPI;

struct GeneticHistory {

    double delta_out_mutate_;
    double gamma_out_mutate_;
    double a_army_out_mutate_;
    double a_vis_out_mutate_;
    double a_econ_out_mutate_; 
    double a_tech_out_mutate_; 
    double loss_rate_;

    GeneticHistory( string file );
};