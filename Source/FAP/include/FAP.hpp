#pragma once

#include "Unit.hpp"

#include <vector>

namespace FAP {
  Unit <> inline makeUnit() {
    return Unit<>{};
  }

  struct FastAPproximation {
    /**
     * \brief Adds the unit to the simulator for player 1
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addUnitPlayer1(Unit<uv> &&fu);

    /**
     * \brief Adds the unit to the simulator for player 1, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addIfCombatUnitPlayer1(Unit<uv> &&fu);
    /**
     * \brief Adds the unit to the simulator for player 2
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addUnitPlayer2(Unit<uv> &&fu);
    /**
     * \brief Adds the unit to the simulator for player 2, only if it is a combat unit
     * \param fu The FAPUnit to add
     */
    template<UnitValues uv>
    void addIfCombatUnitPlayer2(Unit<uv> &&fu);

    /**
     * \brief Starts the simulation. You can run this function multiple times. Feel free to run once, get the state and keep running.
     * \param nFrames the number of frames to simulate. A negative number runs the sim until combat is over.
     */
    void simulate(int nFrames = 96); // = 24*4, 4 seconds on fastest

    /**
     * \brief Gets the internal state of the simulator. You can use this to get any info about the unit participating in the simulation or edit the state.
     * \return Returns a pair of pointers, where each pointer points to a vector containing that player's units.
     */
    std::pair<std::vector<FAPUnit> *, std::vector<FAPUnit> *> getState();

    /**
     * \brief Clears the simulation. All units are removed for both players. Equivalent to reconstructing.
     */
    void clear();

  private:
    std::vector<FAPUnit> player1, player2;

    bool didSomething = false;
    static void dealDamage(FAPUnit &u, int damage, BWAPI::DamageType damageType);
    static int distSquared(FAPUnit const &u1, const FAPUnit &u2);
    static bool isSuicideUnit(BWAPI::UnitType ut);
    void unitsim(FAPUnit &u, std::vector<FAPUnit> &enemyUnits);
    static void medicsim(FAPUnit &u, std::vector<FAPUnit> &friendlyUnits);
    bool suicideSim(FAPUnit &u, std::vector<FAPUnit> &enemyUnits);
    void isimulate();
    static void unitDeath(FAPUnit &&u, std::vector<FAPUnit> &itsFriendlies);

    static auto max(int a, int b) {
      int vars[2] = { a, b };
      return vars[b > a];
    }

    static auto min(int a, int b) {
      int vars[2] = { a, b };
      return vars[a < b];
    }

    static bool isCombatUnit(FAPUnit &u) {
      return (u.unitType != BWAPI::UnitTypes::Protoss_Interceptor) &
        (static_cast<bool>(u.airDamage) | static_cast<bool>(u.groundDamage) | static_cast<bool>(u.unitType == BWAPI::UnitTypes::Terran_Medic));
    }
  };

