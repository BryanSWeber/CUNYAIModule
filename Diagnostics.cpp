#pragma once

#include "Source/Diagnostics.h"
#include <BWAPI.h>
#include "Source/CUNYAIModule.h"
#include <chrono>
#include <map>

// This function limits the drawing that needs to be done by the bot.
void Diagnostics::drawLine(const Position &s_pos, const Position &f_pos, const Position &screen_pos, Color col = Colors::White) {
    if constexpr (DIAGNOSTIC_MODE) {
        if (CUNYAIModule::isOnScreen(s_pos, screen_pos) || CUNYAIModule::isOnScreen(f_pos, screen_pos)) {
            Broodwar->drawLineMap(s_pos, f_pos, col);
        }
    }
}

    // This function limits the drawing that needs to be done by the bot.
void Diagnostics::drawTiles(const Position &screen_pos, Color col = Colors::White) {
    if constexpr (DIAGNOSTIC_MODE) {
        for (int x = TilePosition(screen_pos).x; x <= TilePosition(screen_pos).x + 640 / 16; x += 2) {
            for (int y = TilePosition(screen_pos).y; y <= TilePosition(screen_pos).y + 480 / 16; y += 2) {
                Broodwar->drawTextMap(Position(TilePosition(x, y)), "(%d,%d)", x, y);
                break;
            }
        }
    }
}

    // This function limits the drawing that needs to be done by the bot.
void Diagnostics::watchTile(TilePosition &tp) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position centered = Position(TilePosition(tp.x - 640 / (4 * 16) + 2, tp.y - 480 / (4 * 16) + 1));
        Broodwar->setScreenPosition(centered);
    }
}


    // This function limits the drawing that needs to be done by the bot.
void Diagnostics::drawDestination(const UnitInventory &ui, const Position &screen_pos, Color col = Colors::White) {
    if constexpr (DIAGNOSTIC_MODE) {
        for (auto u : ui.unit_map_) {
            Position fin = u.second.pos_;
            Position start = u.second.bwapi_unit_->getTargetPosition();
            drawLine(start, fin, screen_pos, col);
        }
    }
}

    // This function limits the drawing that needs to be done by the bot.
void Diagnostics::drawDot(const Position &s_pos, const Position &screen_pos, Color col = Colors::White) {
    if constexpr (DIAGNOSTIC_MODE) {
        if (CUNYAIModule::isOnScreen(s_pos, screen_pos)) {
            Broodwar->drawCircleMap(s_pos, 25, col, true);
        }
    }
}

    // This function limits the drawing that needs to be done by the bot.
void Diagnostics::drawCircle(const Position &s_pos, const Position &screen_pos, const int &radius, Color col = Colors::White) {
    if constexpr (DIAGNOSTIC_MODE) {
        if (CUNYAIModule::isOnScreen(s_pos, screen_pos)) {
            Broodwar->drawCircleMap(s_pos, radius, col, false);
        }
    }
}

void Diagnostics::drawHitPoints(const StoredUnit unit, const Position &screen_pos) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (unit.valid_pos_ && CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.current_hp_ != unit.type_.maxHitPoints() + unit.type_.maxShields()) {
            // Draw the background.
            upper_left.y = upper_left.y + unit.type_.dimensionUp();
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 5;

            //Overlay the appropriate green above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * unit.current_hp_ / static_cast<double> (unit.type_.maxHitPoints() + unit.type_.maxShields()));
            lower_right.y = upper_left.y + 5;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Green, true);

            int temp_hp_value = (unit.type_.maxHitPoints() + unit.type_.maxShields());
            for (int i = 0; i <= static_cast<int>((unit.type_.maxHitPoints() + unit.type_.maxShields()) / 25); i++) {
                lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * temp_hp_value / static_cast<double>(unit.type_.maxHitPoints() + unit.type_.maxShields()));
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Black, false);
                temp_hp_value -= 25;
            }
        }
    }
}

void Diagnostics::drawFAP(const StoredUnit unit, const Position &screen_pos) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (unit.valid_pos_ && CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.future_fap_value_ > 0) {
            // Draw the red background.
            upper_left.y = upper_left.y + unit.type_.dimensionUp();
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 5;

            //Overlay the appropriate green above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * unit.future_fap_value_ / static_cast<double>(unit.stock_value_));
            lower_right.y = upper_left.y + 5;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::White, true);

            int temp_stock_value = unit.stock_value_;
            for (int i = 0; i <= static_cast<int>(unit.stock_value_ / 25); i++) {
                lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * temp_stock_value / static_cast<double>(unit.stock_value_));
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Black, false);
                temp_stock_value -= 25;
            }
        }
    }
}
void Diagnostics::drawEstimatedDeath(const StoredUnit unit, const Position &screen_pos) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (unit.valid_pos_ && CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.count_of_consecutive_predicted_deaths_ > 0) {
            // Draw the background.
            upper_left.y = upper_left.y + unit.type_.dimensionUp();
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 5;

            //Overlay the appropriate color above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * min(unit.count_of_consecutive_predicted_deaths_ / static_cast<double>(FAP_SIM_DURATION), 1.0));
            lower_right.y = upper_left.y + 5;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::White, true);

            for (int i = 0; i <= static_cast<int>(FAP_SIM_DURATION / 12); i++) {
                lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * i * 12 / static_cast<double>(FAP_SIM_DURATION));
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Black, false);
                //temp_stock_value -= 15;
            }
        }
    }
}

