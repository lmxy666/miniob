/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#pragma once

#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/stmt.h"

class Table;

/**
 * @brief 更新语句
 * @ingroup Statement
 */
class UpdateStmt : public Stmt
{
public:
  UpdateStmt(Table *table, const FieldMeta *field, Value value, unique_ptr<FilterStmt> filter)
      : table_(table), field_(field), value_(std::move(value)), filter_(std::move(filter))
  {}

  static RC create(Db *db, const UpdateSqlNode &update_sql, Stmt *&stmt);

  StmtType type() const override { return StmtType::UPDATE; }
  Table *table() const { return table_; }
  const FieldMeta *field() const { return field_; }
  const Value &value() const { return value_; }
  FilterStmt *filter_stmt() const { return filter_.get(); }

private:
  Table *table_;
  const FieldMeta *field_;
  Value value_;
  unique_ptr<FilterStmt> filter_;
};
