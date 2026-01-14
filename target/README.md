# 远程控制软件（课程项目）

本项目实现了基于 TCP 的远程控制系统，包含客户端与服务端、JSON 协议、会话权限、登录验证以及文件上传/下载。实现重点是可验收、易调试与结构清晰，不追求生产级安全性。

## 功能概览

- TCP 可靠帧通信（4 字节长度头 + payload）
- 统一 JSON 协议（CMD/RSP）
- 会话权限：GUEST -> LOW -> HIGH（每连接独立）
- 低级登录（明文比对）、高级登录（DES-ECB + OpenSSL）
- Router 统一权限拦截
- 上传/下载（Base64 分块）
- 多客户端并发（每连接一个线程）

## 目录结构

- `common/`：协议、网络、加密、工具
- `server/`：服务端、路由、处理器、配置
- `client/`：客户端 CLI 与配置
- `build/`：构建输出（生成物）

## Quick Start（从克隆到运行）

1) 克隆仓库
```bash
git clone <your-repo-url>
cd <repo>/target
```

2) 准备配置  
根据需要编辑：
- `server/server_config.json`
- `client/client_config.json`

3) 构建
```bash
cmake -S . -B build
cmake --build build
```

4) 启动服务端
```bash
./build/Server
```

5) 启动客户端
```bash
./build/Client
```

客户端是交互式 CLI，输入 `help` 查看当前权限可用命令，输入 `exit` 仅在本地退出（不会发送协议）。

## 构建

依赖：
- CMake 3.20+
- C++17 编译器
- OpenSSL（用于 DES 与 Base64）

在 `target` 目录执行：
```bash
cmake -S . -B build
cmake --build build
```

## 运行

启动服务端：
```bash
./build/Server
```

启动客户端：
```bash
./build/Client
```

## 协议摘要

请求：
```json
{ "type": "CMD", "cmd": "PING", "args": {} }
```

响应：
```json
{ "type": "RSP", "ok": true, "code": 0, "msg": "pong", "data": {} }
```

## Socket 通信原理（简述）

本项目基于 TCP 字节流，为避免粘包/拆包，采用“长度前置帧”协议：
- 发送时：先发送 4 字节长度（网络序），再发送 payload
- 接收时：先可靠收满 4 字节长度，再按长度收满 payload
- sendAll/recvAll 保证一次消息完整收发，避免 TCP 不确定分片

这样上层逻辑可以按“消息”处理，而无需关心字节流分段细节。

常用错误码：
- 0 OK
- 1001 BAD_REQUEST
- 1002 UNKNOWN_CMD
- 1003 NOT_LOGIN
- 1004 NO_PERMISSION
- 1500 INTERNAL_ERROR
- 2001 FILE_EXISTS
- 2002 FILE_NOT_FOUND
- 2003 TRANSFER_STATE_ERROR
- 2004 SIZE_MISMATCH

## 登录与权限

- 初始为 GUEST
- `LOGIN_LOW` 成功进入 LOW
- `LOGIN_HIGH` 必须在 LOW 状态下执行，成功进入 HIGH
- `LOGOUT`：LOW -> GUEST，HIGH -> LOW

高级登录使用 DES-ECB（PKCS#7 padding）进行“密文比对”，仅满足课程要求，不具备真实安全性。

## 文件传输（HIGH）

上传：
- `UPLOAD_INIT`：文件名/大小/分块大小
- `UPLOAD_CHUNK`：Base64 数据
- `UPLOAD_FINISH`：结束并落盘

下载：
- `DOWNLOAD_INIT`
- `DOWNLOAD_CHUNK`
- `DOWNLOAD_ABORT`（下载失败时清理会话）

服务端会限制单文件大小与分块大小，并对文件名做安全过滤，防止路径穿越。

## 配置文件

服务端：`target/server/server_config.json`
- `bind_ip`：绑定地址
- `port`：监听端口
- `low_users`：LOW 账号列表（明文）
- `admin_user` / `admin_pass_plain`
- `des_key_hex`：16 位 hex（8 bytes）
- `storage_dir`：文件存储目录
- `max_file_size` / `max_chunk_bytes`
- `overwrite`：reject | overwrite | rename

客户端：`target/client/client_config.json`
- `des_key_hex`：必须与服务端一致

示例（占位符）：
```json
{
  "bind_ip": "<bind_ip>",
  "port": "<port>",
  "low_users": [{ "username": "<user>", "password": "<pass>" }],
  "admin_user": "<admin_user>",
  "admin_pass_plain": "<admin_pass>",
  "des_key_hex": "<16-hex-chars>",
  "storage_dir": "<dir>",
  "max_file_size": "<bytes>",
  "max_chunk_bytes": "<bytes>",
  "overwrite": "<reject|overwrite|rename>"
}
```

## 客户端命令（按权限显示）

GUEST：
- help
- ping
- echo <text>
- login_low <user> <pass>

LOW：
- time
- whoami
- login_high <user> <pass_plain>
- logout

HIGH：
- admin_ping
- check
- upload <local_path> [remote_name]
- download <remote_name> <local_path>
- logout

说明：
- `exit` 为客户端本地退出命令
- HELP 输出会按当前权限过滤

## 并发模型

- 服务端采用“一连接一线程”
- 每个连接独立 Session（登录态、上传/下载状态互不影响）
- 允许同账号多客户端同时登录

## 安全说明（课程视角）

- DES-ECB 与明文密码仅用于课程要求
- 未实现 TLS/挑战响应/抗重放
- 重点是协议结构与功能完整性

## 验收建议（简要）

1) GUEST：PING / ECHO / HELP
2) LOW：LOGIN_LOW 后执行 TIME / WHOAMI
3) HIGH：LOGIN_HIGH 后执行 ADMIN_PING
4) 上传 5MB 文件并验证服务端落盘大小
5) 下载已上传文件并验证大小一致
6) 多客户端并发操作与同账号并发登录

