#pragma once

#include "storm/adapters/IntervalForward.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/interval/Interval.h>
#pragma clang diagnostic pop

namespace carl {
template<typename Number>
inline size_t hash_value(carl::Interval<Number> const& i) {
    std::hash<carl::Interval<Number>> h;
    return h(i);
}
}  // namespace carl

namespace storm {

/*!
 * Type describing the interval bounds.
 */
using BoundType = carl::BoundType;

}  // namespace storm

namespace carl {
// carl::Interval<Number>::isNan() calls std::isnan() on the interval's bounds, which has no overload for exact
// rational types (GMP/CLN) -- a latent bug in carl that implicit instantiation never surfaces (nothing calls
// isNan() on a rational interval), but that a full explicit class instantiation (below) would, since that forces
// every member to be instantiated regardless of use. A rational number can never be NaN by construction, so this
// specialization isn't a workaround, it's the mathematically correct answer, and it must be declared here, before
// the explicit instantiation below, per the rule that an explicit specialization must precede any point of
// instantiation of the specialized entity.
template<>
inline bool Interval<storm::RationalNumber>::isNan() const {
    return false;
}
}  // namespace carl

// See the matching comment in RationalFunctionAdapter.h for the full explanation. In short: __attribute__((
// visibility("default"))) here pins these types' *declared* visibility (what the Itanium ABI's min-visibility rule
// for template instantiations reads) so that a hidden-visibility consumer (e.g. storm-parsers) instantiating, say,
// storm::Interval as part of one of its own public classes doesn't get that outer instantiation silently pulled
// down to hidden; extern template then avoids a local, redundant instantiation, linking instead against the
// definition in IntervalAdapter.cpp, part of storm (which is not compiled with hidden visibility).
namespace carl {
extern template class __attribute__((visibility("default"))) Interval<double>;
extern template class __attribute__((visibility("default"))) Interval<storm::RationalNumber>;
}  // namespace carl
