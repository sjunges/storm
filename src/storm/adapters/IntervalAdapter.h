#pragma once

#include "storm/adapters/IntervalForward.h"

// Not currently load-bearing: <carl/interval/Interval.h> below already transitively pulls in the complete GMP/CLN
// definitions (via its own numbers.h, which includes every number backend's adaptation layer unconditionally,
// since Interval<Number> needs arithmetic/comparison operators for whatever Number turns out to be). But that's an
// implementation detail of carl's header organization, not a documented guarantee, and this file's
// isNan()/explicit-instantiation code below genuinely needs storm::RationalNumber to be a complete type. Including
// this explicitly (as opposed to the lighter RationalNumberForward.h, which only forward-declares the type) makes
// that dependency self-documenting and independent of carl's current include structure.
#include "storm/adapters/RationalNumberAdapter.h"

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
// isNan() on a rational interval), but that a full explicit class instantiation (below and in IntervalAdapter.cpp)
// would, since that forces every member to be instantiated regardless of use. A rational number can never be NaN by
// construction, so this specialization isn't a workaround, it's the mathematically correct answer, and it must be
// declared here, before the explicit instantiation below, per the rule that an explicit specialization must precede
// any point of instantiation of the specialized entity.
template<>
inline bool Interval<storm::RationalNumber>::isNan() const {
    return false;
}
}  // namespace carl

// Unlike storm::RationalFunction's dependency chain (see RationalFunctionAdapter.h), carl::Interval itself does not
// need a visibility pin: it is fully defined inline in carl's header, so any external consumer (storm-dft,
// storm-pars, tests, ...) that uses it implicitly instantiates its own default-visibility copy of Interval's
// methods locally -- there is nothing to link against in storm-parsers, hidden or not. What does need to stay
// exported is the (out-of-line-defined) public API that hands Interval-typed values across the storm-parsers
// boundary in the first place -- ValueParser<storm::Interval>/<storm::RationalInterval>'s explicit class
// instantiations in ValueParser.cpp, and parseDirectEncodingModel<...>'s explicit instantiations in
// DirectEncodingParser.cpp -- and those already carry their own STORM_PARSERS_API directly, unaffected by whatever
// visibility Interval itself ends up with. (A GCC-incompatible attempt to pin Interval's visibility here anyway,
// on the mistaken assumption that it was required, briefly existed in this file's history; it wasn't needed --
// confirmed via nm on the built library and via storm's own DirectEncodingParserTest.{IntervalDtmcTest,
// RationalIntervalDtmcTest}, which exercise exactly this cross-DSO path.)
namespace carl {
extern template class Interval<double>;
extern template class Interval<storm::RationalNumber>;
}  // namespace carl
