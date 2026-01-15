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

#include "base/kernel/interfaces/IDnsBackend.h"
#include "base/kernel/interfaces/IHttpListener.h"
#include "base/net/dns/DnsConfig.h"
#include "base/net/dns/DnsRecords.h"
#include "base/tools/String.h"

#include <deque>
#include <memory>
#include <vector>


struct uv_timer_s;
typedef struct uv_timer_s uv_timer_t;


namespace xmrig {


class DnsTcpClient;
class IDnsListener;
class PoolDoHListener;


class DnsPoolNsBackend : public IDnsBackend, public IHttpListener, public std::enable_shared_from_this<DnsPoolNsBackend>
{
    friend class PoolDoHListener;

public:
    XMRIG_DISABLE_COPY_MOVE(DnsPoolNsBackend)

    DnsPoolNsBackend();
    ~DnsPoolNsBackend() override;

    // IDnsBackend
    void resolve(const String &host, const std::weak_ptr<IDnsListener> &listener, const DnsConfig &config) override;

protected:
    // IHttpListener
    void onHttpData(const HttpData &data) override;

private:
    enum State {
        IDLE,
        NS_LOOKUP,
        NS_RESOLVE,
        POOL_QUERY,
        SIMPLE_DOH,
        FALLBACK
    };

    void startSimpleDoH();
    void startNsLookup();
    void onNsLookupComplete(const std::vector<String> &nsServers);
    void startNsResolve();
    void startPoolQuery();
    void startPoolQueryDoH(const String &nsHost);
    void startPoolQueryTcp();
    void onPoolDoHResponse(const HttpData &data);
    void onPoolQueryComplete(bool success);
    void onTcpQueryComplete(bool success, const DnsRecords &records);
    void tryNextNs();
    void fallbackToSystem();
    void notify();
    void onTimeout();

    static void onTimer(uv_timer_t *handle);

    DnsConfig m_config;
    DnsRecords m_records;
    int m_status = 0;
    State m_state = IDLE;
    String m_host;
    String m_baseDomain;
    std::deque<std::weak_ptr<IDnsListener>> m_queue;
    std::shared_ptr<IDnsBackend> m_fallback;
    std::shared_ptr<IDnsListener> m_fallbackListener;
    std::shared_ptr<IHttpListener> m_poolDoHListener;
    std::unique_ptr<DnsTcpClient> m_tcpClient;
    std::vector<String> m_nsServers;
    std::vector<std::pair<String, String>> m_nsEntries;  // hostname, ip
    size_t m_currentNsIndex = 0;
    size_t m_dohServerIndex = 0;
    uv_timer_t *m_timer = nullptr;
    uint64_t m_ts = 0;
    bool m_poolQueryDoH = true;
    bool m_addedToActiveSet = false;

    static const char *kDoHPath;
};


} // namespace xmrig
