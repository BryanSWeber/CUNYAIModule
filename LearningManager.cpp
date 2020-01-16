#pragma once
// Remember not to use "Broodwar" in any global class constructor!
# include "Source\CUNYAIModule.h"
# include "Source\LearningManager.h"
# include "Source\Diagnostics.h"
# include <fstream>
# include <BWAPI.h>
# include <cstdlib>
# include <cstring>
# include <ctime>
# include <string>
# include <algorithm>
# include <set>
# include <random> // C++ base random is low quality.
# include <thread>
# include <filesystem>
# include <pybind11/pybind11.h>
# include <pybind11/embed.h>
# include <pybind11/eval.h>
# include <Python.h>

using namespace BWAPI;
using namespace Filter;
using namespace std;
namespace py = pybind11;

bool LearningManager::confirmHistoryPresent()
{
    rename( (readDirectory + "history.txt").c_str(), (writeDirectory + "history.txt").c_str()); // Copy our history to the write folder. There needs to be a file called history.txt.

    if constexpr (PRINT_PARENT) {
        ofstream a; // Prints to brood war file while in the WRITE file. ** Works at home, prints in directory of CUNYbot.exe
        a.open(".\\test.txt", ios_base::app);
        a << "This is the parent directory, friend!" << endl;
        a.close();
    }

    ifstream input; // brings in info;
    input.open( (writeDirectory + "history.txt").c_str(), ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open( (writeDirectory + "history.txt").c_str(), ios_base::app);
        output << "gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening, score_building, score_kills, score_raze, score_units, detector_count, flyers, duration" << endl;
        output.close();
        return false;
    }

    return true;
}

