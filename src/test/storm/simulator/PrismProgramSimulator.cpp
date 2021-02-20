#include "test/storm_gtest.h"
#include "storm/simulator/PrismProgramSimulator.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/environment/Environment.h"

TEST(PrismProgramSimulatorTest, KnuthYaoDieTest) {
    storm::Environment env;
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/die_c1.nm");
    storm::builder::BuilderOptions options;
    options.setBuildAllRewardModels();

    storm::simulator::DiscreteTimePrismProgramSimulator<double> sim(program, options);
    auto rew = sim.getLastRewards();
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(0.0, rew[0]);
#if 0
    std::cout << "reward: ";
    for (auto const& r : rew) {
        std::cout << r << " ";
    }
    std::cout << std::endl;
    std::cout << sim.getCurrentStateAsValuation() << std::endl;
    for (auto const& c : sim.getChoices()) {
        std::cout << "Choice ";
        std::cout << "action index: " << program.getActionName(c.getActionIndex()) << std::endl;
    }
#endif
    EXPECT_EQ(2ul, sim.getChoices().size());
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(0.0, rew[0]);
#if 0
    std::cout << "reward: ";
    for (auto const& r : rew) {
        std::cout << r << " ";
    }
    std::cout << std::endl;
    std::cout << sim.getCurrentStateString() << std::endl;
#endif
    EXPECT_EQ(1ul, sim.getChoices().size());
    sim.step(0);
    rew = sim.getLastRewards();
    EXPECT_EQ(1ul, rew.size());
    EXPECT_EQ(1.0, rew[0]);
#if 0
    std::cout << "reward: ";
    for (auto const& r : rew) {
        std::cout << r << " ";
    }
    std::cout << std::endl;
    std::cout << sim.getCurrentStateString() << std::endl;
#endif

}