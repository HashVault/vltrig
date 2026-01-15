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

#include <uv.h>
#include <cstring>
#include <cstdlib>

#include "base/net/dns/DnsWireFormat.h"
#include "base/net/dns/DnsRecords.h"


namespace xmrig {


struct DnsHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};


std::vector<uint8_t> DnsWireFormat::buildQuery(const String &name, Type type)
{
    std::vector<uint8_t> query;
    query.reserve(512);

    uint16_t id = static_cast<uint16_t>(rand());  // NOLINT
    query.push_back(id >> 8);
    query.push_back(id & 0xFF);

    query.push_back(0x01);
    query.push_back(0x00);

    query.push_back(0x00);
    query.push_back(0x01);

    query.push_back(0x00);
    query.push_back(0x00);
    query.push_back(0x00);
    query.push_back(0x00);
    query.push_back(0x00);
    query.push_back(0x00);

    if (!encodeName(name, query)) {
        return {};
    }

    query.push_back(static_cast<uint8_t>(type >> 8));
    query.push_back(static_cast<uint8_t>(type & 0xFF));

    query.push_back(0x00);
    query.push_back(0x01);

    return query;
}


bool DnsWireFormat::parseResponse(const uint8_t *data, size_t len, std::vector<String> &results, Type type, const char *)
{
    if (len < sizeof(DnsHeader)) {
        return false;
    }

    const uint16_t flags = readU16(data + 2);
    const uint16_t qdcount = readU16(data + 4);
    const uint16_t ancount = readU16(data + 6);

    if ((flags & 0x000F) != 0) {
        return false;
    }

    size_t offset = sizeof(DnsHeader);

    for (uint16_t i = 0; i < qdcount; ++i) {
        String qname;
        if (!decodeName(data, len, offset, qname)) {
            return false;
        }
        offset += 4;
        if (offset > len) {
            return false;
        }
    }

    for (uint16_t i = 0; i < ancount; ++i) {
        if (offset + 10 > len) {
            return false;
        }

        String aname;
        if (!decodeName(data, len, offset, aname)) {
            return false;
        }

        const uint16_t atype = readU16(data + offset);
        offset += 8;
        const uint16_t rdlength = readU16(data + offset);
        offset += 2;

        if (offset + rdlength > len) {
            return false;
        }

        if (atype == type) {
            if (type == NS || type == CNAME) {
                size_t rdataOffset = offset;
                String name;
                if (decodeName(data, len, rdataOffset, name)) {
                    results.push_back(std::move(name));
                }
            }
            else if (type == A && rdlength == 4) {
                char ipStr[INET_ADDRSTRLEN];
                uv_inet_ntop(AF_INET, data + offset, ipStr, sizeof(ipStr));
                results.push_back(String(static_cast<const char*>(ipStr)));
            }
            else if (type == AAAA && rdlength == 16) {
                char ipStr[INET6_ADDRSTRLEN];
                uv_inet_ntop(AF_INET6, data + offset, ipStr, sizeof(ipStr));
                results.push_back(String(static_cast<const char*>(ipStr)));
            }
        }

        offset += rdlength;
    }

    return !results.empty();
}


