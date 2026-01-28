# iAP2协议栈架构说明

## 分层结构

iAP2协议栈采用清晰的分层架构，从底层到上层依次为：

```
┌─────────────────────────────────────────┐
│         应用层 (Application)             │
│    - 业务逻辑                            │
│    - EA数据处理                          │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│       会话层 (iAP2Session)               │
│    - Control Session管理                 │
│    - EA Session管理                      │
│    - 认证流程 (Authentication)           │
│    - 识别流程 (Identification)           │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│       Link层 (iAP2Link)                  │
│    - 数据包管理                          │
│    - 可靠传输 (ACK/重传)                 │
│    - 流量控制                            │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│     传输层 (iAP2Transport)               │
│    - 物理传输封装                        │
│    - 蓝牙/USB/UART适配                   │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│       物理层 (Physical)                  │
│    - 蓝牙SPP/BLE                         │
│    - USB                                 │
│    - UART                                │
└─────────────────────────────────────────┘
```

## 各层职责

### 1. 传输层 (iAP2Transport)

**文件位置**: `iAP2Transport/`

**职责**:
- 封装物理传输（蓝牙、USB、UART等）
- 管理Link层（iAP2LinkRunLoop）
- 提供统一的数据收发接口
- 不处理会话逻辑

**主要接口**:
- `iAP2TransportCreate()` - 创建传输层实例
- `iAP2TransportConnect()` - 通知物理连接建立
- `iAP2TransportReceiveData()` - 接收物理层数据
- `iAP2TransportSendData()` - 发送数据到指定会话
- `iAP2TransportProcess()` - 处理传输层任务

**蓝牙适配层**:
- `iAP2BluetoothTransport` 是通用传输层的简单封装
- 提供蓝牙特定的接口，内部调用通用传输层

### 2. Link层 (iAP2Link)

**文件位置**: `iAP2Link/`

**职责**:
- 数据包的封装和解析
- 可靠传输（ACK、重传机制）
- 流量控制（窗口管理）
- SYN/SYN-ACK协商

**特点**:
- 由传输层内部管理
- 应用层不直接操作Link层

### 3. 会话层 (iAP2Session)

**文件位置**: `iAP2Session/`

**职责**:
- 管理Control Session（控制会话）
- 管理EA Session（外部附件会话）
- 处理认证流程（Authentication）
- 处理识别流程（Identification）
- 路由会话数据到应用层

**主要接口**:
- `iAP2SessionCreate()` - 创建会话管理器
- `iAP2SessionStart()` - 启动会话管理器
- `iAP2SessionSendData()` - 发送会话数据
- `iAP2SessionProcess()` - 处理会话任务
- `iAP2SessionIsReady()` - 检查会话是否就绪

**Control Session**:
- 会话ID: 0x0A
- 类型: kIAP2SessionTypeControl
- 用途: 认证、识别、控制消息

**EA Session**:
- 类型: kIAP2SessionTypeEA
- 用途: 应用业务数据传输

### 4. 应用层 (Application)

**职责**:
- 实现具体业务逻辑
- 处理EA会话数据
- 响应认证和识别事件

## 数据流向

### 接收数据流

```
物理层 (蓝牙SPP)
    ↓ 原始字节流
传输层 (iAP2Transport)
    ↓ 解析数据包
Link层 (iAP2Link)
    ↓ 提取会话数据
会话层 (iAP2Session)
    ↓ 路由到对应会话
应用层 (Application)
```

### 发送数据流

```
应用层 (Application)
    ↓ 会话数据
会话层 (iAP2Session)
    ↓ 添加会话ID
Link层 (iAP2Link)
    ↓ 封装数据包
传输层 (iAP2Transport)
    ↓ 原始字节流
物理层 (蓝牙SPP)
```

## 使用示例

### 1. 初始化协议栈

```c
/* 创建蓝牙传输层 */
iAP2BTConfig_t btConfig = {
    .sendDataCB = MyBluetoothSend,
    .connectionChangeCB = MyBluetoothConnectionChange,
    .userContext = &myContext,
    .maxPacketSize = 1024,
    .maxOutstanding = 1,
    .retransmitTimeout = 1000
};
iAP2BTTransport_t* btTransport = iAP2BTTransportCreate(&btConfig);

/* 创建会话管理器 */
iAP2SessionConfig_t sessionConfig = {
    .transport = (iAP2Transport_t*)btTransport,
    .sessionDataCB = MySessionDataHandler,
    .authCompleteCB = MyAuthCompleteHandler,
    .identifyCompleteCB = MyIdentifyCompleteHandler,
    .context = &myContext,
    .deviceName = "My Device",
    .manufacturer = "My Company"
};
iAP2Session_t* session = iAP2SessionCreate(&sessionConfig);

/* 启动会话管理器 */
iAP2SessionStart(session);
```

### 2. 蓝牙连接事件

```c
void OnBluetoothConnected(void)
{
    /* 通知传输层蓝牙已连接 */
    iAP2BTTransportConnect(btTransport);
}

void OnBluetoothDataReceived(const uint8_t* data, uint32_t len)
{
    /* 将数据交给传输层处理 */
    iAP2BTTransportReceiveData(btTransport, data, len);
}
```

### 3. 主循环处理

```c
void MainLoop(void)
{
    while (running) {
        /* 处理传输层任务 */
        iAP2BTTransportProcess(btTransport);
        
        /* 处理会话层任务 */
        iAP2SessionProcess(session);
        
        /* 其他应用任务... */
    }
}
```

### 4. 发送EA数据

```c
void SendEAData(uint8_t eaSessionID, const uint8_t* data, uint32_t len)
{
    /* 检查会话是否就绪 */
    if (iAP2SessionIsReady(session)) {
        iAP2SessionSendData(session, eaSessionID, data, len);
    }
}
```

## 关键设计原则

1. **职责分离**: 每一层只负责自己的功能，不越界处理其他层的逻辑

2. **向上抽象**: 下层为上层提供简洁的接口，隐藏实现细节

3. **回调机制**: 使用回调函数实现层间通信，保持松耦合

4. **状态管理**: 每一层独立管理自己的状态

5. **错误处理**: 错误在发生层处理，必要时向上传递

## 注意事项

1. **会话注册**: Control Session由会话层自动注册，EA Session需要应用层注册

2. **认证流程**: 认证由会话层自动处理，应用层只需响应认证完成事件

3. **数据发送**: 只有在会话就绪（认证和识别完成）后才能发送EA数据

4. **线程安全**: 当前实现不是线程安全的，需要在单线程或加锁保护下使用

5. **内存管理**: 所有create函数返回的对象需要调用对应的destroy函数释放
