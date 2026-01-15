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

#include <cstring>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <uv.h>

#include "base/net/http2/Http2Client.h"
#include "base/io/log/Log.h"
#include "base/kernel/interfaces/IHttpListener.h"


#define MAKE_NV(NAME, VALUE) \
    { (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1, NGHTTP2_NV_FLAG_NONE }

#define MAKE_NV_CS(NAME, VALUE, VALUELEN) \
    { (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, VALUELEN, NGHTTP2_NV_FLAG_NONE }


xmrig::Http2Client::Http2Client(const char *tag, FetchRequest &&req, const std::weak_ptr<IHttpListener> &listener) :
    HttpsClient(tag, std::move(req), listener)
{
    static const unsigned char alpn[] = "\x02h2";
    SSL_CTX_set_alpn_protos(m_ctx, alpn, sizeof(alpn) - 1);
}


xmrig::Http2Client::~Http2Client()
{
    if (m_session) {
        nghttp2_session_del(m_session);
    }
}


void xmrig::Http2Client::handshake()
{
    m_ssl = SSL_new(m_ctx);
    if (!m_ssl) {
        return;
    }

    SSL_set_connect_state(m_ssl);
    SSL_set_bio(m_ssl, m_read, m_write);
    SSL_set_tlsext_host_name(m_ssl, host());

    SSL_do_handshake(m_ssl);

    flush(false);
}


void xmrig::Http2Client::read(const char *data, size_t size)
{
    BIO_write(m_read, data, size);

    if (!SSL_is_init_finished(m_ssl)) {
        const int rc = SSL_connect(m_ssl);

        if (rc < 0 && SSL_get_error(m_ssl, rc) == SSL_ERROR_WANT_READ) {
            flush(false);
        }
        else if (rc == 1) {
            X509 *cert = SSL_get_peer_certificate(m_ssl);
            if (!verify(cert)) {
                X509_free(cert);
                return close(UV_EPROTO);
            }
            X509_free(cert);

            if (!verifyAlpn()) {
                LOG_ERR("%s HTTP/2 ALPN negotiation failed", tag());
                return close(UV_EPROTO);
            }

            m_ready = true;
            initSession();
            submitRequest();
            sendPendingData();
        }
        return;
    }

    static char buf[16384];
    int rc = 0;

    while ((rc = SSL_read(m_ssl, buf, sizeof(buf))) > 0) {
        ssize_t rv = nghttp2_session_mem_recv(m_session, reinterpret_cast<const uint8_t *>(buf), rc);
        if (rv < 0) {
            LOG_ERR("%s nghttp2_session_mem_recv error: %s", tag(), nghttp2_strerror(static_cast<int>(rv)));
            return close(UV_EPROTO);
        }
        sendPendingData();
    }

    if (rc == 0) {
        close(UV_EOF);
    }
}


bool xmrig::Http2Client::verifyAlpn()
{
    const unsigned char *alpn_data = nullptr;
    unsigned int alpn_len = 0;

    SSL_get0_alpn_selected(m_ssl, &alpn_data, &alpn_len);

    return alpn_len == 2 && memcmp(alpn_data, "h2", 2) == 0;
}


void xmrig::Http2Client::initSession()
{
    nghttp2_session_callbacks *callbacks = nullptr;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_header_callback(callbacks, onHeader);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, onDataChunkRecv);
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, onStreamClose);

    nghttp2_session_client_new(&m_session, callbacks, this);
    nghttp2_session_callbacks_del(callbacks);

    nghttp2_settings_entry iv[] = {
        { NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 1 },
        { NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535 }
    };
    nghttp2_submit_settings(m_session, NGHTTP2_FLAG_NONE, iv, 2);
}


