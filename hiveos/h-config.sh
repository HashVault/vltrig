#!/usr/bin/env bash

. /hive/miners/custom/$CUSTOM_MINER/h-manifest.conf

[[ -z $CUSTOM_TEMPLATE ]] && echo -e "${YELLOW}CUSTOM_TEMPLATE is empty${NOCOLOR}" && return 1
[[ -z $CUSTOM_URL ]] && echo -e "${YELLOW}CUSTOM_URL is empty${NOCOLOR}" && return 1

# Build pools array from CUSTOM_URL (space-separated for multiple pools)
pools='[]'
for url in $CUSTOM_URL; do
    pool=$(cat <<POOL
{
    "algo": null,
    "coin": null,
    "url": "$url",
    "user": "$CUSTOM_TEMPLATE",
    "pass": "${CUSTOM_PASS:-$WORKER_NAME}",
    "rig-id": "$WORKER_NAME",
    "nicehash": false,
    "keepalive": true,
    "enabled": true,
    "tls": true,
    "tls-fingerprint": null,
    "daemon": false,
    "socks5": null,
    "self-select": null,
    "submit-to-origin": false
}
POOL
    )
    pools=$(jq --null-input --argjson pools "$pools" --argjson pool "$pool" '$pools + [$pool]')
done

# Detect TLS from URL scheme or port
pools=$(echo "$pools" | jq '
    [.[] | if (.url | test("^(tls|ssl)://")) or (.url | test(":443$")) then
        .tls = true
    else . end]
')

# Replace template placeholders
[[ ! -z $EWAL ]] && pools=$(echo "$pools" | sed "s/%EWAL%/$EWAL/g")
[[ ! -z $DWAL ]] && pools=$(echo "$pools" | sed "s/%DWAL%/$DWAL/g")
[[ ! -z $ZWAL ]] && pools=$(echo "$pools" | sed "s/%ZWAL%/$ZWAL/g")
[[ ! -z $WORKER_NAME ]] && pools=$(echo "$pools" | sed "s/%WORKER_NAME%/$WORKER_NAME/g")

# Build base config
conf=$(cat <<CONF
{
    "autosave": false,
    "background": false,
    "colors": true,
    "randomx": {
        "init": -1,
        "init-avx2": -1,
        "mode": "auto",
        "1gb-pages": true,
        "rdmsr": true,
        "wrmsr": true,
        "cache_qos": false,
        "numa": true,
        "scratchpad_prefetch_mode": 1
    },
    "cpu": {
        "enabled": true,
        "huge-pages": true,
        "huge-pages-jit": false,
        "hw-aes": null,
        "priority": null,
        "memory-pool": false,
        "yield": true,
        "max-threads-hint": 100,
        "asm": true,
        "argon2-impl": null
    },
    "http": {
        "enabled": true,
        "host": "127.0.0.1",
        "port": $MINER_API_PORT,
        "access-token": null,
        "restricted": true
    },
    "donate-level": 1,
    "donate-over-proxy": 1,
    "log-file": "$CUSTOM_LOG_BASENAME.log",
    "print-time": 60,
    "health-print-time": 60,
    "retries": 5,
    "retry-pause": 5,
    "syslog": false,
    "dns": {
        "ip_version": 0,
        "ttl": 30,
        "pool-ns": true,
        "doh-primary": "dns.google",
        "doh-fallback": "dns.nextdns.io"
    },
    "verbose": 1,
    "watch": false,
    "pause-on-battery": false,
    "pause-on-active": false
}
CONF
)

# Merge pools
conf=$(jq --null-input --argjson conf "$conf" --argjson pools "$pools" '$conf + {"pools": $pools}')

# Merge user config overrides (one JSON fragment per line)
if [[ ! -z $CUSTOM_USER_CONFIG ]]; then
    while IFS= read -r line; do
        [[ -z $line ]] && continue
        conf=$(jq -s '.[0] * .[1]' <<< "$conf {$line}")
    done <<< "$CUSTOM_USER_CONFIG"
fi

echo "$conf" | jq . > $CUSTOM_CONFIG_FILENAME
