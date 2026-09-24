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

// carl::Interval, like storm::RationalFunction's dependency chain (see RationalFunctionAdapter.h), carries no
// explicit visibility attribute of its own, and would in principle need the same pin. It deliberately does NOT get
// one here: neither an attributed extern template of a specific instantiation, nor an attributed primary-template
// redeclaration in IntervalForward.h, survives GCC's -Werror=attributes across the whole codebase -- confirmed by
// hitting three independent, unrelated translation units (resources/3rdparty/sylvan/src/storm_wrapper.cpp,
// src/storm/generator/CompressedState.cpp, src/storm/generator/TransientVariableInformation.cpp) where some other
// header reaches carl::Interval first via a path storm doesn't control (e.g. carl's own
// numbers/adaption_float/FLOAT_T.h independently forward-declares it). The attribute has to precede *every*
// reference to the type in *every* translation unit that includes this header, which is not achievable from
// storm's side against carl's own scattered, unattributed forward declarations. The real fix has to happen in carl
// itself: attach the attribute directly to carl::Interval's own declaration(s), the same way this PR's isNan() fix
// below already had to. Until that lands, storm::Interval/RationalInterval stay unpinned here: their
// ValueParser/DirectEncodingParser explicit instantiations remain unexported from storm-parsers under hidden
// visibility -- a pre-existing gap, not a regression introduced by this fix.
namespace carl {
extern template class Interval<double>;
extern template class Interval<storm::RationalNumber>;
}  // namespace carl
