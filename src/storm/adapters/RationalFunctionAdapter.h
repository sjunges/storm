#pragma once

#include "storm/adapters/RationalFunctionForward.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"  // clash for likely() macro between Ginac and Sylvan
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#pragma GCC diagnostic ignored "-Wpessimizing-move"

#include <carl/core/FactorizedPolynomial.h>
#include <carl/core/MultivariatePolynomial.h>
#include <carl/core/RationalFunction.h>
#include <carl/core/Relation.h>
#include <carl/core/VariablePool.h>

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

// None of these carl/polynomial class templates carry an explicit visibility attribute of their own. Per the
// Itanium ABI, a template instantiation's effective visibility is the *minimum* of the template's own visibility
// and its template arguments' visibility -- so a consumer built with hidden symbol visibility (e.g. storm-parsers)
// that instantiates, say, storm::RationalFunction as part of one of its own (correctly default-visibility-attributed)
// public classes would still see that outer instantiation pulled down to hidden, because these carl types are
// computed as ambient-hidden within that consumer's translation units. The fix has two parts, both required:
//  1. An explicit __attribute__((visibility("default"))) redeclaration here, seen before any other translation unit
//     uses these types, so their *declared* visibility -- the value the min-visibility rule reads -- is pinned to
//     default regardless of any consumer's ambient -fvisibility flag. (`extern template` alone does not do this: it
//     only suppresses local instantiation, it does not change what visibility a type is considered to declare.)
//  2. A matching non-extern explicit instantiation, so consumers don't instantiate their own copy at all and instead
//     link against the single definition below, provided by RationalFunctionAdapter.cpp as part of storm (which is
//     not compiled with hidden visibility).
// This block must come immediately after the raw carl includes above and before anything else in this header (the
// hash_value templates below are fine, they're generic and uninstantiated until called -- but the RawPolynomialCache
// typedef that used to sit here is not: forming it is enough to instantiate Cache<PolynomialFactorizationPair<
// RawPolynomial>>, and GCC rejects an attribute added after a type is already instantiated in the same translation
// unit). The same rule applies transitively to every other translation unit that includes this header, including
// ones storm doesn't control the internals of (e.g. resources/3rdparty/sylvan/src/storm_wrapper.cpp, which is
// storm's own file but built as part of Sylvan's fetched CMake project) -- so this must stay the first thing in the
// file that touches these types, not just earlier than the specific declarations that used to precede it here.
namespace carl {
extern template class __attribute__((visibility("default"))) MultivariatePolynomial<storm::RationalFunctionCoefficient>;
extern template class __attribute__((visibility("default"))) FactorizedPolynomial<storm::RawPolynomial>;
extern template class __attribute__((visibility("default"))) Cache<carl::PolynomialFactorizationPair<storm::RawPolynomial>>;
extern template class __attribute__((visibility("default"))) RationalFunction<storm::Polynomial, true>;
}  // namespace carl

namespace carl {
// Define hash values for all polynomials and rational function.
// Needed for boost::hash_combine() and other functions
template<typename C, typename O, typename P>
inline size_t hash_value(carl::MultivariatePolynomial<C, O, P> const& p) {
    std::hash<carl::MultivariatePolynomial<C, O, P>> h;
    return h(p);
}

template<typename Pol>
inline size_t hash_value(carl::FactorizedPolynomial<Pol> const& p) {
    std::hash<carl::FactorizedPolynomial<Pol>> h;
    return h(p);
}

template<typename Pol, bool AutoSimplify>
inline size_t hash_value(carl::RationalFunction<Pol, AutoSimplify> const& f) {
    std::hash<carl::RationalFunction<Pol, AutoSimplify>> h;
    return h(f);
}
}  // namespace carl

namespace storm {
typedef carl::Cache<carl::PolynomialFactorizationPair<RawPolynomial>> RawPolynomialCache;
typedef carl::Relation CompareRelation;

RationalFunctionVariable createRFVariable(std::string const& name);

/*!
 * Retrieves the (already existing) rational-function variable with the given name.
 *
 * @param name Variable name.
 * @return Variable with the given name or an invalid variable, if no such variable exists.
 */
RationalFunctionVariable findRFVariable(std::string const& name);

/*!
 * Clears the global pool of rational-function variables.
 * This is mainly used to reset the global state in-between tests.
 */
void clearRFVariablePool();

}  // namespace storm
