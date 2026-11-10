/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/hash_join_physical_operator.h"

RC HashJoinPhysicalOperator::open(Trx *trx)
{
  if (children_.size() != 2) {
    return RC::INTERNAL;
  }
  matches_ = nullptr;
  match_pos_ = 0;
  right_rows_.clear();

  PhysicalOperator &right = *children_[1];
  RC rc = right.open(trx);
  if (OB_FAIL(rc)) {
    return rc;
  }
  while (OB_SUCC(rc = right.next())) {
    Tuple *tuple = right.current_tuple();
    Value key;
    rc = tuple->find_cell(right_key_, key);
    if (OB_FAIL(rc)) {
      break;
    }
    auto &bucket = right_rows_[key.get_string()];
    bucket.emplace_back();
    rc = ValueListTuple::make(*tuple, bucket.back());
    if (OB_FAIL(rc)) {
      break;
    }
  }
  RC close_rc = right.close();
  if (rc != RC::RECORD_EOF) {
    return rc;
  }
  if (OB_FAIL(close_rc) || right_rows_.empty()) {
    return close_rc;
  }

  rc = children_[0]->open(trx);
  left_open_ = OB_SUCC(rc);
  return rc;
}

RC HashJoinPhysicalOperator::next()
{
  if (!left_open_) {
    return RC::RECORD_EOF;
  }
  while (matches_ == nullptr || match_pos_ >= matches_->size()) {
    PhysicalOperator &left = *children_[0];
    RC rc = left.next();
    if (OB_FAIL(rc)) {
      return rc;
    }
    Tuple *tuple = left.current_tuple();
    Value key;
    rc = tuple->find_cell(left_key_, key);
    if (OB_FAIL(rc)) {
      return rc;
    }
    auto match = right_rows_.find(key.get_string());
    if (match == right_rows_.end()) {
      continue;
    }
    joined_.set_left(tuple);
    matches_ = &match->second;
    match_pos_ = 0;
  }
  joined_.set_right(&(*matches_)[match_pos_++]);
  return RC::SUCCESS;
}

RC HashJoinPhysicalOperator::close()
{
  matches_ = nullptr;
  right_rows_.clear();
  if (left_open_) {
    left_open_ = false;
    return children_[0]->close();
  }
  return RC::SUCCESS;
}