void Diagnostics::drawLastDamage(const StoredUnit unit, const Position &screen_pos) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (unit.valid_pos_ && CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.time_since_last_dmg_ > 0) {
            // Draw the background.
            upper_left.y = upper_left.y + unit.type_.dimensionUp();
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 5;

            //Overlay the appropriate color above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * min(unit.time_since_last_dmg_ / static_cast<double>(FAP_SIM_DURATION), 1.0));
            lower_right.y = upper_left.y + 5;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::White, true);

            for (int i = 0; i <= static_cast<int>(FAP_SIM_DURATION / 12); i++) {
                lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * i * 12 / static_cast<double>(FAP_SIM_DURATION));
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Black, false);
            }
        }
    }
}

void Diagnostics::drawMineralsRemaining(const Stored_Resource resource, const Position &screen_pos) {
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = resource.pos_;
        if (CUNYAIModule::isOnScreen(upper_left, screen_pos) && /*resource.current_stock_value_ != static_cast<double>(resource.max_stock_value_) &&*/ resource.occupied_resource_) {
            // Draw the orange background.
            upper_left.y = upper_left.y + resource.type_.dimensionUp();
            upper_left.x = upper_left.x - resource.type_.dimensionLeft();

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + resource.type_.width();
            lower_right.y = upper_left.y + 5;

            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Orange, false);

            //Overlay the appropriate blue above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + resource.type_.width() * resource.current_stock_value_ / static_cast<double>(resource.max_stock_value_));
            lower_right.y = upper_left.y + 5;
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Orange, true);

            //Overlay the 10hp rectangles over it.
        }
    }
}

void Diagnostics::drawSpamGuard(const StoredUnit unit, const Position & screen_pos)
{
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.time_since_last_command_ < 24) {
            // Draw the black background.
            upper_left.x = upper_left.x - unit.type_.dimensionLeft();
            upper_left.y = upper_left.y - 10;

            Position lower_right = upper_left;
            lower_right.x = upper_left.x + unit.type_.width();
            lower_right.y = upper_left.y + 5;

            if (unit.bwapi_unit_ && !CUNYAIModule::spamGuard(unit.bwapi_unit_))
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Red, false);
            else
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, false);

            //Overlay the appropriate grey above it.
            lower_right = upper_left;
            lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * (1 - min(unit.time_since_last_command_, 24) / static_cast<double>(24)));
            lower_right.y = upper_left.y + 5;

            if (unit.bwapi_unit_ && !CUNYAIModule::spamGuard(unit.bwapi_unit_))
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Red, true);
            else
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, true);
        }
    }
}

void Diagnostics::printLastOrder(const StoredUnit unit, const Position & screen_pos)
{
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = unit.pos_;
        if (CUNYAIModule::isOnScreen(upper_left, screen_pos)) {
            Broodwar->drawTextMap(unit.pos_, unit.order_.c_str());
        }
    }
}

void Diagnostics::printPhase(const StoredUnit unit, const Position & screen_pos)
{
    if constexpr (DIAGNOSTIC_MODE) {
        map<StoredUnit::Phase, string> enum_to_string = { { StoredUnit::Phase::None,"None" } ,
        { StoredUnit::Phase::Attacking,"Attacking" },
        { StoredUnit::Phase::Retreating,"Retreating" },
        { StoredUnit::Phase::Prebuilding,"Prebuilding" },
        { StoredUnit::Phase::PathingOut,"PathingOut" },
        { StoredUnit::Phase::PathingHome,"PathingHome" },
        { StoredUnit::Phase::Surrounding,"Surrounding" },
        { StoredUnit::Phase::NoRetreat,"NoRetreat" },
        { StoredUnit::Phase::MiningMin,"Gather Min" },
        { StoredUnit::Phase::MiningGas,"Gather Gas" },
        { StoredUnit::Phase::Returning,"Returning" },
        { StoredUnit::Phase::DistanceMining,"DistanceMining" },
        { StoredUnit::Phase::Clearing,"Clearing" },
        { StoredUnit::Phase::Upgrading,"Upgrading" },
        { StoredUnit::Phase::Researching,"Researching" },
        { StoredUnit::Phase::Morphing,"Morphing" },
        { StoredUnit::Phase::Building,"Building" },
        { StoredUnit::Phase::Detecting,"Detecting" } };
        Position upper_left = unit.pos_;
        if (CUNYAIModule::isOnScreen(upper_left, screen_pos) && unit.phase_ != StoredUnit::Phase::None) {
            Broodwar->drawTextMap(unit.pos_ + Position(unit.type_.dimensionDown(), unit.type_.dimensionRight()), enum_to_string[unit.phase_].c_str());
        }
    }
}

void Diagnostics::drawReservations(const Reservation reservations, const Position & screen_pos)
{
    if constexpr (DIAGNOSTIC_MODE) {
        for (auto const res : reservations.getReservedBuildings()) {
            Position upper_left = Position(res.first);
            Position lower_right = Position(res.first) + Position(32,32) + Position(res.second.width(), res.second.height()); //thank goodness I overloaded the + operator for the pathing operations! The +32 is because the drawing is off by a tile.
            if (CUNYAIModule::isOnScreen(upper_left, screen_pos)) {
                Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, true);
                Broodwar->drawTextMap(upper_left, res.second.c_str());
            }
        }
    }
}

void Diagnostics::drawExpo()
{
    Position pos = Position(CUNYAIModule::assemblymanager.getExpoPosition());
    if constexpr (DIAGNOSTIC_MODE) {
        Position upper_left = pos;
        Position lower_right = pos + Position(32, 32) + Position(Broodwar->self()->getRace().getResourceDepot().width(), Broodwar->self()->getRace().getResourceDepot().height()); //thank goodness I overloaded the + operator for the pathing operations! The +32 is because the drawing is off by a tile.
        if (CUNYAIModule::isOnScreen(upper_left, Broodwar->getScreenPosition())) {
            Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, true);
            Broodwar->drawTextMap(upper_left, Broodwar->self()->getRace().getResourceDepot().c_str());
        }
    }
}

