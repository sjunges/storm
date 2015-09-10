#ifndef STORM_STORAGE_PRISM_MENU_GAMES_VARIABLEPARTITION_H_
#define STORM_STORAGE_PRISM_MENU_GAMES_VARIABLEPARTITION_H_

#include <unordered_map>
#include <set>
#include <vector>

#include "src/storage/expressions/Variable.h"
#include "src/storage/expressions/Expression.h"

namespace storm {
    namespace prism {
        namespace menu_games {
            
            class VariablePartition{
            public:
                /*!
                 * Constructs a new variable partition.
                 *
                 * @param relevantVariables The variables of this partition.
                 * @param expressions The (initial) expressions that define the partition.
                 */
                VariablePartition(std::set<storm::expressions::Variable> const& relevantVariables, std::vector<storm::expressions::Expression> const& expressions = std::vector<storm::expressions::Expression>());
                
                /*!
                 * Adds the expression and therefore indirectly may cause blocks of variables to be merged.
                 *
                 * @param expression The expression to add.
                 * @return True iff the partition changed.
                 */
                bool addExpression(storm::expressions::Expression const& expression);
                
                /*!
                 * Retrieves whether the two given variables are in the same block of the partition.
                 *
                 * @param firstVariable The first variable.
                 * @param secondVariable The second variable.
                 * @return True iff the two variables are in the same block.
                 */
                bool areRelated(storm::expressions::Variable const& firstVariable, storm::expressions::Variable const& secondVariable);
                
                /*!
                 * Places the given variables in the same block of the partition and performs the implied merges.
                 *
                 * @param firstVariable The first variable.
                 * @param secondVariable The second variable.
                 * @return True iff the partition changed.
                 */
                bool relate(storm::expressions::Variable const& firstVariable, storm::expressions::Variable const& secondVariable);
                
                /*!
                 * Places the given variables in the same block of the partition and performs the implied merges.
                 *
                 * @param variables The variables to relate.
                 * @return True iff the partition changed.
                 */
                bool relate(std::set<storm::expressions::Variable> const& variables);
                
                /*!
                 * Retrieves the block of related variables of the given variable.
                 *
                 * @param variable The variable whose block to retrieve.
                 * @return The block of the variable.
                 */
                std::set<storm::expressions::Variable> const& getBlockOfVariable(storm::expressions::Variable const& variable) const;

                /*!
                 * Retrieves the block index of the given variable.
                 *
                 * @param variable The variable for which to retrieve the block.
                 * @return The block index of the given variable.
                 */
                uint_fast64_t getBlockIndexOfVariable(storm::expressions::Variable const& variable) const;
                
                /*!
                 * Retrieves the number of blocks of the varible partition.
                 *
                 * @return The number of blocks in this partition.
                 */
                uint_fast64_t getNumberOfBlocks() const;
                
                /*!
                 * Retrieves the block with the given index.
                 *
                 * @param blockIndex The index of the block to retrieve.
                 * @return The block with the given index.
                 */
                std::set<storm::expressions::Variable> const& getVariableBlockWithIndex(uint_fast64_t blockIndex) const;
                
                /*!
                 * Retrieves the indices of the expressions related to the given variable.
                 *
                 * @param variable The variable for which to retrieve the related expressions.
                 * @return The related expressions.
                 */
                std::set<uint_fast64_t> const& getRelatedExpressions(storm::expressions::Variable const& variable) const;
                
                /*!
                 * Retrieves the indices of the expressions related to any of the given variables.
                 *
                 * @param variables The variables for which to retrieve the related expressions.
                 * @return The related expressions.
                 */
                std::set<uint_fast64_t> getRelatedExpressions(std::set<storm::expressions::Variable> const& variables) const;
                
            private:
                /*!
                 * Merges the blocks with the given indices.
                 *
                 * @param blocksToMerge The indices of the blocks to merge.
                 */
                void mergeBlocks(std::set<uint_fast64_t> const& blocksToMerge);
                
                // The set of variables relevant for this partition.
                std::set<storm::expressions::Variable> relevantVariables;
                
                // A mapping from variables to their blocks.
                std::unordered_map<storm::expressions::Variable, uint_fast64_t> variableToBlockMapping;
                
                // The variable blocks of the partition.
                std::vector<std::set<storm::expressions::Variable>> variableBlocks;
                                
                // The expression blocks of the partition.
                std::vector<std::set<uint_fast64_t>> expressionBlocks;
                
                // A mapping from variables to the indices of all expressions they appear in.
                std::unordered_map<storm::expressions::Variable, std::set<uint_fast64_t>> variableToExpressionsMapping;
                
                // The vector of all expressions.
                std::vector<storm::expressions::Expression> expressions;
            };
            
        }
    }
}

#endif /* STORM_STORAGE_PRISM_MENU_GAMES_VARIABLEPARTITION_H_ */