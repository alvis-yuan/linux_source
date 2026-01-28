# iAP2 会话层架构总结

## 整体架构

```
应用层 (Application)
    ↓
会话层 (Session Layer) - iAP2Session
    ↓
Link层 (Link Layer) - iAP2Link
    ↓
传输层 (Transport Layer) - iAP2Transport
    ↓
物理层 (Physical Layer) - Bluetooth/USB/UART
```

## 会话层模块组成

### 1. 会话管理器 (iAP2Session)

**职责**：
- 管理Control Session和EA Session
- 协调认证和识别两阶段流程
- 路由会话数据到应用层

**状态机**：
```
Idle → WaitLink → Authenticating → Identifying → Ready
                        ↓               ↓
                     Failed          Failed
```

**关键接口**：
```c
iAP2Session_t* iAP2SessionCreate(const iAP2SessionConfig_t* config);
BOOL iAP2SessionStart(iAP2Session_t* session);
BOOL iAP2SessionProcess(iAP2Session_t* session);
BOOL iAP2SessionIsReady(iAP2Session_t* session);
```

### 2. 认证模块 (iAP2Authentication)

**职责**：
- 实现iAP2认证流程（第一阶段）
- 与MFi协处理器交互
- 使用ControlCodec编解码认证消息

**认证流程**：
```
1. Device → RequestAuthenticationCertificate (0xAA00)
2. Accessory → AuthenticationCertificate (0xAA01)
3. Device → RequestAuthenticationChallengeResponse (0xAA02)
4. Accessory → AuthenticationResponse (0xAA03)
5. Device → AuthenticationSucceeded (0xAA05) / AuthenticationFailed (0xAA04)
```

**状态机**：
```
Idle → WaitCertRequest → ReadingCert → SendingCert → WaitChallenge
    → SigningChallenge → SendingResponse → WaitResult → Authenticated/Failed
```

**关键特性**：
- 异步CP操作（不阻塞主循环）
- 使用ControlCodec统一编解码
- 支持线程模式和轮询模式

### 3. 识别模块 (iAP2Identification)

**职责**：
- 实现iAP2识别流程（第二阶段）
- 向Apple设备声明配件能力
- 使用ControlCodec编解码识别消息

**识别流程**：
```
1. Device → StartIdentification (0x1D00)
2. Accessory → IdentificationInformation (0x1D01)
3. Device → IdentificationAccepted (0x1D02) / IdentificationRejected (0x1D03)
```

**配件信息包含**：
- 基本信息：名称、型号、制造商、序列号、版本
- EA协议：协议ID、协议名称、匹配动作
- 蓝牙传输：传输ID、传输名称、MAC地址
- 消息能力：支持发送/接收的消息ID列表

**状态机**：
```
Idle → WaitStart → WaitResult → Identified/Rejected
```

### 4. CP任务模块 (iAP2CPTask)

**职责**：
- 封装MFi协处理器操作
- 提供异步操作接口
- 支持线程模式和轮询模式

**支持的操作**：
- 读取证书 (ReadCert)
- 签名挑战 (SignChallenge)
- 读取序列号 (ReadSerial)

**两种工作模式**：

1. **线程模式**（推荐）：
   - CP操作在独立线程中执行
   - 不阻塞主循环
   - 通过回调通知结果

2. **轮询模式**：
   - CP操作在主循环中执行
   - 会短暂阻塞主循环
   - 适用于无RTOS环境

### 5. 控制消息编解码 (iAP2ControlCodec)

**职责**：
- 提供统一的控制消息编解码接口
- 支持所有iAP2参数类型
- 处理大小端转换

**编码接口**：
```c
ctrlSess_BuilderInit(&builder, buffer, size, msgId);
ctrlSess_AddString(&builder, paramId, "value");
ctrlSess_AddBlob(&builder, paramId, data, len);
ctrlSess_AddUint16(&builder, paramId, value);
// ... 其他类型
uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
```

**解码接口**：
```c
ctrlSess_DecodeMessageHeader(data, len, &msg);
while (remaining > 0) {
    ctrlSessParameter param;
    int consumed = ctrlSess_GetNextParameter(ptr, remaining, &param);
    // 处理参数
}
```

## 数据流

### 1. 接收数据流

```
Physical Layer
    ↓
Transport Layer (iAP2Transport)
    ↓
Link Layer (iAP2Link) - 解包、重组
    ↓
Session Layer (iAP2Session)
    ↓
    ├─ Control Session → Authentication/Identification
    └─ EA Session → Application
```

### 2. 发送数据流

```
Application
    ↓
Session Layer (iAP2Session)
    ↓
Link Layer (iAP2Link) - 分片、打包
    ↓
Transport Layer (iAP2Transport)
    ↓
Physical Layer
```

## 使用示例

### 基本初始化

