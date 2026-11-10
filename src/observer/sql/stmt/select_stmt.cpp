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
// Created by Wangyunlai on 2022/6/6.
//

#include "sql/stmt/select_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/table/table.h"
#include "sql/parser/expression_binder.h"
#include "sql/expr/expression_iterator.h"

using namespace std;
using namespace common;

SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC SelectStmt::create(Db *db, SelectSqlNode &select_sql, Stmt *&stmt)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  BinderContext binder_context;

  // collect tables in `from` statement
  vector<Table *>                tables;
  unordered_map<string, Table *> table_map;
  for (size_t i = 0; i < select_sql.relations.size(); i++) {
    const char *table_name = select_sql.relations[i].c_str();
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    binder_context.add_table(table);
    tables.push_back(table);
    table_map.insert({table_name, table});
  }

  const bool explicit_join = !select_sql.on_clauses.empty();
  if (explicit_join && table_map.size() != tables.size()) {
    return RC::INVALID_ARGUMENT;
  }

  // collect query fields in `select` statement
  vector<unique_ptr<Expression>> bound_expressions;
  ExpressionBinder expression_binder(binder_context);
  
  for (unique_ptr<Expression> &expression : select_sql.expressions) {
    const string alias = expression->alias();
    const size_t first = bound_expressions.size();
    RC rc = expression_binder.bind_expression(expression, bound_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
    if (!alias.empty()) {
      if (bound_expressions.size() != first + 1) {
        return RC::INVALID_ARGUMENT;
      }
      bound_expressions.back()->set_alias(alias);
      bound_expressions.back()->set_name(alias);
    }
  }

  if (explicit_join) {
    for (auto &expression : bound_expressions) {
      if (expression->type() == ExprType::FIELD && expression->alias().empty()) {
        const Field &field = static_cast<const FieldExpr &>(*expression).field();
        expression->set_name(string(field.table_name()) + "." + field.field_name());
      }
    }
  }

  vector<unique_ptr<Expression>> group_by_expressions;
  for (unique_ptr<Expression> &expression : select_sql.group_by) {
    RC rc = expression_binder.bind_expression(expression, group_by_expressions);
    if (OB_FAIL(rc)) {
      LOG_INFO("bind expression failed. rc=%s", strrc(rc));
      return rc;
    }
  }

  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }

  // create filter statement in `where` statement
  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(db,
      default_table,
      &table_map,
      select_sql.conditions.data(),
      static_cast<int>(select_sql.conditions.size()),
      filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  unique_ptr<FilterStmt> filters(filter_stmt);
  for (const auto &clause : select_sql.on_clauses) {
    // An ON clause sees its own comma-separated join chain up to this join.
    unordered_map<string, Table *> scope;
    for (size_t i = clause.first_relation; i < clause.end_relation; ++i) {
      scope.emplace(select_sql.relations[i], tables[i]);
    }
    FilterStmt *bound = nullptr;
    rc = FilterStmt::create(db, nullptr, &scope, clause.conditions.data(), static_cast<int>(clause.conditions.size()), bound);
    if (OB_FAIL(rc)) {
      return rc;
    }
    unique_ptr<FilterStmt> on_filter(bound);
    filters->append(*on_filter);
  }

  // everything alright
  SelectStmt *select_stmt = new SelectStmt();

  select_stmt->tables_.swap(tables);
  select_stmt->query_expressions_.swap(bound_expressions);
  select_stmt->filter_stmt_ = filters.release();
  select_stmt->has_explicit_join_ = explicit_join;
  for (Table *table : select_stmt->tables_) {
    for (const auto &field : *table->table_meta().field_metas()) {
      select_stmt->uses_vectors_ |= field.type() == AttrType::VECTORS;
    }
  }
  auto detect_vectors = [&](auto &self, Expression &expression) -> RC {
    if (expression.type() == ExprType::VECTOR_FUNCTION || expression.value_type() == AttrType::VECTORS) {
      select_stmt->uses_vectors_ = true;
      return RC::SUCCESS;
    }
    return ExpressionIterator::iterate_child_expr(expression, [&](unique_ptr<Expression> &child) {
      return child ? self(self, *child) : RC::SUCCESS;
    });
  };
  for (auto &expression : select_stmt->query_expressions_) {
    if (select_stmt->uses_vectors_) {
      break;
    }
    detect_vectors(detect_vectors, *expression);
  }
  select_stmt->group_by_.swap(group_by_expressions);
  stmt                      = select_stmt;
  return RC::SUCCESS;
}
