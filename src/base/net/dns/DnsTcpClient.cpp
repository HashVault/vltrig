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

#include "base/net/dns/DnsTcpClient.h"
#include "base/net/dns/DnsWireFormat.h"
#include "base/net/tools/NetBuffer.h"


xmrig::DnsTcpClient::DnsTcpClient(const sockaddr *addr, const String &host,
                                   int ai_family, uint32_t timeout, Callback callback)
    : m_host(host)
    , m_ai_family(ai_family)
    , m_timeout(timeout)
    , m_callback(std::move(callback))
{
    memcpy(&m_addr, addr, sizeof(sockaddr_storage));

    m_tcp = new uv_tcp_t;
    m_tcp->data = this;
    uv_tcp_init(uv_default_loop(), m_tcp);
    uv_tcp_nodelay(m_tcp, 1);

    m_timer = new uv_timer_t;
    m_timer->data = this;
    uv_timer_init(uv_default_loop(), m_timer);
}


xmrig::DnsTcpClient::~DnsTcpClient()
{
    close();
}


void xmrig::DnsTcpClient::start()
{
    auto *req = new uv_connect_t;
    req->data = this;

    uv_tcp_connect(req, m_tcp, reinterpret_cast<sockaddr*>(&m_addr), onConnect);
    uv_timer_start(m_timer, onTimer, m_timeout, 0);
}


void xmrig::DnsTcpClient::onConnect(uv_connect_t *req, int status)
{
    auto *client = static_cast<DnsTcpClient*>(req->data);
    delete req;

    if (client->m_closed) {
        return;
    }

    if (status < 0) {
        return client->done(false);
    }

    uv_read_start(reinterpret_cast<uv_stream_t*>(client->m_tcp), NetBuffer::onAlloc, onRead);
    client->send();
}


void xmrig::DnsTcpClient::onRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    auto *client = static_cast<DnsTcpClient*>(stream->data);

    if (client->m_closed) {
        NetBuffer::release(buf);
        return;
    }

    if (nread < 0) {
        NetBuffer::release(buf);
        return client->done(false);
    }

    if (nread > 0) {
        client->read(buf->base, static_cast<size_t>(nread));
    }

    NetBuffer::release(buf);
}


void xmrig::DnsTcpClient::onTimer(uv_timer_t *handle)
{
    auto *client = static_cast<DnsTcpClient*>(handle->data);

    if (!client->m_closed) {
        client->done(false);
    }
}


void xmrig::DnsTcpClient::send()
{
    const auto type = (m_ai_family == AF_INET6) ? DnsWireFormat::AAAA : DnsWireFormat::A;
    auto query = DnsWireFormat::buildQuery(m_host, type);

    if (query.empty()) {
        return done(false);
    }

    std::vector<char> buf(2 + query.size());
    buf[0] = static_cast<char>((query.size() >> 8) & 0xFF);
    buf[1] = static_cast<char>(query.size() & 0xFF);
    memcpy(buf.data() + 2, query.data(), query.size());

    uv_buf_t uvBuf = uv_buf_init(buf.data(), static_cast<unsigned int>(buf.size()));
    const int rc = uv_try_write(reinterpret_cast<uv_stream_t*>(m_tcp), &uvBuf, 1);

    if (rc < 0 || static_cast<size_t>(rc) != buf.size()) {
        done(false);
    }
}


void xmrig::DnsTcpClient::read(const char *data, size_t size)
{
    m_recvBuf.insert(m_recvBuf.end(), data, data + size);

    if (m_recvBuf.size() < 2) {
        return;
    }

    if (m_expectedLen == 0) {
        m_expectedLen = (static_cast<uint8_t>(m_recvBuf[0]) << 8) | static_cast<uint8_t>(m_recvBuf[1]);
    }

    if (m_expectedLen > 65535 || m_expectedLen < 12) {
        return done(false);
    }

    if (m_recvBuf.size() < 2 + m_expectedLen) {
        return;
    }

    const uint8_t *response = reinterpret_cast<const uint8_t*>(m_recvBuf.data() + 2);
    done(DnsWireFormat::parseAddressRecords(response, m_expectedLen, m_records, m_ai_family));
}


void xmrig::DnsTcpClient::done(bool success)
{
    if (m_closed) {
        return;
    }

    m_closed = true;
    uv_timer_stop(m_timer);

    if (m_callback) {
        auto callback = std::move(m_callback);
        m_callback = nullptr;
        callback(success, m_records);
    }

    close();
}


void xmrig::DnsTcpClient::close()
{
    if (m_tcp && !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_tcp))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_tcp), [](uv_handle_t *h) {
            delete reinterpret_cast<uv_tcp_t*>(h);
        });
        m_tcp = nullptr;
    }

    if (m_timer && !uv_is_closing(reinterpret_cast<uv_handle_t*>(m_timer))) {
        uv_close(reinterpret_cast<uv_handle_t*>(m_timer), [](uv_handle_t *h) {
            delete reinterpret_cast<uv_timer_t*>(h);
        });
        m_timer = nullptr;
    }
}
