/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/expr/vector_function_expr.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <strings.h>

VectorFunctionExpr::VectorFunctionExpr(Kind kind, Expression *first, Expression *second, Expression *metric)
    : kind_(kind)
{
  arguments_.emplace_back(first);
  if (kind == Kind::DISTANCE) {
    arguments_.emplace_back(second);
    arguments_.emplace_back(metric);
  }
}

AttrType VectorFunctionExpr::value_type() const
{
  switch (kind_) {
    case Kind::FROM_STRING: return AttrType::VECTORS;
    case Kind::TO_STRING: return AttrType::CHARS;
    case Kind::DISTANCE: return AttrType::FLOATS;
  }
  return AttrType::UNDEFINED;
}

unique_ptr<Expression> VectorFunctionExpr::copy() const
{
  auto result = make_unique<VectorFunctionExpr>(kind_, arguments_[0]->copy().release(),
      kind_ == Kind::DISTANCE ? arguments_[1]->copy().release() : nullptr,
      kind_ == Kind::DISTANCE ? arguments_[2]->copy().release() : nullptr);
  result->set_name(name());
  result->set_alias(alias());
  return result;
}

RC VectorFunctionExpr::validate_types() const
{
  if (kind_ == Kind::FROM_STRING) {
    return arguments_[0]->value_type() == AttrType::CHARS ? RC::SUCCESS : RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  if (arguments_[0]->value_type() != AttrType::VECTORS) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  if (kind_ == Kind::DISTANCE &&
      (arguments_[1]->value_type() != AttrType::VECTORS || arguments_[2]->value_type() != AttrType::CHARS)) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  return RC::SUCCESS;
}

RC VectorFunctionExpr::get_value(const Tuple &tuple, Value &value) const { return evaluate(&tuple, value); }
RC VectorFunctionExpr::try_get_value(Value &value) const { return evaluate(nullptr, value); }

RC VectorFunctionExpr::evaluate(const Tuple *tuple, Value &result) const
{
  Value values[3];
  for (size_t i = 0; i < arguments_.size(); ++i) {
    RC rc = tuple != nullptr ? arguments_[i]->get_value(*tuple, values[i]) : arguments_[i]->try_get_value(values[i]);
    if (OB_FAIL(rc)) {
      return rc;
    }
  }
  if (kind_ == Kind::FROM_STRING) {
    if (values[0].attr_type() != AttrType::CHARS) {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
    return DataType::type_instance(AttrType::VECTORS)->set_value_from_str(result, values[0].get_string());
  }
  if (values[0].attr_type() != AttrType::VECTORS) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  if (kind_ == Kind::TO_STRING) {
    result.set_string(values[0].get_string().c_str());
    return RC::SUCCESS;
  }
  if (values[1].attr_type() != AttrType::VECTORS || values[2].attr_type() != AttrType::CHARS ||
      values[0].length() != values[1].length()) {
    return RC::INVALID_ARGUMENT;
  }

  const string metric = values[2].get_string();
  const bool cosine = strcasecmp(metric.c_str(), "COSINE") == 0;
  const bool dot = strcasecmp(metric.c_str(), "DOT") == 0;
  const bool euclidean = strcasecmp(metric.c_str(), "EUCLIDEAN") == 0;
  if (!cosine && !dot && !euclidean) {
    return RC::INVALID_ARGUMENT;
  }
  double inner = 0, norm_left = 0, norm_right = 0, squared_distance = 0;
  for (int i = 0; i < values[0].length() / static_cast<int>(sizeof(float)); ++i) {
    const double left = values[0].vector_element(i);
    const double right = values[1].vector_element(i);
    if (euclidean) {
      const double difference = left - right;
      squared_distance += difference * difference;
    } else {
      inner += left * right;
      if (cosine) {
        norm_left += left * left;
        norm_right += right * right;
      }
    }
  }
  if (cosine && (norm_left == 0 || norm_right == 0)) {
    return RC::INVALID_ARGUMENT;
  }
  double distance = inner;
  if (euclidean) {
    distance = std::sqrt(squared_distance);
  } else if (cosine) {
    distance = 1 - std::clamp(inner / (std::sqrt(norm_left) * std::sqrt(norm_right)), -1.0, 1.0);
  }
  if (!std::isfinite(distance) || std::abs(distance) > std::numeric_limits<float>::max()) {
    return RC::INVALID_ARGUMENT;
  }
  result.set_float(static_cast<float>(distance));
  return RC::SUCCESS;
}