bool DnsWireFormat::parseAddressRecords(const uint8_t *data, size_t len, DnsRecords &records, int ai_family, const char *)
{
    if (len < sizeof(DnsHeader)) {
        return false;
    }

    const uint16_t flags = readU16(data + 2);
    const uint16_t qdcount = readU16(data + 4);
    const uint16_t ancount = readU16(data + 6);

    if ((flags & 0x000F) != 0) {
        return false;
    }

    size_t offset = sizeof(DnsHeader);

    for (uint16_t i = 0; i < qdcount; ++i) {
        String qname;
        if (!decodeName(data, len, offset, qname)) {
            return false;
        }
        offset += 4;
        if (offset > len) {
            return false;
        }
    }

    std::vector<addrinfo> infos;
    std::vector<sockaddr_storage> addrs;
    infos.reserve(ancount);
    addrs.reserve(ancount);

    for (uint16_t i = 0; i < ancount; ++i) {
        if (offset + 10 > len) {
            break;
        }

        String aname;
        if (!decodeName(data, len, offset, aname)) {
            break;
        }

        const uint16_t atype = readU16(data + offset);
        offset += 8;
        const uint16_t rdlength = readU16(data + offset);
        offset += 2;

        if (offset + rdlength > len) {
            break;
        }

        if (atype == A && rdlength == 4 && (ai_family == AF_UNSPEC || ai_family == AF_INET)) {
            sockaddr_storage ss{};
            auto *sin = reinterpret_cast<sockaddr_in *>(&ss);
            sin->sin_family = AF_INET;
            memcpy(&sin->sin_addr, data + offset, 4);
            addrs.push_back(ss);

            addrinfo ai{};
            ai.ai_family = AF_INET;
            ai.ai_socktype = SOCK_STREAM;
            ai.ai_protocol = IPPROTO_TCP;
            infos.push_back(ai);
        }
        else if (atype == AAAA && rdlength == 16 && (ai_family == AF_UNSPEC || ai_family == AF_INET6)) {
            sockaddr_storage ss{};
            auto *sin6 = reinterpret_cast<sockaddr_in6 *>(&ss);
            sin6->sin6_family = AF_INET6;
            memcpy(&sin6->sin6_addr, data + offset, 16);
            addrs.push_back(ss);

            addrinfo ai{};
            ai.ai_family = AF_INET6;
            ai.ai_socktype = SOCK_STREAM;
            ai.ai_protocol = IPPROTO_TCP;
            infos.push_back(ai);
        }

        offset += rdlength;
    }

    if (infos.empty()) {
        return false;
    }

    for (size_t i = 0; i < infos.size(); ++i) {
        infos[i].ai_addr = reinterpret_cast<sockaddr *>(&addrs[i]);
        infos[i].ai_addrlen = (infos[i].ai_family == AF_INET6) ? sizeof(sockaddr_in6) : sizeof(sockaddr_in);
        if (i + 1 < infos.size()) {
            infos[i].ai_next = &infos[i + 1];
        }
    }

    records = DnsRecords(&infos[0], ai_family);
    return !records.isEmpty();
}


bool DnsWireFormat::encodeName(const String &name, std::vector<uint8_t> &out)
{
    if (name.isEmpty()) {
        out.push_back(0);
        return true;
    }

    const auto parts = name.split('.');

    for (const auto &part : parts) {
        if (part.size() > 63) {
            return false;
        }
        out.push_back(static_cast<uint8_t>(part.size()));
        for (size_t i = 0; i < part.size(); ++i) {
            out.push_back(static_cast<uint8_t>(part.data()[i]));
        }
    }

    out.push_back(0);
    return true;
}


bool DnsWireFormat::decodeName(const uint8_t *data, size_t len, size_t &offset, String &name)
{
    char buf[256];
    size_t bufPos = 0;
    size_t pos = offset;
    bool jumped = false;
    int maxJumps = 10;

    while (pos < len && maxJumps > 0) {
        const uint8_t labelLen = data[pos];

        if (labelLen == 0) {
            if (!jumped) {
                offset = pos + 1;
            }
            break;
        }

        if ((labelLen & 0xC0) == 0xC0) {
            if (pos + 1 >= len) {
                return false;
            }
            if (!jumped) {
                offset = pos + 2;
            }
            pos = ((labelLen & 0x3F) << 8) | data[pos + 1];
            jumped = true;
            maxJumps--;
            continue;
        }

        pos++;
        if (pos + labelLen > len) {
            return false;
        }

        if (bufPos > 0 && bufPos < sizeof(buf) - 1) {
            buf[bufPos++] = '.';
        }

        for (uint8_t i = 0; i < labelLen && bufPos < sizeof(buf) - 1; ++i) {
            buf[bufPos++] = static_cast<char>(data[pos++]);
        }
    }

    buf[bufPos] = '\0';
    name = String(static_cast<const char*>(buf));

    if (!jumped) {
        offset = pos + 1;
    }

    return true;
}


uint16_t DnsWireFormat::readU16(const uint8_t *data)
{
    return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}


uint32_t DnsWireFormat::readU32(const uint8_t *data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           data[3];
}


} // namespace xmrig
