#pragma once

#include "Source\Diagnostics.h"
#include <BWAPI.h>
#include "Source/CUNYAIModule.h"



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
                }
            }
        }
    }

    // This function limits the drawing that needs to be done by the bot.
    void Diagnostics::watchTile(TilePosition &tp) {
        if constexpr (DIAGNOSTIC_MODE) {
            if (CUNYAIModule::current_map_inventory.next_expo_ != TilePositions::Origin) {
                Position centered = Position(TilePosition(tp.x - 640 / (4 * 16) + 2, tp.y - 480 / (4 * 16) + 1));
                Broodwar->setScreenPosition(centered);
            }
        }
    }


    // This function limits the drawing that needs to be done by the bot.
    void Diagnostics::drawDestination(const Unit_Inventory &ui, const Position &screen_pos, Color col = Colors::White) {
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

    void Diagnostics::drawHitPoints(const Stored_Unit unit, const Position &screen_pos) {
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

    void Diagnostics::drawFAP(const Stored_Unit unit, const Position &screen_pos) {
        if constexpr (DIAGNOSTIC_MODE) {
            Position upper_left = unit.pos_;
            if (unit.valid_pos_ && CUNYAIModule::isOnScreen(upper_left, screen_pos) /*&& unit.ma_future_fap_value_ < unit.stock_value_*/ && unit.ma_future_fap_value_ > 0) {
                // Draw the red background.
                upper_left.y = upper_left.y + unit.type_.dimensionUp();
                upper_left.x = upper_left.x - unit.type_.dimensionLeft();

                Position lower_right = upper_left;
                lower_right.x = upper_left.x + unit.type_.width();
                lower_right.y = upper_left.y + 5;

                //Overlay the appropriate green above it.
                lower_right = upper_left;
                lower_right.x = static_cast<int>(upper_left.x + unit.type_.width() * unit.ma_future_fap_value_ / static_cast<double>(unit.stock_value_));
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
    void Diagnostics::drawEstimatedDeath(const Stored_Unit unit, const Position &screen_pos) {
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

    void Diagnostics::drawLastDamage(const Stored_Unit unit, const Position &screen_pos) {
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

    void Diagnostics::drawSpamGuard(const Stored_Unit unit, const Position & screen_pos)
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

                if(unit.bwapi_unit_ && !CUNYAIModule::spamGuard(unit.bwapi_unit_)) 
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

    void Diagnostics::printLastOrder(const Stored_Unit unit, const Position & screen_pos)
    {
        if constexpr (DIAGNOSTIC_MODE) {
            Position upper_left = unit.pos_;
            if (CUNYAIModule::isOnScreen(upper_left, screen_pos)) {
                Broodwar->drawTextMap(unit.pos_, unit.order_.c_str());
            }
        }
    }

    void Diagnostics::printPhase(const Stored_Unit unit, const Position & screen_pos)
    {
        if constexpr (DIAGNOSTIC_MODE) {
            map<Stored_Unit::Phase, string> enum_to_string = { { Stored_Unit::Phase::None,"None" } ,
            { Stored_Unit::Phase::Attacking,"Attacking" },
            { Stored_Unit::Phase::Retreating,"Retreating" },
            { Stored_Unit::Phase::Prebuilding,"Prebuilding" },
            { Stored_Unit::Phase::PathingOut,"PathingOut" },
            { Stored_Unit::Phase::PathingHome,"PathingHome" },
            { Stored_Unit::Phase::Surrounding,"Surrounding" },
            { Stored_Unit::Phase::NoRetreat,"NoRetreat" },
            { Stored_Unit::Phase::MiningMin,"Gather Min" },
            { Stored_Unit::Phase::MiningGas,"Gather Gas" },
            { Stored_Unit::Phase::Returning,"Returning" },
            { Stored_Unit::Phase::DistanceMining,"DistanceMining" },
            { Stored_Unit::Phase::Clearing,"Clearing" },
            { Stored_Unit::Phase::Upgrading,"Upgrading" },
            { Stored_Unit::Phase::Researching,"Researching" },
            { Stored_Unit::Phase::Morphing,"Morphing" },
            { Stored_Unit::Phase::Building,"Building" },
            { Stored_Unit::Phase::Detecting,"Detecting" } };
            Position upper_left = unit.pos_;
            if (CUNYAIModule::isOnScreen(upper_left, screen_pos)) {
                Broodwar->drawTextMap(unit.pos_, enum_to_string[unit.phase_].c_str());
            }
        }
    }

    void Diagnostics::drawReservations(const Reservation reservations, const Position & screen_pos)
    {
        if constexpr (DIAGNOSTIC_MODE) {
            for (auto res : reservations.reservation_map_) {
                Position upper_left = Position(res.first);
                Position lower_right = Position(res.first) + Position(res.second.width(), res.second.height()); //thank goodness I overloaded the + operator for the pathing operations!
                if (CUNYAIModule::isOnScreen(upper_left, screen_pos)) {
                    Broodwar->drawBoxMap(upper_left, lower_right, Colors::Grey, true);
                    Broodwar->drawTextMap(upper_left, res.second.c_str());
                }
            }
        }
    }


    void Diagnostics::DiagnosticTrack(const Unit &u) {
        Broodwar->setScreenPosition(u->getPosition() - Position{ 320,200 });
    }

    void Diagnostics::DiagnosticTrack(const Position &p) {
        Broodwar->setScreenPosition(p - Position{ 320,200 });
    }


    // Announces to player the name and count of all units in the unit inventory. Bland but practical.
    void Diagnostics::Print_Unit_Inventory(const int &screen_x, const int &screen_y, const Unit_Inventory &ui) {
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
        for (auto i : CUNYAIModule::friendly_player_model.unit_type_)
        { // iterating through all known combat units. See unit type for enumeration, also at end of page.
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
    void Diagnostics::Print_Research_Inventory(const int &screen_x, const int &screen_y, const Research_Inventory &ri) {
        int another_row_of_printing_ups = 1;

        for (auto r : ri.upgrades_)
        { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            if (r.second > 0) {
                Broodwar->drawTextScreen(screen_x, screen_y, "Upgrades:");  //
                Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_ups * 10, "%s: %d", r.first.c_str(), r.second);  //
                another_row_of_printing_ups++;
            }
        }

        int another_row_of_printing_research = another_row_of_printing_ups + 1;

        for (auto r : ri.tech_)
        { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            if (r.second) {
                Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_ups * 10, "Tech:");  //
                Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_research * 10, "%s", r.first.c_str());  //
                another_row_of_printing_research++;
            }
        }

        int another_row_of_printing_buildings = another_row_of_printing_research + 1;

        for (auto r : ri.tech_buildings_)
        { // iterating through all known combat units. See unit type for enumeration, also at end of page.
            if (r.second > 0) {
                Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_research * 10, "R.Buildings:");  //
                Broodwar->drawTextScreen(screen_x, screen_y + another_row_of_printing_buildings * 10, "%s: %d", r.first.c_str(), r.second);  //
                another_row_of_printing_buildings++;
            }
        }
    }

    // Announces to player the name and type of all units remaining in the Buildorder. Bland but practical.
    void Diagnostics::Print_Build_Order_Remaining(const int &screen_x, const int &screen_y, const Building_Gene &bo) {
        int another_row_of_printing = 0;
        if (!bo.building_gene_.empty()) {
            for (auto i : bo.building_gene_) { // iterating through all known combat units. See unit type for enumeration, also at end of page.
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
    void Diagnostics::Print_Upgrade_Inventory(const int &screen_x, const int &screen_y) {
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
                Broodwar->drawTextScreen(screen_x, screen_y, "Reserved Buildings:");  //
                Broodwar->drawTextScreen(screen_x, screen_y + 10 + another_row_of_printing * 10, "%s: %d", CUNYAIModule::noRaceName(((UnitType)i).c_str()), u_count);  //
                another_row_of_printing++;
            }
        }
    }

    void Diagnostics::onFrame()
    {
        //bwemMap.Draw(BWAPI::BroodwarPtr);
        BWEB::Map::draw();
        Print_Unit_Inventory(0, 50, CUNYAIModule::friendly_player_model.units_);
        //Print_Cached_Inventory(0, 50);
        //assemblymanager.Print_Assembly_FAP_Cycle(0, 50);
        //Print_Test_Case(0, 50);
        Print_Unit_Inventory(375, 130, CUNYAIModule::enemy_player_model.imputedUnits_);
        Print_Reservations(250, 190, CUNYAIModule::my_reservation);
        //enemy_player_model.Print_Average_CD(500, 170);
        //techmanager.Print_Upgrade_FAP_Cycle(500, 170);
        if (CUNYAIModule::buildorder.isEmptyBuildOrder()) {
            //    techmanager.Print_Upgrade_FAP_Cycle(500, 170);
                //Print_Unit_Inventory(500, 170, enemy_player_model.units_); // actual units on ground.
            Print_Research_Inventory(500, 170, CUNYAIModule::enemy_player_model.researches_); // tech stuff
        }
        else {
            Print_Build_Order_Remaining(500, 170, CUNYAIModule::buildorder);
        }

        Broodwar->drawTextScreen(0, 0, "Reached Min Fields: %d", CUNYAIModule::land_inventory.getLocalMinPatches());
        Broodwar->drawTextScreen(0, 20, "Workers (alt): (m%d, g%d)", CUNYAIModule::workermanager.min_workers_, CUNYAIModule::workermanager.gas_workers_);  //
        Broodwar->drawTextScreen(0, 30, "Miners: %d vs %d", CUNYAIModule::workermanager.min_workers_, CUNYAIModule::land_inventory.getLocalMiners()); // This a misuse of local miners.
        Broodwar->drawTextScreen(0, 40, "Gas-ers: %d vs %d", CUNYAIModule::workermanager.gas_workers_, CUNYAIModule::land_inventory.getLocalGasCollectors()); // this is a misuse of local gas.

        Broodwar->drawTextScreen(125, 0, "Econ Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.econ_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 10, "Army Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.army_starved() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 20, "Tech Starved: %s", CUNYAIModule::friendly_player_model.spending_model_.tech_starved() ? "TRUE" : "FALSE");  //

        //Broodwar->drawTextScreen(125, 40, "Supply Starved: %s", supply_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 50, "Gas Starved: %s", CUNYAIModule::gas_starved ? "TRUE" : "FALSE");
        Broodwar->drawTextScreen(125, 60, "Gas Outlet: %s", CUNYAIModule::workermanager.checkGasOutlet() ? "TRUE" : "FALSE");  //
        Broodwar->drawTextScreen(125, 70, "Xtra Gas Avail: %s", CUNYAIModule::workermanager.excess_gas_capacity_ ? "TRUE" : "FALSE");  //


        //Broodwar->drawTextScreen(125, 80, "Ln Y/L: %4.2f", friendly_player_model.spending_model_.getlny()); //
        //Broodwar->drawTextScreen(125, 90, "Ln Y: %4.2f", friendly_player_model.spending_model_.getlnY()); //

        //Broodwar->drawTextScreen(125, 100, "Game Time: %d minutes", (Broodwar->elapsedTime()) / 60); //
        //Broodwar->drawTextScreen(125, 110, "Win Rate: %1.2f", win_rate); //
        //Broodwar->drawTextScreen(125, 120, "Race: %s", Broodwar->enemy()->getRace().c_str());
        //Broodwar->drawTextScreen(125, 130, "Opponent: %s", Broodwar->enemy()->getName().c_str()); //
        //Broodwar->drawTextScreen(125, 140, "Map: %s", Broodwar->mapFileName().c_str()); //
        //Broodwar->drawTextScreen(125, 150, "Min Reserved: %d", my_reservation.min_reserve_); //
        //Broodwar->drawTextScreen(125, 160, "Gas Reserved: %d", my_reservation.gas_reserve_); //

        Broodwar->drawTextScreen(250, 0, "Econ Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.econ_derivative);  //
        Broodwar->drawTextScreen(250, 10, "Army Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.army_derivative); //
        Broodwar->drawTextScreen(250, 20, "Tech Gradient: %.2g", CUNYAIModule::friendly_player_model.spending_model_.tech_derivative); //
        Broodwar->drawTextScreen(250, 30, "Enemy R: %.2g ", CUNYAIModule::adaptation_rate); //
        Broodwar->drawTextScreen(250, 40, "Alpha_Econ: %4.2f %%", CUNYAIModule::friendly_player_model.spending_model_.alpha_econ * 100);  // As %s
        Broodwar->drawTextScreen(250, 50, "Alpha_Army: %4.2f %%", CUNYAIModule::friendly_player_model.spending_model_.alpha_army * 100); //
        Broodwar->drawTextScreen(250, 60, "Alpha_Tech: %4.2f ", CUNYAIModule::friendly_player_model.spending_model_.alpha_tech * 100); // No longer a % with capital-augmenting technology.
        Broodwar->drawTextScreen(250, 70, "gas_proportion_gas: %4.2f", CUNYAIModule::gas_proportion); //
        Broodwar->drawTextScreen(250, 80, "supply_ratio_supply: %4.2f", CUNYAIModule::supply_ratio); //
        //Broodwar->drawTextScreen(250, 90, "Time to Completion: %d", my_reservation.building_timer_); //
        //Broodwar->drawTextScreen(250, 100, "Freestyling: %s", buildorder.isEmptyBuildOrder() ? "TRUE" : "FALSE"); //
        //Broodwar->drawTextScreen(250, 110, "Last Builder Sent: %d", my_reservation.last_builder_sent_);
        //Broodwar->drawTextScreen(250, 120, "Last Building: %s", buildorder.last_build_order.c_str()); //
        //Broodwar->drawTextScreen(250, 130, "Next Expo Loc: (%d , %d)", current_map_inventory.next_expo_.x, current_map_inventory.next_expo_.y); //
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
        //Broodwar->drawTextScreen(375, 20, "Foe Stock(Est.): %d", current_map_inventory.est_enemy_stock_);
        //Broodwar->drawTextScreen(375, 30, "Foe Army Stock: %d", enemy_player_model.units_.stock_fighting_total_); //
        //Broodwar->drawTextScreen(375, 40, "Foe Tech Stock(Est.): %d", enemy_player_model.researches_.research_stock_);
        Broodwar->drawTextScreen(375, 50, "Foe Workers (Est.): %d", static_cast<int>(CUNYAIModule::enemy_player_model.estimated_workers_));
        Broodwar->drawTextScreen(375, 60, "Est. Expenditures: %4.2f,  %4.2f, %4.2f", CUNYAIModule::enemy_player_model.estimated_resources_per_frame_, CUNYAIModule::enemy_player_model.estimated_unseen_army_per_frame_, CUNYAIModule::enemy_player_model.estimated_unseen_tech_per_frame_);
        Broodwar->drawTextScreen(375, 70, "Net lnY (est.): E:%4.2f, F:%4.2f", CUNYAIModule::enemy_player_model.spending_model_.getlnYusing(CUNYAIModule::friendly_player_model.spending_model_.alpha_army, CUNYAIModule::friendly_player_model.spending_model_.alpha_tech), CUNYAIModule::friendly_player_model.spending_model_.getlnY());  //
        Broodwar->drawTextScreen(375, 80, "Unseen Army: %4.2f", CUNYAIModule::enemy_player_model.estimated_unseen_army_);  //
        Broodwar->drawTextScreen(375, 90, "Unseen Tech/Up: %4.2f", CUNYAIModule::enemy_player_model.estimated_unseen_tech_);  //
        Broodwar->drawTextScreen(375, 100, "Unseen Flyer: %4.2f", CUNYAIModule::enemy_player_model.estimated_unseen_flyers_);  //
        Broodwar->drawTextScreen(375, 110, "Unseen Ground: %4.2f", CUNYAIModule::enemy_player_model.estimated_unseen_ground_);  //

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

        for (auto p = CUNYAIModule::land_inventory.resource_inventory_.begin(); p != CUNYAIModule::land_inventory.resource_inventory_.end() && !CUNYAIModule::land_inventory.resource_inventory_.empty(); ++p) {
            if (CUNYAIModule::isOnScreen(p->second.pos_, CUNYAIModule::current_map_inventory.screen_position_)) {
                Broodwar->drawCircleMap(p->second.pos_, (p->second.type_.dimensionUp() + p->second.type_.dimensionLeft()) / 2, Colors::Cyan); // Plot their last known position.
                Broodwar->drawTextMap(p->second.pos_, "%d", p->second.current_stock_value_); // Plot their current value.
                Broodwar->drawTextMap(p->second.pos_.x, p->second.pos_.y + 10, "%d", p->second.number_of_miners_); // Plot their current value.
            }
        }

        //for ( vector<int>::size_type i = 0; i < current_map_inventory.map_veins_.size(); ++i ) {
        //    for ( vector<int>::size_type j = 0; j < current_map_inventory.map_veins_[i].size(); ++j ) {
        //        if (current_map_inventory.map_veins_[i][j] > 175 ) {
        //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_) ) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Cyan );
        //            }
        //        }
        //        else if (current_map_inventory.map_veins_[i][j] <= 2 && current_map_inventory.map_veins_[i][j] > 1 ) { // should only highlight smoothed-out barriers.
        //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_)) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Purple);
        //            }
        //        }
        //        else if (current_map_inventory.map_veins_[i][j] == 1 ) { // should only highlight smoothed-out barriers.
        //            if (isOnScreen( { static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_) ) {
        //                //Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", inventory.map_veins_[i][j] );
        //                Broodwar->drawCircleMap( i * 8 + 4, j * 8 + 4, 1, Colors::Red );
        //            }
        //        }
        //    }
        //} // Pretty to look at!


        //for (vector<int>::size_type i = 0; i < current_map_inventory.map_out_from_home_.size(); ++i) {
        //    for (vector<int>::size_type j = 0; j < current_map_inventory.map_out_from_home_[i].size(); ++j) {
        //        if (current_map_inventory.map_out_from_home_[i][j] <= 5 /*&& current_map_inventory.map_out_from_home_[i][j] <= 1*/ ) {
        //            if (isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, current_map_inventory.screen_position_)) {
        //                Broodwar->drawTextMap(  i * 8 + 4, j * 8 + 4, "%d", current_map_inventory.map_out_from_home_[i][j] );
        //                //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
        //            }
        //        }
        //    }
        //} // Pretty to look at!

        for (vector<int>::size_type i = 0; i < CUNYAIModule::current_map_inventory.map_out_from_safety_.size(); ++i) {
            for (vector<int>::size_type j = 0; j < CUNYAIModule::current_map_inventory.map_out_from_safety_[i].size(); ++j) {
                if (CUNYAIModule::current_map_inventory.map_out_from_safety_[i][j] % 25 == 0 && CUNYAIModule::current_map_inventory.map_out_from_safety_[i][j] > 1) {
                    if (CUNYAIModule::isOnScreen({ static_cast<int>(i) * 8 + 4, static_cast<int>(j) * 8 + 4 }, CUNYAIModule::current_map_inventory.screen_position_)) {
                        Broodwar->drawTextMap(i * 8 + 4, j * 8 + 4, "%d", CUNYAIModule::current_map_inventory.map_out_from_safety_[i][j]);
                        //Broodwar->drawCircleMap(i * 8 + 4, j * 8 + 4, 1, Colors::Green);
                    }
                }
            }
        } // Pretty to look at!

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
            printPhase(j.second, CUNYAIModule::current_map_inventory.screen_position_);
        }

        //Diagnostic_Tiles(current_map_inventory.screen_position_, Colors::White);
        drawDestination(CUNYAIModule::friendly_player_model.units_, CUNYAIModule::current_map_inventory.screen_position_, Colors::Grey);
        //Diagnostic_Watch_Expos();

         
    }

    void Diagnostics::drawAllVelocities(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Position destination = Position(u.second.pos_.x + u.second.velocity_x_ * 24, u.second.pos_.y + u.second.velocity_y_ * 24);
            Diagnostics::drawLine(u.second.pos_, destination, CUNYAIModule::current_map_inventory.screen_position_, Colors::Green);
        }
    }

    void Diagnostics::drawAllHitPoints(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Diagnostics::drawHitPoints(u.second, CUNYAIModule::current_map_inventory.screen_position_);
        }

    }
    void Diagnostics::drawAllMAFAPaverages(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Diagnostics::drawFAP(u.second, CUNYAIModule::current_map_inventory.screen_position_);
        }

    }

    void Diagnostics::drawAllFutureDeaths(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Diagnostics::drawEstimatedDeath(u.second, CUNYAIModule::current_map_inventory.screen_position_);
        }

    }

    void Diagnostics::drawAllLastDamage(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Diagnostics::drawLastDamage(u.second, CUNYAIModule::current_map_inventory.screen_position_);
        }

    }


    void Diagnostics::drawAllSpamGuards(const Unit_Inventory ui)
    {
        for (auto u : ui.unit_map_) {
            Diagnostics::drawSpamGuard(u.second, CUNYAIModule::current_map_inventory.screen_position_);
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