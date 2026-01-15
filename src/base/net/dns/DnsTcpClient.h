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

#include "base/net/dns/DnsRecords.h"
#include "base/tools/String.h"

#include <uv.h>

#include <functional>
#include <vector>


namespace xmrig {


class DnsTcpClient
{
public:
    using Callback = std::function<void(bool success, const DnsRecords &records)>;

    DnsTcpClient(const sockaddr *addr, const String &host, int ai_family,
                 uint32_t timeout, Callback callback);
    ~DnsTcpClient();

    void start();

private:
    static void onConnect(uv_connect_t *req, int status);
    static void onRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    static void onTimer(uv_timer_t *handle);

    void send();
    void read(const char *data, size_t size);
    void done(bool success);
    void close();

    sockaddr_storage m_addr{};
    uv_tcp_t *m_tcp = nullptr;
    uv_timer_t *m_timer = nullptr;
    String m_host;
    int m_ai_family;
    uint32_t m_timeout;
    Callback m_callback;
    DnsRecords m_records;
    std::vector<char> m_recvBuf;
    size_t m_expectedLen = 0;
    bool m_closed = false;
};


} // namespace xmrig
