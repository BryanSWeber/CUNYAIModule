#pragma once

#include <BWAPI.h>
#include "CUNYAIModule.h"

// Manages various types of important diagnostics and print functions.
struct Diagnostics {
    static void onFrame();
    static void Diagnostic_Line(const Position &s_pos, const Position &f_pos, const Position &screen_pos, Color col);
    static void Diagnostic_Tiles(const Position & screen_pos, Color col);
    static void Diagnostic_Watch_Position(TilePosition & tp);
    static void Diagnostic_Destination(const Unit_Inventory & ui, const Position & screen_pos, Color col);
    static void Diagnostic_Dot(const Position & s_pos, const Position & screen_pos, Color col);
    static void DiagnosticHitPoints(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticFAP(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticDeath(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticLastDamage(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticMineralsRemaining(const Stored_Resource unit, const Position & screen_pos);
    static void DiagnosticSpamGuard(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticLastOrder(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticPhase(const Stored_Unit unit, const Position & screen_pos);
    static void DiagnosticReservations(const Reservation reservations, const Position & screen_pos);

    static void DiagnosticTrack(const Unit & u);

    void DiagnosticTrack(const Position & p);

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
        }
    }
    // Defunct: Outlines the case where you cannot attack their type (air/ground/cloaked), while they can attack you.
};