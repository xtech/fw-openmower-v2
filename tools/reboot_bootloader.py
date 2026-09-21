#!/usr/bin/env python3
"""Repeat network bootloader reboots, logging timings until a 60-second timeout.

Example: python3 tools/reboot_bootloader.py -i enx00e04c309dad --csv reboot-times.csv

Detect the target from the first application advertisement on the selected
interface and keep that target for the run. Send RESET, wait for a bootloader
broadcast, then wait for the application again. If the application advertises
while waiting for the bootloader, log the failed reset and retry. Receiving a
bootloader broadcast is mandatory before a cycle can succeed. Send one discovery
request three seconds after each RESET, then BOOT over TCP once confirmed, to
skip the bootloader waiting period. Application detection remains passive.

Linux; no Python dependencies or root required. The board's wire command is
RESET, not the literal word "reboot". Stop on timeout without resetting again so
the failure can be inspected with J-Link.
"""

import argparse
import csv
from datetime import datetime, timezone
import fcntl
from pathlib import Path
import select
import socket
import struct
import sys
import time


PORT = 8007
TIMEOUT = 60.0
ADVERTISEMENT_GROUP = "233.255.255.0"
ADVERTISEMENT_PORT = 4242
BOOTLOADER = b"BOARD_ADVERTISEMENT:xcore-boot"
APPLICATION = BOOTLOADER + b";application-mode"
# Matches xbot/datatypes/XbotHeader.hpp on the little-endian STM32.
HEADER = struct.Struct("<BBBBHBBHHQI")
IP_PKTINFO = getattr(socket, "IP_PKTINFO", 8)  # Linux in_pktinfo
FIELDS = ("cycle", "started_at_utc", "status", "bootloader_seconds", "elapsed_seconds", "detail")


def interface_address(sock, interface):
    """Read the chosen Linux interface's IPv4 address without changing it."""
    name = interface.encode()
    if not name or len(name) >= 16:
        raise ValueError("Ethernet interface name must be 1–15 bytes")
    socket.if_nametoindex(interface)  # Validate before ioctl's fixed-width field.
    result = fcntl.ioctl(sock.fileno(), 0x8915, struct.pack("256s", name))  # SIOCGIFADDR
    return socket.inet_ntoa(result[20:24])


def receive_stage(listeners, target, timeout, interface_index=None):
    """Return (stage, sender IP), filtering to the chosen interface and target."""
    for sock in select.select(listeners, [], [], max(0, timeout))[0]:
        data, ancillary, _, peer = sock.recvmsg(65535, socket.CMSG_SPACE(12))
        if interface_index is not None:
            received_interface = next(
                (struct.unpack_from("=I", value)[0] for level, kind, value in ancillary
                 if level == socket.IPPROTO_IP and kind == IP_PKTINFO and len(value) >= 4),
                None,
            )
            if received_interface != interface_index:
                continue
        if target is not None and peer[0] != target:
            continue
        if sock.getsockname()[1] == PORT:
            if data.strip() == BOOTLOADER:
                return "bootloader", peer[0]
            if data.strip() == APPLICATION:
                return "application", peer[0]
        elif len(data) >= HEADER.size:
            header = HEADER.unpack_from(data)
            if (header[0] == 1 and header[1] == 0x80 and header[-1] > 0
                    and len(data) == HEADER.size + header[-1]):
                return "application", peer[0]
    return None


def drain(listeners):
    """Discard queued announcements before sending a fresh reset request."""
    for sock in listeners:
        sock.setblocking(False)
        try:
            while sock.recvfrom(65535):
                pass
        except BlockingIOError:
            pass
        finally:
            sock.setblocking(True)


def boot_application(target, local_ip, deadline):
    """Request immediate boot; the following application broadcast proves success."""
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise TimeoutError("Boot cycle deadline expired")
    with socket.create_connection(
        (target, PORT), timeout=min(5.0, remaining), source_address=(local_ip, 0)
    ) as sock, sock.makefile("rb") as stream:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("Boot cycle deadline expired")
            sock.settimeout(min(5.0, remaining))
            line = stream.readline(1024)
            if not line:
                raise RuntimeError("Bootloader closed connection without accepting BOOT")
            if not line.endswith(b"\n"):
                raise RuntimeError("Invalid bootloader command response")
            if line == b"SEND COMMAND\n":
                sock.sendall(b"BOOT\n")
            elif line == b"BOOT REQUESTED\n":
                # MCU reset destroys the TCP connection without sending FIN.
                return
            elif line == b"BOOT FAILED\n":
                raise RuntimeError("Bootloader rejected the application image")
            elif line == b"SEND HASH\n":
                raise RuntimeError("Installed bootloader does not support BOOT")


