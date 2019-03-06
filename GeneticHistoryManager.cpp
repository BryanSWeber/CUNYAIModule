#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include "Source\GeneticHistoryManager.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>
# include <algorithm>
# include <set>
# include <random> // C++ base random is low quality.


using namespace BWAPI;
using namespace Filter;
using namespace std;

GeneticHistory::GeneticHistory() {};

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { delta_out, gamma_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
GeneticHistory::GeneticHistory(string file) {

    //srand( Broodwar->getRandomSeed() ); // don't want the BW seed if the seed is locked. 

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    double delta_out = dis(gen) * 0.6;
    double gamma_out = dis(gen) * 0.6; // Artifically chosen upper bounds. But above this, they often get truely silly.
    // the values below will be normalized to 1.
    double a_army_out = dis(gen);
    double a_econ_out = dis(gen);
    double a_tech_out = dis(gen);
    //double r_out = log(85 / (double)4) / (double)(14400 + dis(gen) * (25920 - 14400)); //Typical game maxes vary from 12.5min to 16 min according to antiga. Assumes a range from 4 to max in 10 minutes, (14400 frames) to 18 minutes 25920 frames
    double r_out = dis(gen);
    //No longer used.
    double a_vis_out = dis(gen);

    // drone drone drone drone drone overlord drone drone drone hatch pool   // 12-hatch
    // drone drone drone drone drone overlord pool extractor// overpool
    // drone pool ling ling ling // 5-pool.
    // drone drone drone drone drone overlord drone drone drone hatch pool extract ling lair drone drone drone drone drone ovi speed spire extract ovi ovi muta muta muta muta muta muta muta muta muta muta muta // 12 hatch into muta.
    // "drone drone drone drone overlord drone drone drone drone hatch drone drone pool drone drone extract drone drone drone drone drone drone lair drone drone drone drone drone drone drone drone drone drone spire overlord drone overlord hatch drone drone drone drone drone drone drone drone drone drone muta muta muta muta muta muta muta muta muta muta muta muta hatch"; //zerg_3hatchmuta: 
    // "drone drone drone drone overlord drone drone drone drone hatch drone drone pool drone drone extract drone drone drone drone drone drone lair drone drone drone drone drone drone drone drone drone drone spire overlord drone overlord hatch drone drone drone drone drone drone drone drone hatch drone extract drone hatch scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge scourge hatch extract extract hatch"; // zerg_3hatchscourge ??? UAB

    string build_order_out;


    vector<string> build_order_list = {
        "drone drone drone drone drone overlord pool drone creep drone drone", // The blind sunken. For the bots that just won't take no for an answer.
        "drone pool drone drone ling ling ling overlord ling ling ling ling ling ling ling ling", // 5pool with some commitment.
        "drone drone drone drone drone overlord pool drone drone", // 9pool gasless
        "drone drone drone drone drone overlord pool drone extract drone drone", // 9pool
        "drone drone drone drone drone overlord drone drone drone pool drone extract hatch ling ling ling speed", // 12-pool tenative.
        "drone drone drone drone drone overlord drone drone drone hatch pool drone drone", // 12hatch-pool
        "drone drone drone drone drone pool drone extract overlord drone ling ling ling lair drone overlord drone hydra_den hydra hydra hydra hydra ling ling ling ling lurker_tech", //1 h lurker, tenative.
        "drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling lurker lurker lurker lurker ling ling", // 2h lurker
        //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone ling ling ling drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken",  // 2 h turtle, tenative. Dies because the first hatch does not have creep by it when it is time to build.
        //"drone drone drone drone drone overlord drone drone drone hatch pool drone drone drone drone drone drone drone creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
        //"drone drone drone drone drone overlord drone drone drone pool creep drone sunken creep drone sunken creep drone sunken creep drone sunken evo drone creep spore", // Sunken Testing build. Superpassive.
        "drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone ling drone drone lair overlord drone drone speed drone drone drone drone drone drone drone drone spire drone extract drone creep drone creep drone sunken sunken overlord overlord overlord muta muta muta muta muta muta muta muta muta muta muta muta", // 2h - Muta. Extra overlord is for safety.
       "drone drone drone drone drone pool drone extract overlord drone ling ling ling hydra_den drone drone drone drone", //zerg_9pool to hydra one base.
       "drone drone drone drone drone overlord drone drone drone hatch drone drone drone hatch drone drone drone hatch drone drone drone overlord pool", //supermacro cheese
       "drone drone drone drone overlord drone drone drone hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone grooved_spines hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract", //zerg_2hatchhydra -range added an overlord.
       "drone drone drone drone overlord drone drone drone hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone muscular_augments hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract" //zerg_2hatchhydra - speed. added an overlord.
    };

    std::uniform_int_distribution<size_t> rand_bo(0, build_order_list.size() - 1);
    size_t build_order_rand = rand_bo(gen);

    build_order_out = build_order_list[build_order_rand];


    int selected_win_count = 0;
    int selected_lose_count = 0;
    int win_count[3] = { 0,0,0 }; //player, race, map.
    int lose_count[3] = { 0,0,0 }; // player, race, map.
    int relevant_game_count = 0;
    int losing_player_map_race = 0;
    int losing_player_map = 0;
    int losing_player_race = 0;
    int losing_map_race = 0;
    int losing_player = 0;
    int losing_map = 0;
    int losing_race = 0;
    int games_since_last_win = 999; // starting at 0 makes the script think it WON the last game. 999 is a functional marker for having never won.
    double prob_win_given_opponent;

    string entry; // entered string from stream
    //std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, string>;  // all stats for the game.
    //vector<double> delta_total;
    //vector<double> gamma_total;
    //vector<double> a_army_total;
    //vector<double> a_econ_total;
    //vector<double> a_tech_total;
    //vector<double> r_total;
    //vector<string> race_total;
    //vector<int> win_total;
    //vector<int> sdelay_total;
    //vector<int> mdelay_total;
    //vector<int> ldelay_total;
    //vector<string> name_total;
    //vector<string> map_name_total;
    //vector<string> build_order_total;

    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> a_game; //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> parent_1; //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> parent_2; //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)

    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data; //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_well_matched;//(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_partial_match;//(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_parent_match;//(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)


    vector<double> r_win;
    vector<string> map_name_win;
    vector<string> build_order_win;

    std::set<string> build_orders_best_match;
    std::set<string> build_orders_partial_match;
    std::set<string> build_orders_untried;

    loss_rate_ = 1;

    ifstream input; // brings in info;
    input.open(file, ios::in);
    // for each row, m8

    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open(file, ios::in); //ios.in?

    for (int j = 0; j < csv_length; ++j) { // further brute force inelegance.
        // The ugly tuple.
        double delta_total;
        double gamma_total;
        double a_army_total;
        double a_econ_total;
        double a_tech_total;
        double r_total;
        string race_total;
        bool win_total;
        int sdelay_total;
        int mdelay_total;
        int ldelay_total;
        string name_total;
        string map_name_total;
		double enemy_average_army_;
		double enemy_average_econ_;
		double enemy_average_tech_;
        string build_order_total;

        getline(input, entry, ',');
        delta_total=stod(entry);

        getline(input, entry, ',');
        gamma_total=stod(entry);

        getline(input, entry, ',');
        a_army_total=stod(entry);

        getline(input, entry, ',');
        a_econ_total=stod(entry);
        getline(input, entry, ',');
        a_tech_total=stod(entry);
        getline(input, entry, ',');
        r_total=stod(entry);

        getline(input, entry, ',');
        race_total=entry;

        getline(input, entry, ',');
        win_total=static_cast<bool>(stoi(entry));

        getline(input, entry, ',');
        sdelay_total=stoi(entry);
        getline(input, entry, ',');
        mdelay_total=stoi(entry);
        getline(input, entry, ',');
        ldelay_total=stoi(entry);

        getline(input, entry, ',');
        name_total=entry;
        getline(input, entry, ',');
        map_name_total=entry;

		getline(input, entry, ',');
		enemy_average_army_ = stod(entry);
		getline(input, entry, ',');
		enemy_average_econ_ = stod(entry);
		getline(input, entry, ',');
		enemy_average_tech_ = stod(entry);

        getline(input, entry); //diff. End of line char, not ','
        build_order_total=entry;

        a_game = std::make_tuple(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, build_order_total);
        game_data.push_back(a_game);
		Broodwar->sendText("Average Army Was:%d", enemy_average_army_);
		Broodwar->sendText("Average Econ Was:%d", enemy_average_econ_);
		Broodwar->sendText("Average Tech Was:%d", enemy_average_tech_);

    } // closure for each row

    string e_name = Broodwar->enemy()->getName().c_str();
    string e_race = Broodwar->enemy()->getRace().c_str();
    string map_name = Broodwar->mapFileName().c_str();


    for (int j = 0; j < csv_length; ++j) { // what is the best conditional to use? Keep in mind we would like variation.
                                           //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)


        if (std::get<11>(game_data[j]) == e_name) {
            if (std::get<7>(game_data[j]) == 1) { 
                game_data_partial_match.push_back(game_data[j]);
                win_count[0]++;
            }
            else lose_count[0]++;
        }

        if (std::get<6>(game_data[j]) == e_race) {
            if (std::get<7>(game_data[j]) == 1) {
                game_data_partial_match.push_back(game_data[j]);
                win_count[1]++;
            }
            else lose_count[1]++;
        }

        if (std::get<12>(game_data[j]) == map_name) {
            if (std::get<7>(game_data[j]) == 1) {
                game_data_partial_match.push_back(game_data[j]);
               win_count[2]++;
            }
            else lose_count[2]++;
        }

    }

    //What model is this? It's greedy...


    double rand_value = dis(gen);



    // start from most recent and count our way back from there.
    for (vector<tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string>>::reverse_iterator game_iter = game_data_partial_match.rbegin(); game_iter != game_data_partial_match.rend(); game_iter++) {
        //(delta_total, gamma_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)

        bool conditions_for_inclusion = true;
        int counter = 0;

        bool name_matches = std::get<11>(*game_iter) == e_name;
        bool race_matches = std::get<6>(*game_iter) == e_race;
        bool map_matches = std::get<12>(*game_iter) == map_name;
        bool game_won = std::get<7>(*game_iter);

        // an inelegant statement follows. How do I make this into a switch?

        if (win_count[0] > 0 && win_count[1] > 0 &&win_count[2] > 0) { //choice in race for random players is like a whole new ball park. Let's only look at player/map collisions. Race is if there's no player data.
            conditions_for_inclusion = name_matches && race_matches && map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] > 0 &&win_count[2] == 0) {
            conditions_for_inclusion = name_matches && race_matches && !map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] == 0 &&win_count[2] > 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }
        else if (win_count[0] == 0 && win_count[1] > 0 &&win_count[2] > 0) {
            conditions_for_inclusion = !name_matches && race_matches && map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] > 0 &&win_count[2] == 0) {
            conditions_for_inclusion = name_matches && race_matches && !map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] == 0 &&win_count[2] == 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }

        if (conditions_for_inclusion && game_won) {
            game_data_well_matched.push_back(*game_iter);
            games_since_last_win = 0;
        }
        else if (conditions_for_inclusion && !game_won) {
            games_since_last_win++;
        }

        if (game_data_well_matched.size() >= 10) { // stop once we have 50 games in the parantage.
            break;
        }
    } //or widest hunt possible.


    if (game_data_well_matched.size() > 0) { // redefine final output.

        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_well_matched.size() - 1); // safe even if there is only 1 win., index starts at 0.
        size_t rand_parent_1 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        parent_1 = game_data_well_matched[rand_parent_1];
        string opening_of_choice = std::get<16>(parent_1); // its matching parents must have a similar opening.

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

        //if we don't need diversity, combine our old wins together.

        if (dis(gen) <  (game_data_well_matched.size() - 1) / static_cast<double>(game_data_well_matched.size())) { // 
            //Parent 2 must match the build of the first one.
            for (auto potential_parent : game_data_well_matched) {
                if (std::get<16>(potential_parent) == opening_of_choice) {
                    game_data_parent_match.push_back(potential_parent);
                }
            }

            std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_parent_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
            size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
            parent_2 = game_data_parent_match[rand_parent_2];


            if constexpr (!LEARNING_MODE) {
                parent_2 = parent_1;
            }

            delta_out = CUNYAIModule::bindBetween(pow(std::get<0>(parent_1), crossover) * pow(std::get<0>(parent_2), (1 - crossover)), 0., 1.);
            gamma_out = CUNYAIModule::bindBetween(pow(std::get<1>(parent_1), crossover) * pow(std::get<1>(parent_2), (1 - crossover)), 0., 1.);
            a_army_out = CUNYAIModule::bindBetween(pow(std::get<2>(parent_1), crossover) * pow(std::get<2>(parent_2), (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
            a_econ_out = CUNYAIModule::bindBetween(pow(std::get<3>(parent_1), crossover) * pow(std::get<3>(parent_2), (1 - crossover)), 0., 1.);
            a_tech_out = CUNYAIModule::bindBetween(pow(std::get<4>(parent_1), crossover) * pow(std::get<4>(parent_2), (1 - crossover)), 0., 1.);
            r_out      = CUNYAIModule::bindBetween(pow(std::get<5>(parent_1), crossover) * pow(std::get<5>(parent_2), (1 - crossover)), 0., 1.);
        }
        else { // we must need diversity.  
            // use the random values we have determined in the beginning and the random opening.
        }

    }
    else if( game_data_partial_match.size() > 0 ){ // do our best with the partial match data.
        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_partial_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
        size_t rand_parent_1 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        parent_1 = game_data_partial_match[rand_parent_1];
        string opening_of_choice = std::get<16>(parent_1); // its matching parents must have a similar opening.

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

                                     //if we don't need diversity, combine our old wins together.

        if (dis(gen) <  (game_data_partial_match.size() - 1) / static_cast<double>(game_data_partial_match.size())) { // 
                                                                                                                    //Parent 2 must match the build of the first one.
            for (auto potential_parent : game_data_partial_match) {
                if (std::get<16>(potential_parent) == opening_of_choice) {
                    game_data_parent_match.push_back(potential_parent);
                }
            }

            std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_parent_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
            size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
            parent_2 = game_data_parent_match[rand_parent_2];


            if constexpr (!LEARNING_MODE) {
                parent_2 = parent_1;
            }

            delta_out = CUNYAIModule::bindBetween(pow(std::get<0>(parent_1), crossover) * pow(std::get<0>(parent_2), (1 - crossover)), 0., 1.);
            gamma_out = CUNYAIModule::bindBetween(pow(std::get<1>(parent_1), crossover) * pow(std::get<1>(parent_2), (1 - crossover)), 0., 1.);
            a_army_out = CUNYAIModule::bindBetween(pow(std::get<2>(parent_1), crossover) * pow(std::get<2>(parent_2), (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
            a_econ_out = CUNYAIModule::bindBetween(pow(std::get<3>(parent_1), crossover) * pow(std::get<3>(parent_2), (1 - crossover)), 0., 1.);
            a_tech_out = CUNYAIModule::bindBetween(pow(std::get<4>(parent_1), crossover) * pow(std::get<4>(parent_2), (1 - crossover)), 0., 1.);
            r_out = CUNYAIModule::bindBetween(pow(std::get<5>(parent_1), crossover) * pow(std::get<5>(parent_2), (1 - crossover)), 0., 1.);
        }
        else { // we must need diversity.  
               // use the random values we have determined in the beginning and the random opening.
        }
    }

    prob_win_given_opponent = fmax(win_count[0] / static_cast<double>(win_count[0] + lose_count[0]), 0.0);
    loss_rate_ = 1 - prob_win_given_opponent;


    //for (int i = 0; i < 1000; i++) {  // no corner solutions, please. Happens with incredibly small values 2*10^-234 ish.

    //From genetic history, random parent for each gene. Mutate the genome
    std::uniform_int_distribution<size_t> unif_dist_to_mutate(0, 5);
    std::normal_distribution<double> normal_mutation_size(0, 0.05);

    size_t mutation_0 = unif_dist_to_mutate(gen); // rand int between 0-5
    //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.

    double mutation = normal_mutation_size(gen); // will generate rand double between 0.99 and 1.01.

    // Chance of mutation.
    if (dis(gen) > 0.95 || selected_win_count < 10) {
        // dis(gen) > (games_since_last_win /(double)(games_since_last_win + 5)) * loss_rate_ // might be worth exploring.
        delta_out_mutate_ = mutation_0 == 0 ? CUNYAIModule::bindBetween(delta_out + mutation, 0., 1.) : delta_out;
        gamma_out_mutate_ = mutation_0 == 1 ? CUNYAIModule::bindBetween(gamma_out + mutation, 0., 1.) : gamma_out;
        a_army_out_mutate_ = mutation_0 == 2 ? CUNYAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
        a_econ_out_mutate_ = mutation_0 == 3 ? CUNYAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
        a_tech_out_mutate_ = mutation_0 == 4 ? CUNYAIModule::bindBetween(a_tech_out + mutation, 0., 1.) : a_tech_out;
        r_out_mutate_ = mutation_0 == 5 ? CUNYAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;

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

    // Overwrite whatever you previously wanted if we're using "test mode".
    if constexpr (TEST_MODE) {
        // Values altered 
        delta_out_mutate_ = 0.3021355;
        gamma_out_mutate_ = 0.35;
        a_army_out_mutate_ = 0.511545;
        a_econ_out_mutate_ = 0.488455;
        a_tech_out_mutate_ = 0.52895;
        r_out_mutate_ = 0.5097605;

        build_order_ = "drone drone drone drone drone pool drone extract overlord drone ling ling ling hydra_den drone drone drone drone"; //Standard Opener

    }
}
