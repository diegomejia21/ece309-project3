// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

bool calc_evaluate(const std::vector<CalcToken>& /*tokens*/, double* /*out*/,
                   std::string* error) {
    *error = "calc_evaluate not implemented yet";   // TODO
    return false;
}
