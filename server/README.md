# Peer Registry Server

基于 TCP 的轻量级 P2P 节点注册中心，用于 P2P 文件传输系统中节点信息的注册、查询和注销。

## 消息协议

```
┌──────────────────────┬────────────────────────┐
│  4 bytes (大端序)     │     N bytes            │
│  负载长度 N            │     UTF-8 JSON         │
└──────────────────────┴────────────────────────┘
```

### register — 注册/更新节点

```
C→S: {"type":"register","ip":"192.168.1.100","port":12345,"name":"win11-node"}
S→C: {"type":"register_ack","peers":[{...},{...}]}
```

- `ip`: P2P 可达地址（客户端通过 `getsockname()` 自发现）
- `port`: P2P 文件传输监听端口
- `name`: 节点名称
- 返回：当前所有已注册节点列表
- 重复注册同 `ip:port` 视为更新

### query — 查询所有节点

```
C→S: {"type":"query"}
S→C: {"type":"query_ack","peers":[{...}]}
```

### unregister — 注销节点

```
C→S: {"type":"unregister","ip":"192.168.1.100","port":12345}
S→C: {"type":"unregister_ack","peers":[]}
```

## 构建

```bash
cd server
mkdir build && cd build
cmake ..
make -j
```

## 运行

```bash
./server [port]          # 默认端口 8888
./server 9999            # 自定义端口
```

## Docker

```bash
# 构建并启动
docker compose up -d --build

# 查看日志
docker compose logs -f

# 停止
docker compose down
```

## 技术栈

- C++17
- epoll (单线程事件循环，非阻塞 I/O)
- [nlohmann/json](https://github.com/nlohmann/json) (自动获取)
- CMake
