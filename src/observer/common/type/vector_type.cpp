/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/type/vector_type.h"

#include <charconv>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>

#include "common/value.h"

int VectorType::compare(const Value &left, const Value &right) const
{
  if (right.attr_type() != AttrType::VECTORS || left.length() != right.length()) {
    return INT32_MAX;
  }
  for (int i = 0; i < left.length() / static_cast<int>(sizeof(float)); ++i) {
    if (left.vector_element(i) != right.vector_element(i)) {
      return INT32_MAX;
    }
  }
  return 0;
}

RC VectorType::set_value_from_str(Value &val, const string &data) const
{
  const char *pos = data.data();
  const char *end = pos + data.size();
  auto skip_space = [&] {
    while (pos != end && std::isspace(static_cast<unsigned char>(*pos))) {
      ++pos;
    }
  };

  skip_space();
  if (pos == end || *pos++ != '[') {
    return RC::INVALID_ARGUMENT;
  }
  std::vector<float> elements;
  while (true) {
    skip_space();
    if (pos != end && *pos == '+') {
      ++pos;
      if (pos == end || *pos == '-') {
        return RC::INVALID_ARGUMENT;
      }
    }
    float number;
    auto parsed = std::from_chars(pos, end, number);
    if (parsed.ec != std::errc() || !std::isfinite(number) || elements.size() >= MAX_CAPACITY) {
      return RC::INVALID_ARGUMENT;
    }
    elements.push_back(number);
    pos = parsed.ptr;
    skip_space();
    if (pos == end) {
      return RC::INVALID_ARGUMENT;
    }
    const char delimiter = *pos++;
    if (delimiter == ']') {
      break;
    }
    if (delimiter != ',') {
      return RC::INVALID_ARGUMENT;
    }
  }
  skip_space();
  if (pos != end) {
    return RC::INVALID_ARGUMENT;
  }
  val.set_vector(reinterpret_cast<const char *>(elements.data()), elements.size() * sizeof(float));
  return RC::SUCCESS;
}

RC VectorType::to_string(const Value &val, string &result) const
{
  std::ostringstream formatted;
  formatted.imbue(std::locale::classic());
  formatted << std::scientific << std::setprecision(5) << '[';
  for (int i = 0; i < val.length() / static_cast<int>(sizeof(float)); ++i) {
    if (i != 0) {
      formatted << ',';
    }
    formatted << val.vector_element(i);
  }
  formatted << ']';
  result = formatted.str();
  return RC::SUCCESS;
}
