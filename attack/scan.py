import socket
import ssl
import re
from concurrent.futures import ThreadPoolExecutor

# Scan tuning (adjust these to speed up or slow down scanning)
TARGET_IP = "127.0.0.1"
START_PORT = 20
END_PORT = 10000
WORKERS = 200
CONNECT_TIMEOUT = 0.5
BANNER_TIMEOUT = 0.3
PROBE_TIMEOUT = 0.6


COMMON_PORT_HINTS = {
    21: "ftp",
    22: "ssh",
    23: "telnet",
    25: "smtp",
    53: "dns",
    80: "http",
    110: "pop3",
    135: "msrpc",
    139: "netbios-ssn",
    143: "imap",
    389: "ldap",
    443: "https",
    445: "smb",
    465: "smtps",
    587: "smtp",
    631: "ipp",
    6379: "redis",
    3306: "mysql",
    3389: "rdp",
    5432: "postgres",
    27017: "mongodb",
    5000: "http",
    8000: "http",
    8080: "http",
    8443: "https",
    11211: "memcached",
}


def recv_some(sock, timeout, max_bytes=4096):
    sock.settimeout(timeout)
    try:
        return sock.recv(max_bytes)
    except Exception:
        return b""


def safe_decode(b):
    return b.decode("utf-8", errors="replace")


def short_one_line(s, limit=220):
    s = re.sub(r"[\r\n\t]+", " ", s)
    s = re.sub(r"\s{2,}", " ", s).strip()
    if len(s) > limit:
        return s[:limit] + "..."
    return s


def normalize_banner(data):
    if not data:
        return "<none>"
    text = short_one_line(safe_decode(data))
    return text or "<none>"


def looks_like_http(text):
    t = text.upper()
    return ("HTTP/1." in t) or t.startswith("HTTP/") or ("SERVER:" in t)


def looks_like_ssh(text):
    return "SSH-" in text.upper()


def looks_like_tls_port(port):
    return port in {443, 8443, 465, 993, 995}


def try_tls_handshake(ip, port, timeout):
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    try:
        with socket.create_connection((ip, port), timeout=timeout) as raw:
            raw.settimeout(timeout)
            with ctx.wrap_socket(raw, server_hostname=ip):
                return True
    except Exception:
        return False


def probe_http(ip, port, timeout):
    req = f"GET / HTTP/1.1\r\nHost: {ip}\r\nUser-Agent: scan.py\r\nConnection: close\r\n\r\n"
    try:
        with socket.create_connection((ip, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(req.encode())
            data = recv_some(sock, timeout, 8192)
            text = safe_decode(data)
            if looks_like_http(text):
                return True, short_one_line(text)
    except Exception:
        pass
    return False, ""


def scan_port(target, port):
    try:
        with socket.create_connection((target, port), timeout=CONNECT_TIMEOUT) as sock:
            sock.settimeout(CONNECT_TIMEOUT)
            banner_bytes = recv_some(sock, BANNER_TIMEOUT, 512)
    except Exception:
        return

    banner = normalize_banner(banner_bytes)
    text = safe_decode(banner_bytes)

    service = COMMON_PORT_HINTS.get(port, "unknown")
    if looks_like_ssh(text):
        service = "ssh"
    elif looks_like_http(text):
        service = "http"

    if service == "unknown" and looks_like_tls_port(port):
        if try_tls_handshake(target, port, PROBE_TIMEOUT):
            service = "https/tls"

    if service == "unknown":
        ok, http_sig = probe_http(target, port, PROBE_TIMEOUT)
        if ok:
            service = "http"
            if banner == "<none>":
                banner = http_sig

    print(f"Port {port} open | banner: {banner} | service: {service}")


def scan_target(target, start_port, end_port):
    with ThreadPoolExecutor(max_workers=WORKERS) as executor:
        for port in range(start_port, end_port + 1):
            executor.submit(scan_port, target, port)


print("Start scanning...")
scan_target(TARGET_IP, START_PORT, END_PORT)
