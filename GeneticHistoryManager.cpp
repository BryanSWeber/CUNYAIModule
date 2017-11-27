#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\MeatAIModule.h"
# include "Source\GeneticHistoryManager.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
GeneticHistory::GeneticHistory( string file ) {

    //srand( Broodwar->getRandomSeed() ); // don't want the BW seed if the seed is locked. 

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( 0, 1 );

    // default values for output.
    double delta_out = dis( gen ) * 0.15 + 0.40;
    double gamma_out = dis( gen ) * 0.55; // Artifically chosen upper bounds. But above this, they often get truely silly.
    // the values below will be normalized to 1.
    double a_army_out = dis( gen ) * 0.75 + 0.25;
    double a_vis_out = dis( gen );
    double a_econ_out = dis( gen ) * 0.50 + 0.50;
    double a_tech_out = dis( gen ) * 0.50;
    
    // drone drone drone drone drone overlord drone drone drone hatch pool   // 12-hatch
    // drone drone drone drone drone overlord pool extractor// overpool
    // drone pool ling ling ling // 5-pool.
    // drone drone drone drone drone overlord drone drone drone hatch pool extract ling lair drone drone drone drone drone ovi speed spire extract ovi ovi muta muta muta muta muta muta muta muta muta muta muta // 12 hatch into muta.

    string build_order_out;
    double build_order_rand = dis( gen );
    if ( build_order_rand <= 0.20 ) {
        build_order_out = "drone pool drone drone ling ling ling ling ling ling overlord ling ling ling ling ling ling ling ling ling ling ling ling ling ling ling ling";
    }
    else if ( build_order_rand <= 0.40  && build_order_rand > 0.20 ) {
        build_order_out = "drone drone drone drone drone overlord pool drone extractor drone drone";
    }
    else if ( build_order_rand <= 0.60  && build_order_rand > 0.40 ){
        build_order_out = "drone drone drone drone drone overlord drone drone drone hatch pool drone drone";
    }
    else if ( build_order_rand <= 0.80  && build_order_rand > 0.60 ) {
        build_order_out = "drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling ling ling lurker lurker lurker lurker ling ling ling ling";
    }
    else {
        build_order_out = "drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone ling ling drone drone lair overlord drone drone speed drone drone drone drone drone drone drone drone spire drone extract drone creep drone creep drone sunken sunken overlord overlord muta muta muta muta muta muta muta muta muta muta muta muta";
    }

    int win_count = 0;
    int relevant_game_count = 0;
    int winning_player_map_race = 0;
    int winning_player_map = 0;
    int winning_player_race = 0;
    int winning_map_race = 0;
    int winning_player = 0;
    int winning_map = 0;
    int winning_race = 0;
    int losing_player_map_race = 0;
    int losing_player_map = 0;
    int losing_player_race = 0;
    int losing_map_race = 0;
    int losing_player = 0;
    int losing_map = 0;
    int losing_race = 0;

    string entry; // entered string from stream
    vector<double> delta_in;
    vector<double> gamma_in;
    vector<double> a_army_in;
    vector<double> a_vis_in;
    vector<double> a_econ_in;
    vector<double> a_tech_in;
    vector<string> race_in;

    vector<int> win_in;
    vector<int> sdelay_in;
    vector<int> mdelay_in;
    vector<int> ldelay_in;
    vector<string> name_in;
    vector<string> map_name_in;
    vector<string> build_order_in;

    vector<double> delta_win;
    vector<double> gamma_win;
    vector<double> a_army_win;
    vector<double> a_econ_win;
    vector<double> a_tech_win;
    vector<string> map_name_win;
    vector<string> build_order_win;

    loss_rate_ = 1;

    ifstream input; // brings in info;
    input.open( file, ios::in );
    // for each row, m8

    string line;
    int csv_length = 0;
    while ( getline( input, line ) ) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open( file, ios::in ); //ios.in?

    for ( int j = 0; j < csv_length; ++j ) { // further brute force inelegance.

        getline( input, entry, ',' );
        delta_in.push_back( stod( entry ) );

        getline( input, entry, ',' );
        gamma_in.push_back( stod( entry ) );

        getline( input, entry, ',' );
        a_army_in.push_back( stod( entry ) );
        getline( input, entry, ',' );
        a_econ_in.push_back( stod( entry ) );
        getline( input, entry, ',' );
        a_tech_in.push_back( stod( entry ) );

        getline( input, entry, ',' );
        race_in.push_back( entry );

        getline( input, entry, ',' );
        win_in.push_back( stoi( entry ) );

        getline( input, entry, ',' );
        sdelay_in.push_back( stoi( entry ) );
        getline( input, entry, ',' );
        mdelay_in.push_back( stoi( entry ) );
        getline( input, entry, ',' );
        ldelay_in.push_back( stoi( entry ) );

        getline( input, entry, ',' );
        name_in.push_back( entry );
        getline( input, entry, ',' );
        map_name_in.push_back( entry );
        getline( input, entry ); //diff. End of line char, not ','
        build_order_in.push_back( entry );

    } // closure for each row

    string e_name = Broodwar->enemy()->getName().c_str();
    string e_race = Broodwar->enemy()->getRace().c_str();
    string map_name = Broodwar->mapFileName().c_str();

    for ( int j = 0; j < csv_length; ++j ) { // what is the best conditional to use? Keep in mind we would like variation.
        if ( name_in[j] == e_name ) {
            if ( win_in[j] == 1 ) {
                winning_player++;
            }
            else {
                losing_player++;
            }
        }

        if ( race_in[j] == e_race ) {
            if ( win_in[j] == 1 ) {
                winning_race++;
            }
            else {
                losing_race++;
            }
        } 

        if ( map_name_in[j] == map_name ) {
            if ( win_in[j] == 1 ) {
                winning_map++;
            }
            else {
                losing_map++;
            }
        }

        if ( name_in[j] == e_name && race_in[j] == e_race ) {
            if ( win_in[j] == 1 ) {
                winning_player_race++;
            }
            else {
                losing_player_race++;
            }
        }

        if ( name_in[j] == e_name && map_name_in[j] == map_name ) {
            if ( win_in[j] == 1 ) {
                winning_player_map++;
            }
            else {
                losing_player_map++;
            }
        }


        if ( race_in[j] == e_race && map_name_in[j] == map_name ) {
            if ( win_in[j] == 1 ) {
                winning_map_race++;
            }
            else {
                losing_map_race++;
            }
        }

        if ( name_in[j] == e_name && race_in[j] == e_race && map_name_in[j] == map_name ) {
            if ( win_in[j] == 1 ) {
                winning_player_map_race++;
            }
            else {
                losing_player_map_race++;
            }
        }
    } 

    //What model is this? It's greedy...
    vector<double> probabilities = { winning_player_map_race / (double)max( winning_player_map_race + losing_player_map_race, 1 ), winning_player_map / (double)max( winning_player_map + losing_player_map, 1 ) , winning_player_race / (double)max( winning_player_race + losing_player_race, 1 ), winning_player / (double)max( winning_player + losing_player,1 ), winning_race / (double)max( winning_race + losing_race, 1 ) , winning_map / (double)max( winning_map + losing_map,1 ) };
    int counter = 1;
    for ( auto it = probabilities.begin(); it != probabilities.end() && !probabilities.empty(); it++ ) {
        if ( dis( gen ) > *it ) { // If random die roll would not indicate a victory for that run.
            counter++;
            continue;
        }
        else {
            break;
        }
    }

    for ( int j = 0; j < csv_length; ++j ) {
        bool condition;
        switch ( counter )
        {
        case 1: condition = name_in[j] == e_name && race_in[j] == e_race && map_name_in[j] == map_name;
            break;
        case 2: condition = name_in[j] == e_name && map_name_in[j] == map_name;
            break;
        case 3: condition = name_in[j] == e_name && race_in[j] == e_race;
            break;
        case 4: condition = race_in[j] == e_race && map_name_in[j] == map_name;
            break;
        case 5: condition = name_in[j] == e_name;
            break;
        case 6: condition = race_in[j] == e_race;
            break;
        case 7: condition = map_name_in[j] == map_name;
            break;
        default:
            break;
        }

        if ( condition && win_in[j] == 1 ) {
            delta_win.push_back( delta_in[j] );
            gamma_win.push_back( gamma_in[j] );
            a_army_win.push_back( a_army_in[j] );
            a_econ_win.push_back( a_econ_in[j] );
            a_tech_win.push_back( a_tech_in[j] );
            build_order_win.push_back( build_order_in[j] );
            win_count++;
            relevant_game_count++;
        } 
        else if ( condition ) {
            relevant_game_count++;
        }
    } //or widest hunt possible.

    if ( win_count > 0 ) { // redefine final output.

        std::uniform_real_distribution<double> unif_dist_to_win_count( max( 0, win_count - 25 ), win_count );

        int parent_1 = (int)(unif_dist_to_win_count( gen )); // safe even if there is only 1 win., index starts at 0.
        int parent_2 = (int)(unif_dist_to_win_count( gen ));

        double linear_combo = dis( gen ); //linear_crossover, interior of parents. Big mutation at the end, though.
        delta_out = linear_combo * delta_win[parent_1] + (1 - linear_combo) * delta_win[parent_2];
        gamma_out = linear_combo * gamma_win[parent_1] + (1 - linear_combo) * gamma_win[parent_2];
        a_army_out = linear_combo * a_army_win[parent_1] + (1 - linear_combo) * a_army_win[parent_2];
        a_econ_out = linear_combo * a_econ_win[parent_1] + (1 - linear_combo) * a_econ_win[parent_2];
        a_tech_out = linear_combo * a_tech_win[parent_1] + (1 - linear_combo) * a_tech_win[parent_2];

        build_order_out = linear_combo < 0.5 ? build_order_win[parent_2] : build_order_win[parent_1]; // Otherwise, use the build from which you used more of the history.


        //Gene swapping between parents. Not as popular for continuous optimization problems.
        //int chrom_0 = (rand() % 100 + 1) / 2;
        //int chrom_1 = (rand() % 100 + 1) / 2;
        //int chrom_2 = (rand() % 100 + 1) / 2;
        //int chrom_3 = (rand() % 100 + 1) / 2;
        //int chrom_4 = (rand() % 100 + 1) / 2;
        //int chrom_5 = (rand() % 100 + 1) / 2;

        //double delta_out_temp = chrom_0 > 50 ? delta_win[parent_1] : delta_win[parent_2];
        //double gamma_out_temp = chrom_1 > 50 ? gamma_win[parent_1] : gamma_win[parent_2];
        //double a_army_out_temp = chrom_2 > 50 ? a_army_win[parent_1] : a_army_win[parent_2];
        //double a_vis_out_temp = chrom_3 > 50 ? a_vis_win[parent_1] : a_vis_win[parent_2];
        //double a_econ_out_temp = chrom_4 > 50 ? a_econ_win[parent_1] : a_econ_win[parent_2];
        //double a_tech_out_temp = chrom_5 > 50 ? a_tech_win[parent_1] : a_tech_win[parent_2];
    }

    for ( int i = 0; i<1000; i++ ) {  // no corner solutions, please. Happens with incredibly small values 2*10^-234 ish.

        //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.
        if ( win_count > 0 ) { // if we have a win to work with

            double loss_rate_temp = 1 - (double)win_count / (double)relevant_game_count;

            if ( loss_rate_temp < 0.01 ) {
                loss_rate_ = 0.01; // Don't set all your parameters to zero on game two if you win game one.
            }
            else if ( loss_rate_temp > 0.99 ) {
                loss_rate_ = 0.99; // Don't set all your parameters to zero on game two if you win game one.
            }
            else {
                loss_rate_ = loss_rate_temp; // Don't set all your parameters to zero on game two if you win game one.
            }
        }

        //From genetic history, random parent for each gene. Mutate the genome
        std::uniform_real_distribution<double> unif_dist_to_mutate( 0, 3 );
        int mutation_0 = (int)unif_dist_to_mutate( gen ); // rand int between 0-2
        int mutation_1 = (int)unif_dist_to_mutate( gen ); // rand int between 0-2

        double mutation = pow( 1 + loss_rate_ * (dis( gen ) - 0.5) * 0.05, 2 ); // will generate rand double between 0.05 and 1.05.

        delta_out_mutate_ = mutation_0 == 0 ? delta_out  * mutation : delta_out;
        gamma_out_mutate_ = mutation_0 == 1 ? gamma_out  * mutation : gamma_out;
        a_vis_out_mutate_ = mutation_0 == 2 ? a_vis_out  * mutation : a_vis_out;// currently does nothing, vision is an artifact atm.

        a_army_out_mutate_ = mutation_1 == 0 ? a_army_out * mutation : a_army_out;
        a_econ_out_mutate_ = mutation_1 == 1 ? a_econ_out * mutation : a_econ_out;
        a_tech_out_mutate_ = mutation_1 == 2 ? a_tech_out * mutation : a_tech_out;

        // Normalize the CD part of the gene.
        //double a_tot = a_army_out_mutate_ + a_econ_out_mutate_ + a_tech_out_mutate_;
        //a_army_out_mutate_ = a_army_out_mutate_ / a_tot;
        //a_econ_out_mutate_ = a_econ_out_mutate_ / a_tot;
        //a_tech_out_mutate_ = a_tech_out_mutate_ / a_tot;

        // Normalize the CD part of the gene with CAPITAL AUGMENTING TECHNOLOGY.
        double a_tot = a_army_out_mutate_ + a_econ_out_mutate_;
        a_army_out_mutate_ = a_army_out_mutate_ / a_tot;
        a_econ_out_mutate_ = a_econ_out_mutate_ / a_tot;
        a_tech_out_mutate_ = a_tech_out_mutate_; // this is no longer normalized.

        build_order_ = build_order_out;

        if ( a_army_out_mutate_ > 0.01 && a_econ_out_mutate_ > 0.25 && a_tech_out_mutate_ > 0.01 && a_tech_out_mutate_ < 0.50 && delta_out_mutate_ < 0.55 && delta_out_mutate_ > 0.40 && gamma_out_mutate_ < 0.55 && gamma_out_mutate_ > 0.01 ) {
            break; // if we have an interior solution, let's use it, if not, we try again.
        }
    }

}
