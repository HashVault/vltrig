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

#include <set>
#include <uv.h>

#include "base/io/log/Log.h"
#include "base/kernel/interfaces/IDnsListener.h"
#include "base/net/dns/DnsPoolNsBackend.h"
#include "base/net/dns/DnsTcpClient.h"
#include "base/net/dns/DnsUvBackend.h"
#include "base/net/dns/DnsWireFormat.h"
#include "base/net/dns/DomainUtils.h"
#include "base/net/http/Fetch.h"
#include "base/net/http/HttpData.h"
#include "base/tools/Chrono.h"


namespace xmrig {


static std::set<String> s_activeBaseDomains;


const char *DnsPoolNsBackend::kDoHPath      = "/dns-query";


class PoolDoHListener : public IHttpListener, public std::enable_shared_from_this<PoolDoHListener>
{
public:
    explicit PoolDoHListener(DnsPoolNsBackend *backend) : m_backend(backend) {}

    void onHttpData(const HttpData &data) override
    {
        m_backend->onPoolDoHResponse(data);
    }

private:
    DnsPoolNsBackend *m_backend;
};


} // namespace xmrig


xmrig::DnsPoolNsBackend::DnsPoolNsBackend()
{
    m_timer = new uv_timer_t;
    m_timer->data = this;
    uv_timer_init(uv_default_loop(), m_timer);
}


xmrig::DnsPoolNsBackend::~DnsPoolNsBackend()
{
    if (m_timer) {
        uv_timer_stop(m_timer);
        uv_close(reinterpret_cast<uv_handle_t *>(m_timer), [](uv_handle_t *handle) {
            delete reinterpret_cast<uv_timer_t *>(handle);
        });
    }
}


void xmrig::DnsPoolNsBackend::resolve(const String &host, const std::weak_ptr<IDnsListener> &listener, const DnsConfig &config)
{
    m_queue.emplace_back(listener);
    m_config = config;

    // Check TTL cache
    if (Chrono::currentMSecsSinceEpoch() - m_ts <= config.ttl() && !m_records.isEmpty()) {
        return notify();
    }

    // If already resolving, wait for result
    if (m_state != IDLE) {
        return;
    }

    m_host = host;
    m_baseDomain = DomainUtils::extractBaseDomain(host);
    m_status = 0;
    m_nsServers.clear();
    m_nsEntries.clear();
    m_currentNsIndex = 0;
    m_dohServerIndex = 0;
    m_poolQueryDoH = true;
    m_addedToActiveSet = false;
    m_tcpClient.reset();
    m_poolDoHListener.reset();

    // If pool-ns is disabled or host is an IP, use system DNS
    if (!config.poolNs() || DomainUtils::isIpAddress(host)) {
        return fallbackToSystem();
    }

    // Detect recursion: if another resolution for this base domain is active,
    // use simple DoH to avoid circular dependency (e.g., resolving NS hostnames)
    if (s_activeBaseDomains.count(m_baseDomain) > 0) {
        return startSimpleDoH();
    }

    s_activeBaseDomains.insert(m_baseDomain);
    m_addedToActiveSet = true;
    startNsLookup();
}


