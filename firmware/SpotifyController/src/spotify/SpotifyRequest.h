#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spotctl {

std::string urlEncode(const std::string &value);
std::string playContextBody(const std::string &context_uri,
                            uint32_t position);
std::string playUrisBody(const std::vector<std::string> &uris);

} // namespace spotctl

