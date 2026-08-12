#pragma once

#include <cstdint>
#include <map>

#include "storm/adapters/RationalFunctionForward.h"

namespace storm::pars::scp {

enum class ScpStatus { Feasible, Infeasible, MaxIterationsReached, Timeout };

/*!
 * The outcome of an SCP search, at the same ConstantType precision the search itself ran at (double
 * for V1's CLI usage; storm::RationalNumber is also instantiated and gives a genuinely exact
 * result). `value` is not a display-only double cast of some internal exact value -- it *is* the
 * internal value: the accept/reject decision and the threshold comparison both run at ConstantType
 * precision throughout (see SparseScpInstantiationSearcher::runLoop), and `value` is that same
 * ConstantType value, never downcast. The only place double is unavoidable is the LP relaxation
 * itself, which is solved in double regardless of ConstantType -- but the LP's candidate is only
 * ever a *proposal*; every reported value here comes from an exact model check of the corresponding
 * instantiated model.
 *
 * `valuation` and `value` always describe the *best* point found, i.e. the one model-checked to be
 * closest to (or already satisfying) the threshold -- regardless of status. Only status == Feasible
 * means `valuation` was verified by actually model-checking the instantiated model (never derived
 * from the LP relaxation alone); the other statuses report a best-effort point for diagnostics, not
 * a verified answer.
 */
template<typename ConstantType>
struct ScpResult {
    ScpStatus status;
    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
    ConstantType value;
    /// The iteration at which (`valuation`, `value`) was established -- 0 if it's the initial
    /// point. Not a count of total iterations run: on Timeout/MaxIterationsReached, later
    /// iterations may have run without ever improving on this point, and are not reflected here.
    uint64_t iterations;
};

}  // namespace storm::pars::scp
