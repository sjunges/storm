#pragma once

#include "storm/environment/SubEnvironment.h"

namespace storm {

// Forward declare sub-environments
class SolverEnvironment;
class ModelCheckerEnvironment;
class ExplorationEnvironment;
class DdEnvironment;

// Avoid implementing ugly copy constructors for environment by using an internal environment.
struct InternalEnvironment {
    SubEnvironment<SolverEnvironment> solverEnvironment;
    SubEnvironment<ModelCheckerEnvironment> modelcheckerEnvironment;
    SubEnvironment<ExplorationEnvironment> explorationEnvironment;
    SubEnvironment<DdEnvironment> ddEnvironment;
};

class Environment {
   public:
    Environment();
    virtual ~Environment();
    Environment(Environment const& other);
    Environment& operator=(Environment const& other);

    SolverEnvironment& solver();
    SolverEnvironment const& solver() const;
    ModelCheckerEnvironment& modelchecker();
    ModelCheckerEnvironment const& modelchecker() const;
    ExplorationEnvironment& exploration();
    ExplorationEnvironment const& exploration() const;
    DdEnvironment& dd();
    DdEnvironment const& dd() const;

   private:
    SubEnvironment<InternalEnvironment> internalEnv;
};
}  // namespace storm
