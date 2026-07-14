#!/usr/bin/env python3
"""
Network corruption / checksum test tool.

Sends hand-crafted UDP packets (raw socket, full IP+UDP control) to the
firmware's checksum test thread and checks the verdict it replies with. Used to
characterise whether the network stack (lwIP + STM32 MAC) drops or delivers
corrupted packets, before and after enabling hardware checksumming / D-cache.

Wire format must match src/debug/checksum_test_interface.cpp:

  request  payload: magic("TEST") | test_id | declared_len | crc32 | payload[declared_len]
  reply    payload: magic("RPLP") | test_id | verdict | received_total_len | computed_crc

  all fields little-endian uint32.

Requires root (raw sockets):  sudo ./send_corrupt.py <firmware_ip>
"""

import argparse
import binascii
import fcntl
import socket
import struct
import sys
import time

TEST_PORT = 4500
TEST_MAGIC = 0x54534554  # "TEST"
REPLY_MAGIC = 0x504C5052  # "RPLP"

TEST_HEADER_LEN = 16  # magic + test_id + declared_len + crc32
# Must match xbot::config::max_packet_size: the largest UDP payload that fits a
# single unfragmented Ethernet frame, MTU(1500) - IP(20) - UDP(8).
MAX_PACKET_SIZE = 1472

VERDICT_NAMES = {
    0: "OK",
    1: "BAD_MAGIC",
    2: "LEN_MISMATCH",
    3: "CRC_MISMATCH",
    4: "TOO_SHORT",
}
DROP = "DROP"  # expected outcome: stack swallows the packet, no reply


# ---------------------------------------------------------------------------
# checksum helpers
# ---------------------------------------------------------------------------
def inet_checksum(data: bytes) -> int:
    """Standard 16-bit one's-complement Internet checksum (RFC 1071)."""
    if len(data) % 2:
        data += b"\x00"
    total = 0
    for i in range(0, len(data), 2):
        total += (data[i] << 8) | data[i + 1]
    total = (total & 0xFFFF) + (total >> 16)
    total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def build_ip_header(src_ip, dst_ip, payload_len, ident, *, frag_off=0, mf=False, bad_csum=False):
    ver_ihl = (4 << 4) | 5
    tos = 0
    total_len = 20 + payload_len
    flags_frag = ((0x1 if mf else 0x0) << 13) | (frag_off & 0x1FFF)
    ttl = 64
    proto = socket.IPPROTO_UDP
    src = socket.inet_aton(src_ip)
    dst = socket.inet_aton(dst_ip)

    hdr = struct.pack("!BBHHHBBH4s4s", ver_ihl, tos, total_len, ident,
                      flags_frag, ttl, proto, 0, src, dst)
    csum = inet_checksum(hdr)
    if bad_csum:
        csum ^= 0xFFFF  # guaranteed-wrong value
    hdr = struct.pack("!BBHHHBBH4s4s", ver_ihl, tos, total_len, ident,
                      flags_frag, ttl, proto, csum, src, dst)
    return hdr


def build_udp(src_ip, dst_ip, src_port, dst_port, udp_payload, *, csum_mode="correct"):
    length = 8 + len(udp_payload)
    hdr = struct.pack("!HHHH", src_port, dst_port, length, 0)
    if csum_mode == "zero":
        csum = 0
    else:
        pseudo = socket.inet_aton(src_ip) + socket.inet_aton(dst_ip) + \
                 struct.pack("!BBH", 0, socket.IPPROTO_UDP, length)
        csum = inet_checksum(pseudo + hdr + udp_payload)
        if csum == 0:
            csum = 0xFFFF  # 0 means "no checksum"; never emit it for a real one
        if csum_mode == "bad":
            csum ^= 0xFFFF
    return struct.pack("!HHHH", src_port, dst_port, length, csum) + udp_payload


def make_test_payload(test_id, payload, *, declared_len=None, crc_payload=None, magic=TEST_MAGIC):
    """Build the application-layer test packet (header + payload).

    The crc32 field covers the whole packet except the field itself:
    CRC32(magic || test_id || declared_len || payload). crc_payload overrides
    the bytes fed into the CRC (used to send a stale CRC over a mutated payload).
    """
    if declared_len is None:
        declared_len = len(payload)
    crc_body = payload if crc_payload is None else crc_payload
    crc_input = struct.pack("<III", magic, test_id, declared_len) + crc_body
    crc = binascii.crc32(crc_input) & 0xFFFFFFFF
    header = struct.pack("<IIII", magic, test_id, declared_len, crc)
    return header + payload


def pattern(n):
    return bytes(i & 0xFF for i in range(n))


# ---------------------------------------------------------------------------
# test cases
# ---------------------------------------------------------------------------
# Each builder returns a list of raw IP packets (bytes) to send on the wire.
# Most cases are a single packet; fragmentation returns two.
def case_baseline(ctx):
    tp = make_test_payload(ctx.test_id, pattern(64))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_bad_udp_csum(ctx):
    tp = make_test_payload(ctx.test_id, pattern(64))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp, csum_mode="bad")
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_udp_csum_zero_bad_payload(ctx):
    # UDP checksum disabled (0); payload corrupted but crc field NOT updated.
    good = pattern(64)
    corrupt = bytearray(good)
    corrupt[10] ^= 0xFF
    tp = make_test_payload(ctx.test_id, bytes(corrupt), crc_payload=good)
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp, csum_mode="zero")
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_bad_ip_csum(ctx):
    tp = make_test_payload(ctx.test_id, pattern(64))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident, bad_csum=True) + udp]


