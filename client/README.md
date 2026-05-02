# P2P 文件传输客户端

基于 Qt 6 的跨平台 P2P 文件传输客户端，向 Registry Server 注册后可在局域网内直连传输文件。

## 构建

### 依赖

- CMake >= 3.16
- C++17 编译器 (GCC 9+, Clang 10+, MSVC 2019+)
- Qt 6 (Core, Network, Widgets)
- nlohmann/json (CMake FetchContent 自动获取)

### Linux (Ubuntu)

```bash
# 安装 Qt 6
sudo apt install qt6-base-dev cmake g++

# 构建
cd client
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Windows

```powershell
# 1. 安装 Qt 6（官方在线安装器: https://www.qt.io/download）
#    选择 Qt 6.x → MSVC 2019 64-bit 或 MinGW 64-bit
# 2. 安装 CMake（https://cmake.org/download/）
# 3. 安装 Visual Studio 2022 或 MinGW-w64

# 构建（MSVC）
cd client
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/msvc2019_64
cmake --build . --config Release

# 构建（MinGW）
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64
cmake --build .
```

## 启动

### 前置条件

1. 确保 Registry Server 已在局域网中运行（Docker 或本地编译启动）
2. 确保两个客户端之间网络可达（虚拟机需使用桥接模式）

### Linux

```bash
# 基本用法（P2P 端口随机分配）
./client/build/client

# 指定 P2P 端口
./client/build/client 12345
```

### Windows

```powershell
# 基本用法
.\client\build\Release\client.exe

# 指定 P2P 端口
.\client\build\Release\client.exe 12345
```

## 使用

1. 启动后 GUI 窗口打开，**不会自动注册**
2. 在配置栏填写 Registry Server 的 IP 和端口
3. 填写本机名称（默认 hostname）和 P2P 端口
4. 点击 **「注册」** 按钮，向 Registry Server 注册
5. 注册成功后左侧显示其他已注册节点
6. 在右侧文件树中选择文件/目录（Ctrl+Click 多选），或点击「选择文件」「选择文件夹」
7. 点击 **「→」** 按钮发送文件
8. 底部传输队列可实时查看进度
9. 关闭窗口前点击 **「注销」** 或在关闭时自动注销

## 接收文件默认路径

| 系统 | 路径 |
|------|------|
| Linux | `~/P2P_Received/` |
| Windows | `%USERPROFILE%\P2P_Received\` |

## 错误排查

| 问题 | 解决 |
|------|------|
| 注册失败 | 检查 Registry Server 是否运行、IP 是否正确、端口是否一致 |
| 对端列表为空 | 确保两个客户端都注册到了同一个 Registry Server |
| 发送失败（对端离线） | 检查防火墙是否放行 P2P 端口、虚拟机是否桥接模式 |
| 编译找不到 Qt6 | 设置 `CMAKE_PREFIX_PATH` 指向 Qt 安装目录 |
