#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"

// exprtk should be case sensitive in our case.
#define exprtk_disable_caseinsensitivity

// exprtk is an internal implementation detail of storm's expression evaluation: its templates are instantiated in
// storm's translation units but are never part of storm's public API and never consumed by external libraries. Compile
// it with hidden visibility so that none of its symbols leak out of libstorm.
#pragma GCC visibility push(hidden)
#include "exprtk.hpp"
#pragma GCC visibility pop

#pragma clang diagnostic pop