void Diagnostics::drawMousePosition()
{
    if constexpr (DIAGNOSTIC_MODE) {
            Position p = Broodwar->getMousePosition() + Broodwar->getScreenPosition();
            Broodwar->drawTextMap(p, "T: %d, %d", TilePosition(p).x, TilePosition(p).y);
            Broodwar->drawTextMap(p + Position(0, 10), "P: %d, %d", p.x, p.y);

    }
}

void Diagnostics::writeMap(Position pos, string s)
{
    if constexpr (DIAGNOSTIC_MODE) {
        if (CUNYAIModule::isOnScreen(pos, Broodwar->getScreenPosition()))
            Broodwar->drawTextMap(pos, s.c_str());
    }
}


void Diagnostics::DiagnosticTrack(const Unit &u) {
    Broodwar->setScreenPosition(u->getPosition() - Position{ 320,200 });
}

void Diagnostics::DiagnosticTrack(const Position &p) {
    Broodwar->setScreenPosition(p - Position{ 320,200 });
}


    // Announces to player the name and count of all units in the unit inventory. Bland but practical.
void Diagnostics::Print_UnitInventory(const int &screen_x, const int &screen_y, const UnitInventory &ui) {
    int another_row_of_printing = 0;
    for (int i = 0; i != 229; i++)
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = CUNYAIModule::countUnits(((UnitType)i), ui);
        if (u_count > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Inventoried Units:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s: %d", CUNYAIModule::noRaceName(((UnitType)i).c_str()), u_count);  //
            another_row_of_printing++;
        }
    }
}

    // Prints some test onscreen in the given location.
void Diagnostics::Print_Test_Case(const int &screen_x, const int &screen_y) {
    int another_row_of_printing = 0;
    for (int i = 0; i != 229; i++)
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        if (((UnitType)i).isBuilding() && (!((UnitType)i).upgradesWhat().empty() || !((UnitType)i).researchesWhat().empty()) && ((UnitType)i) != UnitTypes::Zerg_Hatchery) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Confirmed Hits:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s", CUNYAIModule::noRaceName(((UnitType)i).c_str()));  //
            another_row_of_printing++;
        }
    }
}
    // Announces to player the name and count of all units in the unit inventory. Bland but practical.
void Diagnostics::Print_Cached_Inventory(const int &screen_x, const int &screen_y) {
    int another_row_of_printing = 0;
    for (auto i : UnitTypes::allUnitTypes())
    { // iterating through all known units. See unit type for enumeration, also at end of page.
        int u_count = CUNYAIModule::countUnits(i);
        int u_incomplete_count = CUNYAIModule::countUnitsInProgress(i);
        if (u_count > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Inventoried Units:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s: %d Inc: %d", CUNYAIModule::noRaceName(i.c_str()), u_count, u_incomplete_count);  //
            another_row_of_printing++;
        }
    }
}

    // Announces to player the name and count of all units in the research inventory. Bland but practical.
void Diagnostics::Print_ResearchInventory(const int &screen_x, const int &screen_y, const ResearchInventory &ri) {
    int another_row_of_printing_ups = 1;

    for (auto r : ri.getUpgrades())
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        if (r.second > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Upgrades:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_ups * 10, "%s: %d", r.first.c_str(), r.second);  //
            another_row_of_printing_ups++;
        }
    }

    int another_row_of_printing_research = another_row_of_printing_ups + 1;

    for (auto r : ri.getTech())
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        if (r.second) {
            Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_ups * 10, "Tech:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_research * 10, "%s", r.first.c_str());  //
            another_row_of_printing_research++;
        }
    }

    int another_row_of_printing_buildings = another_row_of_printing_research + 1;

    for (auto r : ri.getTechBuildings())
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        if (r.second > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_research * 10, "R.Buildings:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_buildings * 10, "%s: %4.0f", r.first.c_str(), r.second);  //
            another_row_of_printing_buildings++;
        }
    }
}

    // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
void Diagnostics::Print_Build_Order_Remaining(const int &screen_x, const int &screen_y, Build &bo) {
    int another_row_of_printing = 0;
    if (!bo.isEmptyBuildOrder()) {
        for (auto i : bo.getQueue()) { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            Broodwar->drawTextScreen(screen_x, screen_y, "Build Order:");  //
            if (i.getUnit() != UnitTypes::None) {
                Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s", CUNYAIModule::noRaceName(i.getUnit().c_str()));  //
            }
            else if (i.getUpgrade() != UpgradeTypes::None) {
                Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s", i.getUpgrade().c_str());  //
            }
            else if (i.getResearch() != UpgradeTypes::None) {
                Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s", i.getResearch().c_str());  //
            }
            another_row_of_printing++;
        }
    }
    else {
        Broodwar->drawTextScreen(screen_x, screen_y, "Build Order:");  //
        Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "Build Order Empty");  //
    }
}

    // Announces to player the name and type of all of their upgrades. Bland but practical. Counts those in progress.
void Diagnostics::printUpgrade_Inventory(const int &screen_x, const int &screen_y) {
    int another_sort_of_upgrade = 0;
    for (int i = 0; i != 62; i++)
    { // iterating through all upgrades.
        int up_count = Broodwar->self()->getUpgradeLevel(((UpgradeType)i)) + static_cast<int>(Broodwar->self()->isUpgrading(((UpgradeType)i)));
        if (up_count > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Upgrades:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: %d", ((UpgradeType)i).c_str(), up_count);  //
            another_sort_of_upgrade++;
        }
    }
    if (Broodwar->self()->hasResearched(TechTypes::Lurker_Aspect)) {
        Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_sort_of_upgrade * 10, "%s: 1", TechTypes::Lurker_Aspect.c_str(), 1);  //
    }
}

    // Announces to player the name and type of all buildings in the reservation system. Bland but practical.
