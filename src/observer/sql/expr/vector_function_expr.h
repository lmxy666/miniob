/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "sql/expr/expression.h"

class VectorFunctionExpr final : public Expression
{
public:
  enum class Kind { FROM_STRING, TO_STRING, DISTANCE };

  VectorFunctionExpr(Kind kind, Expression *first, Expression *second = nullptr, Expression *metric = nullptr);
  ExprType type() const override { return ExprType::VECTOR_FUNCTION; }
  AttrType value_type() const override;
  unique_ptr<Expression> copy() const override;
  RC get_value(const Tuple &tuple, Value &value) const override;
  RC try_get_value(Value &value) const override;
  RC validate_types() const;
  vector<unique_ptr<Expression>> &arguments() { return arguments_; }

private:
  RC evaluate(const Tuple *tuple, Value &value) const;

  Kind kind_;
  vector<unique_ptr<Expression>> arguments_;
};
