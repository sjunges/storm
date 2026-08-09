#pragma once

namespace storm {
namespace dd {

// An enumeration of all available reordering techniques of CUDD.
enum class CuddReorderingTechnique {
    None,
    Random,
    RandomPivot,
    Sift,
    SiftConv,
    SymmetricSift,
    SymmetricSiftConv,
    GroupSift,
    GroupSiftConv,
    Win2,
    Win2Conv,
    Win3,
    Win3Conv,
    Win4,
    Win4Conv,
    Annealing,
    Genetic,
    Exact
};

}  // namespace dd
}  // namespace storm
