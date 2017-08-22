#pragma once

#include <memory>
#include <vector>

#include "storm/storage/dd/DdType.h"
#include "storm/storage/bisimulation/BisimulationType.h"
#include "storm/storage/dd/bisimulation/SignatureMode.h"
#include "storm/storage/dd/bisimulation/PreservationInformation.h"

#include "storm/logic/Formula.h"

namespace storm {
    namespace models {
        template <typename ValueType>
        class Model;
        
        namespace symbolic {
            template <storm::dd::DdType DdType, typename ValueType>
            class Model;
        }
    }
    
    namespace dd {
        namespace bisimulation {
            template <storm::dd::DdType DdType, typename ValueType>
            class Partition;

            template <storm::dd::DdType DdType, typename ValueType>
            class PartitionRefiner;
        }
        
        template <storm::dd::DdType DdType, typename ValueType>
        class BisimulationDecomposition {
        public:
            BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model, storm::storage::BisimulationType const& bisimulationType);
            BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas, storm::storage::BisimulationType const& bisimulationType);
            BisimulationDecomposition(storm::models::symbolic::Model<DdType, ValueType> const& model, bisimulation::Partition<DdType, ValueType> const& initialPartition, bisimulation::PreservationInformation<DdType, ValueType> const& preservationInformation);
            
            ~BisimulationDecomposition();
            
            /*!
             * Computes the decomposition.
             */
            void compute(bisimulation::SignatureMode const& mode = bisimulation::SignatureMode::Eager);
            
            /*!
             * Retrieves the quotient model after the bisimulation decomposition was computed.
             */
            std::shared_ptr<storm::models::Model<ValueType>> getQuotient() const;
            
        private:
            void refineWrtRewardModels();
            
            // The model for which to compute the bisimulation decomposition.
            storm::models::symbolic::Model<DdType, ValueType> const& model;
            
            // The object capturing what is preserved.
            bisimulation::PreservationInformation<DdType, ValueType> preservationInformation;
            
            // The refiner to use.
            std::unique_ptr<bisimulation::PartitionRefiner<DdType, ValueType>> refiner;
        };
        
    }
}
