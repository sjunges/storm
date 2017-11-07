#include "storm/modelchecker/abstraction/AbstractAbstractionRefinementModelChecker.h"

#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/Bdd.h"

#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"

#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"

#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"

#include "storm/modelchecker/prctl/helper/SymbolicDtmcPrctlHelper.h"
#include "storm/modelchecker/prctl/helper/SymbolicMdpPrctlHelper.h"

#include "storm/solver/SymbolicGameSolver.h"

#include "storm/abstraction/StateSet.h"
#include "storm/abstraction/SymbolicStateSet.h"
#include "storm/abstraction/QualitativeResultMinMax.h"
#include "storm/abstraction/QualitativeMdpResult.h"
#include "storm/abstraction/QualitativeMdpResultMinMax.h"
#include "storm/abstraction/QualitativeGameResultMinMax.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/AbstractionSettings.h"

#include "storm/utility/graph.h"

#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/InvalidPropertyException.h"

namespace storm {
    namespace modelchecker {

        template<typename ModelType>
        AbstractAbstractionRefinementModelChecker<ModelType>::AbstractAbstractionRefinementModelChecker() : AbstractModelChecker<ModelType>(), checkTask(nullptr) {
            storm::settings::modules::AbstractionSettings::ReuseMode reuseMode = storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getReuseMode();
            reuseQualitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All || reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Qualitative;
            reuseQuantitativeResults = reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::All || reuseMode == storm::settings::modules::AbstractionSettings::ReuseMode::Quantitative;
        }
        
