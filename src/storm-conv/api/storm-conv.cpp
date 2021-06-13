#include "storm-conv/api/storm-conv.h"
#include "storm-conv/converter/aiger/bddutils.h"

#include "storm/storage/expressions/Variable.h"
#include "storm/storage/prism/Program.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/Constant.h"
#include "storm/storage/jani/JaniLocationExpander.h"
#include "storm/storage/jani/JaniScopeChanger.h"
#include "storm/storage/jani/JSONExporter.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/SymbolicModelDescription.h"

#include "storm/api/storm.h"
#include "storm/api/properties.h"
#include "storm/io/file.h"
#include "storm/models/symbolic/StandardRewardModel.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"

extern "C" {
#include "aiger.h"
}

namespace storm {
    namespace api {
        
        void transformJani(storm::jani::Model& janiModel, std::vector<storm::jani::Property>& properties, storm::converter::JaniConversionOptions const& options) {
        
            if (options.replaceUnassignedVariablesWithConstants) {
                janiModel.replaceUnassignedVariablesWithConstants();
            }
            
            if (options.substituteConstants) {
                janiModel.substituteConstantsInPlace();
            }
            
            if (options.localVars) {
                STORM_LOG_WARN_COND(!options.globalVars, "Ignoring 'globalvars' option, since 'localvars' is also set.");
                storm::jani::JaniScopeChanger().makeVariablesLocal(janiModel, properties);
            } else if (options.globalVars) {
                storm::jani::JaniScopeChanger().makeVariablesGlobal(janiModel);
            }
            
            if (!options.locationVariables.empty()) {
                // Make variables local if necessary/possible
                for (auto const& pair : options.locationVariables) {
                    if (janiModel.hasGlobalVariable(pair.second)) {
                        auto var = janiModel.getGlobalVariable(pair.second).getExpressionVariable();
                        if (storm::jani::JaniScopeChanger().canMakeVariableLocal(var, janiModel, properties, janiModel.getAutomatonIndex(pair.first)).first) {
                            storm::jani::JaniScopeChanger().makeVariableLocal(var, janiModel, janiModel.getAutomatonIndex(pair.first));
                        } else {
                            STORM_LOG_ERROR("Can not transform variable " << pair.second << " into locations since it can not be made local to automaton " << pair.first << ".");
                        }
                    }
                }
                
                for (auto const& pair : options.locationVariables) {
                    storm::jani::JaniLocationExpander expander(janiModel);
                    expander.transform(pair.first, pair.second);
                    janiModel = expander.getResult();
                }
            }

            if (options.simplifyComposition) {
                janiModel.simplifyComposition();
            }
            
            if (options.flatten) {
                std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory;
                if (storm::settings::hasModule<storm::settings::modules::CoreSettings>()) {
                    smtSolverFactory = std::make_shared<storm::utility::solver::SmtSolverFactory>();
                } else {
                    smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
                }
                janiModel = janiModel.flattenComposition(smtSolverFactory);
            }

            if (!options.edgeAssignments) {
                janiModel.pushEdgeAssignmentsToDestinations();
            }
            
            auto uneliminatedFeatures = janiModel.restrictToFeatures(options.allowedModelFeatures);
            STORM_LOG_WARN_COND(uneliminatedFeatures.empty(), "The following model features could not be eliminated: " << uneliminatedFeatures.toString());
            
            if (options.modelName) {
                janiModel.setName(options.modelName.get());
            }
            
            if (options.addPropertyConstants) {
                for (auto& f : properties) {
                    for (auto const& constant : f.getUndefinedConstants()) {
                        if (!janiModel.hasConstant(constant.getName())) {
                            janiModel.addConstant(storm::jani::Constant(constant.getName(), constant));
                        }
                    }
                }
            }
            
        }
        
