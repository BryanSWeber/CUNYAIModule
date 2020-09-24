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
        max_value_ = max(max_value_, StoredUnit::getTraditionalWeight(u));
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
            string temp = to_string(2.0*static_cast<double>(StoredUnit(u).stock_value_) / static_cast<double>(max_value_) - 1.0);
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
    string e_name = CUNYAIModule::safeString(Broodwar->enemy()->getName().c_str());
    string e_race = CUNYAIModule::safeString(Broodwar->enemy()->getRace().c_str());
    string map_name = CUNYAIModule::safeString(Broodwar->mapFileName().c_str());

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<double> dis(0, 1);    // default values for output.
    int population_size = e_race == CUNYAIModule::safeString(BWAPI::Races::Zerg.c_str()) ? 3 : build_order_list.size(); // default size of "breeding population". Typically, larger sizes are better to ensure a maximum but smaller sizes converge faster. ZvZ has fewer viable openings.

    double gas_proportion_out = dis(gen);
    double supply_ratio_out = dis(gen) * 0.3 + 0.3; // Artifically chosen upper and lower bounds. But outside of this, they often get truely silly.

    // the values below will be normalized to 1.
    double a_army_out = dis(gen);
    double a_econ_out = dis(gen);
    double a_tech_out = 3*dis(gen);

    double r_out = dis(gen);
    //No longer used.
    double a_vis_out = dis(gen);




    HistoryEntry parent_1;
    HistoryEntry parent_2;

    string build_order_out;

    std::uniform_int_distribution<size_t> rand_bo(0, build_order_list.size() - 1);
    size_t build_order_rand = rand_bo(gen);

    build_order_out = build_order_list[0];

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

    vector<HistoryEntry> game_data;
    vector<HistoryEntry> game_data_well_matched;

    ifstream input; // brings in info;
    input.open( (writeDirectory + "history.txt").c_str(), ios::in);   // for each row
    string line;
    int csv_length = 0;
    while (getline(input, line)) {
        ++csv_length;
    }
    input.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

    input.open( (writeDirectory + "history.txt").c_str(), ios::in);
    getline(input, line); //skip the first line of the document.
    csv_length--; // that means the remaining csv is shorter by 1 line.
    for (int j = 0; j < csv_length; ++j) {
        // Adding each line as a HistoryEntry class.
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
        int score_building;
        int score_kills;
        int score_raze;
        int score_units;
        int detector_count;
        int flyers;
        int duration;

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

        getline(input, entry, ',');
        score_building = stoi(entry);
        getline(input, entry, ',');
        score_kills = stoi(entry);
        getline(input, entry, ',');
        score_raze = stoi(entry);
        getline(input, entry, ',');
        score_units = stoi(entry);
        getline(input, entry, ',');
        detector_count = stoi(entry);
        getline(input, entry, ',');
        flyers = stoi(entry);

        getline(input, entry); //diff. End of line char, not ','
        duration = stoi(entry);


        HistoryEntry a_game = HistoryEntry(gas_proportion_total, supply_ratio_total,
                                            a_army_total, a_econ_total, a_tech_total, r_total, race_total, win_total,
                                            sdelay_total, mdelay_total, ldelay_total, name_total, map_name_total,
                                            enemy_average_army_, enemy_average_econ_, enemy_average_tech_, build_order_total,
                                            score_building, score_kills, score_raze, score_units, detector_count, flyers, duration);
        game_data.push_back(a_game);
    } // closure for each row
    input.close();

    //What model is this? It's greedy...


    double game_score = 0;
    int game_count = 0;

    for (vector<HistoryEntry>::reverse_iterator game_iter = game_data.rbegin(); game_iter != game_data.rend(); game_iter++) {
        bool name_matches = game_iter->name_total_ == e_name;
        bool race_matches = game_iter->race_total_ == e_race;
        bool map_matches = game_iter->map_name_total_ == map_name;

        if (name_matches) {
            double weight_of_match_quality = 0.80 * name_matches + 0.10 * race_matches + 0.10 * map_matches;
            game_score += weight_of_match_quality * getOutcomeScore(game_iter->win_total_, game_iter->score_building_, game_iter->score_kills_, game_iter->score_raze_, game_iter->score_units_);
            game_count++;
        }
    }

    if (game_count <= population_size - 1) {
        build_order_out = build_order_list[game_count];
    }
    else {
        build_order_out = build_order_list[build_order_rand];
    }

    if (e_race == CUNYAIModule::safeString(BWAPI::Races::Zerg.c_str()))
        build_order_out = build_order_list[build_order_rand % population_size]; // if opponent is zerg there are really only 3 viable openings (the first 3)

    // start from most recent and count our way back from there.
    for (vector<HistoryEntry>::reverse_iterator game_iter = game_data.rbegin(); game_iter != game_data.rend(); game_iter++) {
        bool name_matches = game_iter->name_total_ == e_name;
        bool race_matches = game_iter->race_total_ == e_race;
        bool map_matches = game_iter->map_name_total_ == map_name;
        bool opening_matches = game_iter->opening_ == build_order_out;

        double weight_of_match_quality = 0.80 * name_matches + 0.10 * race_matches + 0.10 * map_matches;
        double weighted_game_score = getOutcomeScore(game_iter->win_total_, game_iter->score_building_, game_iter->score_kills_, game_iter->score_raze_, game_iter->score_units_);

        if (game_count > population_size) {
            if (weight_of_match_quality * weighted_game_score >= max(game_count > 0 ? game_score / game_count : 0, 100.0)) // either you won in a match or you did fairly well by our standards
                game_data_well_matched.push_back(*game_iter);
        }
        else {
            if (opening_matches && game_iter->win_total_) // Or you won with this opening (for broad cross-pollination).
                game_data_well_matched.push_back(*game_iter);
        }

        if (game_data_well_matched.size() >= population_size)
            break;
    } //or widest hunt possible.


    if (game_data_well_matched.size() >= population_size) { // redefine final output.

        std::uniform_int_distribution<size_t> unif_dist_to_win_count(0, game_data_well_matched.size() - 1); // safe even if there is only 1 win., index starts at 0.
        size_t rand_parent_1 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        size_t rand_parent_2 = unif_dist_to_win_count(gen); // choose a random 'parent'.
        parent_1 = game_data_well_matched[rand_parent_1];
        parent_2 = game_data_well_matched[rand_parent_2];

        double crossover = dis(gen); //crossover, interior of parents. Big mutation at the end, though.

        gas_proportion_out = CUNYAIModule::bindBetween(pow(parent_1.gas_proportion_total_, crossover) * pow(parent_2.gas_proportion_total_, (1. - crossover)), 0., 1.);
        supply_ratio_out = CUNYAIModule::bindBetween(pow(parent_1.supply_ratio_total_, crossover) * pow(parent_2.supply_ratio_total_, (1. - crossover)), 0.0, 1.);
        a_army_out = CUNYAIModule::bindBetween(pow(parent_1.a_army_total_, crossover) * pow(parent_2.a_army_total_, (1. - crossover)), 0., 1.);  //geometric crossover, interior of parents.
        a_econ_out = CUNYAIModule::bindBetween(pow(parent_1.a_econ_total_, crossover) * pow(parent_2.a_econ_total_, (1. - crossover)), 0., 1.);
        a_tech_out = CUNYAIModule::bindBetween(pow(parent_1.a_tech_total_, crossover) * pow(parent_2.a_tech_total_, (1. - crossover)), 0., 3.);
        r_out = CUNYAIModule::bindBetween(pow(parent_1.r_total_, crossover) * pow(parent_2.r_total_, (1. - crossover)), 0., 1.);
        build_order_out = dis(gen) > 0.5 ? parent_1.opening_ : parent_2.opening_;
    }

    //From genetic history, random parent for each gene. Mutate the genome
    std::uniform_int_distribution<size_t> unif_dist_to_mutate(0, 6);
    std::normal_distribution<double> normal_mutation_size(0, 0.5);

    // Chance of mutation.
    if (dis(gen) > 0.25) {

        //genetic mutation rate ought to slow with success. Consider the following approach: Ackley (1987) suggested that mutation probability is analogous to temperature in simulated annealing.
        size_t mutation_0 = unif_dist_to_mutate(gen); // rand int between 0-5
        double mutation = normal_mutation_size(gen); // will generate rand double between 0.99 and 1.01.

        gas_proportion_t0 = mutation_0 == 0 ? CUNYAIModule::bindBetween(gas_proportion_out + mutation, 0., 1.) : gas_proportion_out;
        supply_ratio_t0 = mutation_0 == 1 ? CUNYAIModule::bindBetween(supply_ratio_out + mutation, 0.3, 0.6) : supply_ratio_out;
        a_army_t0 = mutation_0 == 2 ? CUNYAIModule::bindBetween(a_army_out + mutation, 0., 1.) : a_army_out;
        a_econ_t0 = mutation_0 == 3 ? CUNYAIModule::bindBetween(a_econ_out + mutation, 0., 1.) : a_econ_out;
        a_tech_t0 = mutation_0 == 4 ? CUNYAIModule::bindBetween(a_tech_out + mutation, 0., 3.) : a_tech_out;
        r_out_t0 = mutation_0 == 5 ? CUNYAIModule::bindBetween(r_out + mutation, 0., 1.) : r_out;
        build_order_t0 = mutation_0 == 6 ? build_order_list[build_order_rand] : build_order_out;

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

    build_order_t0 = "12hatch pool drone extract drone drone drone drone drone drone hydra_den drone overlord drone drone drone grooved_spines hydra hydra hydra hydra hydra hydra hydra overlord hydra hydra hydra hydra hydra hatch extract"; 
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
    vector<double> all_units(BWAPI::UnitTypes::allUnitTypes().size()+1);

    //Get benchmarks for inclusion as "good scores"
    int game_score = 0;
    int game_count = 0;
    for (int j = 0; j < csv_length; ++j) {
        vector<double> local_copy = all_units;
        for (auto &i : local_copy) {
            getline(input, val, ',');
            i = stod(val);
        }
        if (local_copy.back() > 0) {
            game_score += local_copy.back();
            game_count++;
        }
    }

    //Include all "good scores" appropriately.
    for (int j = 0; j < csv_length; ++j) {
        vector<double> local_copy = all_units;
        for (auto &i:local_copy) {
             getline(input, val, ',');
             i = stod(val);
        }
        if (local_copy.back() > 0) {
            int weight = local_copy.back();
            if (weight >= max(game_score/game_count, 100)) {
                matrix_of_unit_weights.push_back(local_copy); //if we did well, keep it.
            }
        }
    }

    Diagnostics::DiagnosticText("%d",matrix_of_unit_weights.size());
    vector<vector<double>> matrix_of_unit_weights_reversed;

    //We only want the N most recent wins, so older genes will "die".
    if (!matrix_of_unit_weights.empty()) {
        for (auto r = matrix_of_unit_weights.rbegin(); r < matrix_of_unit_weights.rend(); r++) {
            matrix_of_unit_weights_reversed.push_back(*r);
            if (matrix_of_unit_weights_reversed.size() >= 5)
                break;
        }
    }

    //Generate more if we don't have enough, otherwise combine some winners with a mutation chance:
    if (matrix_of_unit_weights_reversed.empty() || matrix_of_unit_weights_reversed.size() < 5) {
        //We're good, use the random one.
    }
    else {
        std::uniform_int_distribution<size_t> rand_bo(0, matrix_of_unit_weights_reversed.size() - 1);
        size_t row_choice1 = rand_bo(gen);
        size_t row_choice2 = rand_bo(gen);
        size_t row_choice3 = rand_bo(gen); // three parents. More stability since each set (funtionally) only trains one race.

        for (int x = 0; x <= BWAPI::UnitTypes::allUnitTypes().size(); ++x) {
            passed_unit_weights[x] = (matrix_of_unit_weights_reversed[row_choice1][x] + matrix_of_unit_weights_reversed[row_choice2][x] + matrix_of_unit_weights_reversed[row_choice3][x]) / 3.0;
            if (dis(gen) > 0.98) { // 1% chance of mutation.
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

double LearningManager::getOutcomeScore(const bool isWinner, const int buildScore, const int killScore, const int razeScore, const int unitScore) {
    return isWinner * sqrt(500000) + (1 - isWinner) * sqrt(buildScore + killScore + razeScore + unitScore);
}

HistoryEntry::HistoryEntry()
{

}

HistoryEntry::HistoryEntry(double gas_proportion_total, double supply_ratio_total, double a_army_total, double a_econ_total, double a_tech_total, double r_total, string race_total, bool win_total, int sdelay_total, int mdelay_total, int ldelay_total, string name_total, string map_name_total, double enemy_average_army, double enemy_average_econ, double enemy_average_tech, string opening, int score_building, int score_kills, int score_raze, int score_units, int detector_count, int flyers, int duration)
{
    gas_proportion_total_ = gas_proportion_total;
    supply_ratio_total_ = supply_ratio_total;
    a_army_total_ = a_army_total;
    a_econ_total_ = a_econ_total;
    a_tech_total_ = a_tech_total;
    r_total_ = r_total;
    race_total_ = race_total;
    win_total_ = win_total;
    sdelay_total_ = sdelay_total;
    mdelay_total_ = mdelay_total;
    ldelay_total_ = ldelay_total;
    name_total_ = name_total;
    map_name_total_ = map_name_total;
    enemy_average_army_ = enemy_average_army;
    enemy_average_econ_ = enemy_average_econ;
    enemy_average_tech_ = enemy_average_tech;
    opening_ = opening;
    score_building_ = score_building;
    score_kills_ = score_kills;
    score_raze_ = score_raze;
    score_units_ = score_units;
    detector_count_ = detector_count;
    flyers_ = flyers;
    duration_ = duration;
}
