/* XMRig
 * Copyright (c) 2026      HashVault   <https://github.com/HashVault>, <root@hashvault.pro>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "base/tools/String.h"

#include <cstdint>
#include <vector>


namespace xmrig {


class DnsRecords;


class DnsWireFormat
{
public:
    enum Type : uint16_t {
        A       = 1,
        NS      = 2,
        CNAME   = 5,
        AAAA    = 28
    };

    static std::vector<uint8_t> buildQuery(const String &name, Type type);
    static bool parseResponse(const uint8_t *data, size_t len, std::vector<String> &results, Type type, const char *tag = nullptr);
    static bool parseAddressRecords(const uint8_t *data, size_t len, DnsRecords &records, int ai_family, const char *tag = nullptr);

private:
    static bool encodeName(const String &name, std::vector<uint8_t> &out);
    static bool decodeName(const uint8_t *data, size_t len, size_t &offset, String &name);
    static uint16_t readU16(const uint8_t *data);
    static uint32_t readU32(const uint8_t *data);
};


} // namespace xmrig
