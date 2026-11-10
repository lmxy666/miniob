/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/update_physical_operator.h"

#include <cstring>

#include "storage/table/table.h"
#include "storage/trx/trx.h"

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.size() != 1 || trx == nullptr) {
    return RC::INTERNAL;
  }
  if (trx->type() != TrxKit::Type::VACUOUS) {
    return RC::UNSUPPORTED;
  }
  PhysicalOperator &child = *children_.front();
  RC rc = child.open(trx);
  if (OB_FAIL(rc)) {
    return rc;
  }

  // Copy all matching rows before changing any index used by the scan.
  vector<Record> records;
  while (OB_SUCC(rc = child.next())) {
    const auto *tuple = static_cast<const RowTuple *>(child.current_tuple());
    if (tuple == nullptr) {
      rc = RC::INTERNAL;
      break;
    }
    const Record &record = tuple->record();
    records.emplace_back();
    rc = records.back().copy_data(record.data(), record.len());
    if (OB_FAIL(rc)) {
      break;
    }
    records.back().set_rid(record.rid());
  }
  RC close_rc = child.close();
  if (rc != RC::RECORD_EOF) {
    return rc;
  }
  if (OB_FAIL(close_rc)) {
    return close_rc;
  }

  for (Record &old_record : records) {
    Record new_record;
    rc = table_->make_updated_record(old_record, *field_, value_, new_record);
    if (OB_FAIL(rc)) {
      return rc;
    }
    if (memcmp(old_record.data(), new_record.data(), old_record.len()) != 0) {
      rc = trx->update_record(table_, old_record, new_record);
      if (OB_FAIL(rc)) {
        return rc;
      }
    }
  }
  return RC::SUCCESS;
}
