#!/usr/bin/env bash

cd "$(dirname "$0")"
. h-manifest.conf

[[ -z $CUSTOM_LOG_BASENAME ]] && echo -e "${RED}No CUSTOM_LOG_BASENAME is set${NOCOLOR}" && exit 1
[[ -z $CUSTOM_CONFIG_FILENAME ]] && echo -e "${RED}No CUSTOM_CONFIG_FILENAME is set${NOCOLOR}" && exit 1
[[ ! -f $CUSTOM_CONFIG_FILENAME ]] && echo -e "${RED}Custom config $CUSTOM_CONFIG_FILENAME not found${NOCOLOR}" && exit 1

LOG_DIR=$(dirname "$CUSTOM_LOG_BASENAME")
[[ ! -d $LOG_DIR ]] && mkdir -p "$LOG_DIR"

# Enable 2MB hugepages
hugepages -rx 2>/dev/null

# Enable 1GB hugepages for RandomX
NR_1GB=0
for node in /sys/devices/system/node/node*/hugepages/hugepages-1048576kB/nr_hugepages; do
    if [[ -f "$node" ]]; then
        echo 3 > "$node" 2>/dev/null
        NR_1GB=$((NR_1GB + 3))
    fi
done
[[ $NR_1GB -gt 0 ]] && echo "1GB hugepages: allocated $NR_1GB pages"

./vltrig --config=$CUSTOM_CONFIG_FILENAME 2>&1 | tee --append $CUSTOM_LOG_BASENAME.log
