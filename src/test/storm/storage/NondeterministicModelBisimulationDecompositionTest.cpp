#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/storm.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/bisimulation/NondeterministicModelBisimulationDecomposition.h"

namespace {
static constexpr double DefaultTestTolerance = 1e-6;

// Model checks propertyString on both the original model and its bisimulation quotient; returns (ground truth,
// quotient value) for the initial state.
std::pair<double, double> computeGroundTruthAndBisimulationResult(std::string const& modelFile, std::string const& propertyString) {
    storm::prism::Program program = storm::parser::PrismParser::parse(modelFile);

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString(propertyString);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = {formula};

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::api::buildSparseModel<double>(program, formulas);
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = model->as<storm::models::sparse::Mdp<double>>();

    double groundTruth = storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formula, true))
                             ->template asExplicitQuantitativeCheckResult<double>()[*model->getInitialStates().begin()];

    typename storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>>::Options options(*mdp, *formula,
                                                                                                                                 DefaultTestTolerance);
    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim(*mdp, options);
    bisim.computeBisimulationDecomposition();
    std::shared_ptr<storm::models::sparse::Model<double>> quotient = bisim.getQuotient();

    double quotientValue = storm::api::verifyWithSparseEngine(quotient, storm::api::createTask<double>(formula, true))
                               ->template asExplicitQuantitativeCheckResult<double>()[*quotient->getInitialStates().begin()];

    return {groundTruth, quotientValue};
}

// EXPECT_NEAR fails on infinities (inf - inf = NaN). Mirrors expectVectorNear in storm-dft/bdd/TestBdd.cpp.
void expectRewardNear(double expected, double actual, double precision = 1e-6) {
    if (std::isinf(expected)) {
        EXPECT_EQ(expected, actual);
    } else {
        EXPECT_NEAR(expected, actual, precision);
    }
}

}  // namespace

TEST(NondeterministicModelBisimulationDecomposition, TwoDice) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");

    // Build the die model without its reward model.
    std::shared_ptr<storm::models::sparse::Model<double>> model =
        storm::builder::ExplicitModelBuilder<double>(program, storm::generator::NextStateGeneratorOptions(false, true)).build();

    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = model->as<storm::models::sparse::Mdp<double>>();

    using OptionsType = typename storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>>::Options;

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim(
        *mdp, OptionsType::preservingAllLabels(DefaultTestTolerance));
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    std::shared_ptr<storm::models::sparse::Model<double>> result;
    ASSERT_NO_THROW(result = bisim.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(77ul, result->getNumberOfStates());
    EXPECT_EQ(183ul, result->getNumberOfTransitions());
    EXPECT_EQ(97ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());

    OptionsType options = OptionsType::preservingAllLabels(DefaultTestTolerance);
    options.respectedAtomicPropositions = std::set<std::string>({"two"});

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim2(*mdp, options);
    ASSERT_NO_THROW(bisim2.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim2.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(11ul, result->getNumberOfStates());
    EXPECT_EQ(26ul, result->getNumberOfTransitions());
    EXPECT_EQ(14ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());

    // A parser that we use for conveniently constructing the formulas.
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Pmin=? [F \"two\"]");

    OptionsType options2(*mdp, *formula, DefaultTestTolerance);

    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim3(*mdp, options2);
    ASSERT_NO_THROW(bisim3.computeBisimulationDecomposition());
    ASSERT_NO_THROW(result = bisim3.getQuotient());

    EXPECT_EQ(storm::models::ModelType::Mdp, result->getType());
    EXPECT_EQ(11ul, result->getNumberOfStates());
    EXPECT_EQ(26ul, result->getNumberOfTransitions());
    EXPECT_EQ(14ul, result->as<storm::models::sparse::Mdp<double>>()->getNumberOfChoices());
}

// Regression test: measure-driven partition used to merge all probability-1 states into one block regardless of
// their (differing, finite) expected reward, turning a finite Rmin into "infinity" in the quotient.
TEST(NondeterministicModelBisimulationDecomposition, MeasureDrivenRewardUnsound) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards2.nm");

    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula = formulaParser.parseSingleFormulaFromString("Rmin=? [F \"goal\"]");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = {formula};

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::api::buildSparseModel<double>(program, formulas);
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    std::shared_ptr<storm::models::sparse::Mdp<double>> mdp = model->as<storm::models::sparse::Mdp<double>>();
    size_t initialState = *model->getInitialStates().begin();

    // Ground truth: model check the original, unreduced model.
    std::unique_ptr<storm::modelchecker::CheckResult> groundTruthCheckResult =
        storm::api::verifyWithSparseEngine(model, storm::api::createTask<double>(formula, true));
    double groundTruthValue = groundTruthCheckResult->asExplicitQuantitativeCheckResult<double>()[initialState];
    // Sanity check: the correct result is finite (a scheduler exists that reaches "goal" almost surely).
    ASSERT_LT(groundTruthValue, storm::utility::infinity<double>());

    typename storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>>::Options options(*mdp, *formula,
                                                                                                                                 DefaultTestTolerance);
    storm::storage::NondeterministicModelBisimulationDecomposition<storm::models::sparse::Mdp<double>> bisim(*mdp, options);
    ASSERT_NO_THROW(bisim.computeBisimulationDecomposition());
    std::shared_ptr<storm::models::sparse::Model<double>> quotient;
    ASSERT_NO_THROW(quotient = bisim.getQuotient());
    size_t quotientInitialState = *quotient->getInitialStates().begin();

    std::unique_ptr<storm::modelchecker::CheckResult> quotientCheckResult =
        storm::api::verifyWithSparseEngine(quotient, storm::api::createTask<double>(formula, true));
    double quotientValue = quotientCheckResult->asExplicitQuantitativeCheckResult<double>()[quotientInitialState];

    // The quotient must preserve the value of the formula it was built for.
    EXPECT_NEAR(groundTruthValue, quotientValue, 1e-6);
}

