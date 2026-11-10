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

#include "sql/stmt/update_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

RC UpdateStmt::create(Db *db, const UpdateSqlNode &update, Stmt *&stmt)
{
  stmt = nullptr;
  if (db == nullptr) {
    return RC::INVALID_ARGUMENT;
  }
  Table *table = db->find_table(update.relation_name.c_str());
  if (table == nullptr) {
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  const FieldMeta *field = table->table_meta().field(update.attribute_name.c_str());
  if (field == nullptr || !field->visible()) {
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  Value value = update.value;
  if (value.attr_type() != field->type()) {
    RC rc = Value::cast_to(update.value, field->type(), value);
    if (OB_FAIL(rc)) {
      return rc;
    }
  }

  unordered_map<string, Table *> tables{{update.relation_name, table}};
  FilterStmt *filter = nullptr;
  RC rc = FilterStmt::create(db, table, &tables, update.conditions.data(), static_cast<int>(update.conditions.size()), filter);
  if (OB_FAIL(rc)) {
    return rc;
  }
  stmt = new UpdateStmt(table, field, std::move(value), unique_ptr<FilterStmt>(filter));
  return RC::SUCCESS;
}
