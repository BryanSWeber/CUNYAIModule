#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

// Manages various types of important diagnostics and print functions.
struct Diagnostics {
    static void onFrame();
    static void drawAllVelocities(const Unit_Inventory ui);
    static void drawAllHitPoints(const Unit_Inventory ui);
    static void drawAllMAFAPaverages(const Unit_Inventory ui);
    static void drawAllFutureDeaths(const Unit_Inventory ui);
    static void drawAllLastDamage(const Unit_Inventory ui);
    static void drawAllSpamGuards(const Unit_Inventory ui);
    static void drawStats();
    static void drawBullets();
    static void drawVisibilityData();
    static void showPlayers();
    static void showForces();
    static void drawLine(const Position &s_pos, const Position &f_pos, const Position &screen_pos, Color col);
    static void drawTiles(const Position & screen_pos, Color col);
    static void watchTile(TilePosition & tp);
    static void drawDestination(const Unit_Inventory & ui, const Position & screen_pos, Color col);
    static void drawDot(const Position & s_pos, const Position & screen_pos, Color col);
    static void drawCircle(const Position & s_pos, const Position & screen_pos, const int & radius, Color col);
    static void drawHitPoints(const StoredUnit unit, const Position & screen_pos);
    static void drawFAP(const StoredUnit unit, const Position & screen_pos);
    static void drawEstimatedDeath(const StoredUnit unit, const Position & screen_pos);
    static void drawLastDamage(const StoredUnit unit, const Position & screen_pos);
    static void drawMineralsRemaining(const Stored_Resource unit, const Position & screen_pos);
    static void drawSpamGuard(const StoredUnit unit, const Position & screen_pos);
    static void printLastOrder(const StoredUnit unit, const Position & screen_pos);
    static void printPhase(const StoredUnit unit, const Position & screen_pos);
    static void drawReservations(const Reservation reservations, const Position & screen_pos);
    static void DiagnosticTrack(const Unit & u);
    static void DiagnosticTrack(const Position & p);

    // Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
    static void Print_Upgrade_Inventory(const int &screen_x, const int &screen_y);
    // Announces to player the name and type of all known units in set.
    static void Print_Unit_Inventory(const int &screen_x, const int &screen_y, const Unit_Inventory &ui);
    static void Print_Test_Case(const int & screen_x, const int & screen_y);
    static void Print_Cached_Inventory(const int & screen_x, const int & screen_y);
    static void Print_Research_Inventory(const int & screen_x, const int & screen_y, const Research_Inventory & ri);
    // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
    static void Print_Build_Order_Remaining(const int & screen_x, const int & screen_y, const Building_Gene & bo);
    // Announces to player the name and type of all units remaining in the reservation system. Bland but practical.
    static void Print_Reservations(const int &screen_x, const int &screen_y, const Reservation &res);
    

    //Sends a diagnostic text message, accepts another argument..
    template<typename ...Ts>
    static void DiagnosticText(char const *fmt, Ts && ... vals) {
        if constexpr (DIAGNOSTIC_MODE) {
            Broodwar->sendText(fmt, std::forward<Ts>(vals) ...);
            ofstream output; // Prints to brood war file while in the WRITE file.
            output.open(CUNYAIModule::learned_plan.writeDirectory + "Debug.txt", ios_base::app);
            output << fmt;
            ((output << ',' << std::forward<Ts>(vals)), ...);
            output << endl;
            output.close();
        }
    }
    // Defunct: Outlines the case where you cannot attack their type (air/ground/cloaked), while they can attack you.
};