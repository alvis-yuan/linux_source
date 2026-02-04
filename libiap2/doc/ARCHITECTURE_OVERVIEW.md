# iAP2协议栈架构概览

## 快速导航

- 📖 [完整架构分析](doc/iAP2_ARCHITECTURE_ANALYSIS.md) - 详细的架构分析文档（含Mermaid图表）
- 📦 [模块详细说明](doc/MODULE_DETAILS.md) - 每个模块的详细API和使用说明
- 📋 [架构总结](doc/ARCHITECTURE_SUMMARY.md) - 会话层架构总结
- 📘 [架构说明](doc/README_Architecture.md) - 分层架构说明
- 📊 [架构图](doc/ARCHITECTURE_DIAGRAM.txt) - ASCII架构图

---

## 项目简介

这是一个**iAP2（iPod Accessory Protocol 2）协议栈**的参考实现，用于iOS设备与MFi（Made for iPhone/iPod/iPad）配件之间的通信。

### 核心功能

- ✅ **MFi认证**：与MFi协处理器交互完成认证
- ✅ **配件识别**：向iOS设备声明配件能力
- ✅ **可靠传输**：基于ACK/重传的可靠数据传输
- ✅ **多会话支持**：Control、EA、Buffer会话
- ✅ **多物理层**：支持蓝牙、USB、UART

---

## 架构概览

### 分层结构

```
┌─────────────────────────────────────────┐
│         应用层 (Application)             │  ← 业务逻辑、EA数据处理
├─────────────────────────────────────────┤
│       适配层 (iAP2Adapt)                 │  ← 统一接口封装
├─────────────────────────────────────────┤
│       会话层 (iAP2Session)               │  ← 认证、识别、会话管理
│  ├─ Authentication (认证)                │
│  ├─ Identification (识别)                │
│  ├─ CPTask (CP任务)                      │
│  └─ ControlCodec (消息编解码)            │
├─────────────────────────────────────────┤
│       Link层 (iAP2Link)                  │  ← 可靠传输、流量控制
│  ├─ Packet (数据包)                      │
│  └─ LinkRunLoop (运行循环)               │
├─────────────────────────────────────────┤
│     传输层 (iAP2Transport)               │  ← 物理传输封装
├─────────────────────────────────────────┤
│       物理层 (Physical)                  │  ← 蓝牙/USB/UART
└─────────────────────────────────────────┘
```

### 模块目录

| 目录 | 说明 |
|------|------|
| `iAP2Adapt/` | 适配层 - 统一接口封装 |
| `iAP2Session/` | 会话层 - 认证、识别、会话管理 |
| `iAP2Link/` | Link层 - 可靠传输、数据包管理 |
| `iAP2Transport/` | 传输层 - 物理传输封装 |
| `iAP2Utility/` | 工具层 - FSM、缓冲池、时间等 |
| `iAP2UtilityImplementation/` | 工具层平台实现 |
| `driver/` | 硬件驱动 - MFi协处理器 |
| `doc/` | 文档目录 |

---

## 核心流程

### 连接建立流程

```
1. 物理连接 (蓝牙/USB)
   ↓
2. Link协商 (DETECT → SYN → SYN-ACK)
   ↓
3. 认证阶段 (MFi Authentication)
   - 读取证书
   - 签名挑战
   ↓
4. 识别阶段 (Identification)
   - 发送配件信息
   - 声明EA协议
   ↓
5. 会话就绪 (Ready)
   - 可以发送业务数据
```

### 数据传输流程

**接收**：物理层 → 传输层 → Link层 → 会话层 → 应用层  
**发送**：应用层 → 会话层 → Link层 → 传输层 → 物理层

---

## 关键特性

### 1. 可靠传输

- **滑动窗口协议**：流量控制
- **自动重传**：超时重传、快速重传
- **序列号管理**：SEQ/ACK机制
- **累积确认**：减少ACK包数量

### 2. 异步非阻塞

- **CP操作异步化**：认证操作不阻塞主循环
- **回调通知机制**：事件驱动
- **线程/轮询模式**：灵活适配不同平台

### 3. 状态机驱动