void Diagnostics::Print_Reservations(const int &screen_x, const int &screen_y, const Reservation &res) {
    int another_row_of_printing = 0;
    for (int i = 0; i != 229; i++)
    { // iterating through all known combat units. See unit type for enumeration, also at end of page.
        int u_count = CUNYAIModule::countUnits(((UnitType)i), res);
        if (u_count > 0) {
            Broodwar->drawTextScreen(screen_x, screen_y, "Reserved Buildings/Units:");  //
            Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s: %d", CUNYAIModule::noRaceName(((UnitType)i).c_str()), u_count);  //
            another_row_of_printing++;
        }
    }
    int current_row = another_row_of_printing;
    for (auto const r : res.getReservedUpgrades()) {
        Broodwar->drawTextScreen(screen_x, screen_y + 20 + current_row * 10, "Reserved Upgrades:");  //
        Broodwar->drawTextScreen(screen_x, screen_y + 30 + another_row_of_printing * 10, "%s: %d", CUNYAIModule::noRaceName(r.c_str()), 1);  //
        another_row_of_printing++;
    }
    Broodwar->drawTextScreen(screen_x, screen_y + 40 + another_row_of_printing * 10, "Excess Resources");  //
    Broodwar->drawTextScreen(screen_x, screen_y + 50 + another_row_of_printing * 10, "Min: %d", CUNYAIModule::my_reservation.getExcessMineral() );  //
    Broodwar->drawTextScreen(screen_x, screen_y + 60 + another_row_of_printing * 10, "Gas: %d", CUNYAIModule::my_reservation.getExcessGas());  //
    Broodwar->drawTextScreen(screen_x, screen_y + 70 + another_row_of_printing * 10, "Supply: %d", CUNYAIModule::my_reservation.getExcessSupply()/2);  // Conver to the human scale.
    Broodwar->drawTextScreen(screen_x, screen_y + 80 + another_row_of_printing * 10, "Larva: %d", CUNYAIModule::my_reservation.getExcessLarva());  //

}

void Diagnostics::issueCheats()
{
    if (INF_MONEY) {
        Broodwar->sendText("show me the money");
    }
    if (MAP_REVEAL) {
        Broodwar->sendText("black sheep wall");
    }
    if (NEVER_DIE) {
        Broodwar->sendText("power overwhelming");
    }
    if (INSTANT_WIN) {
        Broodwar->sendText("there is no cow level");
    }

    if (!(INF_MONEY || MAP_REVEAL || NEVER_DIE || INSTANT_WIN)) {
        Broodwar->sendText("Cough Cough: Power Overwhelming! (Please work!)");
    }

}

void Diagnostics::writeMacroIssues()
{
    if (Broodwar->self()->minerals() > 300 || Broodwar->self()->gas() > 300 || (Broodwar->self()->supplyUsed() > 40 * 2 && Broodwar->self()->supplyTotal() <= Broodwar->self()->supplyUsed())) {
        DiagnosticWrite(Broodwar->mapName().c_str());
        DiagnosticWrite("Frame: %d", Broodwar->getFrameCount());
        for (int i = 0; i != 229; i++)
        { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            int u_count = CUNYAIModule::countUnits(((UnitType)i), CUNYAIModule::my_reservation);
            if (u_count > 0) {
                DiagnosticWrite("%s: %d", CUNYAIModule::noRaceName(((UnitType)i).c_str()), u_count);  //
            }
        }
        for (auto const r : CUNYAIModule::my_reservation.getReservedUpgrades()) {
            DiagnosticWrite("Reserved Upgrades:");  //
            DiagnosticWrite("%s: %d", CUNYAIModule::noRaceName(r.c_str()), 1);  //
        }
        DiagnosticWrite("Excess Resources/Current Resources");  //
        DiagnosticWrite("Min: %d/%d", CUNYAIModule::my_reservation.getExcessMineral(), Broodwar->self()->minerals());  //
        DiagnosticWrite("Gas: %d/%d", CUNYAIModule::my_reservation.getExcessGas(), Broodwar->self()->gas());  //
        DiagnosticWrite("Supply: %d/%d", CUNYAIModule::my_reservation.getExcessSupply() / 2, Broodwar->self()->supplyTotal() / 2);  // Conver to the human scale.
        DiagnosticWrite("Larva: %d/%d", CUNYAIModule::my_reservation.getExcessLarva(), CUNYAIModule::countUnits(UnitTypes::Zerg_Larva));  //

        DiagnosticWrite("Econ : %s, D.Econ:  %4.2f", CUNYAIModule::friendly_player_model.spending_model_.econ_starved() ? "TRUE" : "FALSE", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::EconAlpha));  //
        DiagnosticWrite("Army : %s, D.Army:  %4.2f", CUNYAIModule::friendly_player_model.spending_model_.army_starved() ? "TRUE" : "FALSE", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::ArmyAlpha));  //
        DiagnosticWrite("Tech : %s, D.Tech:  %4.2f", CUNYAIModule::friendly_player_model.spending_model_.tech_starved() ? "TRUE" : "FALSE", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::TechAlpha));  //
    }

}

