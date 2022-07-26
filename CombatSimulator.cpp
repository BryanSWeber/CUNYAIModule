#pragma once

#include "Source/CombatSimulator.h"
#include "Source/UnitInventory.h"
#include <numeric>


double CombatSimulator::unitWeight(FAP::FAPUnit<StoredUnit*> FAPunit)
{
    return  FAPunit.data->stock_value_ * static_cast<double>(FAPunit.health + FAPunit.shields) / static_cast<double>(FAPunit.maxHealth + FAPunit.maxShields);
}

auto CombatSimulator::createFAPVersion(StoredUnit u,const ResearchInventory & ri)
{
    int armor_upgrades = ri.getUpLevel(u.type_.armorUpgrade()) + 2 * (u.type_ == UnitTypes::Zerg_Ultralisk * ri.getUpLevel(UpgradeTypes::Chitinous_Plating));

    int gun_upgrades = max(ri.getUpLevel(u.type_.groundWeapon().upgradeType()), ri.getUpLevel(u.type_.airWeapon().upgradeType()));
    int shield_upgrades = static_cast<int>(u.shields_ > 0) * ri.getUpLevel(UpgradeTypes::Protoss_Plasma_Shields);

    bool speed_tech = // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Zergling && ri.getUpLevel(UpgradeTypes::Metabolic_Boost)) ||
        (u.type_ == UnitTypes::Zerg_Hydralisk && ri.getUpLevel(UpgradeTypes::Muscular_Augments)) ||
        (u.type_ == UnitTypes::Zerg_Overlord && ri.getUpLevel(UpgradeTypes::Pneumatized_Carapace)) ||
        (u.type_ == UnitTypes::Zerg_Ultralisk && ri.getUpLevel(UpgradeTypes::Anabolic_Synthesis)) ||
        (u.type_ == UnitTypes::Protoss_Scout && ri.getUpLevel(UpgradeTypes::Gravitic_Thrusters)) ||
        (u.type_ == UnitTypes::Protoss_Observer && ri.getUpLevel(UpgradeTypes::Gravitic_Boosters)) ||
        (u.type_ == UnitTypes::Protoss_Zealot && ri.getUpLevel(UpgradeTypes::Leg_Enhancements)) ||
        (u.type_ == UnitTypes::Terran_Vulture && ri.getUpLevel(UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Hydralisk && ri.getUpLevel(UpgradeTypes::Grooved_Spines)) ||
        (u.type_ == UnitTypes::Protoss_Dragoon && ri.getUpLevel(UpgradeTypes::Singularity_Charge)) ||
        (u.type_ == UnitTypes::Terran_Marine && ri.getUpLevel(UpgradeTypes::U_238_Shells)) ||
        (u.type_ == UnitTypes::Terran_Goliath && ri.getUpLevel(UpgradeTypes::Charon_Boosters)) ||
        (u.type_ == UnitTypes::Terran_Barracks && ri.getUpLevel(UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Zergling && ri.getUpLevel(UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (u.type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.getUpLevel(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present.

    return FAP::makeUnit<StoredUnit*>()
        .setData(&u)
        .setUnitType(u.type_)
        .setPosition(u.pos_)
        .setHealth(u.health_)
        .setShields(u.shields_)
        .setFlying(u.is_flying_)
        .setElevation(u.elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(u.cd_remaining_)
        .setStimmed(u.stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

auto CombatSimulator::createModifiedFAPVersion(StoredUnit u, const ResearchInventory &ri, const Position & chosen_pos, const UpgradeType &upgrade, const TechType &tech)
{
    int armor_upgrades = ri.getUpLevel(u.type_.armorUpgrade()) +
        2 * (u.type_ == UnitTypes::Zerg_Ultralisk * ri.getUpLevel(UpgradeTypes::Chitinous_Plating)) +
        (u.type_.armorUpgrade() == upgrade);

    int gun_upgrades = max(ri.getUpLevel(u.type_.groundWeapon().upgradeType()) + u.type_.groundWeapon().upgradeType() == upgrade, ri.getUpLevel(u.type_.airWeapon().upgradeType()) + u.type_.airWeapon().upgradeType() == upgrade);

    int shield_upgrades = static_cast<int>(u.shields_ > 0) * (ri.getUpLevel(UpgradeTypes::Protoss_Plasma_Shields) + UpgradeTypes::Protoss_Plasma_Shields == upgrade); // No tests here.

    bool speed_tech = // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Zergling && (ri.getUpLevel(UpgradeTypes::Metabolic_Boost) || upgrade == UpgradeTypes::Metabolic_Boost)) ||
        (u.type_ == UnitTypes::Zerg_Hydralisk && (ri.getUpLevel(UpgradeTypes::Muscular_Augments) || upgrade == UpgradeTypes::Muscular_Augments)) ||
        (u.type_ == UnitTypes::Zerg_Overlord && (ri.getUpLevel(UpgradeTypes::Pneumatized_Carapace) || upgrade == UpgradeTypes::Pneumatized_Carapace)) ||
        (u.type_ == UnitTypes::Zerg_Ultralisk && (ri.getUpLevel(UpgradeTypes::Anabolic_Synthesis) || upgrade == UpgradeTypes::Anabolic_Synthesis)) ||
        (u.type_ == UnitTypes::Protoss_Scout && (ri.getUpLevel(UpgradeTypes::Gravitic_Thrusters) || upgrade == UpgradeTypes::Gravitic_Thrusters)) ||
        (u.type_ == UnitTypes::Protoss_Observer && (ri.getUpLevel(UpgradeTypes::Gravitic_Boosters) || upgrade == UpgradeTypes::Gravitic_Boosters)) ||
        (u.type_ == UnitTypes::Protoss_Zealot && (ri.getUpLevel(UpgradeTypes::Leg_Enhancements) || upgrade == UpgradeTypes::Leg_Enhancements)) ||
        (u.type_ == UnitTypes::Terran_Vulture && (ri.getUpLevel(UpgradeTypes::Ion_Thrusters) || upgrade == UpgradeTypes::Ion_Thrusters));

    bool range_upgrade = // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Hydralisk && (ri.getUpLevel(UpgradeTypes::Grooved_Spines) || upgrade == UpgradeTypes::Grooved_Spines)) ||
        (u.type_ == UnitTypes::Protoss_Dragoon && (ri.getUpLevel(UpgradeTypes::Singularity_Charge) || upgrade == UpgradeTypes::Singularity_Charge)) ||
        (u.type_ == UnitTypes::Terran_Marine && (ri.getUpLevel(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells)) ||
        (u.type_ == UnitTypes::Terran_Goliath && (ri.getUpLevel(UpgradeTypes::Charon_Boosters) || upgrade == UpgradeTypes::Charon_Boosters)) ||
        (u.type_ == UnitTypes::Terran_Barracks && (ri.getUpLevel(UpgradeTypes::U_238_Shells) || upgrade == UpgradeTypes::U_238_Shells));

    bool attack_speed_upgrade =  // safer to hardcode this.
        (u.type_ == UnitTypes::Zerg_Zergling && (ri.getUpLevel(UpgradeTypes::Adrenal_Glands) || upgrade == UpgradeTypes::Adrenal_Glands));

    int units_inside_object = 2 + (u.type_ == UnitTypes::Protoss_Carrier) * (2 + 4 * ri.getUpLevel(UpgradeTypes::Carrier_Capacity)); // 2 if bunker, 4 if carrier, 8 if "carrier capacity" is present. // Needs to extend for every race. Needs to include an indicator for self.

    return FAP::makeUnit<StoredUnit*>()
        .setData(&u)
        .setUnitType(u.type_)
        .setPosition(chosen_pos)
        .setHealth(u.health_)
        .setShields(u.shields_)
        .setFlying(u.is_flying_)
        .setElevation(u.elevation_)
        .setAttackerCount(units_inside_object)
        .setArmorUpgrades(armor_upgrades)
        .setAttackUpgrades(gun_upgrades)
        .setShieldUpgrades(shield_upgrades)
        .setSpeedUpgrade(speed_tech)
        .setAttackSpeedUpgrade(attack_speed_upgrade)
        .setAttackCooldownRemaining(u.cd_remaining_)
        .setStimmed(u.stimmed_)
        .setRangeUpgrade(range_upgrade)
        ;
}

int CombatSimulator::getScoreGap(bool friendly) const
{
    if (friendly)
        return getFriendlyScore() - getEnemyScore();
    else
        return getEnemyScore() - getFriendlyScore();
}

void CombatSimulator::runSimulation(int duration)
{
    // Run Sim
    if(duration)
        internalFAP_.simulate(duration);
    else
        internalFAP_.simulate(FAP_SIM_DURATION);

    // Update scores
    if (internalFAP_.getState().first && !internalFAP_.getState().first->empty())
        friendly_fap_score_ = std::accumulate(internalFAP_.getState().first->begin(), internalFAP_.getState().first->end(), 0, [this](int currentScore, auto FAPunit) { return static_cast<int>(currentScore + unitWeight(FAPunit)); });
    else
        friendly_fap_score_ = 0;

    if (internalFAP_.getState().second && !internalFAP_.getState().second->empty())
        enemy_fap_score_ = std::accumulate(internalFAP_.getState().second->begin(), internalFAP_.getState().second->end(), 0, [this](int currentScore, auto FAPunit) { return static_cast<int>(currentScore + unitWeight(FAPunit)); });
    else
        enemy_fap_score_ = 0;
}

bool CombatSimulator::unitDeadInFuture(const StoredUnit & unit, const int & number_of_frames_voted_death) const
{
    return unit.count_of_consecutive_predicted_deaths_ >= number_of_frames_voted_death;
}


Position CombatSimulator::positionMiniFAP(const bool friendly)
{
    std::uniform_int_distribution<int> small_map(miniMap_ * friendly, miniMap_ + miniMap_ * friendly);     // default values for output.
    int rand_x = small_map(generator_);
    int rand_y = small_map(generator_);
    return Position(rand_x, rand_y);
}



Position CombatSimulator::positionMCFAP(const StoredUnit su)
{
    std::uniform_int_distribution<int> small_noise(static_cast<int>(-CUNYAIModule::getProperSpeed(su.type_)) * 4, static_cast<int>(CUNYAIModule::getProperSpeed(su.type_)) * 4);     // default values for output.
    int rand_x = small_noise(generator_);
    int rand_y = small_noise(generator_);
    return Position(rand_x, rand_y) + su.pos_;
}


void CombatSimulator::addExtraUnitToSimulation(StoredUnit u, bool friendly)
{
    if(friendly)
        internalFAP_.addIfCombatUnitPlayer1(createFAPVersion(u, CUNYAIModule::friendly_player_model.researches_));
    else
        internalFAP_.addIfCombatUnitPlayer1(createFAPVersion(u, CUNYAIModule::enemy_player_model.researches_));
}

void CombatSimulator::addPlayersToSimulation()
{
    for (auto &u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        internalFAP_.addIfCombatUnitPlayer1(createFAPVersion(u.second, CUNYAIModule::friendly_player_model.researches_));
    }
    for (auto &u : CUNYAIModule::enemy_player_model.units_.unit_map_) {
        internalFAP_.addIfCombatUnitPlayer2(createFAPVersion(u.second, CUNYAIModule::enemy_player_model.researches_));
    }

}

void CombatSimulator::addPlayersToMiniSimulation(const UpgradeType &upgrade, const TechType &tech)
{
    for (auto &u : CUNYAIModule::friendly_player_model.units_.unit_map_) {
        Position pos = positionMiniFAP(true);
        internalFAP_.addIfCombatUnitPlayer1(createModifiedFAPVersion(u.second, CUNYAIModule::friendly_player_model.researches_, pos, upgrade, tech));
    }
    for (auto &u : CUNYAIModule::enemy_player_model.units_.unit_map_) {
        Position pos = positionMiniFAP(false);
        internalFAP_.addIfCombatUnitPlayer2(createModifiedFAPVersion(u.second, CUNYAIModule::enemy_player_model.researches_, pos));
    }
}

const std::vector<FAP::FAPUnit<StoredUnit*>> CombatSimulator::getFriendlySim()
{
    return *internalFAP_.getState().first;
}

const std::vector<FAP::FAPUnit<StoredUnit*>> CombatSimulator::getEnemySim()
{
    return *internalFAP_.getState().second;
}

int CombatSimulator::getFriendlyScore() const
{
    return friendly_fap_score_;
}

int CombatSimulator::getEnemyScore() const
{
    return enemy_fap_score_;
}