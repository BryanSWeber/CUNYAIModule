#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

using namespace std;
using namespace BWAPI;

struct GeneticHistory {

    void initializeHistory();

    double delta_out_mutate_;
    double gamma_out_mutate_;
    double a_army_out_mutate_;
    double a_vis_out_mutate_;
    double a_econ_out_mutate_; 
    double a_tech_out_mutate_; 
    double r_out_mutate_;
    double loss_rate_;
    string build_order_;

};