void Diagnostics::onFrameWritePlayerModel(PlayerModel &pmodel)
{
    if (Broodwar->getFrameCount() == 0) {
        DiagnosticWrite("Player's Cum. Army is:  %4.2f", pmodel.getCumArmy());
        DiagnosticWrite("Player's Cum. Eco is:  %4.2f", pmodel.getCumEco());
        DiagnosticWrite("Player's Cum. Tech is:  %4.2f", pmodel.getCumTech());

        for (auto i : pmodel.getBuildingCartridge())
            Diagnostics::DiagnosticWrite("Players Legal Buildings are: %s", i.first.c_str());
        for (auto i : pmodel.getCombatUnitCartridge())
            Diagnostics::DiagnosticWrite("Players Legal combatants are: %s", i.first.c_str());
        for (auto i : pmodel.getTechCartridge())
            Diagnostics::DiagnosticWrite("Players techs are: %s", i.first.c_str());
        for (auto i : pmodel.getUpgradeCartridge())
            Diagnostics::DiagnosticWrite("Players upgrades are: %s", i.first.c_str());
    }

    if (Broodwar->getFrameCount() % (24 * 60) == 0 && RIP_REPLAY) {
        pmodel.units_.printUnitInventory(Broodwar->self());
        pmodel.casualties_.printUnitInventory(Broodwar->self(), "casualties");
    }
}

