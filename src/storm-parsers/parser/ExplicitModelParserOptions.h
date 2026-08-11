#pragma once

namespace storm {
namespace parser {

struct ExplicitModelParserOptions {
    bool dontFixDeadlocks = false;
    bool buildChoiceLabels = false;
};

}  // namespace parser
}  // namespace storm