def run(csv_path, interface=None):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reset_sock, socket.socket(
        socket.AF_INET, socket.SOCK_DGRAM
    ) as boot_sock, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as app_sock:
        local_ip = interface_address(reset_sock, interface) if interface else "0.0.0.0"
        interface_index = socket.if_nametoindex(interface) if interface else None
        reset_sock.bind((local_ip, 0))
        for sock, address, port in (
            (boot_sock, "", PORT),
            (app_sock, ADVERTISEMENT_GROUP, ADVERTISEMENT_PORT),
        ):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            if interface_index is not None:
                sock.setsockopt(socket.IPPROTO_IP, IP_PKTINFO, 1)
            sock.bind((address, port))
        app_sock.setsockopt(
            socket.IPPROTO_IP,
            socket.IP_ADD_MEMBERSHIP,
            socket.inet_aton(ADVERTISEMENT_GROUP) + socket.inet_aton(local_ip),
        )
        listeners = (boot_sock, app_sock)
        print(f"Listening via {interface or 'default interface'} ({local_ip}) for firmware advertisements", flush=True)
        with csv_path.open("a+", newline="") as output:
            output.seek(0)
            existing_header = next(csv.reader(output), None)
            if existing_header is not None and existing_header != list(FIELDS):
                raise ValueError(f"{csv_path} has an incompatible CSV header")
            output.seek(0, 2)
            writer = csv.DictWriter(output, fieldnames=FIELDS)
            if existing_header is None:
                writer.writeheader()
                output.flush()

            cycle = 0
            target = None
            while True:
                cycle += 1
                started = time.monotonic()
                row = dict.fromkeys(FIELDS, "")
                row.update(cycle=cycle, started_at_utc=datetime.now(timezone.utc).isoformat())

                def log(status, detail=""):
                    row.update(status=status, detail=detail, elapsed_seconds=f"{time.monotonic() - started:.3f}")
                    writer.writerow(row)
                    output.flush()
                    print(f"  {status}: {row['elapsed_seconds']}s {detail}", flush=True)

                phase = "initial application"
                try:
                    if target is None:
                        print("Waiting for application broadcast...", flush=True)
                        deadline = time.monotonic() + TIMEOUT
                        while time.monotonic() < deadline:
                            announcement = receive_stage(
                                listeners, None, deadline - time.monotonic(), interface_index
                            )
                            if announcement is not None and announcement[0] == "application":
                                target = announcement[1]
                                reset_sock.connect((target, PORT))
                                local_ip = reset_sock.getsockname()[0]
                                print(f"Detected firmware at {target}", flush=True)
                                break
                        if target is None:
                            raise TimeoutError("No initial application broadcast within 60 seconds")

                    drain(listeners)
                    started = time.monotonic()
                    row["started_at_utc"] = datetime.now(timezone.utc).isoformat()
                    deadline = started + TIMEOUT
                    print(f"Cycle {cycle}: RESET -> bootloader -> application", flush=True)
                    reset_sock.send(b"RESET")
                    last_reset = started
                    discovery_at = last_reset + 3.0
                    phase = "bootloader"
                    completed = False
                    while time.monotonic() < deadline:
                        now = time.monotonic()
                        if phase == "bootloader" and discovery_at is not None and now >= discovery_at:
                            reset_sock.send(b"DISCOVER_REQUEST")
                            discovery_at = None
                        wake_at = min(deadline, discovery_at) if discovery_at is not None else deadline
                        announcement = receive_stage(
                            listeners, target, wake_at - time.monotonic(), interface_index
                        )
                        stage = announcement[0] if announcement is not None else None
                        now = time.monotonic()
                        if now >= deadline:
                            break
                        if phase == "bootloader":
                            if stage == "bootloader":
                                row["bootloader_seconds"] = f"{now - started:.3f}"
                                print(f"  Bootloader broadcast after {now - started:.3f}s", flush=True)
                                discovery_at = None
                                boot_application(target, local_ip, deadline)
                                phase = "application"
                            elif stage == "application" and now - last_reset >= 1.0:
                                # Ignore packets already in flight at RESET and
                                # avoid a burst of retries from multiple services.
                                log("reboot_retry", "Application still advertising before bootloader; retrying RESET")
                                drain(listeners)
                                reset_sock.send(b"RESET")
                                last_reset = time.monotonic()
                                discovery_at = last_reset + 3.0
                        elif stage == "application":
                            log("ok")
                            completed = True
                            break
                    if not completed:
                        raise TimeoutError(f"No {phase} broadcast within 60 seconds of RESET")
                except TimeoutError as exc:
                    log("timeout", str(exc))
                    return 1
                except (OSError, RuntimeError) as exc:
                    log("error", str(exc))
                    return 1
                except KeyboardInterrupt:
                    log("interrupted", f"Stopped by user while waiting for {phase}")
                    return 130


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-i", "--interface", help="Ethernet interface, e.g. enx00e04c309dad")
    parser.add_argument("--csv", type=Path, default=Path("reboot-times.csv"), help="Append results here")
    args = parser.parse_args()
    try:
        return run(args.csv, args.interface)
    except (OSError, ValueError) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
