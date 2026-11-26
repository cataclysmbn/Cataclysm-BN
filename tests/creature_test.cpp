#include "bodypart.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "monster.h"
#include "mtype.h"
#include "rng.h"
#include "test_statistics.h"

#include <cstdlib>
#include <map>
#include <sstream>
#include <utility>
namespace {

using Weights = std::map<body_part, double>;
struct Expected {
    Weights base, max;
};


const auto expected_smaller = Expected{
    Weights{
        {bp_torso, 20.0},
        {bp_head, 0.0},
        {bp_eyes, 0.0},
        {bp_mouth, 0.0},
        {bp_arm_l, 15.0},
        {bp_arm_r, 15.0},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 25.0},
        {bp_leg_r, 25.0},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}},
    Weights{
        {bp_torso, 4960.0},
        {bp_head, 0.0},
        {bp_eyes, 0.0},
        {bp_mouth, 0.0},
        {bp_arm_l, 1143.0},
        {bp_arm_r, 1186.0},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 3844.0},
        {bp_leg_r, 3867.0},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}}};


const auto expected_same = Expected{
    Weights{
        {bp_torso, 33.33},
        {bp_head, 2.33},
        {bp_eyes, 0.33},
        {bp_mouth, 0.0},
        {bp_arm_l, 20.0},
        {bp_arm_r, 20.0},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 12.0},
        {bp_leg_r, 12.0},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}},
    Weights{
        {bp_torso, 6513},
        {bp_head, 2928},
        {bp_eyes, 150},
        {bp_mouth, 0},
        {bp_arm_l, 1224},
        {bp_arm_r, 1235},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 1458.0},
        {bp_leg_r, 1492.0},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}}};

const auto expected_larger = Expected{
    Weights{
        {bp_torso, 36.57},
        {bp_head, 5.71},
        {bp_eyes, 0.57},
        {bp_mouth, 0.0},
        {bp_arm_l, 22.86},
        {bp_arm_r, 22.86},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 5.71},
        {bp_leg_r, 5.71},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}},
    Weights{
        {bp_torso, 5689.0},
        {bp_head, 5682.0},
        {bp_eyes, 221.0},
        {bp_mouth, 0.0},
        {bp_arm_l, 1185.0},
        {bp_arm_r, 1089.0},
        {bp_hand_l, 0.0},
        {bp_hand_r, 0.0},
        {bp_leg_l, 578.0},
        {bp_leg_r, 556.0},
        {bp_foot_l, 0.0},
        {bp_foot_r, 0.0}}};


void calculate_bodypart_distribution(
    const enum creature_size attacker_size, const enum creature_size defender_size,
    const int hit_roll, const Weights& expected) {
    INFO("hit roll = " << hit_roll);
    auto selected_part_histogram = Weights{
        {bp_torso, 0.0}, {bp_head, 0.0},  {bp_eyes, 0.0},   {bp_mouth, 0.0},
        {bp_arm_l, 0.0}, {bp_arm_r, 0.0}, {bp_hand_l, 0.0}, {bp_hand_r, 0.0},
        {bp_leg_l, 0.0}, {bp_leg_r, 0.0}, {bp_foot_l, 0.0}, {bp_foot_r, 0.0}};

    mtype atype;
    atype.size = attacker_size;
    monster attacker;
    attacker.type = &atype;
    mtype dtype;
    dtype.size = defender_size;
    monster defender;
    defender.type = &dtype;

    const int num_tests = 15000;

    for (int i = 0; i < num_tests; ++i) {
        const auto bp = defender.select_body_part(&attacker, hit_roll);
        selected_part_histogram.at(bp->token)++;
    }

    const double total_weight =
        std::accumulate(expected.begin(), expected.end(), 0.0, [](double acc, const auto& p) {
            return acc + p.second;
        });

    std::stringstream ss;
    for (const auto& [bp, weight] : selected_part_histogram) {
        ss << body_part_name(bp) << ": " << weight << ", ";
    }
    INFO('{' << ss.str() << "}\n");
    for (const auto& [bp, weight] : selected_part_histogram) {
        const double expected_proportion = expected.at(bp) / total_weight;
        CHECK_THAT(weight, IsBinomialObservation(num_tests, expected_proportion));
    }
}
} // namespace

TEST_CASE("Check distribution of attacks to body parts for same sized opponents.") {
    rng_set_engine_seed(4242424242);

    calculate_bodypart_distribution(
        creature_size::small, creature_size::small, 0, expected_same.base);
    calculate_bodypart_distribution(
        creature_size::small, creature_size::small, 1, expected_same.base);
    calculate_bodypart_distribution(
        creature_size::small, creature_size::small, 100, expected_same.max);
}

TEST_CASE("Check distribution of attacks to body parts for smaller attacker.") {
    rng_set_engine_seed(4242424242);

    calculate_bodypart_distribution(
        creature_size::small, creature_size::medium, 0, expected_smaller.base);
    calculate_bodypart_distribution(
        creature_size::small, creature_size::medium, 1, expected_smaller.base);
    calculate_bodypart_distribution(
        creature_size::small, creature_size::medium, 100, expected_smaller.max);
}

TEST_CASE("Check distribution of attacks to body parts for larger attacker.") {
    rng_set_engine_seed(4242424242);

    calculate_bodypart_distribution(
        creature_size::medium, creature_size::small, 0, expected_larger.base);
    calculate_bodypart_distribution(
        creature_size::medium, creature_size::small, 1, expected_larger.base);
    calculate_bodypart_distribution(
        creature_size::medium, creature_size::small, 100, expected_larger.max);
}
