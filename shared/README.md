# 共享常量配置

本目录存放客户端和服务器共用的常量配置文件。

## 文件格式

每个 JSON 文件对应一种常量类型，格式如下：

```json
{
  "<常量类型>": [
    {
      "code": 1001,
      "name": "CONSTANT_NAME",
      "description": "常量描述"
    }
  ]
}
```

## 命名规范

- **文件名**：使用 snake_case，如 `error_codes.json`
- **常量名称**：使用 UPPER_SNAKE_CASE，如 `INVALID_PASSWORD`
- **类名/枚举名**：使用 PascalCase，如 `ErrorCode`

## 使用方法

### 手动生成

```bash
python tools/generate_constants.py
```

### 自动监控

```bash
python tools/file_watcher.py
```

修改本目录下的 JSON 文件后，会自动生成对应的 Python 和 C++ 代码文件。

## 生成的文件

- **Python**：`scripts/client/<常量类型>.py`
- **C++**：`scripts/server/common/include/<常量类型>.h`

## 现有配置

- `error_codes.json` - 错误码定义
- `message_ids.json` - 消息ID定义