  template<UnitValues uv>
  constexpr bool AssertValidUnit() {
    static_assert(Unit<uv>::hasFlag(UnitValues::x));
    static_assert(Unit<uv>::hasFlag(UnitValues::y));
    static_assert(Unit<uv>::hasFlag(UnitValues::health));
    static_assert(Unit<uv>::hasFlag(UnitValues::maxHealth));
    static_assert(Unit<uv>::hasFlag(UnitValues::armor));
    static_assert(Unit<uv>::hasFlag(UnitValues::shields));
    static_assert(Unit<uv>::hasFlag(UnitValues::maxShields));
    static_assert(Unit<uv>::hasFlag(UnitValues::speed));
    static_assert(Unit<uv>::hasFlag(UnitValues::flying));
    static_assert(Unit<uv>::hasFlag(UnitValues::elevation));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundDamage));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundCooldown));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundMaxRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundMinRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::groundDamageType));
    static_assert(Unit<uv>::hasFlag(UnitValues::airDamage));
    static_assert(Unit<uv>::hasFlag(UnitValues::airMinRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::airCooldown));
    static_assert(Unit<uv>::hasFlag(UnitValues::airMaxRange));
    static_assert(Unit<uv>::hasFlag(UnitValues::airDamageType));
    static_assert(Unit<uv>::hasFlag(UnitValues::unitType));
    static_assert(Unit<uv>::hasFlag(UnitValues::unitSize));
    static_assert(Unit<uv>::hasFlag(UnitValues::isOrganic));
    static_assert(Unit<uv>::hasFlag(UnitValues::score));
    static_assert(Unit<uv>::hasFlag(UnitValues::armorUpgrades));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackUpgrades));
    static_assert(Unit<uv>::hasFlag(UnitValues::shieldUpgrades));
    static_assert(Unit<uv>::hasFlag(UnitValues::speedUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackCooldownRemaining));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackSpeedUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::stimmed));
    static_assert(Unit<uv>::hasFlag(UnitValues::rangeUpgrade));
    static_assert(Unit<uv>::hasFlag(UnitValues::attackerCount));
    return true;
  }

  template<UnitValues uv>
  void FastAPproximation::addUnitPlayer1(Unit<uv> &&fu) {
    static_assert(AssertValidUnit<uv>());
    player1.emplace_back(fu.unit);
  }

  template<UnitValues uv>
  void FastAPproximation::addIfCombatUnitPlayer1(Unit<uv> &&fu) {
    if (isCombatUnit(fu))
      addUnitPlayer1(fu);
  }

  template <UnitValues uv>
  void FastAPproximation::addUnitPlayer2(Unit<uv> &&fu) {
    static_assert(AssertValidUnit<uv>());
    player2.emplace_back(fu.unit);
  }

  template<UnitValues uv>
  void FastAPproximation::addIfCombatUnitPlayer2(Unit<uv> &&fu) {
    if (isCombatUnit(fu))
      addUnitPlayer2(fu);
  }

  inline void FastAPproximation::simulate(int nFrames) {
    while (nFrames--) {
      if (player1.empty() || player2.empty())
        break;

      didSomething = false;

      isimulate();

      if (!didSomething)
        break;
    }
  }

  inline std::pair<std::vector<FAPUnit> *, std::vector<FAPUnit> *> FastAPproximation::getState() {
    return { &player1, &player2 };
  }

  inline void FastAPproximation::clear() {
    player1.clear(), player2.clear();
  }

  inline void FastAPproximation::dealDamage(FAPUnit &fu, int damage, BWAPI::DamageType const damageType) {
    damage <<= 8;
    auto const remainingShields = fu.shields - damage + (fu.shieldArmor << 8);
    if (remainingShields > 0) {
      fu.shields = remainingShields;
      return;
    }
    else if (fu.shields) {
      damage -= fu.shields + (fu.shieldArmor << 8);
      fu.shields = 0;
    }

    if (!damage)
      return;

    damage -= fu.armor << 8;

    if (damageType == BWAPI::DamageTypes::Concussive) {
      if (fu.unitSize == BWAPI::UnitSizeTypes::Large)
        damage = damage / 4;
      else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
        damage = damage / 2;
    }
    else if (damageType == BWAPI::DamageTypes::Explosive) {
      if (fu.unitSize == BWAPI::UnitSizeTypes::Small)
        damage = damage / 2;
      else if (fu.unitSize == BWAPI::UnitSizeTypes::Medium)
        damage = (damage * 3) / 4;
    }

    fu.health -= max(128, damage);
  }

  int inline FastAPproximation::distSquared(const FAPUnit &u1, const FAPUnit &u2) {
    return (u1.x - u2.x) * (u1.x - u2.x) + (u1.y - u2.y) * (u1.y - u2.y);
  }

  bool inline FastAPproximation::isSuicideUnit(BWAPI::UnitType const ut) {
    return (ut == BWAPI::UnitTypes::Zerg_Scourge ||
      ut == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine ||
      ut == BWAPI::UnitTypes::Zerg_Infested_Terran ||
      ut == BWAPI::UnitTypes::Protoss_Scarab);
  }

  void inline FastAPproximation::unitsim(FAPUnit &fu, std::vector<FAPUnit> &enemyUnits) {
    if (fu.attackCooldownRemaining) {
      didSomething = true;
      return;
    }

    auto closestEnemy = enemyUnits.end();
    int closestDistSquared;

    for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
      if (enemyIt->flying) {
        if (fu.airDamage) {
          auto const d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.airMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
      else {
        if (fu.groundDamage) {
          auto const d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.groundMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
    }

    if (closestEnemy != enemyUnits.end() && closestDistSquared <= fu.speedSquared &&
      !(fu.x == closestEnemy->x && fu.y == closestEnemy->y)) {
      fu.x = closestEnemy->x;
      fu.y = closestEnemy->y;
      closestDistSquared = 0;

      didSomething = true;
    }

    if (closestEnemy != enemyUnits.end() &&
      closestDistSquared <= (closestEnemy->flying ? fu.airMaxRangeSquared : fu.groundMaxRangeSquared)) {
      if (closestEnemy->flying) {
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType);
        fu.attackCooldownRemaining = fu.airCooldown;
      }
      else {
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);
        fu.attackCooldownRemaining =
          fu.groundCooldown << static_cast<int>((fu.elevation != -1) & (closestEnemy->elevation != -1) & (closestEnemy->elevation > fu.elevation));
      }

      if (closestEnemy->health < 1) {
        auto temp = *closestEnemy;
        *closestEnemy = enemyUnits.back();
        enemyUnits.pop_back();
        unitDeath(std::move(temp), enemyUnits);
      }

      didSomething = true;
    }
    else if (closestEnemy != enemyUnits.end() && closestDistSquared > fu.speedSquared) {
      auto const dx = closestEnemy->x - fu.x;
      auto const dy = closestEnemy->y - fu.y;

      fu.x += static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
      fu.y += static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

      didSomething = true;
    }
  }

  void inline FastAPproximation::medicsim(FAPUnit &fu, std::vector<FAPUnit> &friendlyUnits) {
    auto closestHealable = friendlyUnits.end();
    int closestDist;

    for (auto it = friendlyUnits.begin(); it != friendlyUnits.end(); ++it) {
      if (it->isOrganic && it->health < it->maxHealth && !it->didHealThisFrame) {
        auto const d = distSquared(fu, *it);
        if (closestHealable == friendlyUnits.end() || d < closestDist) {
          closestHealable = it;
          closestDist = d;
        }
      }
    }

    if (closestHealable != friendlyUnits.end()) {
      fu.x = closestHealable->x;
      fu.y = closestHealable->y;

      closestHealable->health += 150;

      if (closestHealable->health > closestHealable->maxHealth)
        closestHealable->health = closestHealable->maxHealth;

      closestHealable->didHealThisFrame = true;
    }
  }

  bool inline FastAPproximation::suicideSim(FAPUnit &fu, std::vector<FAPUnit> &enemyUnits) {
    auto closestEnemy = enemyUnits.end();
    int closestDistSquared;

    for (auto enemyIt = enemyUnits.begin(); enemyIt != enemyUnits.end(); ++enemyIt) {
      if (enemyIt->flying) {
        if (fu.airDamage) {
          auto const d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.airMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
      else {
        if (fu.groundDamage) {
          int d = distSquared(fu, *enemyIt);
          if ((closestEnemy == enemyUnits.end() || d < closestDistSquared) &&
            d >= fu.groundMinRangeSquared) {
            closestDistSquared = d;
            closestEnemy = enemyIt;
          }
        }
      }
    }

    if (closestEnemy != enemyUnits.end() && closestDistSquared <= fu.speedSquared) {
      if (closestEnemy->flying)
        dealDamage(*closestEnemy, fu.airDamage, fu.airDamageType);
      else
        dealDamage(*closestEnemy, fu.groundDamage, fu.groundDamageType);

      if (closestEnemy->health < 1) {
        auto temp = *closestEnemy;
        *closestEnemy = enemyUnits.back();
        enemyUnits.pop_back();
        unitDeath(std::move(temp), enemyUnits);
      }

      didSomething = true;
      return true;
    }
    else if (closestEnemy != enemyUnits.end() && closestDistSquared > fu.speedSquared) {
      auto const dx = closestEnemy->x - fu.x;
      auto const dy = closestEnemy->y - fu.y;

      fu.x += static_cast<int>(dx * (fu.speed / sqrt(dx * dx + dy * dy)));
      fu.y += static_cast<int>(dy * (fu.speed / sqrt(dx * dx + dy * dy)));

      didSomething = true;
    }

    return false;
  }

  void inline FastAPproximation::isimulate() {
    const auto simUnit = [this](auto &unit, auto &friendly, auto &enemy) {
      if (isSuicideUnit(unit->unitType)) {
        auto const unitDied = suicideSim(*unit, enemy);
        if (unitDied)
          unit = friendly.erase(unit);
        else ++unit;
      }
      else {
        if (unit->unitType == BWAPI::UnitTypes::Terran_Medic)
          medicsim(*unit, friendly);
        else
          unitsim(*unit, enemy);
        ++unit;
      }
    };

    for (auto fu = player1.begin(); fu != player1.end();) {
      simUnit(fu, player1, player2);
    }

    for (auto fu = player2.begin(); fu != player2.end();) {
      simUnit(fu, player2, player1);
    }

    const auto updateUnit = [](FAPUnit &fu) {
      if (fu.attackCooldownRemaining)
        --fu.attackCooldownRemaining;
      if (fu.didHealThisFrame)
        fu.didHealThisFrame = false;

      if (fu.unitType.getRace() == BWAPI::Races::Zerg) {
        if (fu.health < fu.maxHealth)
          fu.health += 4;
        if (fu.health > fu.maxHealth)
          fu.health = fu.maxHealth;
      }
      else if (fu.unitType.getRace() == BWAPI::Races::Protoss) {
        if (fu.shields < fu.maxShields)
          fu.shields += 7;
        if (fu.shields > fu.maxShields)
          fu.shields = fu.maxShields;
      }
    };

    for (auto &fu : player1)
      updateUnit(fu);

    for (auto &fu : player2)
      updateUnit(fu);
  }

  void inline FastAPproximation::unitDeath(FAPUnit &&fu, std::vector<FAPUnit> &itsFriendlies) {
    if (fu.unitType == BWAPI::UnitTypes::Terran_Bunker) {
      fu.unitType = BWAPI::UnitTypes::Terran_Marine;

      auto squaredRange = [](int tiles) constexpr {
        return (tiles * 32) * (tiles * 32);
      };

      switch(fu.groundMaxRangeSquared) {
      // No range upgrade
      case squaredRange(5):
        fu.groundMaxRangeSquared = squaredRange(4);
      // Range upgrade
      case squaredRange(6):
        fu.groundMaxRangeSquared = squaredRange(5);
      }

      // @TODO: I guess I need to ask the bot here, I'll make some interface for that
      fu.armor = 0;

      fu.health = fu.maxHealth = BWAPI::UnitTypes::Terran_Marine.maxHitPoints();

      fu.groundCooldown *= 4;
      fu.airCooldown *= 4;

      for (unsigned i = 0; i < 3; ++i)
        itsFriendlies.push_back(reinterpret_cast<FAPUnit&>(fu));
      itsFriendlies.emplace_back(std::forward<FAPUnit>(fu));
    }
  }

} // namespace Neolib
