#pragma once

#include "messages.pb.h"
#include "net/protocol_codec.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace bz3::net::detail {

std::vector<std::byte> ToBytes(const std::string& buffer);
std::vector<std::byte> SerializeOrEmpty(const karma::ServerMsg& message);
std::vector<std::byte> SerializeOrEmpty(const karma::ClientMsg& message);
const char* PayloadCaseName(karma::ServerMsg::PayloadCase payload_case);
Vec3 ToVec3(const karma::Vec3& wire);
void SetVec3(karma::Vec3* wire, const Vec3& value);

} // namespace bz3::net::detail