void xmrig::DnsPoolNsBackend::onHttpData(const HttpData &data)
{
    if (m_state == IDLE || m_state == FALLBACK) {
        return;
    }

    uv_timer_stop(m_timer);

    if (data.status != 200) {
        if (m_state == NS_LOOKUP && ++m_dohServerIndex < 2) {
            return startNsLookup();
        }

        if (m_state == SIMPLE_DOH && ++m_dohServerIndex < 2) {
            return startSimpleDoH();
        }

        if (m_state == NS_RESOLVE) {
            return startPoolQueryTcp();
        }

        return fallbackToSystem();
    }

    const auto *response = reinterpret_cast<const uint8_t *>(data.body.data());
    const size_t len = data.body.size();

    if (m_state == NS_LOOKUP) {
        std::vector<String> nsServers;
        if (DnsWireFormat::parseResponse(response, len, nsServers, DnsWireFormat::NS)) {
            onNsLookupComplete(nsServers);
        }
        else if (++m_dohServerIndex < 2) {
            startNsLookup();
        }
        else {
            fallbackToSystem();
        }
    }
    else if (m_state == NS_RESOLVE) {
        std::vector<String> nsIps;
        if (DnsWireFormat::parseResponse(response, len, nsIps, DnsWireFormat::A) && !nsIps.empty()) {
            m_nsEntries.emplace_back(m_nsServers[m_currentNsIndex], nsIps[0]);
            startPoolQueryTcp();
        }
        else {
            tryNextNs();
        }
    }
    else if (m_state == SIMPLE_DOH) {
        DnsRecords records;
        if (DnsWireFormat::parseAddressRecords(response, len, records, m_config.ai_family())) {
            m_records = records;
            m_status = 0;
            m_ts = Chrono::currentMSecsSinceEpoch();

            const char *dohServer = (m_dohServerIndex == 0) ? m_config.dohPrimary().data() : m_config.dohFallback().data();
            LOG_DEBUG("[DNS] %s -> %s (via %s)", m_host.data(), m_records.get().ip().data(), dohServer);

            m_state = IDLE;
            notify();
        }
        else if (++m_dohServerIndex < 2) {
            startSimpleDoH();
        }
        else {
            fallbackToSystem();
        }
    }
}


void xmrig::DnsPoolNsBackend::onPoolDoHResponse(const HttpData &data)
{
    uv_timer_stop(m_timer);

    if (m_state != POOL_QUERY) {
        return;
    }

    if (data.status != 200) {
        LOG_DEBUG("[DNS] DoH to NS failed with status %d", data.status);
        return startNsResolve();
    }

    const auto *response = reinterpret_cast<const uint8_t *>(data.body.data());
    DnsRecords records;

    if (DnsWireFormat::parseAddressRecords(response, data.body.size(), records, m_config.ai_family())) {
        m_records = records;
        m_status = 0;
        m_ts = Chrono::currentMSecsSinceEpoch();
        onPoolQueryComplete(true);
    }
    else {
        startNsResolve();
    }
}


void xmrig::DnsPoolNsBackend::startSimpleDoH()
{
    m_state = SIMPLE_DOH;

    const char *dohServer = (m_dohServerIndex == 0) ? m_config.dohPrimary().data() : m_config.dohFallback().data();
    auto query = DnsWireFormat::buildQuery(m_host, DnsWireFormat::A);

    LOG_DEBUG("[DNS] resolving %s via %s", m_host.data(), dohServer);

    if (query.empty()) {
        return fallbackToSystem();
    }

    FetchRequest req(HTTP_POST, dohServer, 443, kDoHPath, true, true);
    req.body = std::string(reinterpret_cast<const char *>(query.data()), query.size());
    req.headers.insert({ "Content-Type", "application/dns-message" });
    req.headers.insert({ "Accept", "application/dns-message" });
    req.timeout = m_config.poolNsTimeout();

    fetch("[DNS]", std::move(req), shared_from_this());
    uv_timer_start(m_timer, onTimer, m_config.poolNsTimeout(), 0);
}


void xmrig::DnsPoolNsBackend::startNsLookup()
{
    m_state = NS_LOOKUP;

    const char *dohServer = (m_dohServerIndex == 0) ? m_config.dohPrimary().data() : m_config.dohFallback().data();
    auto query = DnsWireFormat::buildQuery(m_baseDomain, DnsWireFormat::NS);

    if (m_dohServerIndex == 0) {
        LOG_DEBUG("[DNS] looking up NS for %s via %s", m_baseDomain.data(), dohServer);
    } else {
        LOG_DEBUG("[DNS] retrying NS lookup for %s via %s", m_baseDomain.data(), dohServer);
    }

    if (query.empty()) {
        return fallbackToSystem();
    }

    FetchRequest req(HTTP_POST, dohServer, 443, kDoHPath, true, true);
    req.body = std::string(reinterpret_cast<const char *>(query.data()), query.size());
    req.headers.insert({ "Content-Type", "application/dns-message" });
    req.headers.insert({ "Accept", "application/dns-message" });
    req.timeout = m_config.poolNsTimeout();

    fetch("[DNS]", std::move(req), shared_from_this());
    uv_timer_start(m_timer, onTimer, m_config.poolNsTimeout(), 0);
}