def case_valid_csum_bad_payload(ctx):
    # Payload differs from what the crc field claims, but UDP checksum is valid
    # for the bytes on the wire -> stack delivers, app-layer CRC must catch it.
    good = pattern(64)
    corrupt = bytearray(good)
    corrupt[20] ^= 0xFF
    tp = make_test_payload(ctx.test_id, bytes(corrupt), crc_payload=good)
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_len_field_lie(ctx):
    # declared_len says 100, but only 50 payload bytes are actually sent.
    payload = pattern(50)
    tp = make_test_payload(ctx.test_id, payload, declared_len=100)
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_bad_magic(ctx):
    tp = make_test_payload(ctx.test_id, pattern(64), magic=0xDEADBEEF)
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_large_within_mtu(ctx):
    # Total IP packet stays under 1500: 1400 payload + headers.
    tp = make_test_payload(ctx.test_id, pattern(1400))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_max_packet_size(ctx):
    # Exactly xbot::config::max_packet_size of UDP payload. This is the largest
    # packet that fits a single, unfragmented Ethernet frame:
    #   UDP payload  = MAX_PACKET_SIZE (1472)
    #   IP total     = 20 + 8 + 1472 = 1500 = MTU  -> one frame, no fragmentation
    app_payload = MAX_PACKET_SIZE - TEST_HEADER_LEN
    tp = make_test_payload(ctx.test_id, pattern(app_payload))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)
    assert len(udp) - 8 == MAX_PACKET_SIZE  # UDP payload == max_packet_size
    return [build_ip_header(ctx.src_ip, ctx.dst_ip, len(udp), ctx.ident) + udp]


