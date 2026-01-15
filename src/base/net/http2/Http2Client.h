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

#ifndef XMRIG_HTTP2CLIENT_H
#define XMRIG_HTTP2CLIENT_H


#include "base/net/https/HttpsClient.h"

#include <nghttp2/nghttp2.h>
#include <string>


namespace xmrig {


class Http2Client : public HttpsClient
{
public:
    XMRIG_DISABLE_COPY_MOVE_DEFAULT(Http2Client)

    Http2Client(const char *tag, FetchRequest &&req, const std::weak_ptr<IHttpListener> &listener);
    ~Http2Client() override;

protected:
    void handshake() override;
    void read(const char *data, size_t size) override;

private:
    static ssize_t onDataSourceRead(nghttp2_session *session, int32_t stream_id,
                                    uint8_t *buf, size_t length, uint32_t *data_flags,
                                    nghttp2_data_source *source, void *user_data);
    static int onHeader(nghttp2_session *session, const nghttp2_frame *frame,
                        const uint8_t *name, size_t namelen,
                        const uint8_t *value, size_t valuelen,
                        uint8_t flags, void *user_data);
    static int onDataChunkRecv(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                               const uint8_t *data, size_t len, void *user_data);
    static int onStreamClose(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data);

    bool verifyAlpn();
    void initSession();
    void submitRequest();
    void sendPendingData();

    nghttp2_session *m_session  = nullptr;
    int32_t m_streamId          = 0;
    std::string m_responseBody;
    int m_responseStatus        = 0;
    size_t m_bodyPos            = 0;
};


} // namespace xmrig


#endif // XMRIG_HTTP2CLIENT_H
