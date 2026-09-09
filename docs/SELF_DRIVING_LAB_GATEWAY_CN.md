# 自驱动实验室接口预留说明

原生 C++ / Qt 桌面程序预留了三类网络接口，用于后续连接实验编排器、仪器适配器、数据采集程序和闭环优化 Agent。

当前版本的目标是**先把协议边界和通信通道固定下来**，不直接控制真实硬件。

## 默认端口

| 端口 | 协议 | 当前用途 | 后续可接入对象 |
| --- | --- | --- | --- |
| `49321` | HTTP/JSON | 控制与编排 API | 自驱动实验室 Orchestrator、Agent、LIMS/ELN 中间层 |
| `49322` | TCP + JSON Lines | 事件流 | 实验状态订阅、任务状态、结果到达、告警广播 |
| `49323` | TCP + JSON Lines | 仪器桥接入口 | GC、MS、流量计、温控器、PLC/OPC-UA/MQTT 适配器 |

三个端口都可以通过环境变量或命令行参数修改。

## 默认网络边界

软件默认只监听：

```text
127.0.0.1
```

因此默认只能由同一台电脑上的程序访问，不会主动暴露到校园网、实验室局域网或公网。

如果后续需要让另一台实验控制电脑访问，可以显式设置：

```text
CATALYST_SDL_BIND=0.0.0.0
```

或：

```text
--sdl-bind=0.0.0.0
```

当绑定到非本机地址时，程序强制要求提供至少 24 字符的访问令牌：

```text
CATALYST_SDL_TOKEN=<your-long-random-token>
```

这样可以避免为了“开放端口”而把实验接口变成无认证控制入口。

## 端口配置

环境变量：

```text
CATALYST_SDL_CONTROL_PORT=49321
CATALYST_SDL_EVENT_PORT=49322
CATALYST_SDL_INSTRUMENT_PORT=49323
```

命令行参数：

```text
--sdl-control-port=49321
--sdl-event-port=49322
--sdl-instrument-port=49323
--sdl-token=<token>
```

完全禁用接口：

```text
--no-sdl-gateway
```

## 49321：控制 API

公开无需认证：

```text
GET /v1/health
GET /v1/capabilities
```

仅在 `127.0.0.1` 本机监听模式下可用：

```text
GET /v1/pairing
```

它返回当前软件会话的临时访问令牌，方便本机 Agent 自动配对。若软件绑定到局域网地址，这个接口会被拒绝，此时必须显式配置令牌。

需要 Bearer Token：

```text
GET  /v1/state
POST /v1/jobs
POST /v1/results
POST /v1/events
```

认证头：

```text
Authorization: Bearer <token>
```

### 任务入口

示例：

```json
{
  "id": "screen-ni-001",
  "type": "stability_test",
  "sample": "Ni-Al2O3-A",
  "requested_conditions": {
    "temperature_c": 700,
    "ghsv": 24000,
    "pressure_bar": 1.0
  }
}
```

当前版本会返回 `202 Accepted`，但同时明确：

```json
{
  "execution_enabled": false
}
```

也就是说，接口链路已经打通，但**不会因为收到网络任务就直接操作反应器**。后续必须再接入经过确认的硬件适配器和安全状态机，才允许从“任务接收”升级到“硬件执行”。

### 结果入口

`POST /v1/results` 用于未来接收仪器或上位机整理后的结果包。当前版本只进入集成事件链，不会自动改写 `.clrproj` 中已有实验记录。

## 49322：事件流

协议为 TCP + UTF-8 JSON Lines，每条消息一行 JSON。

连接后服务器先发送 `gateway.hello`。客户端第一条消息必须是：

```json
{"token":"<token>","client":"lab-orchestrator"}
```

认证成功后会收到：

```json
{"ok":true,"type":"gateway.authenticated","channel":"events"}
```

之后该连接可持续接收：

```text
gateway.ready
job.received
result.received
instrument.message
```

等事件。

该端口适合后续由自驱动实验室 Orchestrator 长连接订阅，而不是反复轮询 HTTP。

## 49323：仪器桥

同样使用 TCP + JSON Lines。

第一行先认证：

```json
{"token":"<token>","client":"gc-adapter-01"}
```

之后可以发送仪器适配器标准化后的消息，例如：

```json
{
  "source": "gc-01",
  "type": "measurement",
  "run_id": "screen-ni-001",
  "timestamp": "2026-09-09T10:00:00+08:00",
  "data": {
    "ch4_conversion_percent": 81.7,
    "co2_conversion_percent": 84.2
  }
}
```

当前仪器桥是**telemetry ingress only**：只接收标准化遥测/结果消息，不向设备下发真实控制指令。

## 为什么暂时不开放直接硬件控制

自驱动实验室真正执行实验时，还必须增加至少三层：

1. **设备适配层**：把 GC、MFC、炉温控制器、PLC、机器人等不同协议统一成标准设备能力接口；
2. **安全状态机**：独立判断温度、压力、流量、阀位、联锁和异常状态，Agent 不能绕过；
3. **任务执行与审计层**：每个动作必须有 `run_id`、操作者/Agent、前置条件、时间戳、设备响应和失败恢复记录。

当前三个端口正是为这三层预留通信边界。

## 建议的后续架构

```text
LLM / Bayesian Optimizer / Active Learning Agent
                    │
                    ▼
           Experiment Orchestrator
                    │
          HTTP 49321 / Events 49322
                    │
                    ▼
     Catalyst Longevity Research Desktop
                    │
             Instrument 49323
                    │
          Safety / Adapter Gateway
        ┌───────────┼────────────┐
        ▼           ▼            ▼
       GC          MFC        Furnace / PLC
```

下一阶段建议优先实现**虚拟仪器适配器（digital twin / simulator）**，先让闭环 Agent 在不接真实设备的情况下完整跑通：

```text
提出实验 → 排队 → 虚拟执行 → 返回结果 → 分析 → 下一轮实验
```

这一步稳定后，再逐台替换为真实设备适配器，会比直接让 Agent 操作真实仪器可靠得多。
