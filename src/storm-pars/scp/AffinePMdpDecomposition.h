#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "storm-pars/storage/ParameterRegion.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm::pars::scp {

/*!
 * The affine decomposition of a single scalar quantity, so that everything downstream only ever
 * touches doubles:
 *
 *   value(v) = constant + sum_i coefficient_i * v_i
 *
 * where the sum ranges over the (few) parameters that actually occur in the quantity.
 */
struct UnpackedValue {
    double constant;
    std::vector<std::pair<storm::RationalFunctionVariable, double>> parameterCoefficients;
};

/*!
 * The affine part of a single transition (row, successor): UnpackedValue plus which successor
 * state it leads to.
 *
 *   P(row, successor)(v) = constant + sum_i coefficient_i * v_i
 */
struct UnpackedTransition {
    uint64_t successor;
    double constant;
    std::vector<std::pair<storm::RationalFunctionVariable, double>> parameterCoefficients;
};

/*!
 * One-time symbolic-to-numeric preprocessing of a pMDP's (or pDTMC's) transition matrix -- and,
 * for reward properties, one reward model's state-action reward vector -- as needed by the SCP
 * feasibility search.
 *
 * This validates that the model is an *affine* pMDP: every transition-probability
 * storm::RationalFunction (and, if a reward model is given, every state-action reward) must have
 * total degree <= 1 in the parameters (no p*q or p^2 terms). That precondition is exactly what
 * makes the only nonlinearity in the Bellman inequality the bilinear product
 * P(s,alpha,s') * p_{s'}, which SCP linearizes -- a model violating it would make that
 * linearization meaningless, so it is rejected outright rather than silently mishandled. The
 * reward itself does not need to be affine-free of nonlinearity concerns beyond this, since it
 * enters the Bellman inequality additively (no product with a p_{s'} term).
 *
 * Given that, every transition value P(row, successor) -- and, if applicable, every row's reward
 * -- is decomposed into a constant part and a list of (parameter, coefficient) pairs. This class
 * does not know about the property being checked beyond which reward model (if any) is relevant
 * (e.g. it does not know which states are Prob0/Prob1 or Rew0/RewInf) -- that is the concern of
 * the LP encoder, which combines this purely structural decomposition with property-specific
 * information.
 */
class AffinePMdpDecomposition {
   public:
    /*!
     * @param model A pMDP or pDTMC with storm::RationalFunction transition values.
     * @param rewardModel If given, this reward model's state-action reward vector is also
     * decomposed (see getReward()); its parameters are folded into getParameters() alongside the
     * transition matrix's. Must outlive this object. Leave as nullptr for a probability property,
     * which has no reward model to speak of.
     * @throws storm::exceptions::NotSupportedException if some transition (or, if rewardModel is
     * given, some state-action reward) is not affine in the parameters.
     */
    explicit AffinePMdpDecomposition(storm::models::sparse::Model<storm::RationalFunction> const& model,
                                     storm::models::sparse::StandardRewardModel<storm::RationalFunction> const* rewardModel = nullptr);

    /*!
     * The unpacked transitions of the given matrix row (a state for a DTMC, or a single
     * state-action choice for an MDP).
     */
    std::vector<UnpackedTransition> const& getRow(uint64_t row) const;

    /// The number of rows (matrix rows, i.e. choices) that were decomposed.
    uint64_t getNumberOfRows() const;

    /*!
     * The affine decomposition of the reward accrued by taking the given matrix row, from the
     * reward model passed to the constructor. Only valid to call if a reward model was actually
     * given there.
     */
    UnpackedValue const& getReward(uint64_t row) const;

    /// All parameters occurring anywhere in the model's transition matrix, or (if a reward model
    /// was given to the constructor) that reward model's state-action reward vector.
    std::set<storm::RationalFunctionVariable> const& getParameters() const;

    /*!
     * Checks whether the given region is graph-preserving for this model: every transition either
     * stays constant throughout the region (touching 0 or 1 is then fine -- that's a structural
     * fact of the model, e.g. a self-loop with probability 1), or stays strictly within (0,1)
     * throughout.
     *
     * This is sound and complete, not a heuristic proxy: since every transition is affine (this
     * class's own validated precondition), its extrema over a box region are always attained at
     * the region's vertices, so checking those 2^|getParameters()| points is both necessary and
     * sufficient -- no need for interval arithmetic, sampling, or an SMT solve over the general
     * polynomial case.
     *
     * @param region The region to check; must cover every parameter in getParameters() (not
     * validated here -- callers are expected to have already checked this, e.g. because they
     * needed it for a clear error message of their own).
     * @param maxVertices Enumerating all vertices is only tractable for a modest parameter count;
     * if the region has more parameters than this many vertices would require, the check is
     * skipped (returning std::nullopt) rather than silently taking an intractably long time.
     * @return true/false if the check ran to completion, std::nullopt if it was skipped because
     * the region has too many parameters to enumerate exhaustively.
     */
    std::optional<bool> isGraphPreserving(storm::storage::ParameterRegion<storm::RationalFunction> const& region,
                                          uint64_t maxVertices = uint64_t{1} << 20) const;

   private:
    /*!
     * Decomposes a single affine RationalFunction into its constant part and (parameter,
     * coefficient) pairs, and folds its occurring parameters into `parameters`. Shared between the
     * transition-matrix loop and the (optional) reward-vector loop in the constructor.
     *
     * @param errorContext Human-readable description of what `value` is (e.g. "the transition from
     * state 3 (row 5) to state 7"), used only to build a precise error message on violation.
     * @throws storm::exceptions::NotSupportedException if `value` is not affine in the parameters.
     */
    UnpackedValue decomposeAffine(storm::RationalFunction const& value, std::string const& errorContext);

    std::vector<std::vector<UnpackedTransition>> rows;
    std::vector<UnpackedValue> rewardRows;
    std::set<storm::RationalFunctionVariable> parameters;
};

}  // namespace storm::pars::scp
