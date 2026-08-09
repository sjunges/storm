#pragma once

#include <cstdint>

#include "storm/storage/dd/cudd/CuddReorderingTechnique.h"

namespace storm {

class CuddDdManagerEnvironment {
   public:
    CuddDdManagerEnvironment();
    ~CuddDdManagerEnvironment();

    double getConstantPrecision() const;
    void setConstantPrecision(double value);

    /*!
     * Retrieves the maximal amount of memory (in megabytes) that CUDD can occupy.
     */
    uint_fast64_t getMaximalMemory() const;
    void setMaximalMemory(uint_fast64_t value);

    bool isReorderingEnabled() const;
    void setReorderingEnabled(bool value);

    storm::dd::CuddReorderingTechnique getReorderingTechnique() const;
    void setReorderingTechnique(storm::dd::CuddReorderingTechnique value);

   private:
    double constantPrecision;
    uint_fast64_t maximalMemory;
    bool reorderingEnabled;
    storm::dd::CuddReorderingTechnique reorderingTechnique;
};
}  // namespace storm