        void transformPrism(storm::prism::Program& prismProgram, std::vector<storm::jani::Property>& properties, bool simplify, bool flatten) {
            if (simplify) {
                prismProgram = prismProgram.simplify().simplify();
                properties = storm::api::substituteConstantsInProperties(properties, prismProgram.getConstantsFormulasSubstitution());
            }
            if (flatten) {
               prismProgram = prismProgram.flattenModules();
               if (simplify) {
                    // Let's simplify the flattened program again ... just to be sure ... twice ...
                    prismProgram = prismProgram.simplify().simplify();
               }
            }
        }

        std::shared_ptr<aiger> convertPrismToAiger(storm::prism::Program const& program, std::vector<storm::jani::Property> const & properties, storm::converter::PrismToAigerConverterOptions options) {
            // we start by replacing constants and formulas by their values
            storm::prism::Program replaced = program.substituteConstantsFormulas();
            // we recover BDD-style information from the prism program by
            // building its symbolic representation
            std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>> model = storm::builder::DdPrismModelBuilder<storm::dd::DdType::Sylvan, double>().build(replaced);
            // we can now start loading the aiger structure
            std::shared_ptr<aiger> aig(aiger_init(), [](auto ptr) {
                aiger_reset(ptr);
            });
            // STEP 1:
            // the first thing we need is to add input variables, these come
            // from the encoding of the nondeterminism, a.k.a. actions, and
            // the encoding of successors (because of the probabilistic
            // transition function)
            for (auto const& var : model->getNondeterminismVariables()) {
                auto const& ddMetaVar = model->getManagerAsSharedPointer()->getMetaVariable(var);
                for (auto const& i : ddMetaVar.getIndices()) {
                    std::string name = var.getName() + std::to_string(i);
                    aiger_add_input(aig.get(), var2lit(i), name.c_str());
                }
            }
            for (auto const& var : model->getColumnVariables()) {
                auto const& ddMetaVar = model->getManagerAsSharedPointer()->getMetaVariable(var);
                for (auto const& i : ddMetaVar.getIndices()) {
                    std::string name = var.getName() + std::to_string(i);
                    aiger_add_input(aig.get(), var2lit(i), name.c_str());
                }
            }
            // STEP 2:
            // we need to create latches per state-encoding variable
            unsigned maxvar = aig->maxvar;
            for (auto const& inVar : model->getRowVariables())
                for (unsigned const& i : model->getManagerAsSharedPointer()->getMetaVariable(inVar).getIndices())
                    maxvar = std::max(maxvar, i);
            // we will need Boolean logic for the transition relation
            storm::dd::Bdd<storm::dd::DdType::Sylvan> qualTrans = model->getQualitativeTransitionMatrix();
            for (auto const& varPair : model->getRowColumnMetaVariablePairs()) {
                auto inVar = varPair.first;
                auto ddInMetaVar = model->getManagerAsSharedPointer()->getMetaVariable(inVar);
                auto ddOutMetaVar = model->getManagerAsSharedPointer()->getMetaVariable(varPair.second);
                auto inIndices = ddInMetaVar.getIndices();
                auto idxIt = inIndices.begin();
                for (auto const& encVar : ddOutMetaVar.getDdVariables()) {
                    auto encVarFun = qualTrans && encVar;
                    unsigned lit = bdd2lit(encVarFun.getInternalBdd().getSylvanBdd(), aig.get(), maxvar);
                    unsigned idx = (unsigned)(*idxIt);
                    std::string name = inVar.getName() + std::to_string(idx);
                    aiger_add_latch(aig.get(), var2lit(idx), lit, name.c_str());
                    idxIt++;
                }
            }
            // STEP 3:
            // we add a "bad" output representing when the transition is
            // invalid
            aiger_add_output(aig.get(),
                             aiger_not(bdd2lit(qualTrans.getInternalBdd().getSylvanBdd(),
                                               aig.get(), maxvar)),
                             "invalid_transition");
            // STEP 4:
            // add labels as outputs as a function of state sets
            std::vector<std::string> labels = model->getLabels();
            for (auto const& label : labels) {
               storm::dd::Bdd<storm::dd::DdType::Sylvan> states4label = model->getStates(label);
               unsigned lit = bdd2lit(states4label.getInternalBdd().getSylvanBdd(), aig.get(), maxvar);
               aiger_add_output(aig.get(), lit, label.c_str());
            }
            // STEP 5:
            // add coin outputs
            std::set<double> nonzeroLeaves;
            storm::dd::Add<storm::dd::DdType::Sylvan> trans = model->getTransitionMatrix();
            for (auto addit = trans.begin(); addit != trans.end(); ++addit) {
                nonzeroLeaves.insert((*addit).second);
            }
            for (auto const& prob : nonzeroLeaves) {
                auto lteq = trans.lessOrEqual(prob);
                auto gteq = trans.greaterOrEqual(prob);
                auto eq = lteq && gteq;
                unsigned lit = bdd2lit(eq.getInternalBdd().getSylvanBdd(), aig.get(), maxvar);
                std::string name = "coin_" + std::to_string(prob);
                aiger_add_output(aig.get(), lit, name.c_str());
            }
            // STEP 6:
            // we set the initial values for all latches
            storm::dd::Bdd<storm::dd::DdType::Sylvan> initStates = model->getInitialStates();
            STORM_LOG_ASSERT(initStates.getNonZeroCount() == 1, "Expected a single initial state");
            for (auto const& inVar : model->getRowVariables()) {
                auto ddInMetaVar = model->getManagerAsSharedPointer()->getMetaVariable(inVar);
                auto inIndices = ddInMetaVar.getIndices();
                auto idxIt = inIndices.begin();
                for (auto const& encInVar : ddInMetaVar.getDdVariables()) {
                    unsigned idx = (unsigned)(*idxIt);
                    unsigned init = ((encInVar && initStates) == initStates) ? 1 : 0;
                    aiger_add_reset(aig.get(), var2lit(idx), init);
                    idxIt++;
                }
            }
            
            const char* check = aiger_check(aig.get());
            STORM_LOG_ASSERT(check == NULL, check);
            return aig;
        }
        
