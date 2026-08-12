#pragma once

#include <map>
#include <memory>
#include <vector>

#include "storm-pars/scp/AffinePMdpDecomposition.h"
#include "storm-pars/scp/ScpLpEncoder.h"
#include "storm-pars/scp/ScpOptions.h"
#include "storm-pars/scp/ScpResult.h"
#include "storm-pars/utility/FeasibilitySynthesisTask.h"
#include "storm/environment/Environment.h"

namespace storm::pars::scp {

/*!
 * Finds a parameter valuation satisfying an unbounded-reachability feasibility task -- either a
 * reachability probability or a reachability reward -- on an affine pMDP (or, as the |A(s)| = 1
 * special case, a pDTMC) using Sequential Convex Programming (Cubuktepe et al.; see the
 * implementation plan for the full derivation and its cross-check against the paper and the
 * reference implementation; reward support is an extension beyond what the paper itself covers,
 * following the same Bellman-linearization idea with an added additive reward term).
 *
 * SparseModelType is storm::models::sparse::Mdp<storm::RationalFunction> or
 * storm::models::sparse::Dtmc<storm::RationalFunction>; ConstantType is the precision used for the
 * model-checking soundness gate *and* for the returned ScpResult<ConstantType>::value (double for
 * V1's CLI usage; storm::RationalNumber gives a genuinely exact result end-to-end) -- see
 * ScpResult's doc comment for exactly what this covers. The LP relaxation solved each iteration is
 * still double regardless of ConstantType (LP solvers are inherently double-precision here), but
 * its candidate is only ever a proposal: every accept/reject decision and every reported value
 * comes from an exact ConstantType-precision model check.
 *
 * V1 scope (see runFeasibilityWithSCP for the guards enforcing this): unbounded reachability
 * threshold properties only (probability or reward, `F` subformula, no step/time bounds). For Mdp
 * models, which extremal (Pmax/Pmin, or for a reward property Rmax/Rmin) is computed is derived
 * from the threshold direction under the *robust* reading of the property (AtLeast -> min, AtMost
 * -> max; see ThresholdDirection's doc comment) -- matching storm's own default for an unqualified
 * bounded formula on a nondeterministic model. If the formula explicitly specifies
 * Pmax=?/Pmin=?/Rmax=?/Rmin=?, it must agree with that derived direction; mismatched combinations
 * (e.g. "Pmax >= lambda") are rejected rather than silently mishandled. For a reward property, a
 * reward model name given on the formula (e.g. "R{"time"}>=lambda [F ...]") is used; otherwise the
 * model's unique reward model is used, matching storm's own reward-formula convention -- a
 * NotSupportedException is thrown if neither applies. The parameter region (explicit, or [0,1]^n
 * if none is given) is checked for graph-preservation via AffinePMdpDecomposition::isGraphPreserving
 * whenever that's tractable (see its doc comment) -- this check is purely about the transition
 * matrix and is the same for both probability and reward properties; a violation only ever produces
 * a warning, never a hard error -- it cannot break soundness (every Feasible result is still exactly
 * model-checked), only search effectiveness.
 *
 * Soundness: a result with status == Feasible is always verified by model-checking `valuation`
 * itself (not just the LP relaxation) -- this class never reports Feasible otherwise. A
 * non-Feasible status does not mean no witness exists; SCP is a local heuristic.
 */
template<typename SparseModelType, typename ConstantType>
class SparseScpInstantiationSearcher {
   public:
    explicit SparseScpInstantiationSearcher(SparseModelType const& model, ScpOptions options = ScpOptions());

    ScpResult<ConstantType> run(storm::Environment const& env, std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task);

   private:
    using ParametricType = typename SparseModelType::ValueType;
    using VariableType = typename storm::utility::parametric::VariableType<ParametricType>::type;
    using CoefficientType = typename storm::utility::parametric::CoefficientType<ParametricType>::type;
    using Valuation = storm::utility::parametric::Valuation<ParametricType>;

    /*!
     * The actual SCP iteration loop, shared between the Mdp and Dtmc branches of run() (which
     * differ only in which concrete instantiation-model-checker type they construct).
     */
    template<typename InstantiationChecker>
    ScpResult<ConstantType> runLoop(storm::Environment const& env, InstantiationChecker& instantiationChecker, ScpLpEncoder const& encoder,
                                    storm::logic::Bound const& bound, ThresholdDirection direction, uint64_t initialState,
                                    Valuation const& initialValuation) const;

    SparseModelType const& model;
    ScpOptions options;
};

}  // namespace storm::pars::scp
