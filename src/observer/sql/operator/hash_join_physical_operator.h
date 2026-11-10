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

#include "sql/operator/physical_operator.h"
#include <unordered_map>

/**
 * @brief Hash Join 算子
 * @ingroup PhysicalOperator
 */
class HashJoinPhysicalOperator final : public PhysicalOperator
{
public:
  HashJoinPhysicalOperator(TupleCellSpec left_key, TupleCellSpec right_key)
      : left_key_(std::move(left_key)), right_key_(std::move(right_key))
  {}

  PhysicalOperatorType type() const override { return PhysicalOperatorType::HASH_JOIN; }
  OpType get_op_type() const override { return OpType::INNERHASHJOIN; }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;
  Tuple *current_tuple() override { return &joined_; }

private:
  TupleCellSpec left_key_;
  TupleCellSpec right_key_;
  std::unordered_map<string, vector<ValueListTuple>> right_rows_;
  vector<ValueListTuple> *matches_ = nullptr;
  size_t match_pos_ = 0;
  bool left_open_ = false;
  JoinedTuple joined_;
};