# WiGLE 3DS (`3ds_wigle`)

> **WiFi Wardriver & Radar** — Nintendo 3DS Homebrew (`CTR-P-WIGL`, UniqueId `0x0D3E4`) by [**lafiabrew**](https://github.com/orgs/lafiabrew/repositories)

WiGLE-Wifi-1.4 CSV wardriver, 2.4 GHz Ch 1–13 spectrum analyzer, and 3DS Local Wireless (nwm::UDS) beacon radar.

## Build & Verify

```bash
# Run host C11 ASan/UBSan unit & UI simulation tests
make test

# Build ARM11 3DS .elf, .smdh, .3dsx, and .cia (via devkitARM or rootless Podman container)
make 3ds && make verify-3ds

# Serve FBI QR code or push .cia over LAN
make qr
```
