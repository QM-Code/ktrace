#pragma once

#include <nlohmann/json.hpp>

#include <initializer_list>
#include <string>
#include <string_view>

namespace karma::common::serialization {

using Value = nlohmann::json;

inline Value Parse(std::string_view text) {
    return Value::parse(text);
}

inline Value Object() {
    return Value::object();
}

inline Value Array() {
    return Value::array();
}

template <typename T>
inline Value Array(std::initializer_list<T> values) {
    return Value(values);
}

inline std::string Dump(const Value& value, int indent = -1) {
    return value.dump(indent);
}

} // namespace karma::common::serialization
