# vltrig

A fork of [XMRig](https://github.com/xmrig/xmrig) miner tailored for [HashVault](https://hashvault.pro) mining pools.

## Project Goals

**Anti-censorship first.** Helping miners bypass network restrictions and DNS blocking that prevent access to mining pools. Mining should be accessible to everyone, everywhere.

**Focus areas:**
- Anti-censorship features (DoH, secure DNS resolution)
- UI/UX improvements
- HashVault pool optimizations
- Tracking upstream XMRig for updates and security fixes

**Not changing:**
- Hashing algorithms
- Mining performance
- Donation mechanics (original XMRig donation is preserved)

## Download

Prebuilt binaries are available on the [Releases](https://github.com/HashVault/vltrig/releases) page:

- Linux x64
- Windows x64 (GCC and MSVC builds)
- macOS x64 and ARM64 (Apple Silicon)

**Note:** Prebuilt binaries are CPU-only (no OpenCL/CUDA). For GPU mining, build from source with `-DWITH_OPENCL=ON` and/or `-DWITH_CUDA=ON`.

## Contributing Back

While vltrig is tailored for HashVault pools, improvements that benefit all miners are submitted as pull requests to the upstream [XMRig](https://github.com/xmrig/xmrig) project.

## Versioning

vltrig uses a four-part version: `X.Y.Z.P`

- `X.Y.Z` = upstream XMRig version
- `.P` = vltrig patch number (resets to 1 on upstream update)

Example:
```
XMRig 6.25.0 → vltrig 6.25.0.1 → 6.25.0.2 → 6.25.0.3
XMRig 6.26.0 → vltrig 6.26.0.1 → 6.26.0.2
```

## Features

### RandomX Only

vltrig is optimized for Monero mining. Only RandomX (rx/0) algorithm is advertised to pools. Other algorithm families are disabled by default:

- CryptoNight (cn/0, cn/1, cn/2, cn/r, etc.) - filtered out
- CryptoNight-Lite, Heavy, Pico, Femto - disabled
- Argon2 - disabled
- KawPow - disabled
- GhostRider - disabled

### Default Pool

HashVault pool is preconfigured as default with TLS and certificate pinning:
- URL: `pool.hashvault.pro:443`
- TLS: enabled
- Fingerprint: `420c7850e09b7c0bdcf748a7da9eb3647daf8515718f36d9ccfdd6b9ff834b14`

### DNS-over-HTTPS (DoH)

vltrig includes secure DNS resolution via DNS-over-HTTPS using HTTP/2. When `--dns-pool-ns` is enabled (default), the miner queries the pool's authoritative nameservers directly via DoH, bypassing potentially censored or compromised local DNS resolvers.

#### How it works
1. Queries a public DoH server for the pool's NS records
2. Resolves NS hostnames via the same DoH server
3. Queries the authoritative NS directly via DoH for pool IP addresses

#### Command line options
| Option | Description |
|--------|-------------|
| `--dns-pool-ns` | Enable authoritative NS resolution (default: enabled) |
| `--no-dns-pool-ns` | Disable, use system DNS |
| `--dns-doh-primary=HOST` | Primary DoH server (default: `dns.google`) |
| `--dns-doh-fallback=HOST` | Fallback DoH server (default: `dns.nextdns.io`) |

#### Config file
```json
{
  "dns": {
    "pool-ns": true,
    "doh-primary": "dns.google",
    "doh-fallback": "dns.nextdns.io"
  }
}
```

#### Build dependency
HTTP/2 support requires **libnghttp2**:
- Ubuntu/Debian: `apt install libnghttp2-dev`
- RHEL/CentOS: `yum install libnghttp2-devel`
- macOS: `brew install nghttp2`

Build with `-DWITH_HTTP2=OFF` to disable HTTP/2 support.

## Building

### Dependencies

Ubuntu/Debian:
```bash
apt install build-essential cmake libuv1-dev libssl-dev libhwloc-dev libnghttp2-dev
```

### Build

```bash
make release    # Release build
make debug      # Debug build
make clean      # Clean build directory
make rebuild    # Clean + release
```

Binary will be at `build/vltrig`.

---

# XMRig

[![Github All Releases](https://img.shields.io/github/downloads/xmrig/xmrig/total.svg)](https://github.com/xmrig/xmrig/releases)
[![GitHub release](https://img.shields.io/github/release/xmrig/xmrig/all.svg)](https://github.com/xmrig/xmrig/releases)
[![GitHub Release Date](https://img.shields.io/github/release-date/xmrig/xmrig.svg)](https://github.com/xmrig/xmrig/releases)
[![GitHub license](https://img.shields.io/github/license/xmrig/xmrig.svg)](https://github.com/xmrig/xmrig/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/xmrig/xmrig.svg)](https://github.com/xmrig/xmrig/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/xmrig/xmrig.svg)](https://github.com/xmrig/xmrig/network)

XMRig is a high performance, open source, cross platform RandomX, KawPow, CryptoNight and [GhostRider](https://github.com/xmrig/xmrig/tree/master/src/crypto/ghostrider#readme) unified CPU/GPU miner and [RandomX benchmark](https://xmrig.com/benchmark). Official binaries are available for Windows, Linux, macOS and FreeBSD.

## Mining backends
- **CPU** (x86/x64/ARMv7/ARMv8/RISC-V)
- **OpenCL** for AMD GPUs.
- **CUDA** for NVIDIA GPUs via external [CUDA plugin](https://github.com/xmrig/xmrig-cuda).

## Download
* **[Binary releases](https://github.com/xmrig/xmrig/releases)**
* **[Build from source](https://xmrig.com/docs/miner/build)**

## Usage
The preferred way to configure the miner is the [JSON config file](https://xmrig.com/docs/miner/config) as it is more flexible and human friendly. The [command line interface](https://xmrig.com/docs/miner/command-line-options) does not cover all features, such as mining profiles for different algorithms. Important options can be changed during runtime without miner restart by editing the config file or executing [API](https://xmrig.com/docs/miner/api) calls.

* **[Wizard](https://xmrig.com/wizard)** helps you create initial configuration for the miner.
* **[Workers](http://workers.xmrig.info)** helps manage your miners via HTTP API.

## Donations
* Default donation 1% (1 minute in 100 minutes) can be increased via option `donate-level` or disabled in source code.
* XMR: `48edfHu7V9Z84YzzMa6fUueoELZ9ZRXq9VetWzYGzKt52XU5xvqgzYnDK9URnRoJMk1j8nLwEVsaSWJ4fhdUyZijBGUicoD`

## Developers
* **[xmrig](https://github.com/xmrig)**
* **[sech1](https://github.com/SChernykh)**

## Contacts
* support@xmrig.com
* [reddit](https://www.reddit.com/user/XMRig/)
* [twitter](https://twitter.com/xmrig_dev)
