/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <cstdio>

#include "common/type/date_type.h"
#include "common/value.h"

namespace {
bool parse_digits(const string &text, size_t &pos, size_t min_digits, size_t max_digits, int &value)
{
  const size_t start = pos;
  value = 0;
  while (pos < text.size() && pos - start < max_digits && text[pos] >= '0' && text[pos] <= '9') {
    value = value * 10 + text[pos++] - '0';
  }
  return pos - start >= min_digits;
}
}  // namespace

int DateType::compare(const Value &left, const Value &right) const
{
  if (right.attr_type() != AttrType::DATES) {
    return INT32_MAX;
  }
  const int32_t a = left.get_date();
  const int32_t b = right.get_date();
  return (a > b) - (a < b);
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  size_t pos = 0;
  int year, month, day;
  if (!parse_digits(data, pos, 4, 4, year) || pos == data.size() || data[pos++] != '-' ||
      !parse_digits(data, pos, 1, 2, month) || pos == data.size() || data[pos++] != '-' ||
      !parse_digits(data, pos, 1, 2, day) || pos != data.size()) {
    return RC::INVALID_ARGUMENT;
  }

  constexpr int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (year == 0 || month < 1 || month > 12 || day < 1) {
    return RC::INVALID_ARGUMENT;
  }
  const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (day > days_per_month[month - 1] + (month == 2 && leap)) {
    return RC::INVALID_ARGUMENT;
  }

  val.set_date(year * 10000 + month * 100 + day);
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  const int date = val.get_date();
  char formatted[32];
  std::snprintf(formatted, sizeof(formatted), "%04d-%02d-%02d", date / 10000, date / 100 % 100, date % 100);
  result = formatted;
  return RC::SUCCESS;
}