void xmrig::Http2Client::submitRequest()
{
    const std::string authority = std::string(host()) + ":" + std::to_string(port());

    nghttp2_nv headers[] = {
        MAKE_NV(":method", "POST"),
        MAKE_NV(":scheme", "https"),
        MAKE_NV_CS(":authority", authority.c_str(), authority.size()),
        MAKE_NV_CS(":path", url.c_str(), url.size()),
        MAKE_NV("content-type", "application/dns-message"),
        MAKE_NV("accept", "application/dns-message"),
    };

    nghttp2_data_provider data_prd;
    data_prd.source.ptr = this;
    data_prd.read_callback = onDataSourceRead;

    m_streamId = nghttp2_submit_request(m_session, nullptr, headers, 6, &data_prd, this);

    LOG_DEBUG("%s HTTP/2 stream %d submitted to %s", tag(), m_streamId, authority.c_str());
}


void xmrig::Http2Client::sendPendingData()
{
    for (;;) {
        const uint8_t *data = nullptr;
        ssize_t len = nghttp2_session_mem_send(m_session, &data);

        if (len < 0) {
            LOG_ERR("%s nghttp2_session_mem_send error", tag());
            return close(UV_EPROTO);
        }

        if (len == 0) {
            break;
        }

        SSL_write(m_ssl, data, static_cast<int>(len));
        flush(false);
    }
}


ssize_t xmrig::Http2Client::onDataSourceRead(nghttp2_session *, int32_t, uint8_t *buf, size_t length,
                                              uint32_t *data_flags, nghttp2_data_source *source, void *)
{
    auto *client = static_cast<Http2Client *>(source->ptr);

    const size_t remaining = client->body.size() - client->m_bodyPos;
    const size_t to_copy = std::min(length, remaining);

    if (to_copy > 0) {
        memcpy(buf, client->body.data() + client->m_bodyPos, to_copy);
        client->m_bodyPos += to_copy;
    }

    if (client->m_bodyPos >= client->body.size()) {
        *data_flags |= NGHTTP2_DATA_FLAG_EOF;
    }

    return static_cast<ssize_t>(to_copy);
}


int xmrig::Http2Client::onHeader(nghttp2_session *, const nghttp2_frame *frame,
                                  const uint8_t *name, size_t namelen,
                                  const uint8_t *value, size_t valuelen, uint8_t, void *user_data)
{
    auto *client = static_cast<Http2Client *>(user_data);

    if (frame->hd.type != NGHTTP2_HEADERS || frame->hd.stream_id != client->m_streamId) {
        return 0;
    }

    if (namelen == 7 && memcmp(name, ":status", 7) == 0) {
        client->m_responseStatus = atoi(reinterpret_cast<const char *>(value));
    }

    return 0;
}


int xmrig::Http2Client::onDataChunkRecv(nghttp2_session *, uint8_t, int32_t stream_id,
                                         const uint8_t *data, size_t len, void *user_data)
{
    auto *client = static_cast<Http2Client *>(user_data);

    if (stream_id == client->m_streamId) {
        client->m_responseBody.append(reinterpret_cast<const char *>(data), len);
    }

    return 0;
}


int xmrig::Http2Client::onStreamClose(nghttp2_session *, int32_t stream_id, uint32_t error_code, void *user_data)
{
    auto *client = static_cast<Http2Client *>(user_data);

    if (stream_id != client->m_streamId) {
        return 0;
    }

    if (error_code != NGHTTP2_NO_ERROR) {
        LOG_ERR("%s HTTP/2 stream %d closed with error: %s",
                client->tag(), stream_id, nghttp2_http2_strerror(error_code));
        client->close(UV_EPROTO);
        return 0;
    }

    LOG_DEBUG("%s HTTP/2 stream %d closed, status=%d, body=%zu bytes",
              client->tag(), stream_id, client->m_responseStatus, client->m_responseBody.size());

    client->status = client->m_responseStatus;
    client->body = std::move(client->m_responseBody);

    if (auto listener = client->httpListener()) {
        listener->onHttpData(*client);
    }

    client->close(0);

    return 0;
}