```c
/* 1. 配置配件信息 */
iAP2AccessoryInfo_t accessoryInfo = {
    .name = "My Accessory",
    .modelIdentifier = "Model-2024",
    .manufacturer = "My Company",
    .serialNumber = "SN123456",
    .firmwareVersion = "1.0.0",
    .hardwareVersion = "1.0"
};

/* 2. 配置EA协议 */
iAP2EAProtocol_t eaProtocols[] = {
    {
        .protocolIdentifier = 1,
        .protocolName = "com.mycompany.protocol",
        .matchAction = 0
    }
};

/* 3. 配置会话层 */
iAP2SessionConfig_t config = {
    .transport = transport,
    .sessionDataCB = SessionDataCallback,
    .authCompleteCB = AuthCompleteCallback,
    .identifyCompleteCB = IdentifyCompleteCallback,
    .cpReadCertCallback = CPReadCertCallback,
    .cpSignChallengeCallback = CPSignChallengeCallback,
    .accessoryInfo = &accessoryInfo,
    .eaProtocols = eaProtocols,
    .eaProtocolCount = 1
};

/* 4. 创建并启动会话 */
iAP2Session_t* session = iAP2SessionCreate(&config);
iAP2SessionStart(session);
```

### 主循环处理

```c
while (1) {
    /* 处理传输层 */
    iAP2TransportProcess(transport);
    
    /* 处理会话层 */
    iAP2SessionProcess(session);
    
    /* 检查会话状态 */
    if (iAP2SessionIsReady(session)) {
        /* 可以发送业务数据 */
        iAP2SessionSendData(session, eaSessionID, data, len);
    }
    
    usleep(10000);  /* 10ms */
}
```

### CP异步操作

```c
/* CP读取证书回调（在后台线程执行） */
void CPReadCertCallback(void* context)
{
    uint8_t certBuffer[1024];
    uint16_t certLen = sizeof(certBuffer);
    
    /* 调用MFi CP接口 */
    int ret = mfiAuthenticationCertificate(certBuffer, &certLen);
    
    /* 通知认证模块 */
    iAP2AuthCPReadCertComplete(certBuffer, certLen, ret == 0);
}

/* CP签名挑战回调（在后台线程执行） */
void CPSignChallengeCallback(const uint8_t* challenge, 
                               uint32_t challengeLen,
                               void* context)
{
    uint8_t responseBuffer[256];
    uint16_t responseLen = sizeof(responseBuffer);
    
    /* 调用MFi CP接口 */
    int ret = mfiAuthenticationResponse(challenge, challengeLen,
                                         responseBuffer, &responseLen);
    
    /* 通知认证模块 */
    iAP2AuthCPSignChallengeComplete(responseBuffer, responseLen, ret == 0);
}
```

## 关键设计原则

### 1. 分层清晰
- 每层职责明确，不越界
- 上层依赖下层，下层不依赖上层
- 通过回调实现下层向上层通知

### 2. 状态机驱动
- 每个模块都有清晰的状态机
- 状态转换条件明确
- 易于调试和维护

### 3. 异步非阻塞
- CP操作异步执行
- 主循环不阻塞
- 通过回调通知结果

### 4. 统一编解码
- 使用ControlCodec统一接口
- 避免手动构造消息
- 减少错误，提高可维护性

### 5. 易于扩展
- 模块化设计
- 接口清晰
- 添加新功能不影响现有代码

## 调试建议

### 1. 启用日志
```c
#define IAP2_LOG_LEVEL IAP2_LOG_DEBUG
```

### 2. 检查状态
```c
printf("Session state: %s\n", iAP2SessionGetStateString(session));
printf("Authenticated: %d\n", iAP2SessionIsAuthenticated(session));
printf("Identified: %d\n", iAP2SessionIsIdentified(session));
printf("Ready: %d\n", iAP2SessionIsReady(session));
```

### 3. 监控消息
- 在编解码处添加日志
- 打印消息ID和长度
- 检查参数内容

### 4. 验证CP操作
- 确认CP回调被调用
- 检查证书和签名数据
- 验证CP操作返回值

## 常见问题

### Q1: 认证失败怎么办？
- 检查MFi CP是否正常工作
- 验证证书读取是否成功
- 确认挑战签名是否正确
- 查看AuthenticationFailed消息的错误码

### Q2: 识别被拒绝怎么办？
- 检查配件信息是否完整
- 验证EA协议配置是否正确
- 确认消息能力声明是否匹配
- 查看IdentificationRejected消息的拒绝原因

### Q3: CP操作超时怎么办？
- 检查CP任务是否正确初始化
- 验证CP回调是否被调用
- 确认CP操作完成通知是否发送
- 考虑增加超时时间

### Q4: 如何支持多个EA协议？
- 在配置中添加多个iAP2EAProtocol_t
- 设置正确的eaProtocolCount
- 每个协议使用不同的protocolIdentifier

## 参考文档

- iAP2协议规范：Apple Accessory Interface Specification
- MFi认证规范：Made for iPhone/iPod/iPad Accessory Design Guidelines
- 代码示例：iAP2Session/iAP2SessionExample.c
- 重构计划：iAP2Session/REFACTOR_PLAN.md
- 架构说明：iAP2Session/README_Session.md
