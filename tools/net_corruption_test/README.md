# Network corruption / checksum test

Characterises whether the firmware's network stack (lwIP + STM32 MAC) drops or
delivers corrupted UDP packets. The current config offloads RX checksum
verification to the STM32H7 ETH DMA (software checks disabled in `lwipopts.h`)
and disables IP reassembly. Use it to confirm corrupted packets are still
dropped and nothing regresses.

## Pieces

- `send_corrupt.py` — Linux host tool. Crafts raw IP+UDP packets with specific
  corruptions and checks the verdict the firmware replies with.
- Firmware side: `src/debug/checksum_test_interface.cpp`, a thread bound to UDP
  port **4500**. It validates each packet (magic / declared length / CRC32) and
  replies with a verdict. It also logs each result via `ULOG_INFO`.
  Opt-in only: build the firmware with `-DENABLE_CHECKSUM_TEST_INTERFACE` to
  compile it in. It is not present in normal firmware (not even debug builds).

## Wire format (little-endian uint32 fields)

```
request: magic("TEST") | test_id | declared_len | crc32 | payload[declared_len]
reply:   magic("RPLP") | test_id | verdict | received_total_len | computed_crc
```

`crc32` covers the whole packet except the crc32 field itself:
`CRC32(magic || test_id || declared_len || payload)`, so corruption in either
the header fields or the payload is detected.

`verdict`: 0=OK 1=BAD_MAGIC 2=LEN_MISMATCH 3=CRC_MISMATCH 4=TOO_SHORT.
No reply at all = the stack dropped the packet (expected for bad checksums).

## Running

Needs root for raw sockets:

```
sudo ./send_corrupt.py <firmware_ip>
sudo ./send_corrupt.py <firmware_ip> --only bad_ip_csum   # single case
```

Source IP, egress interface and firmware MAC are auto-detected; override with
`--src-ip` / `--iface` / `--dst-mac`. The tool listens for replies on
`--src-port` (default 54321).

Packets are injected at **layer 2** (`AF_PACKET`), not via an `IP_HDRINCL` raw
socket. This matters: an `IP_HDRINCL` socket lets the host kernel recompute the
IP header checksum (and NIC TX offload can rewrite IP/UDP checksums), which
would silently "fix" the corruptions we are trying to test — in particular
`bad_ip_csum`. Sending the full Ethernet frame ourselves puts the bytes on the
wire verbatim.

## Test cases and expected outcome (current, HW-offload config)

| case | what it sends | expected |
|------|----------------|----------|
| `baseline` | fully valid packet | `OK` |
| `bad_udp_csum` | valid payload, corrupted UDP checksum | `DROP` (MAC L4 offload) |
| `udp_csum_zero_bad_payload` | UDP checksum 0 + corrupted payload | `CRC_MISMATCH` — the hole: checksum 0 skips the check |
| `bad_ip_csum` | corrupted IP header checksum | `DROP` (MAC L3 offload) |
| `valid_csum_bad_payload` | payload ≠ crc field, UDP checksum valid | `CRC_MISMATCH` — app CRC catches it |
| `len_field_lie` | declared_len ≠ actual length | `LEN_MISMATCH` |
| `bad_magic` | wrong magic | `BAD_MAGIC` |
| `large_within_mtu` | 1400-byte payload, under MTU | `OK` |
| `max_packet_size` | UDP payload = `max_packet_size` (1472); IP total exactly 1500, single frame | `OK` (rx_len=1472) |
| `fragmented` | 1600-byte payload, IP-fragmented | `DROP` — IP reassembly is disabled |
