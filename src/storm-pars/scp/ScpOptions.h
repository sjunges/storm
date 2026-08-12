#pragma once

#include <cstdint>

namespace storm::pars::scp {

/*!
 * Configuration for the SCP feasibility search (SparseScpInstantiationSearcher).
 *
 * Trust region semantics match the reference implementation this port is checked against
 * (Cubuktepe et al., "Convex Optimization for Parameter Synthesis in MDPs"; prophesy's
 * QcqpSolver): the radius is a multiplicative factor >= 1 that bounds how far a variable may
 * move from its value at the current linearization point (v <= radius * v_hat, v >= v_hat /
 * radius). It shrinks towards -- but never reaches -- 1 as steps get rejected, and the natural
 * termination test is therefore "radius - 1 <= trustRegionMinExcess", not "radius <= 0".
 *
 * Consequence of the multiplicative form: a variable (a parameter or a state's linearized
 * probability) whose current linearization value is exactly 0 gets bounds [0, 0] -- i.e. it is
 * pinned and can never move again in a later iteration, regardless of trust region size. This
 * matches the reference implementation (same trust-region formula) and is not something this V1
 * works around; it mainly bites region/starting-point choices that leave a parameter fixed at 0.
 */
struct ScpOptions {
    /// Initial trust region radius (delta_0).
    double trustRegionInitial = 3.0;

    /// Multiplicative factor (gamma) by which the trust region grows after an accepted step and
    /// shrinks after a rejected one.
    double trustRegionFactor = 1.5;

    /// Upper cap on the trust region radius, so a long run of accepted steps can't make the
    /// linearization point jump arbitrarily far in one iteration.
    double trustRegionMax = 10.0;

    /// Termination threshold on (radius - 1): once the trust region has shrunk this close to 1,
    /// further iterations cannot move the linearization point in any useful way.
    double trustRegionMinExcess = 1e-4;

    /// Maximum number of SCP iterations before giving up.
    uint64_t maxIterations = 1000;

    /// If true, the LP objective minimizes the maximum per-state constraint violation
    /// (matches the reference implementation's default). If false, it minimizes the sum of
    /// violations instead.
    bool useMaxViolationObjective = true;
};

}  // namespace storm::pars::scp
