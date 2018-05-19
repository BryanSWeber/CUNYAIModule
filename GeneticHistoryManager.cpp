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
# include <algorithm>
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
GeneticHistory::GeneticHistory( string file ) {

    //srand( Broodwar->getRandomSeed() ); // don't want the BW seed if the seed is locked. 

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen( rd() ); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis( 0, 1 );    // default values for output.

    double delta_out = dis( gen ) ;
    double gamma_out = dis( gen ) ; // Artifically chosen upper bounds. But above this, they often get truely silly.
    // the values below will be normalized to 1.
    double a_army_out = dis( gen ) ;
    double a_econ_out = dis( gen ) ;
    double a_tech_out = dis( gen ) ;
    //double r_out = log(85 / (double)4) / (double)(14400 + dis(gen) * (25920 - 14400)); //Typical game maxes vary from 12.5min to 16 min according to antiga. Assumes a range from 4 to max in 10 minutes, (14400 frames) to 18 minutes 25920 frames
    double r_out = dis(gen);
    //No longer used.
    double a_vis_out = dis(gen);

    if (_TRAINING_AGAINST_BASE_AI) {
    
        delta_out = 0.4;
        gamma_out = 0.4; // Artifically chosen
        // the values below will be normalized to 1.
        a_army_out = dis(gen);
        a_econ_out = dis(gen);
        a_tech_out = dis(gen);
        r_out = dis(gen); 

    }
    // drone drone drone drone drone overlord drone drone drone hatch pool   // 12-hatch
    // drone drone drone drone drone overlord pool extractor// overpool
    // drone pool ling ling ling // 5-pool.
    // drone drone drone drone drone overlord drone drone drone hatch pool extract ling lair drone drone drone drone drone ovi speed spire extract ovi ovi muta muta muta muta muta muta muta muta muta muta muta // 12 hatch into muta.

    string build_order_out;
    

    vector<string> build_order_list = {
        "drone drone drone drone drone overlord pool drone creep drone drone", // The blind sunken. For the bots that just won't take no for an answer.
        "drone pool drone drone ling ling ling ling ling ling overlord ling ling ling ling ling ling ling ling ling ling ling ling ling ling ling ling", // 5pool with some commitment.
        "drone drone drone drone drone overlord pool drone drone", // 9pool gasless
        "drone drone drone drone drone overlord pool drone extract drone drone", // 9pool
        "drone drone drone drone drone overlord drone drone drone pool drone extract hatch ling ling ling ling ling ling speed", // 12-pool tenative.
        "drone drone drone drone drone overlord drone drone drone hatch pool drone drone", // 12hatch-pool
        "drone drone drone drone drone pool drone extract overlord drone ling ling ling ling ling ling lair drone overlord drone hydra_den hydra hydra hydra hydra ling ling ling ling ling ling ling ling lurker_tech", //1 h lurker, tenative.
        "drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling ling ling lurker lurker lurker lurker ling ling ling ling", // 2h lurker
        "drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone ling ling ling ling ling ling drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken",  // 2 h turtle, tenative. Dies because the first hatch does not have creep by it when it is time to build.
        "drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone ling ling drone drone lair overlord drone drone speed drone drone drone drone drone drone drone drone spire drone extract drone creep drone creep drone sunken sunken overlord overlord muta muta muta muta muta muta muta muta muta muta muta muta", // 2h - Muta.  Requires another overlord?
       "drone drone drone drone drone pool drone extract overlord drone ling ling ling ling ling ling hydra_den drone drone drone drone", //zerg_9pool - UAB
       "drone drone drone drone overlord drone drone drone hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone grooved_spines hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra - UAB with edits. added an overlord.
       "drone drone drone drone overlord drone drone drone hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone muscular_augments hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract" //zerg_2hatchhydra - UAB with edits. added an overlord.
    };

    if (_TRAINING_AGAINST_BASE_AI) {
        build_order_list = { "drone drone drone drone drone overlord drone drone drone hatch pool drone drone" };
    }

    std::uniform_int_distribution<size_t> rand_bo(0, build_order_list.size() - 1 );
    size_t build_order_rand = rand_bo(gen);

    build_order_out = build_order_list[ build_order_rand ];
	
  // "drone drone drone drone overlord drone drone drone drone hatch drone drone pool drone drone extract drone drone drone drone drone drone lair drone drone drone drone drone drone drone drone drone drone spire overlord drone overlord hatch drone drone drone drone drone drone drone drone drone drone muta muta muta muta muta muta muta muta muta muta muta muta hatch"; //zerg_3hatchmuta: 
	// "drone drone drone drone overlord drone drone drone drone hatch drone drone pool drone drone extract drone drone drone drone drone drone lair drone drone drone drone drone drone drone drone drone drone spire overlord drone overlord hatch drone drone drone drone drone drone drone drone hatch drone extract drone hatch scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge hatch extract extract hatch"; // zerg_3hatchscourge ??? UAB

    int selected_win_count = 0;
    int selected_lose_count = 0;
    int win_count = 0;
    int lose_count = 0;
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
    int games_since_last_win = 999; // starting at 0 makes the script think it WON the last game. 999 is a functional marker for having never won.
    double prob_win_given_conditions;

    string entry; // entered string from stream
    vector<double> delta_total;
    vector<double> gamma_total;
    vector<double> a_army_total;
    vector<double> a_vis_total;
    vector<double> a_econ_total;
    vector<double> a_tech_total;
    vector<double> r_total;
    vector<string> race_total;

    vector<int> win_total;
    vector<int> sdelay_total;
    vector<int> mdelay_total;
    vector<int> ldelay_total;
    vector<string> name_total;
    vector<string> map_name_total;
    vector<string> build_order_total;

    vector<double> delta_win;
    vector<double> gamma_win;
    vector<double> a_army_win;
    vector<double> a_econ_win;
    vector<double> a_tech_win;
    vector<double> r_win;
    vector<string> map_name_win;
    vector<string> build_order_win;

    vector<string> build_orders_tried;
    vector<string> enemy_races_tried;


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
        delta_total.push_back( stod( entry ) );

        getline( input, entry, ',' );
        gamma_total.push_back( stod( entry ) );

        getline( input, entry, ',' );
        a_army_total.push_back( stod( entry ) );
        getline( input, entry, ',' );
        a_econ_total.push_back( stod( entry ) );
        getline( input, entry, ',' );
        a_tech_total.push_back( stod( entry ) );
        getline(input, entry, ',');
        r_total.push_back( stod(entry) );

        getline( input, entry, ',' );
        race_total.push_back( entry );

        getline( input, entry, ',' );
        win_total.push_back( stoi( entry ) );

        getline( input, entry, ',' );
        sdelay_total.push_back( stoi( entry ) );
        getline( input, entry, ',' );
        mdelay_total.push_back( stoi( entry ) );
        getline( input, entry, ',' );
        ldelay_total.push_back( stoi( entry ) );

        getline( input, entry, ',' );
        name_total.push_back( entry );
        getline( input, entry, ',' );
        map_name_total.push_back( entry );

        getline( input, entry ); //diff. End of line char, not ','
        build_order_total.push_back( entry );

    } // closure for each row

    string e_name = Broodwar->enemy()->getName().c_str();
    string e_race = Broodwar->enemy()->getRace().c_str();
    string map_name = Broodwar->mapFileName().c_str();


    for ( int j = 0; j < csv_length; ++j ) { // what is the best conditional to use? Keep in mind we would like variation.

        if (win_total[j] == 1) {
            win_count++;
        }
        else {
            lose_count++;
        }

        if (name_total[j] == e_name && (e_race == "Unknown" || race_total[j] == e_race)) {
            if ( win_total[j] == 1 ) {
                winning_player++;
            }
            else {
                losing_player++;
            }
        }

        if ( race_total[j] == e_race ) {
            if ( win_total[j] == 1 ) {
                winning_race++;
            }
            else {
                losing_race++;
            }
        } 

        if ( map_name_total[j] == map_name) {
            if ( win_total[j] == 1 ) {
                winning_map++;
            }
            else {
                losing_map++;
            }
        }

        if (name_total[j] == e_name ) {
            enemy_races_tried.push_back(race_total[j]);
        }
    } 

    //What model is this? It's greedy...

    double race_or_player_w = winning_player > 0 ? winning_player : winning_race;
    double race_or_player_l = losing_player > 0 ? losing_player : losing_race;

    double likelihood_w =  race_or_player_w/(double)win_count * winning_map/(double)win_count ;
    double likelihood_l = race_or_player_l/(double)lose_count * losing_map/(double)lose_count;
    double rand_value = dis(gen);

    prob_win_given_conditions = fmax( (likelihood_w * win_count ) / (likelihood_w * win_count + likelihood_l * lose_count), 0.0 );

    vector<int> frequency = { winning_player, winning_race, winning_map };


    // start from most recent and count our way back from there.
    for ( int j = csv_length; j > 0 ; --j ) {

        bool conditions_for_inclusion = true;
        int counter = 0;
        //int min_frequency = 9999999999;

        bool name_matches = name_total[j] == e_name;
        bool race_matches = (e_race == "Unknown" || race_total[j] == e_race);
        bool map_matches = map_name_total[j] == map_name;
        // an inelegant statement follows. How do I make this into a switch?
        if (winning_player > 0 && winning_race > 0 && winning_map > 0) { //choice in race for random players is like a whole new ball park.
            conditions_for_inclusion = name_matches && race_matches && map_matches;
        }
        else if (winning_player > 0 && winning_race > 0 && winning_map == 0) {
            conditions_for_inclusion = name_matches && race_matches && !map_matches;
        }
        else if (winning_player > 0 && winning_race == 0 && winning_map > 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }
        else if (winning_player == 0 && winning_race > 0 && winning_map > 0) {
            conditions_for_inclusion = !name_matches && race_matches && map_matches;
        }
        else if (winning_player > 0 && winning_race > 0 && winning_map == 0) {
            conditions_for_inclusion = name_matches && race_matches && !map_matches;
        }
        else if (winning_player > 0 && winning_race == 0 && winning_map == 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }
        else if (winning_player == 0 && winning_race > 0 && winning_map == 0) {
            conditions_for_inclusion = !name_matches && race_matches && !map_matches;
        }
        else if (winning_player == 0 && winning_race == 0 && winning_map > 0) {
            conditions_for_inclusion = !name_matches && !race_matches && map_matches;
        }

        if (conditions_for_inclusion && win_total[j] == 1 ) {
            delta_win.push_back( delta_total[j] );
            gamma_win.push_back( gamma_total[j] );
            a_army_win.push_back( a_army_total[j] );
            a_econ_win.push_back( a_econ_total[j] );
            a_tech_win.push_back( a_tech_total[j] );
            r_win.push_back( r_total[j] );
            build_order_win.push_back( build_order_total[j] );
            build_orders_tried.push_back(build_order_total[j]);
            selected_win_count++;
            games_since_last_win = 0;
        } 
        else if ( conditions_for_inclusion && win_total[j] == 0) {
            selected_lose_count++;
            build_orders_tried.push_back(build_order_total[j]);
            games_since_last_win++;
        }

        if (selected_win_count >= 50) { // stop once we have 50 games in the parantage.
            break;
        }
    } //or widest hunt possible.

    std::sort(build_orders_tried.begin(), build_orders_tried.end());
    int uniqueCount = std::unique(build_orders_tried.begin(), build_orders_tried.end()) - build_orders_tried.begin();

    if ( selected_win_count > 0 ) { // redefine final output.

        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, build_order_win.size() - 1); // safe even if there is only 1 win., index starts at 0.

        size_t parent_1 = unif_dist_to_win_count(gen); 
        size_t parent_2 = unif_dist_to_win_count(gen);

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

        if ( _TRAINING_AGAINST_BASE_AI ) {

            //set size of starting population.
            if ( selected_win_count >= 50) {
                build_order_out = build_order_win[parent_1];
                while (build_order_out != build_order_win[parent_2]) {
                    parent_2 = unif_dist_to_win_count(gen); // get a matching parent.
                }

                if (!_LEARNING_MODE) {
                    parent_2 = parent_1;
                }

                delta_out   = 0.4;
                gamma_out   = 0.4;
                a_army_out = MeatAIModule::bindBetween(pow(a_army_win[parent_1], crossover) * pow(a_army_win[parent_2], (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
                a_econ_out = MeatAIModule::bindBetween(pow(a_econ_win[parent_1], crossover) * pow(a_econ_win[parent_2], (1 - crossover)), 0., 1.);
                a_tech_out = MeatAIModule::bindBetween(pow(a_tech_win[parent_1], crossover) * pow(a_tech_win[parent_2], (1 - crossover)), 0., 1.);
                r_out =      MeatAIModule::bindBetween(pow(r_win[parent_1], crossover)      * pow(r_win[parent_2], (1 - crossover)), 0., 1.);
            }
        }
        else { 

            //if we don't need diversity, combine our old wins together.
            if ( dis(gen) < uniqueCount / (double)build_order_list.size() ) { // 
                //Parent 2 must match the build of the first one.
                build_order_out = build_order_win[parent_1];
                while (build_order_out != build_order_win[parent_2]) {
                    parent_2 = unif_dist_to_win_count(gen); // get a matching parent.
                }

                if (!_LEARNING_MODE) {
                    parent_2 = parent_1;
                }

                delta_out = MeatAIModule::bindBetween(pow(delta_win[parent_1], crossover)* pow(delta_win[parent_2], (1 - crossover)), 0., 1.);
                gamma_out = MeatAIModule::bindBetween(pow(gamma_win[parent_1], crossover) * pow(gamma_win[parent_2], (1 - crossover)), 0., 1.);
                a_army_out = MeatAIModule::bindBetween(pow(a_army_win[parent_1], crossover) * pow(a_army_win[parent_2], (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
                a_econ_out = MeatAIModule::bindBetween(pow(a_econ_win[parent_1], crossover) * pow(a_econ_win[parent_2], (1 - crossover)), 0., 1.);
                a_tech_out = MeatAIModule::bindBetween(pow(a_tech_win[parent_1], crossover) * pow(a_tech_win[parent_2], (1 - crossover)), 0., 1.);
                r_out = MeatAIModule::bindBetween(pow(r_win[parent_1], crossover) * pow(r_win[parent_2], (1 - crossover)), 0., 1.);
            }
            else { // we must need diversity.  
                // use the random values we have determined in the beginning and the random opening.
            }

            //if we won our last game, change nothing.
            //if (games_since_last_win == 0) {
            //    parent_1 = build_order_win.size() - 1; // safe even if there is only 1 win., index starts at 0.
            //    parent_2 = build_order_win.size() - 1;
            //    build_order_out = build_order_win.back();// vectors start at 0.

            //    delta_out = delta_win[parent_1];
            //    gamma_out = gamma_win[parent_1];
            //    a_army_out = a_army_win[parent_1];
            //    a_econ_out = a_econ_win[parent_1];
            //    a_tech_out = a_tech_win[parent_1];
            //    r_out =   r_win[parent_1];
            //}
        }

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

        //linear_crossover, interior of parents.
        //a_army_out  = crossover * a_army_win[parent_1] + (1 - crossover) * a_army_win[parent_2];  
        //a_econ_out  = crossover * a_econ_win[parent_1] + (1 - crossover) * a_econ_win[parent_2];
        //a_tech_out  = crossover * a_tech_win[parent_1] + (1 - crossover) * a_tech_win[parent_2];
        //r_out       = crossover * r_win[parent_1]      + (1 - crossover) * r_win[parent_2];

        loss_rate_ = 1 - prob_win_given_conditions /*(double)win_count / (double)relevant_game_count*/;

    }

    if (_TRAINING_AGAINST_BASE_AI) {
        //From genetic history, random parent for each gene. Mutate the genome
        std::uniform_int_distribution<size_t> unif_dist_to_mutate(0, 3);
        std::normal_distribution<double> normal_mutation_size(0, 0.05);

        size_t mutation_0 = unif_dist_to_mutate(gen); // rand int between 0-5
                                                      //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.

        double mutation = normal_mutation_size(gen); // 

                                                                 // Chance of mutation.
        if (dis(gen) > 0.95) {
            // dis(gen) > (games_since_last_win /(double)(games_since_last_win + 5)) * loss_rate_ // might be worth exploring.

            //a_vis_out_mutate_; // currently does nothing, vision is an artifact atm.
            a_army_out_mutate_ = mutation_0 == 0 ? MeatAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
            a_econ_out_mutate_ = mutation_0 == 1 ? MeatAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
            a_tech_out_mutate_ = mutation_0 == 2 ? MeatAIModule::bindBetween(a_tech_out + mutation, 0., 1.) : a_tech_out;
            r_out_mutate_ =      mutation_0 == 3 ? MeatAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;

        }
        else {

            delta_out_mutate_ = delta_out;
            gamma_out_mutate_ = gamma_out;
            a_army_out_mutate_ = a_army_out;
            a_econ_out_mutate_ = a_econ_out;
            a_tech_out_mutate_ = a_tech_out;
            r_out_mutate_ = r_out;

        }

        // Normalize the CD part of the gene with CAPITAL AUGMENTING TECHNOLOGY.
        double a_tot = a_army_out_mutate_ + a_econ_out_mutate_;
        a_army_out_mutate_ = a_army_out_mutate_ / a_tot;
        a_econ_out_mutate_ = a_econ_out_mutate_ / a_tot;
        a_tech_out_mutate_ = a_tech_out_mutate_; // this is no longer normalized.
        build_order_ = build_order_out;

    }
    else {

        //for (int i = 0; i < 1000; i++) {  // no corner solutions, please. Happens with incredibly small values 2*10^-234 ish.

            //From genetic history, random parent for each gene. Mutate the genome
            std::uniform_int_distribution<size_t> unif_dist_to_mutate(0, 5);
            std::normal_distribution<double> normal_mutation_size(0, 0.05);

            size_t mutation_0 = unif_dist_to_mutate(gen); // rand int between 0-5
            //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.

            double mutation = normal_mutation_size(gen); // will generate rand double between 0.99 and 1.01.

            // Chance of mutation.
            if (dis(gen) > 0.95 || selected_win_count < 50 ) {
                // dis(gen) > (games_since_last_win /(double)(games_since_last_win + 5)) * loss_rate_ // might be worth exploring.
                delta_out_mutate_ = mutation_0 == 0 ? MeatAIModule::bindBetween(delta_out + mutation, 0., 1.) : delta_out;
                gamma_out_mutate_ = mutation_0 == 1 ? MeatAIModule::bindBetween(gamma_out + mutation, 0., 1.) : gamma_out;
                a_army_out_mutate_ = mutation_0 == 2 ? MeatAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
                a_econ_out_mutate_ = mutation_0 == 3 ? MeatAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
                a_tech_out_mutate_ = mutation_0 == 4 ? MeatAIModule::bindBetween(a_tech_out + mutation, 0., 1.) : a_tech_out;
                r_out_mutate_ =      mutation_0 == 5 ? MeatAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;

            }
            else {

                delta_out_mutate_ = delta_out;
                gamma_out_mutate_ = gamma_out;
                a_army_out_mutate_ = a_army_out;
                a_econ_out_mutate_ = a_econ_out;
                a_tech_out_mutate_ = a_tech_out;
                r_out_mutate_ = r_out;

            }

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

            //if (a_army_out_mutate_ > 0.01 && a_econ_out_mutate_ > 0.25 && a_tech_out_mutate_ > 0.01 && a_tech_out_mutate_ < 0.50
            //    && delta_out_mutate_ < 0.55 && delta_out_mutate_ > 0.40 && gamma_out_mutate_ < 0.55 && gamma_out_mutate_ > 0.20) {
            //    break; // if we have an interior solution, let's use it, if not, we try again.
            //}
        //}
    }
}
