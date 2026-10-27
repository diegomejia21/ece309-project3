// src/tools/calculator_eval.cpp
// Author: Diego Mejia
//
// calc_evaluate: the two-stack algorithm (spec §4.6, Appendix C).
//   values: numbers
//   ops:    operators and '('
// Uses loops only, no recursion, so very long or deeply nested
// expressions cannot overflow the call stack.

#include "tools/calculator.h"

#include <stack>

namespace {

using Kind = CalcToken::Kind;

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

// * and / bind tighter than + and -.
int precedence(Kind k) {
    return (k == Kind::Times || k == Kind::Divide) ? 2 : 1;
}

// Pops one operator and two values, pushes `left op right`.
bool apply(std::stack<double>& values, std::stack<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }
    Kind op = ops.top();
    ops.pop();
    double right = values.top();   // the first value popped is the RIGHT operand
    values.pop();
    double left = values.top();
    values.pop();

    switch (op) {
        case Kind::Plus:   values.push(left + right); return true;
        case Kind::Minus:  values.push(left - right); return true;
        case Kind::Times:  values.push(left * right); return true;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            values.push(left / right);
            return true;
        default:
            *error = "malformed expression";
            return false;
    }
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::stack<double> values;
    std::stack<Kind>   ops;

    for (const CalcToken& t : tokens) {
        if (t.kind == Kind::Number) {
            values.push(t.value);
        } else if (t.kind == Kind::LParen) {
            ops.push(Kind::LParen);
        } else if (t.kind == Kind::RParen) {
            // Apply everything back to the matching '(' and then pop it.
            while (!ops.empty() && ops.top() != Kind::LParen) {
                if (!apply(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unbalanced parentheses";
                return false;
            }
            ops.pop();
        } else {
            // >= makes equal-precedence operators run left to right.
            while (!ops.empty() && is_operator(ops.top()) &&
                   precedence(ops.top()) >= precedence(t.kind)) {
                if (!apply(values, ops, error)) return false;
            }
            ops.push(t.kind);
        }
    }

    while (!ops.empty()) {
        if (ops.top() == Kind::LParen) {
            *error = "unbalanced parentheses";
            return false;
        }
        if (!apply(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.top();
    return true;
}
