#pragma once

#include "storm-conv/converter/options/PrismToAigerConverterOptions.h"
#include "storm-conv/converter/options/PrismToJaniConverterOptions.h"
#include "storm-conv/converter/options/JaniConversionOptions.h"

extern "C" {
#include "aiger.h"
}

namespace storm {
    
    namespace prism {
        class Program;
    }
    namespace jani {
        class Model;
        class Property;
    }

    namespace api {
        
        void transformJani(storm::jani::Model& janiModel, std::vector<storm::jani::Property>& properties, storm::converter::JaniConversionOptions const& options);
        
        void transformPrism(storm::prism::Program& prismProgram, std::vector<storm::jani::Property>& properties, bool simplify = false, bool flatten = false);

        std::pair<storm::jani::Model, std::vector<storm::jani::Property>> convertPrismToJani(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties = std::vector<storm::jani::Property>(), storm::converter::PrismToJaniConverterOptions options = storm::converter::PrismToJaniConverterOptions());

        std::shared_ptr<aiger> convertPrismToAiger(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties = std::vector<storm::jani::Property>(), storm::converter::PrismToAigerConverterOptions options = storm::converter::PrismToAigerConverterOptions());
        
        void exportJaniToFile(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::string const& filename, bool compact = false);
        void printJaniToStream(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::ostream& ostream, bool compact = false);
        void exportPrismToFile(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::string const& filename);
        void printPrismToStream(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::ostream& ostream);

        
    }
}
