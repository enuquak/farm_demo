---
name: test-ai-coding-conventions
description: 测试通用 AI 编码规范：Windows 后台进程验证、集成测试前置检查、TCP 服务器测试模式等最佳实践。不涉及具体项目业务逻辑。
license: MIT
metadata:
  author: common
  version: "1.0"
---

# 测试 AI 编码规范

> 本文档是**通用测试指导规范**，适用于任何项目的集成测试、端到端测试场景。

---

## 一、Windows 后台进程启动与验证

### 1.1 `start /B` 的静默失败陷阱

Windows 下 `start /B` 启动后台进程时，即使 exe 因 DLL 缺失弹窗阻塞或进程立即退出，命令本身仍返回成功码 0。

```bash
# ❌ 危险：仅依赖返回值判断
start /B gate_server.exe 8080
echo $?  # 返回 0，但进程可能已死

# ✅ 正确：必须验证进程实际存活
start /B gate_server.exe 8080 > server_output.txt 2>&1
sleep 2
netstat -ano | grep 8080        # 确认端口监听
tasklist | grep gate_server     # 确认进程存活
```

### 1.2 验证命令序列（标准流程）

```bash
# 1. 启动进程，输出重定向到文件
start /B path/to/server.exe 8080 > server_output.txt 2>&1

# 2. 等待进程初始化
sleep 2

# 3. 验证端口监听
netstat -ano | grep 8080

# 4. 若端口未监听，检查进程是否存活
tasklist | grep server

# 5. 若进程不存在，检查输出日志排查原因
cat server_output.txt
```

---

## 二、集成测试前置检查清单

测试任何网络服务器前，按以下顺序执行前置检查，可避免 90% 的"连接超时"类假失败：

1. **可执行文件存在性** — 确认 exe 文件存在于预期路径
2. **依赖完整性** — 确认所有依赖 DLL 在 exe 同目录（Windows）或 LD_LIBRARY_PATH 中（Linux）
3. **端口可用性** — `netstat -ano | grep <端口>` 确认目标端口未被占用
4. **启动进程** — 启动进程，等待 1-2 秒
5. **监听验证** — `netstat` 确认端口已处于 LISTENING 状态
6. **执行测试** — 前置检查全部通过后，才执行功能测试用例

```bash
# 前置检查示例
EXE_PATH="path/to/server.exe"
PORT=8080

# 检查 1: exe 存在
[ -f "$EXE_PATH" ] || { echo "FAIL: exe not found"; exit 1; }

# 检查 2: 端口未占用
netstat -ano | grep ":$PORT " && { echo "FAIL: port in use"; exit 1; }

# 启动
start /B "$EXE_PATH" $PORT > server_output.txt 2>&1
sleep 2

# 检查 3: 端口已监听
netstat -ano | grep ":$PORT " || { echo "FAIL: server not listening"; exit 1; }

# 执行测试
python test_client.py
```

---

## 三、TCP 服务器测试模式

### 3.1 消息处理数据流验证

当 TCP 服务器收到数据但不响应时，按数据流逐环节排查：

```
网络数据 → onRead 回调 → 消息解析器 → 消息路由器 → 处理函数 → 发送响应
```

**常见断点**：
- onRead 读取了数据但未传给解析器
- 解析器工作正常但路由器未注册对应 MsgID
- 处理函数执行了但未调用发送接口

### 3.2 粘包/拆包测试

```bash
# 粘包：一次发送多条消息
python -c "
import socket, struct
s = socket.create_connection(('localhost', 8080))
# 两条消息拼在一起发送
msg1 = struct.pack('>I', 8) + struct.pack('>I', 1) + b'\x00' * 4
msg2 = struct.pack('>I', 8) + struct.pack('>I', 1) + b'\x00' * 4
s.sendall(msg1 + msg2)
s.close()
"

# 拆包：分片发送一条消息
python -c "
import socket, struct, time
s = socket.create_connection(('localhost', 8080))
msg = struct.pack('>I', 8) + struct.pack('>I', 1) + b'\x00' * 4
s.send(msg[:4])       # 先发一半
time.sleep(0.1)
s.send(msg[4:])       # 再发一半
s.close()
"
```

### 3.3 心跳超时测试

```bash
# 连接后不发心跳，等待超时
python -c "
import socket, time
s = socket.create_connection(('localhost', 8080))
print('Connected, waiting for timeout...')
time.sleep(20)  # 等待超过超时时间
try:
    data = s.recv(1024)
    if not data:
        print('PASS: Server closed connection (timeout)')
    else:
        print(f'Received: {data}')
except:
    print('PASS: Connection reset by server')
s.close()
"
```

---

## 四、测试报告规范

### 4.1 报告命名

- 测试全部通过：`*_success.md`
- 存在测试失败：`*_fail.md`

### 4.2 报告内容结构

```markdown
## 概览
- 总测试数：X
- 通过：X | 失败：X | 跳过：X
- 整体状态：PASS / FAIL

## 编译结果
- 状态：成功/失败
- 警告/错误详情

## 测试用例
| ID | 名称 | 状态 | 说明 |
|----|------|------|------|

## 发现缺陷
- BUG-001: 描述、复现步骤、影响

## 建议
- 修复建议
```