void xmrig::DnsPoolNsBackend::onNsLookupComplete(const std::vector<String> &nsServers)
{
    if (nsServers.empty()) {
        return fallbackToSystem();
    }

    for (const auto &ns : nsServers) {
        LOG_DEBUG("[DNS] found NS: %s", ns.data());
    }

    m_nsServers = nsServers;
    startPoolQuery();
}


void xmrig::DnsPoolNsBackend::startNsResolve()
{
    if (m_currentNsIndex >= m_nsServers.size()) {
        return fallbackToSystem();
    }

    m_state = NS_RESOLVE;
    const String &nsHost = m_nsServers[m_currentNsIndex];

    LOG_DEBUG("[DNS] resolving NS %s for TCP fallback", nsHost.data());

    auto query = DnsWireFormat::buildQuery(nsHost, DnsWireFormat::A);
    if (query.empty()) {
        return tryNextNs();
    }

    // Use whichever DoH server succeeded for NS lookup
    const char *dohServer = (m_dohServerIndex == 0) ? m_config.dohPrimary().data() : m_config.dohFallback().data();

    FetchRequest req(HTTP_POST, dohServer, 443, kDoHPath, true, true);
    req.body = std::string(reinterpret_cast<const char *>(query.data()), query.size());
    req.headers.insert({ "Content-Type", "application/dns-message" });
    req.headers.insert({ "Accept", "application/dns-message" });
    req.timeout = m_config.poolNsTimeout();

    fetch("[DNS]", std::move(req), shared_from_this());
    uv_timer_start(m_timer, onTimer, m_config.poolNsTimeout(), 0);
}


void xmrig::DnsPoolNsBackend::startPoolQuery()
{
    if (m_currentNsIndex >= m_nsServers.size()) {
        return fallbackToSystem();
    }

    m_state = POOL_QUERY;
    m_poolQueryDoH = true;
    startPoolQueryDoH(m_nsServers[m_currentNsIndex]);
}


void xmrig::DnsPoolNsBackend::startPoolQueryDoH(const String &nsHost)
{
    LOG_DEBUG("[DNS] querying %s via DoH to %s", m_host.data(), nsHost.data());

    auto query = DnsWireFormat::buildQuery(m_host, DnsWireFormat::A);
    if (query.empty()) {
        return startNsResolve();
    }

    FetchRequest req(HTTP_POST, nsHost.data(), 443, kDoHPath, true, true);
    req.body = std::string(reinterpret_cast<const char *>(query.data()), query.size());
    req.headers.insert({ "Content-Type", "application/dns-message" });
    req.headers.insert({ "Accept", "application/dns-message" });
    req.timeout = m_config.poolNsTimeout();

    m_poolDoHListener = std::make_shared<PoolDoHListener>(this);
    fetch("[DNS]", std::move(req), m_poolDoHListener);
    uv_timer_start(m_timer, onTimer, m_config.poolNsTimeout(), 0);
}


void xmrig::DnsPoolNsBackend::startPoolQueryTcp()
{
    if (m_nsEntries.empty()) {
        return tryNextNs();
    }

    const auto &entry = m_nsEntries.back();
    const String &nsIp = entry.second;

    LOG_DEBUG("[DNS] querying %s via TCP to %s:53", m_host.data(), nsIp.data());

    m_state = POOL_QUERY;
    m_poolQueryDoH = false;
    uv_timer_stop(m_timer);

    sockaddr_storage addr{};
    int rc = 0;

    if (nsIp.contains(":")) {
        auto *sin6 = reinterpret_cast<sockaddr_in6*>(&addr);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(53);
        rc = uv_inet_pton(AF_INET6, nsIp.data(), &sin6->sin6_addr);
    }
    else {
        auto *sin = reinterpret_cast<sockaddr_in*>(&addr);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(53);
        rc = uv_inet_pton(AF_INET, nsIp.data(), &sin->sin_addr);
    }

    if (rc != 0) {
        return tryNextNs();
    }

    m_tcpClient.reset(new DnsTcpClient(
        reinterpret_cast<sockaddr*>(&addr), m_host, m_config.ai_family(), m_config.poolNsTimeout(),
        [this](bool success, const DnsRecords &records) { onTcpQueryComplete(success, records); }
    ));
    m_tcpClient->start();
}


