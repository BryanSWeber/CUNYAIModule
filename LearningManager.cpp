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
# include <pybind11/stl.h> //Needed for arrays.
# include <pybind11/stl_bind.h>
# include "pybind11/numpy.h"
# include <Python.h>

using namespace BWAPI;
using namespace Filter;
using namespace std;
namespace py = pybind11;

bool LearningManager::confirmLearningFilesPresent()
{
    rename( (readDirectory + "history.txt").c_str(), (writeDirectory + "history.txt").c_str()); // Copy our history to the write folder. There needs to be a file called history.txt.

    if constexpr (PRINT_WD) {
        ofstream a; // Prints to brood war file while in the WRITE file. ** Works at home, prints in directory of CUNYbot.exe
        a.open(".\\test.txt", ios_base::app);
        a << "This is the parent directory, friend!" << endl;
        a.close();
    }

    //Get the history file ready.
    ifstream historyFile; // brings in info;
    historyFile.open( (writeDirectory + "history.txt").c_str(), ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(historyFile, line)) {
        ++csv_length;
    }
    historyFile.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    if (csv_length < 1) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open( (writeDirectory + "history.txt").c_str(), ios_base::app);
        output << "gas_proportion_total, supply_ratio_total, a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total, sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total, enemy_average_army_, enemy_average_econ_, enemy_average_tech_, opening, score_building, score_kills, score_raze, score_units, detector_count, flyers, duration" << endl;
        output.close();
        return false;
    }

    // Nukes the Debug file between games.
    ofstream output; 
    output.open((writeDirectory + "Debug.txt").c_str(), ios_base::trunc);
    output.close();

    rename((readDirectory + "UnitWeights.txt").c_str(), (writeDirectory + "UnitWeights.txt").c_str()); // Copy our UnitWeights to the write folder. There needs to be a file called history.txt.

    //Get the unit weights file ready.
    ifstream unitweightsFile; // brings in info;
    unitweightsFile.open((writeDirectory + "UnitWeights.txt").c_str(), ios::in);   // for each row
    csv_length = 0;
    while (getline(unitweightsFile, line)) {
        ++csv_length;
    }
    unitweightsFile.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    max_value_ = 0;
    for (UnitType u : BWAPI::UnitTypes::allUnitTypes()) {
        max_value_ = max(max_value_, Stored_Unit::getTraditionalWeight(u));
    }

    //Provide default starting values if there is nothing else to use.
    if (csv_length < 2) {
        ofstream output; // Prints to brood war file while in the WRITE file.
        output.open((writeDirectory + "UnitWeights.txt").c_str(), ios_base::trunc); // wipe if it does not start at the correct place.

        string name_of_units = "";
        for (auto u : BWAPI::UnitTypes::allUnitTypes()) {
            string temp = u.getName().c_str();
            name_of_units += temp + ",";
        }
        name_of_units += "Score";
        output << name_of_units << endl;

        string weight_of_units = "";
        for (UnitType u : BWAPI::UnitTypes::allUnitTypes()) {
            string temp = to_string(2.0*static_cast<double>(Stored_Unit(u).stock_value_) / static_cast<double>(max_value_) - 1.0);
            weight_of_units += temp + ",";
        }
        weight_of_units += "100";
        output << weight_of_units << endl;

        output.close();
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

        for (int i = 0; i < name_matches + race_matches + map_matches; i++) { //add once for each match, 3x if it matches well.
            game_data_well_matched.push_back(*game_iter);
        }

    } //or widest hunt possible.


    if (game_data_well_matched.size() > 5) { // redefine final output.

        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_well_matched.size() - 1); // safe even if there is only 1 win., index starts at 0.
        size_t rand_parent_1 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        parent_1 = game_data_well_matched[rand_parent_1];
        parent_2 = game_data_well_matched[rand_parent_2];

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

        gas_proportion_out = CUNYAIModule::bindBetween(pow(std::get<0>(parent_1), crossover) * pow(std::get<0>(parent_2), (1 - crossover)), 0., 1.);
        supply_ratio_out = CUNYAIModule::bindBetween(pow(std::get<1>(parent_1), crossover) * pow(std::get<1>(parent_2), (1 - crossover)), 0., 1.);
        a_army_out = CUNYAIModule::bindBetween(pow(std::get<2>(parent_1), crossover) * pow(std::get<2>(parent_2), (1 - crossover)), 0., 1.);  //geometric crossover, interior of parents.
        a_econ_out = CUNYAIModule::bindBetween(pow(std::get<3>(parent_1), crossover) * pow(std::get<3>(parent_2), (1 - crossover)), 0., 1.);
        a_tech_out = CUNYAIModule::bindBetween(pow(std::get<4>(parent_1), crossover) * pow(std::get<4>(parent_2), (1 - crossover)), 0., 3.);
        r_out = CUNYAIModule::bindBetween(pow(std::get<5>(parent_1), crossover) * pow(std::get<5>(parent_2), (1 - crossover)), 0., 1.);
        build_order_out = dis(gen) > 0.5 ? std::get<16>(parent_1) : std::get<16>(parent_2);
    }

    prob_win_given_opponent = fmax(win_count[0] / static_cast<double>(win_count[0] + lose_count[0]), 0.0);

    //From genetic history, random parent for each gene. Mutate the genome
    std::uniform_int_distribution<size_t> unif_dist_to_mutate(0, 6);
    std::normal_distribution<double> normal_mutation_size(0, 0.10);

    // Chance of mutation.
    if (dis(gen) > 0.25) {

        //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.
        size_t mutation_0 = unif_dist_to_mutate(gen); // rand int between 0-5
        double mutation = normal_mutation_size(gen); // will generate rand double between 0.99 and 1.01.

        gas_proportion_t0 = mutation_0 == 0 ? CUNYAIModule::bindBetween(gas_proportion_out + mutation, 0., 1.) : gas_proportion_out;
        supply_ratio_t0 = mutation_0 == 1 ? CUNYAIModule::bindBetween(supply_ratio_out + mutation, 0., 1.) : supply_ratio_out;
        a_army_t0 = mutation_0 == 2 ? CUNYAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
        a_econ_t0 = mutation_0 == 3 ? CUNYAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
        a_tech_t0 = mutation_0 == 4 ? CUNYAIModule::bindBetween(a_tech_out + mutation, 0., 3.) : a_tech_out;
        r_out_t0 = mutation_0 == 5 ? CUNYAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;
        build_order_t0 = mutation_0 == 6 ? std::get<16>(parent_1) : build_order_out;

    }
    else {

        gas_proportion_t0 = gas_proportion_out;
        supply_ratio_t0 = supply_ratio_out;
        a_army_t0 = a_army_out;
        a_econ_t0 = a_econ_out;
        a_tech_t0 = a_tech_out;
        r_out_t0 = r_out;
        build_order_t0 = build_order_out;

    }


    // Normalize the CD part of the gene with CAPITAL AUGMENTING TECHNOLOGY.
    double a_tot = a_army_t0 + a_econ_t0;
    a_army_t0 = a_army_t0 / a_tot;
    a_econ_t0 = a_econ_t0 / a_tot;
    a_tech_t0 = a_tech_t0; // this is no longer normalized.
    build_order_t0 = build_order_out;

}