        template<typename ModelType>
        AbstractAbstractionRefinementModelChecker<ModelType>::~AbstractAbstractionRefinementModelChecker() {
            // Intentionally left empty.
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula> const& checkTask) const {
            storm::logic::Formula const& formula = checkTask.getFormula();
            bool enableRewards = this->supportsReachabilityRewards();
            storm::logic::FragmentSpecification fragment = storm::logic::reachability().setRewardOperatorsAllowed(enableRewards).setReachabilityRewardFormulasAllowed(enableRewards);
            return formula.isInFragment(fragment) && checkTask.isOnlyInitialStatesRelevantSet();
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::supportsReachabilityRewards() const {
            return false;
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeUntilProbabilities(CheckTask<storm::logic::UntilFormula, ValueType> const& checkTask) {
            this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
            return performAbstractionRefinement();
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityProbabilities(CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
            STORM_LOG_THROW(this->supportsReachabilityRewards(), storm::exceptions::NotSupportedException, "Reachability rewards are not supported by this abstraction-refinement model checker.");
            this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
            return performAbstractionRefinement();
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityRewards(storm::logic::RewardMeasureType rewardMeasureType, CheckTask<storm::logic::EventuallyFormula, ValueType> const& checkTask) {
            this->setCheckTask(checkTask.template substituteFormula<storm::logic::Formula>(checkTask.getFormula()));
            return performAbstractionRefinement();
        }
        
        template<typename ModelType>
        void AbstractAbstractionRefinementModelChecker<ModelType>::setCheckTask(CheckTask<storm::logic::Formula, ValueType> const& checkTask) {
            this->checkTask = std::make_unique<CheckTask<storm::logic::Formula, ValueType>>(checkTask);
        }
        
        template<typename ModelType>
        CheckTask<storm::logic::Formula, typename AbstractAbstractionRefinementModelChecker<ModelType>::ValueType> const& AbstractAbstractionRefinementModelChecker<ModelType>::getCheckTask() const {
            return *checkTask;
        }

        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::getReuseQualitativeResults() const {
            return reuseQualitativeResults;
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::getReuseQuantitativeResults() const {
            return reuseQuantitativeResults;
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::performAbstractionRefinement() {
            STORM_LOG_THROW(checkTask->isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidPropertyException, "The abstraction-refinement model checkers can only compute the result for the initial states.");
            
            // Notify the underlying implementation that the abstraction-refinement process is being started.
            this->initializeAbstractionRefinement();

            uint64_t iterations = 0;
            std::unique_ptr<CheckResult> result;
            auto start = std::chrono::high_resolution_clock::now();
            while (!result) {
                ++iterations;
                
                // Obtain the abstract model.
                auto abstractionStart = std::chrono::high_resolution_clock::now();
                std::shared_ptr<storm::models::Model<ValueType>> abstractModel = this->getAbstractModel();
                auto abstractionEnd = std::chrono::high_resolution_clock::now();
                STORM_LOG_TRACE("Model in iteration " << iterations << " has " << abstractModel->getNumberOfStates() << " states and " << abstractModel->getNumberOfTransitions() << " transitions (retrieved in " << std::chrono::duration_cast<std::chrono::milliseconds>(abstractionEnd - abstractionStart).count() << "ms).");

                // Obtain lower and upper bounds from the abstract model.
                std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> bounds = computeBounds(*abstractModel);
                
                // Try to derive the final result from the obtained bounds.
                result = tryToObtainResultFromBounds(*abstractModel, bounds);
                if (!result) {
                    auto refinementStart = std::chrono::high_resolution_clock::now();
                    this->refineAbstractModel();
                    auto refinementEnd = std::chrono::high_resolution_clock::now();
                    STORM_LOG_TRACE("Refinement in iteration " << iterations << " completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(refinementEnd - refinementStart).count() << "ms.");
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            STORM_LOG_TRACE("Completed abstraction-refinement (" << this->getName() << ") in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms.");

            return result;
        }
        
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeBounds(storm::models::Model<ValueType> const& abstractModel) {
            // We go through two phases. In phase (1) we are solving the qualitative part and in phase (2) the quantitative part.
            
            std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> result;
            
            // Preparation: determine the constraint states and the target states of the reachability objective.
            std::pair<std::unique_ptr<storm::abstraction::StateSet>, std::unique_ptr<storm::abstraction::StateSet>> constraintAndTargetStates = getConstraintAndTargetStates(abstractModel);

            // Phase (1): solve qualitatively.
            lastQualitativeResults = computeQualitativeResult(abstractModel, *constraintAndTargetStates.first, *constraintAndTargetStates.second);
            
            // Check whether the answer can be given after the qualitative solution.
            result.first = checkForResultAfterQualitativeCheck(abstractModel);
            if (result.first) {
                return result;
            }

            // Check whether we should skip the quantitative solution (for example if there are initial states for which
            // the value is already known to be different at this point.
            bool doSkipQuantitativeSolution = skipQuantitativeSolution(abstractModel, *lastQualitativeResults);
            STORM_LOG_TRACE("" << (doSkipQuantitativeSolution ? "Skipping" : "Not skipping") << " quantitative solution.");

            // Phase (2): solve quantitatively.
            if (!doSkipQuantitativeSolution) {
                lastBounds = computeQuantitativeResult(abstractModel, *constraintAndTargetStates.first, *constraintAndTargetStates.second, *lastQualitativeResults);
                STORM_LOG_ASSERT(lastBounds.first && lastBounds.second, "Expected both bounds.");
                
                result = std::make_pair(lastBounds.first->clone(), lastBounds.second->clone());
                filterInitialStates(abstractModel, result);
                printBoundsInformation(result);

                // Check whether the answer can be given after the quantitative solution.
                if (checkForResultAfterQuantitativeCheck(abstractModel, true, result.first->asQuantitativeCheckResult<ValueType>())) {
                    result.second = nullptr;
                } else if (checkForResultAfterQuantitativeCheck(abstractModel, false, result.second->asQuantitativeCheckResult<ValueType>())) {
                    result.first = nullptr;
                }
            } else {
                // In this case, we construct the full results from the qualitative results.
                auto symbolicModel = abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>();
                std::unique_ptr<CheckResult> lowerBounds = std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(symbolicModel->getReachableStates(), symbolicModel->getInitialStates(), symbolicModel->getInitialStates().ite(lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates().template toAdd<ValueType>(), symbolicModel->getManager().template getAddZero<ValueType>()));
                std::unique_ptr<CheckResult> upperBounds = std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(symbolicModel->getReachableStates(), symbolicModel->getInitialStates(), symbolicModel->getInitialStates().ite(lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Max().getStates().template toAdd<ValueType>(), symbolicModel->getManager().template getAddZero<ValueType>()));
                
                result = std::make_pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>>(std::move(lowerBounds), std::move(upperBounds));
            }
            
            return result;
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQuantitativeCheck(storm::models::Model<ValueType> const& abstractModel, bool lowerBounds, QuantitativeCheckResult<ValueType> const& quantitativeResult) {
            
            bool result = false;
            if (checkTask->isBoundSet()) {
                storm::logic::ComparisonType comparisonType = checkTask->getBoundComparisonType();
                ValueType threshold = checkTask->getBoundThreshold();
                
                if (lowerBounds) {
                    if (storm::logic::isLowerBound(comparisonType)) {
                        ValueType minimalLowerBound = quantitativeResult.getMin();
                        result = (storm::logic::isStrict(comparisonType) && minimalLowerBound > threshold) || (!storm::logic::isStrict(comparisonType) && minimalLowerBound >= threshold);
                    } else {
                        ValueType maximalLowerBound = quantitativeResult.getMax();
                        result = (storm::logic::isStrict(comparisonType) && maximalLowerBound >= threshold) || (!storm::logic::isStrict(comparisonType) && maximalLowerBound > threshold);
                    }
                } else {
                    if (storm::logic::isLowerBound(comparisonType)) {
                        ValueType minimalUpperBound = quantitativeResult.getMin();
                        result = (storm::logic::isStrict(comparisonType) && minimalUpperBound <= threshold) || (!storm::logic::isStrict(comparisonType) && minimalUpperBound < threshold);
                    } else {
                        ValueType maximalUpperBound = quantitativeResult.getMax();
                        result = (storm::logic::isStrict(comparisonType) && maximalUpperBound < threshold) || (!storm::logic::isStrict(comparisonType) && maximalUpperBound <= threshold);
                    }
                }
                
                if (result) {
                    STORM_LOG_TRACE("Check for result after quantiative check positive.");
                }
            }
            
            return result;
        }

        template<typename ModelType>
        void AbstractAbstractionRefinementModelChecker<ModelType>::printBoundsInformation(std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>>& bounds) {
            STORM_LOG_THROW(bounds.first->isSymbolicQuantitativeCheckResult() && bounds.second->isSymbolicQuantitativeCheckResult(), storm::exceptions::NotSupportedException, "Expected symbolic bounds.");
            
            printBoundsInformation(bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>(), bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>());
        }
        
        template<typename ModelType>
        void AbstractAbstractionRefinementModelChecker<ModelType>::printBoundsInformation(SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds, SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds) {
            
            // If there is exactly one value that we stored, we print the current bounds as an interval.
            if (lowerBounds.getStates().getNonZeroCount() == 1 && upperBounds.getStates().getNonZeroCount() == 1) {
                STORM_LOG_TRACE("Obtained bounds [" << lowerBounds.getValueVector().getMax() << ", " << upperBounds.getValueVector().getMax() << "] on actual result.");
            } else {
                storm::dd::Add<DdType, ValueType> diffs = upperBounds.getValueVector() - lowerBounds.getValueVector();
                storm::dd::Bdd<DdType> maxDiffRepresentative = diffs.maxAbstractRepresentative(diffs.getContainedMetaVariables());
                
                std::pair<ValueType, ValueType> bounds;
                bounds.first = (lowerBounds.getValueVector() * maxDiffRepresentative.template toAdd<ValueType>()).getMax();
                bounds.second = (upperBounds.getValueVector() * maxDiffRepresentative.template toAdd<ValueType>()).getMax();
                
                STORM_LOG_TRACE("Largest interval over initial is [" << bounds.first << ", " << bounds.second << "], difference " << (bounds.second - bounds.first) << ".");
            }
        }
        
        template<typename ModelType>
        void AbstractAbstractionRefinementModelChecker<ModelType>::filterInitialStates(storm::models::Model<ValueType> const& abstractModel, std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>>& bounds) {
            STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");

            filterInitialStates(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(), bounds);
        }
        
        template<typename ModelType>
        void AbstractAbstractionRefinementModelChecker<ModelType>::filterInitialStates(storm::models::symbolic::Model<DdType, ValueType> const& abstractModel, std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>>& bounds) {
            storm::modelchecker::SymbolicQualitativeCheckResult<DdType> initialStateFilter(abstractModel.getReachableStates(), abstractModel.getInitialStates());
            bounds.first->filter(initialStateFilter);
            bounds.second->filter(initialStateFilter);
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::skipQuantitativeSolution(storm::models::Model<ValueType> const& abstractModel, storm::abstraction::QualitativeResultMinMax const& qualitativeResults) {
            STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");
            
            return skipQuantitativeSolution(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(), qualitativeResults.asSymbolicQualitativeResultMinMax<DdType>());
        }

        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::skipQuantitativeSolution(storm::models::symbolic::Model<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            if (isRewardFormula) {
                if ((abstractModel.getInitialStates() && qualitativeResults.getProb1Min().getStates()) != (abstractModel.getInitialStates() && qualitativeResults.getProb1Max().getStates())) {
                    return true;
                }
            } else {
                if ((abstractModel.getInitialStates() && qualitativeResults.getProb0Min().getStates()) != (abstractModel.getInitialStates() && qualitativeResults.getProb0Max().getStates())) {
                    return true;
                } else if ((abstractModel.getInitialStates() && qualitativeResults.getProb1Min().getStates()) != (abstractModel.getInitialStates() && qualitativeResults.getProb1Max().getStates())) {
                    return true;
                }
            }
            return false;
        }
        
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(storm::models::Model<ValueType> const& abstractModel, storm::abstraction::StateSet const& constraintStates, storm::abstraction::StateSet const& targetStates, storm::abstraction::QualitativeResultMinMax const& qualitativeResults) {
            STORM_LOG_ASSERT(abstractModel.isSymbolicModel(), "Expected symbolic abstract model.");
            
            return computeQuantitativeResult(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(), constraintStates.asSymbolicStateSet<DdType>(), targetStates.asSymbolicStateSet<DdType>(), qualitativeResults.asSymbolicQualitativeResultMinMax<DdType>());
        }
        
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(storm::models::symbolic::Model<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates, storm::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {

            STORM_LOG_THROW(abstractModel.isOfType(storm::models::ModelType::Dtmc) || abstractModel.isOfType(storm::models::ModelType::Mdp) || abstractModel.isOfType(storm::models::ModelType::Mdp), storm::exceptions::NotSupportedException, "Abstract model type is not supported.");
            
            if (abstractModel.isOfType(storm::models::ModelType::Dtmc)) {
                return computeQuantitativeResult(*abstractModel.template as<storm::models::symbolic::Dtmc<DdType, ValueType>>(), constraintStates, targetStates, qualitativeResults);
            } else if (abstractModel.isOfType(storm::models::ModelType::Mdp)) {
                return computeQuantitativeResult(*abstractModel.template as<storm::models::symbolic::Mdp<DdType, ValueType>>(), constraintStates, targetStates, qualitativeResults);
            } else {
                return computeQuantitativeResult(*abstractModel.template as<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>(), constraintStates, targetStates, qualitativeResults);
            }
        }
        
        // FIXME: reuse previous result
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(storm::models::symbolic::Dtmc<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates, storm::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
            
            std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> result;
            
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            if (isRewardFormula) {
                storm::dd::Bdd<DdType> maybe = qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates();
                storm::dd::Add<DdType, ValueType> values = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeReachabilityRewards(abstractModel, abstractModel.getTransitionMatrix(), checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybe, targetStates.getStates(), !qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates(), storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType>(), abstractModel.getManager().template getAddZero<ValueType>());
                
                result.first = std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), values);
                result.second = result.first->clone();
            } else {
                storm::dd::Bdd<DdType> maybe = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
                storm::dd::Add<DdType, ValueType> values = storm::modelchecker::helper::SymbolicDtmcPrctlHelper<DdType, ValueType>::computeUntilProbabilities(abstractModel, abstractModel.getTransitionMatrix(), maybe, qualitativeResults.getProb1Min().getStates(), storm::solver::GeneralSymbolicLinearEquationSolverFactory<DdType, ValueType>(), abstractModel.getManager().template getAddZero<ValueType>());
                
                result.first = std::make_unique<SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), values);
                result.second = result.first->clone();
            }
            
            return result;
        }

        // FIXME: reuse previous result
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(storm::models::symbolic::Mdp<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates, storm::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {
            
            std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> result;
            
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            if (isRewardFormula) {
                storm::dd::Bdd<DdType> maybeMin = qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates();
                result.first = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(storm::OptimizationDirection::Minimize, abstractModel, abstractModel.getTransitionMatrix(), abstractModel.getTransitionMatrix().notZero(), checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybeMin, targetStates.getStates(), !qualitativeResults.getProb1Min().getStates() && abstractModel.getReachableStates(), storm::solver::GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>(), abstractModel.getManager().template getAddZero<ValueType>());
                
                storm::dd::Bdd<DdType> maybeMax = qualitativeResults.getProb1Max().getStates() && abstractModel.getReachableStates();
                result.second = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeReachabilityRewards(storm::OptimizationDirection::Maximize, abstractModel, abstractModel.getTransitionMatrix(), abstractModel.getTransitionMatrix().notZero(), checkTask->isRewardModelSet() ? abstractModel.getRewardModel(checkTask->getRewardModel()) : abstractModel.getRewardModel(""), maybeMin, targetStates.getStates(), !qualitativeResults.getProb1Max().getStates() && abstractModel.getReachableStates(), storm::solver::GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>(), maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(), abstractModel.getManager().template getAddZero<ValueType>()));
            } else {
                storm::dd::Bdd<DdType> maybeMin = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
                result.first = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(storm::OptimizationDirection::Minimize, abstractModel, abstractModel.getTransitionMatrix(), maybeMin, qualitativeResults.getProb1Min().getStates(), storm::solver::GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>(), abstractModel.getManager().template getAddZero<ValueType>());
                
                storm::dd::Bdd<DdType> maybeMax = !(qualitativeResults.getProb0Max().getStates() || qualitativeResults.getProb1Max().getStates()) && abstractModel.getReachableStates();
                result.second = storm::modelchecker::helper::SymbolicMdpPrctlHelper<DdType, ValueType>::computeUntilProbabilities(storm::OptimizationDirection::Maximize, abstractModel, abstractModel.getTransitionMatrix(), maybeMax, qualitativeResults.getProb1Max().getStates(), storm::solver::GeneralSymbolicMinMaxLinearEquationSolverFactory<DdType, ValueType>(), maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(), abstractModel.getManager().template getAddZero<ValueType>()));
            }
            
            return result;
        }

        // FIXME: reuse previous result
        template<typename ModelType>
        std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQuantitativeResult(storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates, storm::abstraction::SymbolicQualitativeResultMinMax<DdType> const& qualitativeResults) {

            std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> result;
            
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            if (isRewardFormula) {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Reward properties are not supported for abstract stochastic games.");
            } else {
                storm::dd::Bdd<DdType> maybeMin = !(qualitativeResults.getProb0Min().getStates() || qualitativeResults.getProb1Min().getStates()) && abstractModel.getReachableStates();
                result.first = computeReachabilityProbabilitiesHelper(abstractModel, this->getAbstractionPlayer() == 1 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(), this->getAbstractionPlayer() == 2 ? storm::OptimizationDirection::Minimize : checkTask->getOptimizationDirection(), maybeMin, qualitativeResults.getProb1Min().getStates(), abstractModel.getManager().template getAddZero<ValueType>());
                
                storm::dd::Bdd<DdType> maybeMax = !(qualitativeResults.getProb0Max().getStates() || qualitativeResults.getProb1Max().getStates()) && abstractModel.getReachableStates();
                result.second = computeReachabilityProbabilitiesHelper(abstractModel, this->getAbstractionPlayer() == 1 ? storm::OptimizationDirection::Maximize : checkTask->getOptimizationDirection(), this->getAbstractionPlayer() == 2 ? storm::OptimizationDirection::Maximize : checkTask->getOptimizationDirection(), maybeMin, qualitativeResults.getProb1Max().getStates(), maybeMax.ite(result.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>().getValueVector(), abstractModel.getManager().template getAddZero<ValueType>()));
            }
            
            return result;
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::computeReachabilityProbabilitiesHelper(storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel, storm::OptimizationDirection const& player1Direction, storm::OptimizationDirection const& player2Direction, storm::dd::Bdd<DdType> const& maybeStates, storm::dd::Bdd<DdType> const& prob1States, storm::dd::Add<DdType, ValueType> const& startValues) {
            
            STORM_LOG_TRACE("Performing quantative solution step. Player 1: " << player1Direction << ", player 2: " << player2Direction << ".");
            
            // Compute the ingredients of the equation system.
            storm::dd::Add<DdType, ValueType> maybeStatesAdd = maybeStates.template toAdd<ValueType>();
            storm::dd::Add<DdType, ValueType> submatrix = maybeStatesAdd * abstractModel.getTransitionMatrix();
            storm::dd::Add<DdType, ValueType> prob1StatesAsColumn = prob1States.template toAdd<ValueType>().swapVariables(abstractModel.getRowColumnMetaVariablePairs());
            storm::dd::Add<DdType, ValueType> subvector = submatrix * prob1StatesAsColumn;
            subvector = subvector.sumAbstract(abstractModel.getColumnVariables());
            
            // Cut away all columns targeting non-maybe states.
            submatrix *= maybeStatesAdd.swapVariables(abstractModel.getRowColumnMetaVariablePairs());
            
            // Cut the starting vector to the maybe states of this query.
            storm::dd::Add<DdType, ValueType> startVector = maybeStates.ite(startValues, abstractModel.getManager().template getAddZero<ValueType>());
            
            // Create the solver and solve the equation system.
            storm::solver::SymbolicGameSolverFactory<DdType, ValueType> solverFactory;
            std::unique_ptr<storm::solver::SymbolicGameSolver<DdType, ValueType>> solver = solverFactory.create(submatrix, maybeStates, abstractModel.getIllegalPlayer1Mask(), abstractModel.getIllegalPlayer2Mask(), abstractModel.getRowVariables(), abstractModel.getColumnVariables(), abstractModel.getRowColumnMetaVariablePairs(), abstractModel.getPlayer1Variables(), abstractModel.getPlayer2Variables());
            auto values = solver->solveGame(player1Direction, player2Direction, startVector, subvector);
            
            return std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), prob1States.template toAdd<ValueType>() + values);
        }

        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(storm::models::Model<ValueType> const& abstractModel, storm::abstraction::StateSet const& constraintStates, storm::abstraction::StateSet const& targetStates) {
            STORM_LOG_ASSERT(abstractModel.isSymbolicModel(), "Expected symbolic abstract model.");
            
            return computeQualitativeResult(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>(), constraintStates.asSymbolicStateSet<DdType>(), targetStates.asSymbolicStateSet<DdType>());
        }
        
        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(storm::models::symbolic::Model<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates) {
            STORM_LOG_THROW(abstractModel.isOfType(storm::models::ModelType::Dtmc) || abstractModel.isOfType(storm::models::ModelType::Mdp) || abstractModel.isOfType(storm::models::ModelType::S2pg), storm::exceptions::NotSupportedException, "Expected discrete-time abstract model.");
            
            if (abstractModel.isOfType(storm::models::ModelType::Dtmc)) {
                return computeQualitativeResult(*abstractModel.template as<storm::models::symbolic::Dtmc<DdType, ValueType>>(), constraintStates, targetStates);
            } else if (abstractModel.isOfType(storm::models::ModelType::Mdp)) {
                return computeQualitativeResult(*abstractModel.template as<storm::models::symbolic::Mdp<DdType, ValueType>>(), constraintStates, targetStates);
            } else {
                return computeQualitativeResult(*abstractModel.template as<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>(), constraintStates, targetStates);
            }
        }
        
        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(storm::models::symbolic::Dtmc<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates) {
            STORM_LOG_DEBUG("Computing qualitative solution for DTMC.");
            std::unique_ptr<storm::abstraction::QualitativeMdpResultMinMax<DdType>> result = std::make_unique<storm::abstraction::QualitativeMdpResultMinMax<DdType>>();

            auto start = std::chrono::high_resolution_clock::now();
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().notZero();
            if (isRewardFormula) {
                auto prob1 = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob1Min = result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(prob1);
            } else {
                auto prob01 = storm::utility::graph::performProb01(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                result->prob0Min = result->prob0Max = storm::abstraction::QualitativeMdpResult<DdType>(prob01.first);
                result->prob1Min = result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(prob01.second);
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");
            
            return result;
        }
        
        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(storm::models::symbolic::Mdp<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates) {
            STORM_LOG_DEBUG("Computing qualitative solution for MDP.");
            std::unique_ptr<storm::abstraction::QualitativeMdpResultMinMax<DdType>> result = std::make_unique<storm::abstraction::QualitativeMdpResultMinMax<DdType>>();
            
            auto start = std::chrono::high_resolution_clock::now();
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().notZero();
            if (this->getReuseQualitativeResults()) {
                if (isRewardFormula) {
                    auto states = storm::utility::graph::performProb1E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates() : storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                    result->prob1Min = storm::abstraction::QualitativeMdpResult<DdType>(states);
                    states = storm::utility::graph::performProb1A(abstractModel, transitionMatrixBdd, targetStates.getStates(), states);
                    result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(states);
                } else {
                    auto states = storm::utility::graph::performProb0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                    result->prob0Max = storm::abstraction::QualitativeMdpResult<DdType>(states);
                    states = storm::utility::graph::performProb1E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates() : storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                    result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(states);

                    states = storm::utility::graph::performProb1A(abstractModel, transitionMatrixBdd, lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates() : targetStates.getStates(), storm::utility::graph::performProbGreater0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                    result->prob1Min = storm::abstraction::QualitativeMdpResult<DdType>(states);

                    states = storm::utility::graph::performProb0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                    result->prob0Min = storm::abstraction::QualitativeMdpResult<DdType>(states);
                }
            } else {
                if (isRewardFormula) {
                    auto prob1 = storm::utility::graph::performProb1E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), storm::utility::graph::performProbGreater0E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                    result->prob1Min = storm::abstraction::QualitativeMdpResult<DdType>(prob1);
                    prob1 = storm::utility::graph::performProb1A(abstractModel, transitionMatrixBdd, targetStates.getStates(), storm::utility::graph::performProbGreater0A(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates()));
                    result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(prob1);
                } else {
                    auto prob01 = storm::utility::graph::performProb01Min(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                    result->prob0Min = storm::abstraction::QualitativeMdpResult<DdType>(prob01.first);
                    result->prob1Min = storm::abstraction::QualitativeMdpResult<DdType>(prob01.second);
                    prob01 = storm::utility::graph::performProb01Max(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates());
                    result->prob0Max = storm::abstraction::QualitativeMdpResult<DdType>(prob01.first);
                    result->prob1Max = storm::abstraction::QualitativeMdpResult<DdType>(prob01.second);
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            if (isRewardFormula) {
                STORM_LOG_TRACE("Min: " << result->getProb1Min().getStates().getNonZeroCount() << " states with probability 1.");
                STORM_LOG_TRACE("Max: " << result->getProb1Max().getStates().getNonZeroCount() << " states with probability 1.");
            } else {
                STORM_LOG_TRACE("Min: " << result->getProb0Min().getStates().getNonZeroCount() << " states with probability 0, " << result->getProb1Min().getStates().getNonZeroCount() << " states with probability 1.");
                STORM_LOG_TRACE("Max: " << result->getProb0Max().getStates().getNonZeroCount() << " states with probability 0, " << result->getProb1Max().getStates().getNonZeroCount() << " states with probability 1.");
            }
            STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");
            
            return result;
        }

        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeResultMinMax> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResult(storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates) {
            STORM_LOG_DEBUG("Computing qualitative solution for S2PG.");
            std::unique_ptr<storm::abstraction::QualitativeGameResultMinMax<DdType>> result;

            // Obtain the player optimization directions.
            uint64_t abstractionPlayer = this->getAbstractionPlayer();

            // Obtain direction for player 1 (model nondeterminism).
            storm::OptimizationDirection modelNondeterminismDirection = this->getCheckTask().getOptimizationDirection();
            
            // Convert the transition matrix to a BDD to use it in the qualitative algorithms.
            storm::dd::Bdd<DdType> transitionMatrixBdd = abstractModel.getTransitionMatrix().toBdd();

            // Remembers whether we need to synthesize schedulers.
            bool requiresSchedulers = this->requiresSchedulerSynthesis();
            
            auto start = std::chrono::high_resolution_clock::now();
            if (this->getReuseQualitativeResults()) {
                result = computeQualitativeResultReuse(abstractModel, transitionMatrixBdd, constraintStates, targetStates, abstractionPlayer, modelNondeterminismDirection, requiresSchedulers);
            } else {
                result = std::make_unique<storm::abstraction::QualitativeGameResultMinMax<DdType>>();
                
                result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, abstractionPlayer == 2 ?  storm::OptimizationDirection::Minimize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
                result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, abstractionPlayer == 2 ?  storm::OptimizationDirection::Minimize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
                result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, abstractionPlayer == 2 ?  storm::OptimizationDirection::Maximize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
                result->prob1Max = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, abstractionPlayer == 2 ?  storm::OptimizationDirection::Maximize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
            }
            
            STORM_LOG_TRACE("Qualitative precomputation completed.");
            if (abstractionPlayer == 1) {
                STORM_LOG_TRACE("[" << storm::OptimizationDirection::Minimize << ", " << modelNondeterminismDirection << "]: " << result->prob0Min.player1States.getNonZeroCount() << " 'no', " << result->prob1Min.player1States.getNonZeroCount() << " 'yes'.");
                STORM_LOG_TRACE("[" << storm::OptimizationDirection::Maximize << ", " << modelNondeterminismDirection << "]: " << result->prob0Max.player1States.getNonZeroCount() << " 'no', " << result->prob1Max.player1States.getNonZeroCount() << " 'yes'.");
            } else {
                STORM_LOG_TRACE("[" << modelNondeterminismDirection << ", " << storm::OptimizationDirection::Minimize << "]: " << result->prob0Min.player1States.getNonZeroCount() << " 'no', " << result->prob1Min.player1States.getNonZeroCount() << " 'yes'.");
                STORM_LOG_TRACE("[" << modelNondeterminismDirection << ", " << storm::OptimizationDirection::Maximize << "]: " << result->prob0Max.player1States.getNonZeroCount() << " 'no', " << result->prob1Max.player1States.getNonZeroCount() << " 'yes'.");
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            
            auto timeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            STORM_LOG_DEBUG("Computed qualitative solution in " << timeInMilliseconds << "ms.");
            
            return result;
        }
        
        template<typename ModelType>
        std::unique_ptr<storm::abstraction::QualitativeGameResultMinMax<AbstractAbstractionRefinementModelChecker<ModelType>::DdType>> AbstractAbstractionRefinementModelChecker<ModelType>::computeQualitativeResultReuse(storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType> const& abstractModel, storm::dd::Bdd<DdType> const& transitionMatrixBdd, storm::abstraction::SymbolicStateSet<DdType> const& constraintStates, storm::abstraction::SymbolicStateSet<DdType> const& targetStates, uint64_t abstractionPlayer, storm::OptimizationDirection const& modelNondeterminismDirection, bool requiresSchedulers) {
            std::unique_ptr<storm::abstraction::QualitativeGameResultMinMax<DdType>> result = std::make_unique<storm::abstraction::QualitativeGameResultMinMax<DdType>>();
            
            // Depending on the model nondeterminism direction, we choose a different order of operations.
            if (modelNondeterminismDirection == storm::OptimizationDirection::Minimize) {
                // (1) min/min: compute prob0 using the game functions
                result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, requiresSchedulers, requiresSchedulers);
                
                // (2) min/min: compute prob1 using the MDP functions
                storm::dd::Bdd<DdType> candidates = abstractModel.getReachableStates() && !result->getProb0Min().getStates();
                storm::dd::Bdd<DdType> prob1MinMinMdp = storm::utility::graph::performProb1A(abstractModel, transitionMatrixBdd, lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Min().getStates() : targetStates.getStates(), candidates);
                
                // (3) min/min: compute prob1 using the game functions
                result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), storm::OptimizationDirection::Minimize, storm::OptimizationDirection::Minimize, requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MinMinMdp));
                
                // (4) min/max, max/min: compute prob 0 using the game functions
                result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
                
                // (5) min/max, max/min: compute prob 1 using the game functions
                // We know that only previous prob1 states can now be prob 1 states again, because the upper bound
                // values can only decrease over iterations.
                result->prob1Max = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, abstractionPlayer == 2 ? storm::OptimizationDirection::Maximize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers, lastQualitativeResults ? lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Max().getStates() : boost::optional<storm::dd::Bdd<DdType>>());
            } else {
                // (1) max/max: compute prob0 using the game functions
                result->prob0Max = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, requiresSchedulers, requiresSchedulers);
                
                // (2) max/max: compute prob1 using the MDP functions, reuse prob1 states of last iteration to constrain the candidate states.
                storm::dd::Bdd<DdType> candidates = abstractModel.getReachableStates() && !result->getProb0Max().getStates();
                if (this->getReuseQualitativeResults()) {
                    candidates &= lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>().getProb1Max().getStates();
                }
                storm::dd::Bdd<DdType> prob1MaxMaxMdp = storm::utility::graph::performProb1E(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), candidates);
                
                // (3) max/max: compute prob1 using the game functions, reuse prob1 states from the MDP precomputation
                result->prob1Max = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), storm::OptimizationDirection::Maximize, storm::OptimizationDirection::Maximize, requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MaxMaxMdp));
                
                // (4) max/min, min/max: compute prob0 using the game functions
                result->prob0Min = storm::utility::graph::performProb0(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers);
                
                // (5) max/min:, max/min compute prob1 using the game functions, use prob1 from max/max as the candidate set
                result->prob1Min = storm::utility::graph::performProb1(abstractModel, transitionMatrixBdd, constraintStates.getStates(), targetStates.getStates(), abstractionPlayer == 1 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, abstractionPlayer == 2 ? storm::OptimizationDirection::Minimize : modelNondeterminismDirection, requiresSchedulers, requiresSchedulers, boost::make_optional(prob1MaxMaxMdp));
            }
            
            return result;
        }

        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQualitativeCheck(storm::models::Model<ValueType> const& abstractModel) {
            STORM_LOG_THROW(abstractModel.isSymbolicModel(), storm::exceptions::NotSupportedException, "Expected symbolic model.");
            
            return checkForResultAfterQualitativeCheck(*abstractModel.template as<storm::models::symbolic::Model<DdType, ValueType>>());
        }

        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::checkForResultAfterQualitativeCheck(storm::models::symbolic::Model<DdType, ValueType> const& abstractModel) {
            std::unique_ptr<CheckResult> result;
            
            auto const& symbolicQualitativeResultMinMax = lastQualitativeResults->asSymbolicQualitativeResultMinMax<DdType>();
            
            bool isRewardFormula = checkTask->getFormula().isEventuallyFormula() && checkTask->getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward;
            if (isRewardFormula) {
                // In the reachability reward case, we can give an answer if all initial states of the system are infinity
                // states in the min result.
                if ((abstractModel.getInitialStates() && !symbolicQualitativeResultMinMax.getProb1Min().getStates()) == abstractModel.getInitialStates()) {
                    result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), abstractModel.getInitialStates(), abstractModel.getInitialStates().ite(abstractModel.getManager().getConstant(storm::utility::infinity<ValueType>()), abstractModel.getManager().template getAddZero<ValueType>()));
                }
            } else {
                // In the reachability probability case, we can give the answer if all initial states are prob1 states
                // in the min result or if all initial states are prob0 in the max case.
                // Furthermore, we can give the answer if there are initial states with probability > 0 in the min case
                // and the probability bound was 0 or if there are initial states with probability < 1 in the max case
                // and the probability bound was 1.
                if ((abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Min().getStates()) == abstractModel.getInitialStates()) {
                    result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), abstractModel.getInitialStates(), abstractModel.getManager().template getAddOne<ValueType>());
                } else if ((abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Max().getStates()) == abstractModel.getInitialStates()) {
                    result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), abstractModel.getInitialStates(), abstractModel.getManager().template getAddZero<ValueType>());
                } else if (checkTask->isBoundSet() && checkTask->getBoundThreshold() == storm::utility::zero<ValueType>() && (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Min().getStates()) != abstractModel.getInitialStates()) {
                    result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), abstractModel.getInitialStates(), (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb0Min().getStates()).ite(abstractModel.getManager().template getConstant<ValueType>(0.5), abstractModel.getManager().template getAddZero<ValueType>()));
                } else if (checkTask->isBoundSet() && checkTask->getBoundThreshold() == storm::utility::one<ValueType>() && (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Max().getStates()) != abstractModel.getInitialStates()) {
                    result = std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(abstractModel.getReachableStates(), abstractModel.getInitialStates(), (abstractModel.getInitialStates() && symbolicQualitativeResultMinMax.getProb1Max().getStates()).ite(abstractModel.getManager().template getConstant<ValueType>(0.5), abstractModel.getManager().template getAddZero<ValueType>()) + symbolicQualitativeResultMinMax.getProb1Max().getStates().template toAdd<ValueType>());
                }
            }
            
            if (result) {
                STORM_LOG_TRACE("Check for result after qualitative check positive.");
            }
            
            return result;
        }
        
        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::tryToObtainResultFromBounds(storm::models::Model<ValueType> const& abstractModel, std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>>& bounds) {
            std::unique_ptr<CheckResult> result;
            
            if (bounds.first == nullptr || bounds.second == nullptr) {
                STORM_LOG_ASSERT(bounds.first || bounds.second, "Expected at least one bound.");
                
                if (bounds.first) {
                    return std::move(bounds.first);
                } else {
                    return std::move(bounds.second);
                }
            } else {
                if (boundsAreSufficientlyClose(bounds)) {
                    result = getAverageOfBounds(bounds);
                }
            }
            
            if (result) {
                abstractModel.printModelInformationToStream(std::cout);
            }
            
            return result;
        }
        
        template<typename ModelType>
        bool AbstractAbstractionRefinementModelChecker<ModelType>::boundsAreSufficientlyClose(std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> const& bounds) {
            STORM_LOG_ASSERT(bounds.first->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
            storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds = bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
            STORM_LOG_ASSERT(bounds.second->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
            storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds = bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
            
            return lowerBounds.getValueVector().equalModuloPrecision(upperBounds.getValueVector(), storm::settings::getModule<storm::settings::modules::AbstractionSettings>().getPrecision(), false);
        }

        template<typename ModelType>
        std::unique_ptr<CheckResult> AbstractAbstractionRefinementModelChecker<ModelType>::getAverageOfBounds(std::pair<std::unique_ptr<CheckResult>, std::unique_ptr<CheckResult>> const& bounds) {
            STORM_LOG_ASSERT(bounds.first->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
            storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& lowerBounds = bounds.first->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
            STORM_LOG_ASSERT(bounds.second->isSymbolicQuantitativeCheckResult(), "Expected symbolic quantitative check result.");
            storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType> const& upperBounds = bounds.second->asSymbolicQuantitativeCheckResult<DdType, ValueType>();
            
            return std::make_unique<storm::modelchecker::SymbolicQuantitativeCheckResult<DdType, ValueType>>(lowerBounds.getReachableStates(), lowerBounds.getStates(), (lowerBounds.getValueVector() + upperBounds.getValueVector()) / lowerBounds.getValueVector().getDdManager().getConstant(storm::utility::convertNumber<ValueType>(std::string("2.0"))));
        }

        template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
        template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
        template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
        template class AbstractAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;

    }
}