        std::pair<storm::jani::Model, std::vector<storm::jani::Property>> convertPrismToJani(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, storm::converter::PrismToJaniConverterOptions options) {
        
            // Perform conversion
            auto res = program.toJani(properties, options.allVariablesGlobal);
            if (res.second.empty()) {
                std::vector<storm::jani::Property> clonedProperties;
                for (auto const& p : properties) {
                    clonedProperties.push_back(p.clone());
                }
                res.second = std::move(clonedProperties);
            }
            
            // Postprocess Jani model based on the options
            transformJani(res.first, res.second, options.janiOptions);
            
            return res;
        }

        void exportJaniToFile(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::string const& filename, bool compact) {
            storm::jani::JsonExporter::toFile(model, properties, filename, true, compact);
        }
        
        void printJaniToStream(storm::jani::Model const& model, std::vector<storm::jani::Property> const& properties, std::ostream& ostream, bool compact) {
            storm::jani::JsonExporter::toStream(model, properties, ostream, true, compact);
        }
        
        void exportPrismToFile(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::string const& filename) {
            std::ofstream stream;
            storm::utility::openFile(filename, stream);
            stream << program << std::endl;
            storm::utility::closeFile(stream);
            
            if (!properties.empty()) {
                storm::utility::openFile(filename + ".props", stream);
                for (auto const& prop : properties) {
                    stream << prop.asPrismSyntax() << std::endl;
                    STORM_LOG_WARN_COND(!prop.containsUndefinedConstants(), "A property contains undefined constants. These might not be exported correctly.");
                }
                storm::utility::closeFile(stream);
            }
        }
        void printPrismToStream(storm::prism::Program const& program, std::vector<storm::jani::Property> const& properties, std::ostream& ostream) {
            ostream << program << std::endl;
            for (auto const& prop : properties) {
                STORM_LOG_WARN_COND(!prop.containsUndefinedConstants(), "A property contains undefined constants. These might not be exported correctly.");
                ostream << prop.asPrismSyntax() << std::endl;
            }
        }
        
    }
}
