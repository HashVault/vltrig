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
#include <cctype>
#include <cstdio>
#include <cstring>

#include "base/net/dns/DomainUtils.h"


namespace xmrig {


String DomainUtils::extractBaseDomain(const String &host)
{
    if (host.isEmpty() || isIpAddress(host)) {
        return host;
    }

    const auto parts = host.split('.');

    if (parts.size() <= 2) {
        return host;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "%s.%s", parts[parts.size() - 2].data(), parts[parts.size() - 1].data());

    return String(static_cast<const char*>(buf));
}


bool DomainUtils::isIpAddress(const String &host)
{
    if (host.isEmpty()) {
        return false;
    }

    in_addr addr4{};
    if (uv_inet_pton(AF_INET, host.data(), &addr4) == 0) {
        return true;
    }

    in6_addr addr6{};
    return uv_inet_pton(AF_INET6, host.data(), &addr6) == 0;
}


} // namespace xmrig
