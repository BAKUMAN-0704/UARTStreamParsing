# UART Stream Parsing

一个基于 Qt 的串口数据流解析工具，可按用户自定义的帧格式（通过 Excel/CSV 配置）解析串口或文件中的二进制数据流，并以表格展示、导出为 TXT。

## 主要特性

- **灵活的帧格式配置**：通过 `.xlsx` 或 `.csv` 定义帧结构，无需改动代码即可适配不同协议
- **多配置并行解析**：可同时加载多份配置，在同一数据流中识别不同类型的帧
- **丰富的字段类型**：支持 `HEADER` / `TAIL` / `LENGTH` / `DATA` / `CRC` / `PADDING`
- **常用数据类型**：`uint8/16/32`、`int8/16/32`、`float`、`double`，可配置大小端
- **CRC 校验**：内置 `CRC8` / `CRC16_MODBUS` / `CRC16_CCITT` / `CRC32`
- **两种数据源**：
  - **串口模式**：实时接收数据，支持 HEX 文本模式（将 `55 AA` 文本自动转为二进制）
  - **文件模式**：读取 TXT 十六进制数据文件
- **流式解析**：数据边接收边解析，自动搜索帧头进行同步
- **结束帧 + 自动保存**：可将某份配置标记为 End Frame，检测到时自动保存结果
- **结果导出**：将解析结果导出为带原始字节的可读 TXT

## 目录结构

```
UARTStreamParsing/
├── CMakeLists.txt
├── main.cpp
├── widget.{h,cpp,ui}          # 主界面
├── src/
│   ├── config/                # 配置文件解析（xlsx/csv → FrameFieldDef）
│   ├── parser/                # 帧解析器、流式解析、CRC 计算
│   ├── datasource/            # 串口 / 文件数据源
│   ├── export/                # 结果导出
│   └── worker/                # 后台解析线程
├── doc/
│   └── 使用说明.md             # 详细使用说明（配置格式、示例等）
├── test/
│   ├── generate_test_data.py  # 生成测试数据流与配置
│   ├── test_config*.{xlsx,csv}
│   └── test_stream_mixed.txt
├── third_party/
│   └── QXlsx/                 # xlsx 读取库
└── resource/                  # 字体、图标等资源
```

## 依赖

- Qt 5 或 Qt 6（需要 `Widgets` 和 `SerialPort` 模块）
- CMake ≥ 3.16
- C++17 编译器
- [QXlsx](https://github.com/QtExcel/QXlsx)（已作为 `third_party/` 子模块内置）

## 构建

```bash
cmake -B build -S .
cmake --build build --config Release
```

构建完成后在 `build/` 目录下可找到 `UARTStreamParsing` 可执行文件。

## 快速上手

1. 启动程序，点击「浏览...」选择配置文件（`.xlsx` 或 `.csv`），点击「加载配置」
2. 选择数据源：
   - **文件模式**：选择 TXT 数据文件 → 点击「读取文件」
   - **串口模式**：选择端口和波特率 → 点击「打开串口」
3. 点击「开始解析」查看表格结果
4. 点击「导出TXT」保存解析结果

详细的配置文件格式、字段说明和示例请查看 [doc/使用说明.md](doc/使用说明.md)。

## 测试数据

`test/` 目录提供了 4 种帧格式的配置以及一份统一的混合数据流，可用于功能验证：

```bash
python test/generate_test_data.py
```

生成后加载 4 份配置（将 `test_config_endframe.xlsx` 标记为 End Frame），使用文件模式读取 `test_stream_mixed.txt` 即可进行端到端测试。详见[使用说明「九、测试验证」](doc/使用说明.md#九测试验证)。
