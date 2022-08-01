#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"
#include "Build.h"
#include <chrono> // for in-game frame clock.
#include <map>

// Manages various types of important diagnostics and print functions.
class Diagnostics {
private:
    bool display = false;

public:
    static void onFrame();
    static void drawAllVelocities(const UnitInventory ui);
    static void drawAllHitPoints(const UnitInventory ui);
    static void drawAllMAFAPaverages(const UnitInventory ui);
    static void drawAllFutureDeaths(const UnitInventory ui);
    static void drawAllLastDamage(const UnitInventory ui);
    static void drawAllSpamGuards(const UnitInventory ui);
    static void drawStats();
    static void drawBullets();
    static void drawVisibilityData();
    static void showPlayers();
    static void showForces();
    static void drawLine(const Position &s_pos, const Position &f_pos, Color col);
    static void drawTiles(const Position & screen_pos, Color col);
    static void watchTile(TilePosition & tp);
    static void drawDestination(const UnitInventory & ui, const Position & screen_pos, Color col);
    static void drawDot(const Position & s_pos, const Position & screen_pos, Color col);
    static void drawCircle(const Position & s_pos, const Position & screen_pos, const int & radius, Color col);
    static void drawBar(const Position & s_pos, const UnitType uType, const int barAmountComplete, const int barAmountFull, const Color colTop, const Color colUnder); //Draws a bar underneath unit. Note that if multiple bars are being drawn, there may be strange overlap, BW does not draw them all in the order expected.
    static void drawHitPoints(const StoredUnit unit, const Position & screen_pos);
    static void drawFAP(const StoredUnit unit, const Position & screen_pos);
    static void drawEstimatedDeath(const StoredUnit unit, const Position & screen_pos);
    static void drawLastDamage(const StoredUnit unit, const Position & screen_pos);
    static void drawMineralsRemaining(const Stored_Resource unit, const Position & screen_pos);
    static void drawSpamGuard(const StoredUnit unit, const Position & screen_pos);
    static void printLastOrder(const StoredUnit unit, const Position & screen_pos);
    static void printPhase(const StoredUnit unit, const Position & screen_pos);
    static void drawReservations(const Reservation reservations, const Position & screen_pos);
    static void drawExpo();
    static void drawMousePosition();
    static void writeMap(Position pos, string s);
    static void DiagnosticTrack(const Unit & u);
    static void DiagnosticTrack(const Position & p);

    // Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
    static void printUpgrade_Inventory(const int &screen_x, const int &screen_y);
    // Announces to player the name and type of all known units in set.
    static void Print_UnitInventory(const int &screen_x, const int &screen_y, const UnitInventory &ui);
    static void Print_Test_Case(const int & screen_x, const int & screen_y);
    static void Print_Cached_Inventory(const int & screen_x, const int & screen_y);
    static void Print_ResearchInventory(const int & screen_x, const int & screen_y, const ResearchInventory & ri);
    // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
    static void Print_Build_Order_Remaining(const int & screen_x, const int & screen_y, const Build & bo);
    // Announces to player the name and type of all units remaining in the reservation system. Bland but practical.
    static void Print_Reservations(const int &screen_x, const int &screen_y, const Reservation &res);

    //Announces any cheats for single player games, if I am testing something. Examples - encircling opponent might be tested with invicibility on. Contains comedy.
    static void issueCheats();


    // Dumps most information about a player model to the debug file. Has extra printouts at various times.
    static void onFrameWritePlayerModel(PlayerModel &pmodel);

    // Dumps a collection of macro issues;
    static void writeMacroIssues();

    //Prints discription of an event to file.
    static void printUnitEventDetails(BWAPI::Event e);
    //Makes a short name for us to see what the map is called
    static std::string prettyRepName();
    //Makes a unique string for each event type.
    static std::string EventString(BWAPI::Event e);

    //Sends a diagnostic text message, accepts another argument..
    template<typename ...Ts>
    static void DiagnosticText(char const *fmt, Ts && ... vals) {
        if constexpr (DIAGNOSTIC_MODE) {
            Broodwar->sendText(fmt, std::forward<Ts>(vals) ...);
            cout << "\n" << fmt << "\n";
            ofstream output; // Prints to brood war file while in the WRITE file.
            output.open(CUNYAIModule::learnedPlan.getWriteDir() + "Debug.txt", ios_base::app);
            output << fmt;
            ((output << ',' << std::forward<Ts>(vals)), ...);
            output << endl;
            output.close();
        }
    }

    template<typename ...Ts>
    static void DiagnosticWrite(char const *fmt, Ts && ... vals) {
        if constexpr (DIAGNOSTIC_MODE) {
            ofstream output; // prints to brood war file while in the write file.
            output.open(CUNYAIModule::learnedPlan.getWriteDir() + "Debug.txt", ios_base::app);
            output << fmt;
            ((output << ',' << std::forward<Ts>(vals)), ...);
            output << endl;
            output.close();
        }
    }
};

//This simple class starts and stops a timer. It announces when it is finished and has a unique name.
class DiagnosticTimer {
private:
    std::chrono::high_resolution_clock::time_point clockStart = std::chrono::high_resolution_clock::now();
public:
    void clockFinish(const std::string p)
    {
        auto clockFinish = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(clockFinish - clockStart).count();
        if constexpr (DIAGNOSTIC_MODE)
            if(duration >= 55) std::cout << "This clock " << p << " took " << duration << "ms." << std::endl;
    }
};