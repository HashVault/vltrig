#!/usr/bin/env bash

. /hive/miners/custom/$CUSTOM_MINER/h-manifest.conf

stats_raw=$(curl --connect-timeout 2 --max-time 5 --silent --noproxy '*' \
    http://127.0.0.1:$MINER_API_PORT/1/summary 2>/dev/null)

if [[ $? -ne 0 || -z $stats_raw ]]; then
    echo -e "${YELLOW}Failed to read $CUSTOM_NAME stats from localhost:${MINER_API_PORT}${NOCOLOR}"
    khs=0
    stats="null"
else
    khs=$(echo "$stats_raw" | jq -r '.hashrate.total[0] // 0' | awk '{printf "%.2f", $1/1000}')

    local ac=$(echo "$stats_raw" | jq -r '.results.shares_good // 0')
    local total=$(echo "$stats_raw" | jq -r '.results.shares_total // 0')
    local rj=$((total - ac))
    local uptime=$(echo "$stats_raw" | jq -r '.connection.uptime // 0')
    local ver=$(echo "$stats_raw" | jq -r '.version // "unknown"')
    local algo=$(echo "$stats_raw" | jq -r '.algo // "randomx"')

    # Per-thread hashrates (10-second window)
    local hs=$(echo "$stats_raw" | jq '[.hashrate.threads[][0] // 0]')
    local num_threads=$(echo "$stats_raw" | jq '[.hashrate.threads[][0]] | length')

    # CPU temperature: use HiveOS cpu-temp utility, duplicate across all threads
    local cpu_temp=$(cpu-temp 2>/dev/null)
    [[ -z $cpu_temp ]] && cpu_temp=null
    local l_temps='[]'
    local l_fans='[]'
    local l_bus='[]'
    if [[ $cpu_temp != "null" && $num_threads -gt 0 ]]; then
        l_temps=$(jq -nc --argjson n "$num_threads" --argjson t "$cpu_temp" '[ range($n) | $t ]')
        l_fans=$(jq -nc --argjson n "$num_threads" '[ range($n) | 0 ]')
        l_bus=$(jq -nc --argjson n "$num_threads" '[ range($n) | null ]')
    fi

    stats=$(jq -nc \
        --argjson hs "$hs" \
        --arg hs_units "hs" \
        --argjson temp "$l_temps" \
        --argjson fan "$l_fans" \
        --argjson bus_numbers "$l_bus" \
        --arg uptime "$uptime" \
        --arg ver "$ver" \
        --arg ac "$ac" \
        --arg rj "$rj" \
        --arg algo "$algo" \
        '{$hs, $hs_units, $temp, $fan, $bus_numbers, uptime: ($uptime | tonumber), $ver, ar: [($ac | tonumber), ($rj | tonumber)], $algo}')
fi

[[ -z $khs ]] && khs=0
[[ -z $stats ]] && stats="null"