void Diagnostics::onFrame()
{
    //bwemMap.Draw(BWAPI::BroodwarPtr);
    drawMousePosition();
    BWEB::Map::draw();
    if (Broodwar->getFrameCount() % (30 * 24) == 0) {
        writeMacroIssues();
    }
    Print_UnitInventory(0, 50, CUNYAIModule::friendly_player_model.units_);
    //Print_Cached_Inventory(0, 50);
    //Print_Test_Case(0, 50);
    Print_Reservations(0, 190, CUNYAIModule::my_reservation);
    //enemy_player_model.Print_Average_CD(500, 170);
    if (CUNYAIModule::learnedPlan.inspectCurrentBuild().isEmptyBuildOrder()) {
        CUNYAIModule::assemblymanager.Print_Assembly_FAP_Cycle(500, 170);
        //CUNYAIModule::techmanager.Print_Upgrade_FAP_Cycle(500, 170);
        //Print_UnitInventory(500, 170, enemy_player_model.units_); // actual units on ground.
        //Print_ResearchInventory(500, 170, CUNYAIModule::enemy_player_model.researches_); // tech stuff
    }
    else {
        Print_Build_Order_Remaining(500, 170, CUNYAIModule::learnedPlan.inspectCurrentBuild());
    }

    Broodwar->drawTextScreen(0, 0, "Reached Min Fields: %d", CUNYAIModule::land_inventory.countLocalMinPatches());
    Broodwar->drawTextScreen(0, 20, "Workers (alt): (m%d, g%d)", CUNYAIModule::workermanager.getMinWorkers(), CUNYAIModule::workermanager.getGasWorkers());  //
    Broodwar->drawTextScreen(0, 30, "Miners: %d vs %d", CUNYAIModule::workermanager.getMinWorkers(), CUNYAIModule::land_inventory.countLocalMiners()); // This a misuse of local miners.
    Broodwar->drawTextScreen(0, 40, "Gas-ers: %d vs %d", CUNYAIModule::workermanager.getGasWorkers(), CUNYAIModule::land_inventory.countLocalGasCollectors()); // this is a misuse of local gas.

    Broodwar->drawTextScreen(125, 0, "Econ Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.econ_starved() ? "TRUE" : "FALSE");  //
    Broodwar->drawTextScreen(125, 10, "Army Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.army_starved() ? "TRUE" : "FALSE");  //
    Broodwar->drawTextScreen(125, 20, "Tech Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.tech_starved() ? "TRUE" : "FALSE");  //

    //Broodwar->drawTextScreen(125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
    Broodwar->drawTextScreen(125, 50, "Gas Starved: %s", CUNYAIModule::gas_starved ? "TRUE" : "FALSE");
    Broodwar->drawTextScreen(125, 60, "Gas Outlet: %s", CUNYAIModule::workermanager.checkGasOutlet() ? "TRUE" : "FALSE");  //
    Broodwar->drawTextScreen(125, 70, "Xtra Gas Avail: %s", CUNYAIModule::workermanager.checkExcessGasCapacity() ? "TRUE" : "FALSE");  //


    //Broodwar->drawTextScreen(125, 80, "Ln Y/L: %4.2f", friendly_player_model.spending_model_.getlnYPerCapita()); //
    //Broodwar->drawTextScreen(125, 90, "Ln Y: %4.2f", friendly_player_model.spending_model_.getlnY()); //

    //Broodwar->drawTextScreen(125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60); //
    //Broodwar->drawTextScreen(125, 110, "Win Rate: %1.2f", win_rate); //
    //Broodwar->drawTextScreen(125, 120, "Race: %s", Broodwar->enemy()->getRace().c_str());
    //Broodwar->drawTextScreen(125, 130, "Opponent: %s", Broodwar->enemy()->getName().c_str()); //
    //Broodwar->drawTextScreen(125, 140, "Map: %s", Broodwar->mapFileName().c_str()); //
    //Broodwar->drawTextScreen(125, 150, "Min Reserved: %d", my_reservation.min_reserve_); //
    //Broodwar->drawTextScreen(125, 160, "Gas Reserved: %d", my_reservation.gas_reserve_); //

    Broodwar->drawTextScreen(250, 0, "Econ Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::EconAlpha));  //
    Broodwar->drawTextScreen(250, 10, "Army Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::ArmyAlpha)); //
    Broodwar->drawTextScreen(250, 20, "Tech Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.getDeriviative(BuildParameterNames::TechAlpha)); //
    Broodwar->drawTextScreen(250, 30, "Enemy R: %.2g ", CUNYAIModule::adaptation_rate); //
    Broodwar->drawTextScreen(250, 40, "Alpha_Econ: %4.2f %%", CUNYAIModule::friendly_player_model.spending_model_.getParameter(BuildParameterNames::EconAlpha) * 100);  // As %s
    Broodwar->drawTextScreen(250, 50, "Alpha_Army: %4.2f %%", CUNYAIModule::friendly_player_model.spending_model_.getParameter(BuildParameterNames::ArmyAlpha) * 100); //
    Broodwar->drawTextScreen(250, 60, "Alpha_Tech: %4.2f ", CUNYAIModule::friendly_player_model.spending_model_.getParameter(BuildParameterNames::TechAlpha) * 100); // No longer a % with capital-augmenting technology.
    Broodwar->drawTextScreen(250, 70, "gas_proportion: %4.2f", CUNYAIModule::gas_proportion); //
    Broodwar->drawTextScreen(250, 80, "supply_ratio_supply: %4.2f", CUNYAIModule::supply_ratio); //
    //Broodwar->drawTextScreen(250, 90, "Time to Completion: %d", my_reservation.building_timer_); //
    //Broodwar->drawTextScreen(250, 100, "Freestyling: %s", buildorder.isEmptyBuildOrder() ? "TRUE" : "FALSE"); //
    //Broodwar->drawTextScreen(250, 110, "Last Builder Sent: %d", my_reservation.last_builder_sent_);
    //Broodwar->drawTextScreen(250, 120, "Last Building: %s", buildorder.last_build_order.c_str()); //
    //Broodwar->drawTextScreen(250, 140, "FAPP: (%d , %d)", friendly_player_model.units_.moving_average_fap_stock_, enemy_player_model.units_.moving_average_fap_stock_); //

    //if (buildorder.isEmptyBuildOrder()) {
    //    Broodwar->drawTextScreen(250, 160, "Total Reservations: Min: %d, Gas: %d", my_reservation.min_reserve_, my_reservation.gas_reserve_);
    //}
    //else {
    //    Broodwar->drawTextScreen(250, 160, "Top in Build Order: Min: %d, Gas: %d", buildorder.building_gene_.begin()->getUnit().mineralPrice(), buildorder.building_gene_.begin()->getUnit().gasPrice());
    //}

    ////Broodwar->drawTextScreen(250, 150, "FAPP comparison: (%d , %d)", friendly_fap_score, enemy_fap_score); //
    //Broodwar->drawTextScreen(250, 170, "Air Weakness: %s", friendly_player_model.u_have_active_air_problem_ ? "TRUE" : "FALSE"); //
    //Broodwar->drawTextScreen(250, 180, "Foe Air Weakness: %s", friendly_player_model.e_has_air_vunerability_ ? "TRUE" : "FALSE"); //

    ////vision belongs here.
    Broodwar->drawTextScreen(375, 30, "E. Army Stock/Est.: %d/%4.2f", CUNYAIModule::enemy_player_model.units_.stock_fighting_total_, CUNYAIModule::enemy_player_model.getEstimatedUnseenArmy());
    Broodwar->drawTextScreen(375, 40, "E. Tech Stock/Est.: %d/%4.2f", CUNYAIModule::enemy_player_model.researches_.research_stock_, CUNYAIModule::enemy_player_model.getEstimatedUnseenTech());
    Broodwar->drawTextScreen(375, 50, "E. Workers/Est.: %d/%d", CUNYAIModule::enemy_player_model.units_.worker_count_, static_cast<int>(CUNYAIModule::enemy_player_model.getEstimatedWorkers()));
    Broodwar->drawTextScreen(375, 70, "Comparative lnY (E/F): %4.2f / %4.2f", CUNYAIModule::enemy_player_model.spending_model_.getlnY(), CUNYAIModule::friendly_player_model.spending_model_.getlnY());  //


    ////Broodwar->drawTextScreen( 500, 130, "Supply Heuristic: %4.2f", inventory.getLn_Supply_Ratio() );  //
    ////Broodwar->drawTextScreen( 500, 140, "Vision Tile Count: %d",  inventory.vision_tile_count_ );  //
    ////Broodwar->drawTextScreen( 500, 150, "Map Area: %d", map_area );  //

    //Broodwar->drawTextScreen(500, 20, "Performance:");  //
    //Broodwar->drawTextScreen(500, 30, "APM: %d", Broodwar->getAPM());  //
    //Broodwar->drawTextScreen(500, 40, "APF: %4.2f", (Broodwar->getAPM() / 60) / Broodwar->getAverageFPS());  //
    //Broodwar->drawTextScreen(500, 50, "FPS: %4.2f", Broodwar->getAverageFPS());  //
    //Broodwar->drawTextScreen(500, 60, "Frames of Latency: %d", Broodwar->getLatencyFrames());  //

    //Broodwar->drawTextScreen(500, 70, delay_string);
    //Broodwar->drawTextScreen(500, 80, playermodel_string);
    //Broodwar->drawTextScreen(500, 90, map_string);
    //Broodwar->drawTextScreen(500, 100, larva_string);
    //Broodwar->drawTextScreen(500, 110, worker_string);
    //Broodwar->drawTextScreen(500, 120, scouting_string);
    //Broodwar->drawTextScreen(500, 130, combat_string);
    //Broodwar->drawTextScreen(500, 140, detection_string);
    //Broodwar->drawTextScreen(500, 150, upgrade_string);
    //Broodwar->drawTextScreen(500, 160, creep_colony_string);

    for (auto p = CUNYAIModule::land_inventory.ResourceInventory_.begin(); p != CUNYAIModule::land_inventory.ResourceInventory_.end() && !CUNYAIModule::land_inventory.ResourceInventory_.empty(); ++p) {
        if (CUNYAIModule::isOnScreen(p->second.pos_, CUNYAIModule::currentMapInventory.screen_position_)) {
            Broodwar->drawCircleMap(p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan); // Plot their last known position.
            Broodwar->drawTextMap(p->second.pos_, "%d", p->second.current_stock_value_); // Plot their current value.
            Broodwar->drawTextMap(p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_); // Plot their current value.
        }
    }

    //for ( vector<int>::size_type i = 0; i < current_MapInventory.map_veins_.size(); ++i ) {
    //    for ( vector<int>::size_type j = 0; j < current_MapInventory.map_veins_[i].size(); ++j ) {
    //        if (current_MapInventory.map_veins_[i][j] > 175 ) {
    //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_MapInventory.screen_position_) ) {
    //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
    //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
    //            }
    //        }
    //        else if (current_MapInventory.map_veins_[i][j] <= 2 && current_MapInventory.map_veins_[i][j] > 1 ) { // should only highlight smoothed-out barriers.
    //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_MapInventory.screen_position_)) {
    //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
    //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Purple);
    //            }
    //        }
    //        else if (current_MapInventory.map_veins_[i][j] == 1 ) { // should only highlight smoothed-out barriers.
    //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_MapInventory.screen_position_) ) {
    //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
    //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
    //            }
    //        }
    //    }
    //} // Pretty to look at!


    //for (vector<int>::size_type i = 0; i < current_MapInventory.map_out_from_home_.size(); ++i) {
    //    for (vector<int>::size_type j = 0; j < current_MapInventory.map_out_from_home_[i].size(); ++j) {
    //        if (current_MapInventory.map_out_from_home_[i][j] <= 5 /*&& current_MapInventory.map_out_from_home_[i][j] <= 1*/ ) {
    //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_MapInventory.screen_position_)) {
    //                Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", current_MapInventory.map_out_from_home_[i][j] );
    //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
    //            }
    //        }
    //    }
    //} // Pretty to look at!

    //for (vector<int>::size_type i = 0; i < CUNYAIModule::current_MapInventory.map_out_from_safety_.size(); ++i) {
    //    for (vector<int>::size_type j = 0; j < CUNYAIModule::current_MapInventory.map_out_from_safety_[i].size(); ++j) {
    //        if (CUNYAIModule::current_MapInventory.map_out_from_safety_[i][j] % 25 == 0 && CUNYAIModule::current_MapInventory.map_out_from_safety_[i][j] > 1) {
    //            if (CUNYAIModule::isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, CUNYAIModule::current_MapInventory.screen_position_)) {
    //                Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", CUNYAIModule::current_MapInventory.map_out_from_safety_[i][j]);
    //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
    //            }
    //        }
    //    }
    //} // Pretty to look at!

    //for (vector<int>::size_type i = 0; i < inventory.smoothed_barriers_.size(); ++i) {
    //    for (vector<int>::size_type j = 0; j < inventory.smoothed_barriers_[i].size(); ++j) {
    //        if ( inventory.smoothed_barriers_[i][j] > 0) {
    //            if (isOnScreen({ static_cast<int>i * 8 + 4, static_cast<int>j * 8 + 4 }, inventory.screen_position_)) {
    //                //Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", inventory.smoothed_barriers_[i][j]);
    //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
    //            }
    //        }
    //    }
    //} // Pretty to look at!

    //for (auto &u : Broodwar->self()->getUnits()) {
    //    if (u->getLastCommand().getType() != UnitCommandTypes::Attack_Move /*&& u_type != UnitTypes::Zerg_Extractor && u->getLastCommand().getType() != UnitCommandTypes::Attack_Unit*/) {
    //        Broodwar->drawTextMap(u->getPosition(), u->getLastCommand().getType().c_str());
    //    }
    //}

    for (auto & j : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        printPhase(j.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

    //Diagnostic_Tiles(current_MapInventory.screen_position_, Colors::White);
    drawDestination(CUNYAIModule::friendly_player_model.units_, CUNYAIModule::currentMapInventory.screen_position_, Colors::Grey);

    //Diagnostic_Watch_Expos();
    if (Broodwar->getFrameCount() % (24 * 60) == 0) {
        DiagnosticWrite("Game Frame is: %d", Broodwar->getFrameCount());
        onFrameWritePlayerModel(CUNYAIModule::enemy_player_model);
    }

    //drawTiles(CUNYAIModule::currentMapInventory.screen_position_);
    //for (auto e : CUNYAIModule::currentMapInventory.getExpoTilePositions())
    //    drawCircle(Position(e), CUNYAIModule::currentMapInventory.screen_position_, 250);
}

void Diagnostics::drawAllVelocities(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Position destination = Position(u.second.pos_.x + u.second.velocity_x_ * 24, u.second.pos_.y + u.second.velocity_y_ * 24);
        Diagnostics::drawLine(u.second.pos_, destination, CUNYAIModule::currentMapInventory.screen_position_, Colors::Green);
    }
}

void Diagnostics::drawAllHitPoints(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Diagnostics::drawHitPoints(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

}

void Diagnostics::drawAllMAFAPaverages(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Diagnostics::drawFAP(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

}

void Diagnostics::drawAllFutureDeaths(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Diagnostics::drawEstimatedDeath(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

}

void Diagnostics::drawAllLastDamage(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Diagnostics::drawLastDamage(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }

}


void Diagnostics::drawAllSpamGuards(const UnitInventory ui)
{
    for (auto u : ui.unit_map_) {
        Diagnostics::drawSpamGuard(u.second, CUNYAIModule::currentMapInventory.screen_position_);
    }
}

void Diagnostics::drawStats()
{
    int line = 0;
    Broodwar->drawTextScreen(5, 0, "I have %d units:", Broodwar->self()->allUnitCount());
    for (auto& unitType : UnitTypes::allUnitTypes())
    {
        int count = Broodwar->self()->allUnitCount(unitType);
        if (count)
        {
            Broodwar->drawTextScreen(5, 16 * line, "- %d %s%c", count, unitType.c_str(), count == 1 ? ' ' : 's');
            ++line;
        }
    }
}

void Diagnostics::drawBullets()
{
    for (auto &b : Broodwar->getBullets())
    {
        Position p = b->getPosition();
        double velocityX = b->getVelocityX();
        double velocityY = b->getVelocityY();
        Broodwar->drawLineMap(p, p + Position((int)velocityX, (int)velocityY), b->getPlayer() == Broodwar->self() ? Colors::Green : Colors::Red);
        Broodwar->drawTextMap(p, "%c%s", b->getPlayer() == Broodwar->self() ? Text::Green : Text::Red, b->getType().c_str());
    }
}

void Diagnostics::drawVisibilityData()
{
    int wid = Broodwar->mapHeight(), hgt = Broodwar->mapWidth();
    for (int x = 0; x < wid; ++x)
        for (int y = 0; y < hgt; ++y)
        {
            if (Broodwar->isExplored(x, y))
                Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Broodwar->isVisible(x, y) ? Colors::Green : Colors::Blue);
            else
                Broodwar->drawDotMap(x * 32 + 16, y * 32 + 16, Colors::Red);
        }
}

void Diagnostics::showPlayers()
{
    Playerset players = Broodwar->getPlayers();
    for (auto p : players)
        Broodwar << "Player [" << p->getID() << "]: " << p->getName() << " is in force: " << p->getForce()->getName() << std::endl;
}

void Diagnostics::showForces()
{
    Forceset forces = Broodwar->getForces();
    for (auto f : forces)
    {
        Playerset players = f->getPlayers();
        Broodwar << "Force " << f->getName() << " has the following players:" << std::endl;
        for (auto p : players)
            Broodwar << "  - Player [" << p->getID() << "]: " << p->getName() << std::endl;
    }
}


void Diagnostics::printUnitEventDetails(BWAPI::Event e)
{
    if constexpr (DIAGNOSTIC_MODE) {
        std::ofstream fout;  // Create Object of Ofstream
        std::ifstream fin;
        std::string filename = prettyRepName() + static_cast<std::string>("-Events") + static_cast<std::string>(".csv");

        fin.open(filename);
        std::string line;
        int csv_length = 0;
        while (getline(fin, line)) {
            ++csv_length;
        }
        fin.close(); // I have read the entire file already, need to close it and begin again.  Lacks elegance, but works.

        if (csv_length < 1) {
            fout.open(filename, std::ios::app); // Append mode
            fout << "EventType" << ","
                << "X.Pos" << "," << "Y.Pos" << ","
                << "EventOwner" << ","
                << "UnitType" << ","
                << "UnitOwner" << ","
                << "UnitX.Pos" << "," << "UnitY.Pos" << ","
                << "FrameCount" << ","
                << "UnitID" << "\n";
            fin.close();
        }


        fin.open(filename);
        fout.open(filename, std::ios::app); // Append mode
        if (fin.is_open())
            fout << EventString(e) << ","
            << e.getPosition().x << "," << e.getPosition().y << ","
            << (e.getPlayer() ? e.getPlayer()->getName().c_str() : "No Player") << ","
            << (e.getUnit() ? e.getUnit()->getType().c_str() : "No Unit") << ","
            << (e.getUnit() && e.getUnit()->getPlayer() && e.getUnit()->getPlayer()->getName().c_str() ? e.getUnit()->getPlayer()->getName().c_str() : "No Player") << ","
            << (e.getUnit() ? std::to_string(e.getUnit()->getPosition().x).c_str() : "No X") << "," << (e.getUnit() ? std::to_string(e.getUnit()->getPosition().y).c_str() : "No Y") << ","
            << Broodwar->getFrameCount() << "\n"; // Writing data to file
        fin.close();
        fout.close(); // Closing the file
    }
}

std::string Diagnostics::prettyRepName() {
    std::string body = Broodwar->mapFileName().c_str();
    std::string clean = body.substr(0, body.find("."));
    return clean;
}

std::string Diagnostics::EventString(BWAPI::Event e)
{
    const char* s = 0;
    switch (e.getType()) {
    case EventType::MatchStart:
        s = "MatchStart";
        break;

    case EventType::MatchEnd:
        s = "MatchEnd";
        break;

    case EventType::MatchFrame:
        s = "MatchFrame";
        break;

    case EventType::MenuFrame:
        s = "MenuFrame";
        break;

    case EventType::SendText:
        s = "SendText";
        break;

    case EventType::ReceiveText:
        s = "ReceiveText";
        break;

    case EventType::PlayerLeft:
        s = "PlayerLeft";
        break;

    case EventType::NukeDetect:
        s = "NukeDetect";
        break;

    case EventType::UnitDiscover:
        s = "UnitDiscover";
        break;

    case EventType::UnitEvade:
        s = "UnitEvade";
        break;

    case EventType::UnitShow:
        s = "UnitShow";
        break;

    case EventType::UnitHide:
        s = "UnitHide";
        break;

    case EventType::UnitCreate:
        s = "UnitCreate";
        break;

    case EventType::UnitDestroy:
        s = "UnitDestroy";
        break;

    case EventType::UnitMorph:
        s = "UnitMorph";
        break;

    case EventType::UnitRenegade:
        s = "UnitRenegade";
        break;

    case EventType::SaveGame:
        s = "SaveGame";
        break;

    case EventType::UnitComplete:
        s = "UnitComplete";
        break;

        //TriggerAction,
    case EventType::None:
        s = "None";
        break;

    }
    return s;
}