# attack 脚本说明

本目录包含两个用于与靶机交互的小脚本：

- `attack.py`：建立连接并发送 JSON 帧，完成登录与调试载荷投递。
- `scan.py`：多线程端口扫描、抓取 banner，并做基础服务探测。

## 协议与登录原理

目标服务使用“长度前置帧”：

- 4 字节网络序长度
- JSON payload（如 `{"type":"CMD","cmd":"...","args":{...}}`）

`attack.py` 负责生成 JSON 并加上 4 字节长度头发送。服务端会在连接建立后先发
`PWNREMOTE/1.0 READY`，因此脚本会先丢弃该 banner，避免被误当作长度头。

低级登录示例：

```
LOGIN_LOW: {"username":"user","password":"123456"}
```

高级登录（调试用）示例：

```
LOGIN_HIGH: {"username":"admin","password_cipher_hex":"<payload>"}
```

`attack.py` 里为了方便观察服务端收到的内容，直接把 32 个 `A` 作为
`password_cipher_hex` 发送，不做额外加密。

## 扫描原理

`scan.py` 的流程：

- 连接端口并读取 banner（如有）
- 通过 banner 判断常见服务（SSH/HTTP）
- 对常见 TLS 端口尝试握手
- 无 banner 时发起 HTTP 探针

最终打印：端口、banner、服务名；无法识别则输出 `unknown`。

## 攻击原理（靶场版）

靶场版 `ServerDemo` 的高级登录使用 `strcpy` 把 `password_cipher_hex` 拷贝到
固定大小的栈缓冲区。若输入超长，会覆盖相邻的 `admin` 变量，使其变为非 0，
从而绕过权限检查。这是课程演示用的提权路径。

## Quick Start

1) 启动靶场服务：

```
cd target
./build/ServerDemo
```

2) 扫描并查看 banner：

```
cd attack
python scan.py
```

3) 发送登录请求：

```
cd attack
python attack.py
```

说明：

- `attack.py` 当前为调试脚本，不做加密。
- 若要触发溢出，需把 `password_cipher_hex` 改成更长的字符串。
