#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "storm-config.h"

// Include the headers of CVC5 only if it is available.
#ifdef STORM_HAVE_CVC5
#include <cvc5/cvc5.h>
#endif

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace expressions {
class BaseExpression;
}

namespace adapters {

#ifdef STORM_HAVE_CVC5
class Cvc5ExpressionAdapter : public storm::expressions::ExpressionVisitor {
   public:
    /*!
     * Creates an expression adapter that can translate expressions to the format of CVC5.
     *
     * @param manager The manager that can be used to build expressions.
     * @param solver A reference to the CVC5 solver over which to build the expressions. The lifetime of the
     * solver needs to be guaranteed as long as the instance of this adapter is used.
     */
    Cvc5ExpressionAdapter(storm::expressions::ExpressionManager& manager, cvc5::Solver& solver);

    /*!
     * Translates the given expression to an equivalent expression for CVC5.
     *
     * @param expression The expression to translate.
     * @return An equivalent expression for CVC5.
     */
    cvc5::Term translateExpression(storm::expressions::Expression const& expression);

    /*!
     * Translates the given variable to an equivalent expression for CVC5.
     *
     * @param variable The variable to translate.
     * @return An equivalent expression for CVC5.
     */
    cvc5::Term translateExpression(storm::expressions::Variable const& variable);

    /*!
     * Translates the given CVC5 term back to a Storm expression.
     *
     * @param term The term to translate.
     * @return An equivalent Storm expression.
     */
    storm::expressions::Expression translateExpression(cvc5::Term const& term);

    /*!
     * Finds the counterpart to the given CVC5 constant.
     *
     * @param constant The constant for which to find the equivalent.
     * @return The equivalent counterpart.
     */
    storm::expressions::Variable const& getVariable(cvc5::Term const& constant);

    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data) override;

    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data) override;

   private:
    /*!
     * Creates a CVC5 constant for the provided variable.
     *
     * @param variable The variable for which to create a CVC5 counterpart.
     */
    cvc5::Term createVariable(storm::expressions::Variable const& variable);

    /*!
     * Creates a CVC5 term for the given (non-negative) rational number.
     */
    cvc5::Term createRational(storm::RationalNumber const& value);

    // The manager that can be used to build expressions.
    storm::expressions::ExpressionManager& manager;

    // The solver that is used to translate the expressions.
    cvc5::Solver& solver;

    // A mapping from variables to their CVC5 equivalent.
    std::unordered_map<storm::expressions::Variable, cvc5::Term> variableToExpressionMapping;

    // A mapping from CVC5 constants to the corresponding variables.
    std::unordered_map<cvc5::Term, storm::expressions::Variable> expressionToVariableMapping;

    // A cache of already translated constraints. Only valid during the translation of one expression.
    std::unordered_map<storm::expressions::BaseExpression const*, cvc5::Term> expressionCache;
};
#endif
}  // namespace adapters
}  // namespace storm
