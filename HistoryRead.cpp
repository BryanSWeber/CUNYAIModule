#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\MeatAIModule.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>



using namespace BWAPI;
using namespace Filter;
using namespace std;

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
double MeatAIModule::Win_History( string file, int value ) {
    ifstream input; // brings in info;
    input.open( file, ios::in );
    // for each row, m8
    
    string line;
    int csv_length = 0;
    while( std::getline( input, line ) ) {
        ++csv_length;
    }

        char row_char[20];
        double delta_in[5000];
        double gamma_in[5000];
        double a_army_in[5000];
        double a_vis_in[5000];
        double a_econ_in[5000];
        double a_tech_in[5000];
        string race_in[5000];
        int win_in[5000];
        int sdelay_in[5000];
        int mdelay_in[5000];
        int ldelay_in[5000];
        int seed_in[5000];


        for ( int j = 0; j < csv_length; ++j ) {
            for ( int i = 0; i < 13; ++i ) { // for each column in the row
                input.getline( row_char, 10, ',' );  // keeps the endline symbol unintentionally. Makes row_char a C-string, a null-terminated character array. Not a classic string.
                std::string entry = row_char;
                if ( !entry.empty() ) {
                    switch ( i )
                    {
                    case 1: delta_in[j] = stod( entry );
                    case 2: gamma_in[j] = stod( entry );
                    case 3: a_army_in[j] = stod( entry );
                    case 4: a_vis_in[j] = stod( entry );
                    case 5: a_econ_in[j] = stod( entry );
                    case 6: a_tech_in[j] = stod( entry );
                    case 7: race_in[j] = entry;
                    case 8: win_in[j] = stoi( entry );
                    case 9: sdelay_in[j] = stoi( entry );
                    case 10: mdelay_in[j] = stoi( entry );
                    case 11: ldelay_in[j] = stoi( entry );
                    case 12: seed_in[j] = stoi( entry );
                    } // closure switch
                }
            }// closure for each column in the row
        } // closure for each row

        // default values for output.
        double delta_out = 0.45;
        double gamma_out = 0.71;
        double a_army_out = 0.65;
        double a_vis_out = 0.50;
        double a_econ_out = 0.015;
        double a_tech_out = 0.006;

        double delta_win[5000];
        double gamma_win[5000];
        double a_army_win[5000];
        double a_vis_win[5000];
        double a_econ_win[5000];
        double a_tech_win[5000];
        int win_count = 0;

        for ( int j = 0; j < csv_length; ++j ) {
            if ( (win_in[j] == 1 || sdelay_in[j] > 320) && race_in[j] == "Zerg" && Broodwar->enemy()->getRace() == Races::Zerg ) {
                delta_win[win_count] = delta_in[j];
                gamma_win[win_count] = gamma_in[j];
                a_army_win[win_count] = a_army_in[j];
                a_vis_win[win_count] = a_vis_in[j];
                a_econ_win[win_count] = a_econ_in[j];
                a_tech_win[win_count] = a_tech_in[j];
                win_count++;
            } else if ( (win_in[j] == 1 || sdelay_in[j] > 320) && race_in[j] == "Protoss" && Broodwar->enemy()->getRace() == Races::Protoss ) {
                delta_win[win_count] = delta_in[j];
                gamma_win[win_count] = gamma_in[j];
                a_army_win[win_count] = a_army_in[j];
                a_vis_win[win_count] = a_vis_in[j];
                a_econ_win[win_count] = a_econ_in[j];
                a_tech_win[win_count] = a_tech_in[j];
                win_count++;
            } else if ( (win_in[j] == 1  || sdelay_in[j] > 320) && race_in[j] == "Terran" && Broodwar->enemy()->getRace() == Races::Terran ) {
                delta_win[win_count] = delta_in[j];
                gamma_win[win_count] = gamma_in[j];
                a_army_win[win_count] = a_army_in[j];
                a_vis_win[win_count] = a_vis_in[j];
                a_econ_win[win_count] = a_econ_in[j];
                a_tech_win[win_count] = a_tech_in[j];
                win_count++;
            } else if ( (win_in[j] == 1 || sdelay_in[j] > 320) && race_in[j] == "Random" && Broodwar->enemy()->getRace() == Races::Random ) {
                delta_win[win_count] = delta_in[j];
                gamma_win[win_count] = gamma_in[j];
                a_army_win[win_count] = a_army_in[j];
                a_vis_win[win_count] = a_vis_in[j];
                a_econ_win[win_count] = a_econ_in[j]; 
                a_tech_win[win_count] = a_tech_in[j];
                win_count++;
            }
        }

        if ( win_count != 0 ) { // redefine final output.

            int parent_1 = rand() % win_count + 1;
            int parent_2 = rand() % win_count + 1;

            delta_out = (delta_win[parent_1] + delta_win[parent_2]) / 2 ;
            gamma_out = (gamma_win[parent_1] + gamma_win[parent_2]) / 2;
            a_army_out = (a_army_win[parent_1] + a_army_win[parent_2]) / 2;
            a_vis_out = (a_vis_win[parent_1] + a_vis_win[parent_2]) / 2;
            a_econ_out = (a_econ_win[parent_1] + a_econ_win[parent_2]) / 2;
            a_tech_out = (a_tech_win[parent_1] + a_tech_win[parent_2]) / 2;
        }

        double initial_cond[6] = { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };

        return initial_cond[value];
}