void LearningManager::initializeRFLearning()
{
    //Python loading of critical libraries.
    Diagnostics::DiagnosticText("Python Initializing");
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive. Cannot be used more than once in a game.
    Diagnostics::DiagnosticText("Loading Main");
    py::object scope = py::module::import("__main__").attr("__dict__");

    //Executing script:
    Diagnostics::DiagnosticText("Loading Dictionary Contents");
    auto local = py::dict();
    bool abort_code = false;
    local["e_race"] = CUNYAIModule::safeString(Broodwar->enemy()->getRace().c_str());
    local["e_name"] = CUNYAIModule::safeString(Broodwar->enemy()->getName().c_str());
    local["e_map"] = CUNYAIModule::safeString(Broodwar->mapFileName().c_str());
    local["in_file"] = ".\\write\\history.txt"; // Back directory command '..' is not something you can confidently pass to python here so we have to manually rig our path there.

    local["gas_proportion_t0"] = gas_proportion_t0 = 0;
    local["supply_ratio_t0"] = supply_ratio_t0 = 0;
    local["a_army_t0"] = a_army_t0 = 0;
    local["a_econ_t0"] = a_econ_t0 = 0;
    local["a_tech_t0"] = a_tech_t0 = 0;
    local["r_out_t0"] = r_out_t0 = 0;
    local["build_order_t0"] = build_order_t0 = "Undefined Build Order";
    local["attempt_count"] = 0;
    local["abort_code_t0"] = abort_code = false;
    Diagnostics::DiagnosticText("Evaluating Kiwook.py");
    try {
        py::eval_file(".\\kiwook.py", scope, local);
    }
    catch (py::error_already_set const &pythonErr) { 
        Diagnostics::DiagnosticText(pythonErr.what()); 
        local["abort_code_t0"] = true;
    }
    Diagnostics::DiagnosticText("Evaluation Complete.");

    //Pull the abort code, should be false if we got through, otherwise if true we aborted.
    abort_code = py::bool_(local["abort_code_t0"]);

    string result = abort_code ? "YES" : "NO";
    string print_string = "Did we abort the RF process?: " + result;

    Diagnostics::DiagnosticText(print_string.c_str());

    if (abort_code) {
        Diagnostics::DiagnosticText("We will then pick a random opening");
        initializeRandomStart();
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

void LearningManager::initializeCMAESUnitWeighting()
{
    py::scoped_interpreter guard{}; // start the interpreter and keep it alive. Cannot be used more than once in a game.
    py::object scope = py::module::import("__main__").attr("__dict__");

    //Executing script:
    Diagnostics::DiagnosticText("Loading Dictionary Contents");
    auto local = py::dict();
    vector<double> passed_unit_weights(BWAPI::UnitTypes::allUnitTypes().size());
    for (auto x : passed_unit_weights) {
        passed_unit_weights[x] = 0;
    }

    local["unit_weights"] = passed_unit_weights; //automatic conversion to type requires stl library. needs to be a vector, can't pass a map.
    try {
        py::eval_file(".\\evo_test.py", scope, local);
    }
    catch (py::error_already_set const &pythonErr) {
        Diagnostics::DiagnosticText(pythonErr.what());
    }


    passed_unit_weights = py::cast<std::vector<double>>(local["unit_weights"]);

    int pos = 0;
    for (UnitType u : BWAPI::UnitTypes::allUnitTypes()) {
        unit_weights.insert_or_assign(u, passed_unit_weights.at(pos));
        pos++;
    }
}

void LearningManager::initializeGAUnitWeighting()
{
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(-1, 1);    // default values for output.

    //Executing script:
    vector<double> passed_unit_weights(BWAPI::UnitTypes::allUnitTypes().size());
    for (auto &x : passed_unit_weights) {
        x = dis(gen);
    }
    string val;

    ifstream input; // brings in info;
    input.open((writeDirectory + "UnitWeights.txt").c_str(), ios::in);// for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open((writeDirectory + "UnitWeights.txt").c_str(), ios::in);
    getline(input, line); //skip the first line of the document.
    csv_length--; // that means the remaining csv is shorter by 1 line.
    vector<vector<double>> matrix_of_unit_weights;
    vector<double> entire_vector(BWAPI::UnitTypes::allUnitTypes().size()+1);

    for (int j = 0; j < csv_length; ++j) {
        vector<double> local_copy = entire_vector;
        for (auto &i:local_copy) {
             getline(input, val, ',');
             i = stod(val);
        }
        if (local_copy.back() > 0) {
            int weight = local_copy.back();
            while (weight > 0) {
                matrix_of_unit_weights.push_back(local_copy); //if we did well, keep it.
                weight -= 100;
            }
        }
    }

    Diagnostics::DiagnosticText("%d",matrix_of_unit_weights.size());
    vector<vector<double>> matrix_of_unit_weights_reversed;

    //We only want the 50 most recent wins, so older genes will "die".
    if (!matrix_of_unit_weights.empty()) {
        for (auto r = matrix_of_unit_weights.rbegin(); r < matrix_of_unit_weights.rend(); r++) {
            matrix_of_unit_weights_reversed.push_back(*r);
            if (matrix_of_unit_weights.size() > 50)
                break;
        }
    }

    //Generate more if we don't have enough, otherwise combine some winners with a mutation chance:
    if (matrix_of_unit_weights_reversed.empty() || matrix_of_unit_weights_reversed.size() <= 50) {
        //We're good, use the random one.
    }
    else {
        std::uniform_int_distribution<size_t> rand_bo(0, matrix_of_unit_weights_reversed.size() - 1);
        size_t row_choice1 = rand_bo(gen);
        size_t row_choice2 = rand_bo(gen);
        for (int x = 0; x <= BWAPI::UnitTypes::allUnitTypes().size(); ++x) {
            passed_unit_weights[x] = (matrix_of_unit_weights_reversed[row_choice1][x] + matrix_of_unit_weights_reversed[row_choice2][x])/2.0;
            if (dis(gen) > 0.10) { // 5% chance of mutation.
                passed_unit_weights[x] = dis(gen);
            }
        }
    }

    //submit results.
    int pos = 0;
    for (UnitType u : BWAPI::UnitTypes::allUnitTypes()) {
        unit_weights.insert_or_assign(u, passed_unit_weights.at(pos));
        pos++;
    }
}

int LearningManager::resetScale(const UnitType ut)
{
    map<UnitType, double>::iterator it = CUNYAIModule::learned_plan.unit_weights.find(ut);
    if (it != CUNYAIModule::learned_plan.unit_weights.end()) {
        return (CUNYAIModule::learned_plan.unit_weights.at(ut) + 1.0) / 2.0 * CUNYAIModule::learned_plan.max_value_;
    }
    return 0;
}