def case_fragmented(ctx):
    # Build one big UDP datagram, then IP-fragment it into two on-wire fragments.
    # Tests reassembly (and, after HW checksum is enabled, whether the MAC's
    # L4 offload copes with fragments).
    tp = make_test_payload(ctx.test_id, pattern(1600))
    udp = build_udp(ctx.src_ip, ctx.dst_ip, ctx.src_port, TEST_PORT, tp)

    # Split the UDP datagram. IP fragment offsets count in 8-byte units, so the
    # first fragment's payload length must be a multiple of 8.
    split = 1480  # multiple of 8, fits in MTU with 20-byte IP header
    frag1 = udp[:split]
    frag2 = udp[split:]
    pkts = [
        build_ip_header(ctx.src_ip, ctx.dst_ip, len(frag1), ctx.ident, frag_off=0, mf=True) + frag1,
        build_ip_header(ctx.src_ip, ctx.dst_ip, len(frag2), ctx.ident, frag_off=split // 8, mf=False) + frag2,
    ]
    return pkts


TEST_CASES = [
    ("baseline", case_baseline, "OK"),
    ("bad_udp_csum", case_bad_udp_csum, DROP),
    ("udp_csum_zero_bad_payload", case_udp_csum_zero_bad_payload, "CRC_MISMATCH"),
    ("bad_ip_csum", case_bad_ip_csum, DROP),
    ("valid_csum_bad_payload", case_valid_csum_bad_payload, "CRC_MISMATCH"),
    ("len_field_lie", case_len_field_lie, "LEN_MISMATCH"),
    ("bad_magic", case_bad_magic, "BAD_MAGIC"),
    ("large_within_mtu", case_large_within_mtu, "OK"),
    ("max_packet_size", case_max_packet_size, "OK"),
    ("fragmented", case_fragmented, DROP),
]


# ---------------------------------------------------------------------------
# runner
# ---------------------------------------------------------------------------
class Ctx:
    def __init__(self, src_ip, dst_ip, src_port, test_id, ident):
        self.src_ip = src_ip
        self.dst_ip = dst_ip
        self.src_port = src_port
        self.test_id = test_id
        self.ident = ident


def detect_src_ip(dst_ip):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect((dst_ip, TEST_PORT))
        return s.getsockname()[0]
    finally:
        s.close()


# --- L2 (AF_PACKET) helpers -------------------------------------------------
# We send at layer 2 so the host kernel does not "fix up" our deliberately bad
# IP/UDP checksums (IP_HDRINCL raw sockets let the kernel recompute the IP
# header checksum, and NIC TX offload can rewrite both). AF_PACKET SOCK_RAW
# transmits the frame verbatim.
SIOCGIFADDR = 0x8915
SIOCGIFHWADDR = 0x8927
ETH_P_IP = 0x0800


def _ifreq(ifname):
    return struct.pack("256s", ifname.encode()[:15])


def get_if_addr(ifname):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        return socket.inet_ntoa(fcntl.ioctl(s.fileno(), SIOCGIFADDR, _ifreq(ifname))[20:24])
    finally:
        s.close()


def get_if_hwaddr(ifname):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        return fcntl.ioctl(s.fileno(), SIOCGIFHWADDR, _ifreq(ifname))[18:24]
    finally:
        s.close()


def find_iface_for_ip(src_ip):
    for _, name in socket.if_nameindex():
        try:
            if get_if_addr(name) == src_ip:
                return name
        except OSError:
            continue
    raise RuntimeError(f"no interface found for source IP {src_ip}")


def resolve_dst_mac(dst_ip, iface):
    # Prime the ARP cache with a throwaway datagram, then read /proc/net/arp.
    # Scope the lookup to `iface`: on a multi-homed host the same IP could have
    # a stale/foreign entry on another device, which would yield the wrong MAC.
    p = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        p.sendto(b"\x00", (dst_ip, 9))  # discard port
    except OSError:
        pass
    finally:
        p.close()
    # /proc/net/arp columns: IP address, HW type, Flags, HW address, Mask, Device
    for _ in range(20):
        with open("/proc/net/arp") as f:
            for line in f.read().splitlines()[1:]:
                cols = line.split()
                if len(cols) >= 6 and cols[0] == dst_ip and cols[5] == iface \
                        and cols[3] != "00:00:00:00:00:00":
                    return bytes(int(x, 16) for x in cols[3].split(":"))
        time.sleep(0.1)
    raise RuntimeError(f"could not resolve MAC for {dst_ip} on {iface} (is it reachable?)")


def mac_str(mac):
    return ":".join(f"{b:02x}" for b in mac)


def main():
    ap = argparse.ArgumentParser(description="Send corrupted UDP packets to the firmware checksum tester.")
    ap.add_argument("dst_ip", help="firmware IP address")
    ap.add_argument("--src-ip", help="source IP to use (default: auto-detect)")
    ap.add_argument("--iface", help="egress interface (default: auto-detect from src IP)")
    ap.add_argument("--dst-mac", help="firmware MAC aa:bb:.. (default: resolve via ARP)")
    ap.add_argument("--src-port", type=int, default=54321, help="source UDP port (reply listener)")
    ap.add_argument("--timeout", type=float, default=1.0, help="seconds to wait for a reply")
    ap.add_argument("--only", help="run only the named test case")
    args = ap.parse_args()

    if args.only:
        known = [name for name, _, _ in TEST_CASES]
        if args.only not in known:
            sys.exit(f"unknown test case '{args.only}'; known cases: {', '.join(known)}")

    src_ip = args.src_ip or detect_src_ip(args.dst_ip)
    iface = args.iface or find_iface_for_ip(src_ip)
    src_mac = get_if_hwaddr(iface)
    if args.dst_mac:
        dst_mac = bytes(int(x, 16) for x in args.dst_mac.split(":"))
    else:
        dst_mac = resolve_dst_mac(args.dst_ip, iface)
    eth_hdr = dst_mac + src_mac + struct.pack("!H", ETH_P_IP)

    print(f"iface {iface}  src {src_ip}:{args.src_port} ({mac_str(src_mac)})")
    print(f"  ->  dst {args.dst_ip}:{TEST_PORT} ({mac_str(dst_mac)})\n")

    try:
        raw = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
        raw.bind((iface, 0))
    except PermissionError:
        sys.exit("AF_PACKET socket needs root: run with sudo")

    rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    rx.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    rx.bind((src_ip, args.src_port))
    rx.settimeout(args.timeout)

    passed = failed = 0
    for test_id, (name, builder, expected) in enumerate(TEST_CASES, start=1):
        if args.only and name != args.only:
            continue
        ctx = Ctx(src_ip, args.dst_ip, args.src_port, test_id, ident=0x4000 + test_id)

        # Drain any stale replies.
        rx.setblocking(False)
        try:
            while True:
                rx.recv(2048)
        except (BlockingIOError, socket.error):
            pass
        rx.setblocking(True)
        rx.settimeout(args.timeout)

        for pkt in builder(ctx):
            frame = eth_hdr + pkt
            if len(frame) < 60:  # pad to Ethernet minimum frame size
                frame += b"\x00" * (60 - len(frame))
            raw.send(frame)

        actual = DROP
        deadline = time.time() + args.timeout
        while time.time() < deadline:
            try:
                data, _ = rx.recvfrom(2048)
            except socket.timeout:
                break
            if len(data) < 20:
                continue
            magic, rid, verdict, rlen, ccrc = struct.unpack("<IIIII", data[:20])
            if magic != REPLY_MAGIC or rid != test_id:
                continue
            actual = VERDICT_NAMES.get(verdict, f"verdict_{verdict}")
            actual_detail = f"{actual} (rx_len={rlen})"
            break
        else:
            actual_detail = DROP
        if actual == DROP:
            actual_detail = DROP

        ok = actual == expected
        passed += ok
        failed += not ok
        flag = "PASS" if ok else "FAIL"
        print(f"[{flag}] {name:28s} expected={expected:14s} actual={actual_detail}")

    print(f"\n{passed} passed, {failed} failed")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