// Model from https://github.com/moves-rwth/storm/issues/683: action a3 (at s=2) loops back towards "goal" (finite
// Rmin), while a4 self-loops at s=2 forever (infinite Rmax). Exercises the Prob1E/Prob1A asymmetry.
TEST(NondeterministicModelBisimulationDecomposition, TinyRewards3Issue683) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto [groundTruthMin, quotientMin] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards3.nm", "Rmin=? [F \"goal\"]");
    ASSERT_LT(groundTruthMin, storm::utility::infinity<double>());
    expectRewardNear(groundTruthMin, quotientMin);

    auto [groundTruthMax, quotientMax] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards3.nm", "Rmax=? [F \"goal\"]");
    ASSERT_TRUE(std::isinf(groundTruthMax));
    expectRewardNear(groundTruthMax, quotientMax);
}

// s=0 can self-loop forever ("s=0 -> true"), avoiding "target" without incurring reward. Rmax is still infinite
// by convention, even though the self-loop's own reward is 0.
TEST(NondeterministicModelBisimulationDecomposition, TinyRewardsInfinity) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto [groundTruthMin, quotientMin] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.nm", "Rmin=? [F \"target\"]");
    ASSERT_LT(groundTruthMin, storm::utility::infinity<double>());
    expectRewardNear(groundTruthMin, quotientMin);

    auto [groundTruthMax, quotientMax] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.nm", "Rmax=? [F \"target\"]");
    ASSERT_TRUE(std::isinf(groundTruthMax));
    expectRewardNear(groundTruthMax, quotientMax);
}

// Coverage on a larger, non-artificial model: "coinflips" reward on two_dice.nm (Rmin == Rmax, no nondeterminism).
TEST(NondeterministicModelBisimulationDecomposition, TwoDiceExpectedCoinFlips) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    auto [groundTruthMin, quotientMin] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Rmin=? [F \"done\"]");
    expectRewardNear(groundTruthMin, quotientMin);

    auto [groundTruthMax, quotientMax] = computeGroundTruthAndBisimulationResult(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "Rmax=? [F \"done\"]");
    expectRewardNear(groundTruthMax, quotientMax);
}