void xmrig::DnsPoolNsBackend::onTcpQueryComplete(bool success, const DnsRecords &records)
{
    if (success && !records.isEmpty()) {
        m_records = records;
        m_status = 0;
        m_ts = Chrono::currentMSecsSinceEpoch();
        m_tcpClient.reset();
        return onPoolQueryComplete(true);
    }

    m_tcpClient.reset();
    tryNextNs();
}


void xmrig::DnsPoolNsBackend::onPoolQueryComplete(bool success)
{
    if (!success) {
        return fallbackToSystem();
    }

    const char *method = m_poolQueryDoH ? "DoH" : "TCP";
    const char *via = m_nsServers.empty() ? "" : m_nsServers[m_currentNsIndex].data();

    if (m_records.size() > 1) {
        std::string ips;
        for (const auto &record : m_records.records()) {
            if (!ips.empty()) {
                ips += ", ";
            }
            ips += record.ip().data();
        }
        LOG_INFO("[DNS] " CYAN("%s") " -> " GREEN_BOLD("%s") " (%zu records via %s to %s)",
                 m_host.data(), ips.c_str(), m_records.size(), method, via);
    }
    else {
        LOG_INFO("[DNS] " CYAN("%s") " -> " GREEN_BOLD("%s") " (via %s to %s)",
                 m_host.data(), m_records.get().ip().data(), method, via);
    }

    m_state = IDLE;
    notify();
}


void xmrig::DnsPoolNsBackend::tryNextNs()
{
    m_currentNsIndex++;
    m_poolQueryDoH = true;

    if (m_currentNsIndex < m_nsServers.size()) {
        startPoolQuery();
    }
    else {
        fallbackToSystem();
    }
}


void xmrig::DnsPoolNsBackend::fallbackToSystem()
{
    LOG_DEBUG("[DNS] falling back to system DNS for %s", m_host.data());

    m_state = FALLBACK;
    uv_timer_stop(m_timer);

    if (!m_fallback) {
        m_fallback = std::make_shared<DnsUvBackend>();
    }

    class FallbackListener : public IDnsListener
    {
    public:
        explicit FallbackListener(DnsPoolNsBackend *backend) : m_backend(backend) {}

        void onResolved(const DnsRecords &records, int status, const char *) override
        {
            m_backend->m_records = records;
            m_backend->m_status = status;
            m_backend->m_ts = Chrono::currentMSecsSinceEpoch();
            m_backend->m_state = IDLE;
            m_backend->notify();
        }

    private:
        DnsPoolNsBackend *m_backend;
    };

    m_fallbackListener = std::make_shared<FallbackListener>(this);
    m_fallback->resolve(m_host, m_fallbackListener, m_config);
}


void xmrig::DnsPoolNsBackend::notify()
{
    if (m_addedToActiveSet) {
        s_activeBaseDomains.erase(m_baseDomain);
        m_addedToActiveSet = false;
    }

    const char *error = m_status < 0 ? "DNS resolution failed" : nullptr;

    for (const auto &l : m_queue) {
        if (auto listener = l.lock()) {
            listener->onResolved(m_records, m_status, error);
        }
    }

    m_queue.clear();
}


void xmrig::DnsPoolNsBackend::onTimeout()
{
    if (m_state == NS_LOOKUP && ++m_dohServerIndex < 2) {
        return startNsLookup();
    }

    if (m_state == NS_RESOLVE) {
        return tryNextNs();
    }

    if (m_state == POOL_QUERY) {
        if (m_poolQueryDoH) {
            return startNsResolve();
        }
        return tryNextNs();
    }

    fallbackToSystem();
}


void xmrig::DnsPoolNsBackend::onTimer(uv_timer_t *handle)
{
    auto *backend = static_cast<DnsPoolNsBackend *>(handle->data);
    backend->onTimeout();
}