- **Link层状态机**：连接管理
- **认证状态机**：认证流程控制
- **识别状态机**：识别流程控制

### 4. 统一编解码

- **ControlCodec**：统一的消息编解码接口
- **自动大小端转换**
- **类型安全**：支持多种参数类型

---

## 快速开始

### 1. 编译

```bash
make
```

生成 `libiap2.so` 动态库。

### 2. 基本使用

```c
// 1. 创建传输层
iAP2TransportConfig_t config = {
    .type = kIAP2TransportTypeBluetooth,
    .sendDataCB = MySendCallback,
    .connectionChangeCB = MyConnectionCallback,
    // ...
};
iAP2Transport_t *transport = iAP2TransportCreate(&config);

// 2. 创建会话层
iAP2SessionConfig_t sessionConfig = {
    .transport = transport,
    .sessionDataCB = MySessionDataCallback,
    .authCompleteCB = MyAuthCompleteCallback,
    .identifyCompleteCB = MyIdentifyCompleteCallback,
    // ...
};
iAP2Session_t *session = iAP2SessionCreate(&sessionConfig, type);

// 3. 启动会话
iAP2SessionStart(session);

// 4. 物理连接建立后
iAP2TransportConnect(transport);

// 5. 主循环
while (running) {
    iAP2TransportProcess(transport);
    // 处理其他任务...
    usleep(10000); // 10ms
}

// 6. 发送数据（会话就绪后）
if (iAP2SessionIsReady(session)) {
    iAP2SessionSendData(session, eaSessionID, data, len);
}
```

---

## 设计模式

1. **分层架构**：职责清晰，易于维护
2. **状态机模式**：逻辑清晰，易于调试
3. **回调模式**：层间解耦，事件驱动
4. **对象池模式**：内存优化，减少碎片
5. **生产者-消费者**：队列管理
6. **策略模式**：多物理层适配

---

## 性能优化

- ✅ **预分配缓冲池**：避免运行时malloc
- ✅ **零拷贝**：指针传递
- ✅ **异步操作**：不阻塞主循环
- ✅ **批量处理**：累积确认
- ✅ **快速路径**：常见情况优化

---

## 调试支持

### 日志系统

```c
#define IAP2_LOG_ERROR(...)   // 错误
#define IAP2_LOG_WARNING(...) // 警告
#define IAP2_LOG_INFO(...)    // 信息
#define IAP2_LOG_DEBUG(...)   // 调试
```

### 状态查询

```c
// 会话状态
iAP2SessionIsAuthenticated(session);
iAP2SessionIsIdentified(session);
iAP2SessionIsReady(session);

// Link状态
iAP2LinkIsDetached(link);
iAP2TransportIsConnected(transport);
```

---

## 常见问题

### Q: 认证失败怎么办？

**检查**：
1. MFi芯片是否正确初始化
2. I2C通信是否正常
3. 证书读取是否成功
4. 查看AuthFailed错误码

### Q: 识别被拒绝怎么办？

**检查**：
1. 配件信息是否完整
2. EA协议配置是否正确
3. 消息能力是否匹配

### Q: 数据发送失败？

**检查**：
1. 会话是否就绪（认证+识别完成）
2. Link是否连接
3. 物理层是否正常

---

## 技术栈

- **语言**：C
- **编译器**：GCC
- **平台**：嵌入式Linux（MIPS）
- **依赖**：
  - MFi协处理器（I2C）
  - 蓝牙/USB/UART驱动

---

## 许可证

Apple MFi License - 仅供MFi授权厂商使用

---

## 相关文档

- [完整架构分析](doc/iAP2_ARCHITECTURE_ANALYSIS.md) - 包含详细的Mermaid图表和时序图
- [模块详细说明](doc/MODULE_DETAILS.md) - 每个模块的详细API和使用说明
- [会话层架构](doc/ARCHITECTURE_SUMMARY.md) - 会话层专题分析
- [分层架构说明](doc/README_Architecture.md) - 分层架构详解
- [ASCII架构图](doc/ARCHITECTURE_DIAGRAM.txt) - 文本格式架构图

---

**版本**: 1.0  
**日期**: 2026-01-29  
**维护**: Kiro AI Assistant