// Returns average of historical wins against that race for key heuristic values. For each specific value:[0...5] : { gas_proportion_out, supply_ratio_out, a_army_out, a_vis_out, a_econ_out, a_tech_out };
void LearningManager::initializeGeneticLearning() {

    //srand( Broodwar->getRandomSeed() ); // don't want the BW seed if the seed is locked.

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    double gas_proportion_out = dis(gen);
    double supply_ratio_out = dis(gen) * 0.6; // Artifically chosen upper bounds. But above this, they often get truely silly.
    // the values below will be normalized to 1.
    double a_army_out = dis(gen);
    double a_econ_out = dis(gen);
    double a_tech_out = 3*dis(gen);
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
    //vector<double> gas_proportion_total;
    //vector<double> supply_ratio_total;
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

    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> a_game; //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> parent_1; //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> parent_2; //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)

    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data; //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_well_matched;//(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_partial_match;//(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)
    vector< std::tuple< double, double, double, double, double, double, string, bool, int, int, int, string, string, double, double, double, string> > game_data_parent_match;//(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)


    vector<double> r_win;
    vector<string> map_name_win;
    vector<string> build_order_win;

    std::set<string> build_orders_best_match;
    std::set<string> build_orders_partial_match;
    std::set<string> build_orders_untried;

    loss_rate_ = 1;

    ifstream input; // brings in info;
    input.open( (writeDirectory + "history.txt").c_str(), ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open( (writeDirectory + "history.txt").c_str(), ios::in);   // for each row
    csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open( (writeDirectory + "history.txt").c_str(), ios::in);
    getline(input, line); //skip the first line of the document.
    csv_length--; // that means the remaining csv is shorter by 1 line.
    for (int j = 0; j < csv_length; ++j) {
        // The ugly tuple.
        double gas_proportion_total;
        double supply_ratio_total;
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
        gas_proportion_total = stod(entry);

        getline(input, entry, ',');
        supply_ratio_total = stod(entry);

        getline(input, entry, ',');
        a_army_total = stod(entry);

        getline(input, entry, ',');
        a_econ_total = stod(entry);
        getline(input, entry, ',');
        a_tech_total = stod(entry);
        getline(input, entry, ',');
        r_total = stod(entry);

        getline(input, entry, ',');
        race_total = entry;

        getline(input, entry, ',');
        win_total = static_cast<bool>(stoi(entry));

        getline(input, entry, ',');
        sdelay_total = stoi(entry);
        getline(input, entry, ',');
        mdelay_total = stoi(entry);
        getline(input, entry, ',');
        ldelay_total = stoi(entry);

        getline(input, entry, ',');
        name_total = entry;
        getline(input, entry, ',');
        map_name_total = entry;

        getline(input, entry, ',');
        enemy_average_army_ = stod(entry);
        getline(input, entry, ',');
        enemy_average_econ_ = stod(entry);
        getline(input, entry, ',');
        enemy_average_tech_ = stod(entry);

        getline(input, entry, ',');
        build_order_total = entry;

        //Remaining entries for score, dectectors , game duration - skip.

        getline(input, entry); //diff. End of line char, not ','

        a_game = std::make_tuple(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, build_order_total);
        game_data.push_back(a_game);
    } // closure for each row
    input.close();

    string e_name = Broodwar->enemy()->getName().c_str();
    string e_race = Broodwar->enemy()->getRace().c_str();
    string map_name = Broodwar->mapFileName().c_str();


    for (int j = 0; j < csv_length; ++j) { // what is the best conditional to use? Keep in mind we would like variation.
                                           //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)


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
        //(gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening)

        bool conditions_for_inclusion = true;
        int counter = 0;

        bool name_matches = std::get<11>(*game_iter) == e_name;
        bool race_matches = std::get<6>(*game_iter) == e_race;
        bool map_matches = std::get<12>(*game_iter) == map_name;
        bool game_won = std::get<7>(*game_iter);

        // an inelegant statement follows. How do I make this into a switch?

        if (win_count[0] > 0 && win_count[1] > 0 && win_count[2] > 0) { //choice in race for random players is like a whole new ball park. Let's only look at player/map collisions. Race is if there's no player data.
            conditions_for_inclusion = name_matches && race_matches && map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] > 0 && win_count[2] == 0) {
            conditions_for_inclusion = name_matches && race_matches && !map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] == 0 && win_count[2] > 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }
        else if (win_count[0] > 0 && win_count[1] == 0 && win_count[2] == 0) {
            conditions_for_inclusion = name_matches && !race_matches && !map_matches;
        }
        else if (win_count[0] == 0 && win_count[1] > 0 && win_count[2] > 0) {
            conditions_for_inclusion = !name_matches && race_matches && map_matches;
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

        if (dis(gen) < (game_data_well_matched.size() - 1) / static_cast<double>(game_data_well_matched.size())) { //
            //Parent 2 must match the build of the first one.
            for (auto potential_parent : game_data_well_matched) {
                if (std::get<16>(potential_parent) == opening_of_choice) {
                    game_data_parent_match.push_back(potential_parent);
                }
            }

            std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_parent_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
            size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
            parent_2 = game_data_parent_match[rand_parent_2];


            if constexpr (!INTERBREED_PARENTS) {
                parent_2 = parent_1;
            }

            gas_proportion_out = CUNYAIModule::bindBetween(pow(std::get<0>(parent_1), crossover) * pow(std::get<0>(parent_2), (1 - crossover)), 0., 1.);
            supply_ratio_out = CUNYAIModule::bindBetween(pow(std::get<1>(parent_1), crossover) * pow(std::get<1>(parent_2), (1 - crossover)), 0., 1.);
            a_army_out = CUNYAIModule::bindBetween(pow(std::get<2>(parent_1), crossover) * pow(std::get<2>(parent_2), (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
            a_econ_out = CUNYAIModule::bindBetween(pow(std::get<3>(parent_1), crossover) * pow(std::get<3>(parent_2), (1 - crossover)), 0., 1.);
            a_tech_out = CUNYAIModule::bindBetween(pow(std::get<4>(parent_1), crossover) * pow(std::get<4>(parent_2), (1 - crossover)), 0., 3.);
            r_out = CUNYAIModule::bindBetween(pow(std::get<5>(parent_1), crossover) * pow(std::get<5>(parent_2), (1 - crossover)), 0., 1.);
        }
        else { // we must need diversity.
            // use the random values we have determined in the beginning and the random opening.
        }

    }
    else if (game_data_partial_match.size() > 0) { // do our best with the partial match data.
        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_partial_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
        size_t rand_parent_1 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        parent_1 = game_data_partial_match[rand_parent_1];
        string opening_of_choice = std::get<16>(parent_1); // its matching parents must have a similar opening.

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

                                     //if we don't need diversity, combine our old wins together.

        if (dis(gen) < (game_data_partial_match.size() - 1) / static_cast<double>(game_data_partial_match.size())) { //
                                                                                                                    //Parent 2 must match the build of the first one.
            for (auto potential_parent : game_data_partial_match) {
                if (std::get<16>(potential_parent) == opening_of_choice) {
                    game_data_parent_match.push_back(potential_parent);
                }
            }

            std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_parent_match.size() - 1); // safe even if there is only 1 win., index starts at 0.
            size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
            parent_2 = game_data_parent_match[rand_parent_2];


            if constexpr (!INTERBREED_PARENTS) {
                parent_2 = parent_1;
            }

            gas_proportion_out = CUNYAIModule::bindBetween(pow(std::get<0>(parent_1), crossover) * pow(std::get<0>(parent_2), (1 - crossover)), 0., 1.);
            supply_ratio_out = CUNYAIModule::bindBetween(pow(std::get<1>(parent_1), crossover) * pow(std::get<1>(parent_2), (1 - crossover)), 0., 1.);
            a_army_out = CUNYAIModule::bindBetween(pow(std::get<2>(parent_1), crossover) * pow(std::get<2>(parent_2), (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
            a_econ_out = CUNYAIModule::bindBetween(pow(std::get<3>(parent_1), crossover) * pow(std::get<3>(parent_2), (1 - crossover)), 0., 1.);
            a_tech_out = CUNYAIModule::bindBetween(pow(std::get<4>(parent_1), crossover) * pow(std::get<4>(parent_2), (1 - crossover)), 0., 3.);
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
        gas_proportion_t0 = mutation_0 == 0 ? CUNYAIModule::bindBetween(gas_proportion_out + mutation, 0., 1.) : gas_proportion_out;
        supply_ratio_t0 = mutation_0 == 1 ? CUNYAIModule::bindBetween(supply_ratio_out + mutation, 0., 1.) : supply_ratio_out;
        a_army_t0 = mutation_0 == 2 ? CUNYAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
        a_econ_t0 = mutation_0 == 3 ? CUNYAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
        a_tech_t0 = mutation_0 == 4 ? CUNYAIModule::bindBetween(a_tech_out + mutation, 0., 3.) : a_tech_out;
        r_out_t0 = mutation_0 == 5 ? CUNYAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;

    }
    else {

        gas_proportion_t0 = gas_proportion_out;
        supply_ratio_t0 = supply_ratio_out;
        a_army_t0 = a_army_out;
        a_econ_t0 = a_econ_out;
        a_tech_t0 = a_tech_out;
        r_out_t0 = r_out;

    }

    // Normalize the CD part of the gene.
    //double a_tot = a_army_out_mutate_ + a_econ_out_mutate_ + a_tech_out_mutate_;
    //a_army_out_mutate_ = a_army_out_mutate_ / a_tot;
    //a_econ_out_mutate_ = a_econ_out_mutate_ / a_tot;
    //a_tech_out_mutate_ = a_tech_out_mutate_ / a_tot;

    // Normalize the CD part of the gene with CAPITAL AUGMENTING TECHNOLOGY.
    double a_tot = a_army_t0 + a_econ_t0;
    a_army_t0 = a_army_t0 / a_tot;
    a_econ_t0 = a_econ_t0 / a_tot;
    a_tech_t0 = a_tech_t0; // this is no longer normalized.
    build_order_t0 = build_order_out;

    //if (a_army_out_mutate_ > 0.01 && a_econ_out_mutate_ > 0.25 && a_tech_out_mutate_ > 0.01 && a_tech_out_mutate_ < 0.50
    //    && gas_proportion_out_mutate_ < 0.55 && gas_proportion_out_mutate_ > 0.40 && supply_ratio_out_mutate_ < 0.55 && supply_ratio_out_mutate_ > 0.20) {
    //    break; // if we have an interior solution, let's use it, if not, we try again.
    //}
    //}
}

void LearningManager::initializeRFLearning()
{
    //Python loading of critical libraries.
    Diagnostics::DiagnosticText("Python Initializing");
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive. Cannot be used more than once in a game.
    py::object scope = py::module::import("__main__").attr("__dict__");

    //Executing script:
    Diagnostics::DiagnosticText("Loading Dictionaries");
    auto local = py::dict();
    bool abort_code = false;
    local["e_race"] = CUNYAIModule::safeString(Broodwar->enemy()->getRace().c_str());
    local["e_name"] = CUNYAIModule::safeString(Broodwar->enemy()->getName().c_str());
    local["e_map"] = CUNYAIModule::safeString(Broodwar->mapFileName().c_str());
    local["in_file"] = ".\\write\\history.txt"; // Back directory command '..' is not something you can confidently pass to python here so we have to manually rig our path there.

    local["gas_proportion_t0"] = 0;
    local["supply_ratio_t0"] = 0;
    local["a_army_t0"] = 0;
    local["a_econ_t0"] = 0;
    local["a_tech_t0"] = 0;
    local["r_out_t0"] = 0;
    local["build_order_t0"] = 0;
    local["attempt_count"] = 0;
    local["abort_code_t0"] = false;

    gas_proportion_t0 = py::float_(local["gas_proportion_t0"]);
    supply_ratio_t0 = py::float_(local["supply_ratio_t0"]);
    a_army_t0 = py::float_(local["a_army_t0"]);
    a_econ_t0 = py::float_(local["a_econ_t0"]);
    a_tech_t0 = py::float_(local["a_tech_t0"]);
    r_out_t0 = py::float_(local["r_out_t0"]);
    build_order_t0 = py::str(local["build_order_t0"]);
    abort_code = py::bool_(local["abort_code_t0"]);

    Diagnostics::DiagnosticText("Evaluating Kiwook.py");
    py::eval_file(".\\kiwook.py", scope, local);
    Diagnostics::DiagnosticText("Evaluation Complete.");

    //Pull the abort code, should be false if we got through, otherwise if true we aborted.
    abort_code = py::bool_(local["abort_code_t0"]);
    string result = abort_code ? "YES" : "NO";
    string print_string = "Did we abort the RF process?: " + result;

    Diagnostics::DiagnosticText(print_string.c_str());

    if (abort_code) {
        initializeGeneticLearning();
    }
    else {
        gas_proportion_t0 = py::float_(local["gas_proportion_t0"]);
        supply_ratio_t0 = py::float_(local["supply_ratio_t0"]);
        a_army_t0 = py::float_(local["a_army_t0"]);
        a_econ_t0 = py::float_(local["a_econ_t0"]);
        a_tech_t0 = py::float_(local["a_tech_t0"]);
        r_out_t0 = py::float_(local["r_out_t0"]);
        build_order_t0 = py::str(local["build_order_t0"]);
    }
    return;
}


// Overwrite whatever you previously wanted if we're using "test mode".
void LearningManager::initializeTestStart(){
    // Values altered
    gas_proportion_t0 = 0.3021355;
    supply_ratio_t0 = 0.35;
    a_army_t0 = 0.511545;
    a_econ_t0 = 0.488455;
    a_tech_t0 = 0.52895;
    r_out_t0 = 0.5097605;

    build_order_t0 = "drone drone drone drone drone overlord drone drone drone hatch pool extract drone drone drone drone ling ling ling overlord lair drone drone drone speed drone drone drone overlord hydra_den drone drone drone drone lurker_tech creep drone creep drone sunken sunken drone drone drone drone drone overlord overlord hydra hydra hydra hydra ling ling lurker lurker lurker lurker ling ling"; // 2h lurker; //Standard Opener

}

//Otherwise, use random build order and values from above
void LearningManager::initializeRandomStart(){
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.

    std::uniform_int_distribution<size_t> rand_bo(0, build_order_list.size() - 1);
    size_t build_order_rand = rand_bo(gen);

    gas_proportion_t0 = dis(gen);
    supply_ratio_t0 = dis(gen);
    a_army_t0 = dis(gen);
    a_econ_t0 = 1 - a_army_t0;
    a_tech_t0 = dis(gen) * 3;
    r_out_t0 = dis(gen);
    build_order_t0 = build_order_list[build_order_rand];
}

void LearningManager::initializeUnitWeighting()
{
    ifstream input; // brings in info;
    input.open((writeDirectory + "UnitWeights.txt").c_str(), ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 2) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open((writeDirectory + "UnitWeights.txt").c_str(), ios_base::trunc); // wipe if it does not start at the correct place.

        string name_of_units = "";
        for (auto u : BWAPI::UnitTypes::allUnitTypes()) {
            string temp = u.getName().c_str();
            name_of_units += temp + ",";
        }
        name_of_units += "win";
        output << name_of_units << endl;

        string weight_of_units = "";
        for (UnitType u : BWAPI::UnitTypes::allUnitTypes()) {
            string temp = to_string(Stored_Unit(u).stock_value_);
            weight_of_units += temp + ",";
        }
        name_of_units += "0";
        output << weight_of_units << endl;

        output.close();
    }

    map<UnitType, int> unit_weights;

    //std::cout << "Python Initialization..." << std::endl;

    py::scoped_interpreter guard{}; // start the interpreter and keep it alive. Cannot be used more than once in a game.
    py::object cma = py::module::import("cma");

    py::object es = cma.attr("CMAEvolutionStrategy");
    int zeros [5] = { 0,0,0,0,0 };
    py::object starting_conditions = py::cast(zeros);

    py::object inital_SD = py::cast(0.5);
    py::object RosenbackFunction = cma.attr("ff").attr("rosen");
    py::object initialized_es = es(starting_conditions, inital_SD); //Crashes here.
    py::object fit_es = initialized_es.attr("optimize")(RosenbackFunction);


    //py::print(fit_es.attr("result_pretty"));
}
