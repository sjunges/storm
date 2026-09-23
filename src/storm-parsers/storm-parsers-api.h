#pragma once

// Macro to control the symbol visibility of the storm-parsers library.
//
// storm-parsers is compiled with hidden symbol visibility by default (see CXX_VISIBILITY_PRESET in CMakeLists.txt).
// Only declarations annotated with STORM_PARSERS_API are exported from the shared library and are part of the public
// ABI. Everything else (e.g. the boost::spirit grammar internals) remains internal to the library, which keeps the
// exported symbol table small and the binaries lean.
//
// In-tree consumers (storm, storm-dft, storm-gspn, ...) as well as third-party libraries such as stormpy link against
// these exported symbols.
#if defined(__GNUC__) || defined(__clang__)
#define STORM_PARSERS_API __attribute__((visibility("default")))
#else
#define STORM_PARSERS_API
#endif