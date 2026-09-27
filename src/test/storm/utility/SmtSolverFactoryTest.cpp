#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/Environment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/SolverSelectionOptions.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/solver.h"

#ifdef STORM_HAVE_MATHSAT
#include "storm/solver/MathsatSmtSolver.h"
#endif
#ifdef STORM_HAVE_CVC5
#include "storm/solver/Cvc5SmtSolver.h"
#endif
#ifdef STORM_HAVE_Z3
#include "storm/solver/Z3SmtSolver.h"
#endif

TEST(SmtSolverFactory, EnvironmentIsSeededFromSettings) {
    storm::Environment env;

    // The environment picks up the SMT solver that is selected in the core settings.
    EXPECT_EQ(env.solver().getSmtSolverType(), storm::settings::getModule<storm::settings::modules::CoreSettings>().getSmtSolver());
    EXPECT_EQ(env.solver().isSmtSolverTypeSetFromDefaultValue(),
              storm::settings::getModule<storm::settings::modules::CoreSettings>().isSmtSolverSetFromDefaultValue());
}

#ifdef STORM_HAVE_CVC5
TEST(SmtSolverFactory, EnvironmentOverridesSmtSolver) {
    storm::Environment env;
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    env.solver().setSmtSolverType(storm::solver::SmtSolverType::Cvc5);
    EXPECT_EQ(env.solver().getSmtSolverType(), storm::solver::SmtSolverType::Cvc5);
    EXPECT_FALSE(env.solver().isSmtSolverTypeSetFromDefaultValue());

    // An SMT solver that was selected explicitly in the environment takes precedence over the core setting.
    std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::getSmtSolver(env, *manager);
    ASSERT_NE(solver, nullptr);
    EXPECT_NE(dynamic_cast<storm::solver::Cvc5SmtSolver*>(solver.get()), nullptr);
}
#endif

#if defined STORM_HAVE_CVC5 && defined STORM_HAVE_Z3
TEST(SmtSolverFactory, EnvironmentSelectionIsUsedEvenIfSeededFromDefault) {
    storm::Environment env;
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    // The environment is the single place where the core setting is turned into a solver selection, so
    // the solver stored in it is used even if the value was seeded from the default of the setting.
    env.solver().setSmtSolverType(storm::solver::SmtSolverType::Cvc5, true);
    EXPECT_EQ(env.solver().getSmtSolverType(), storm::solver::SmtSolverType::Cvc5);
    EXPECT_TRUE(env.solver().isSmtSolverTypeSetFromDefaultValue());

    std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::getSmtSolver(env, *manager);
    ASSERT_NE(solver, nullptr);
    EXPECT_NE(dynamic_cast<storm::solver::Cvc5SmtSolver*>(solver.get()), nullptr);
}
#endif

TEST(SmtSolverFactory, WithoutEnvironmentTheCompileTimeDefaultIsUsed) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    // Without an environment, there is no setting to consult, so the compile-time default is used.
    std::unique_ptr<storm::solver::SmtSolver> solver = storm::utility::solver::getSmtSolver(*manager);
    ASSERT_NE(solver, nullptr);
#ifdef STORM_DEFAULT_SMT_SOLVER_Z3
    EXPECT_NE(dynamic_cast<storm::solver::Z3SmtSolver*>(solver.get()), nullptr);
#elif defined STORM_DEFAULT_SMT_SOLVER_MATHSAT
    EXPECT_NE(dynamic_cast<storm::solver::MathsatSmtSolver*>(solver.get()), nullptr);
#elif defined STORM_DEFAULT_SMT_SOLVER_CVC5
    EXPECT_NE(dynamic_cast<storm::solver::Cvc5SmtSolver*>(solver.get()), nullptr);
#else
    FAIL() << "No compile-time default SMT solver was configured.";
#endif
}
