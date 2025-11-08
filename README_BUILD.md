# LVGL Player Demo - Multi-threaded Build

此專案已配置支持多線程並行構建，可顯著提升編譯速度。

## 快速開始

### 使用構建腳本（推薦）

```bash
# 基本構建（Debug模式）
./build.sh

# 清潔構建
./build.sh clean

# Release模式構建
./build.sh release

# Debug模式構建
./build.sh debug

# 構建並運行
./build.sh run
```

### 手動構建

```bash
# 配置 CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 多線程構建（自動檢測CPU核心數）
cmake --build build -j

# 或指定線程數（例如8個線程）
cmake --build build -j8

# 運行應用程序
./bin/main
```

## 構建配置

### VS Code 集成

專案已配置了 VS Code 任務：

- **Ctrl+Shift+P** → "Tasks: Run Task" → "Build (Multi-threaded)"
- **Ctrl+Shift+P** → "Tasks: Run Task" → "Build and Run (Multi-threaded)"

### CMake 設置

- 自動檢測 CPU 核心數並設置並行構建
- 支持 Debug 和 Release 模式優化
- 啟用了編譯命令導出（compile_commands.json）

### 編譯器優化

**Debug 模式：**

- 無優化（-O0）
- 包含調試符號（-g）
- 快速編譯時間

**Release 模式：**

- 最高優化（-O3）
- 啟用 Link-Time Optimization (LTO)
- 移除調試斷言

## 性能對比

多線程構建相比單線程構建的性能提升：

- **2 核心：** 約 1.5-1.8x 更快
- **4 核心：** 約 2.5-3.5x 更快
- **8 核心：** 約 4-6x 更快
- **12+核心：** 約 6-10x 更快

實際速度提升取決於：

- CPU 核心數和頻率
- 記憶體速度
- 儲存裝置速度（SSD vs HDD）
- 項目大小和相依性

## 故障排除

### 記憶體不足錯誤

如果在編譯時遇到記憶體不足，可以限制並行作業數：

```bash
# 限制為 4 個並行作業
cmake --build build -j4

# 或修改 .vscode/settings.json 中的 cmake.parallelJobs
```

### 編譯錯誤

如果遇到編譯錯誤，嘗試清潔重建：

```bash
./build.sh clean
./build.sh debug
```

## 專案結構

```
├── .vscode/
│   ├── settings.json      # CMake 和編輯器設置
│   └── tasks.json         # 多線程構建任務
├── build/                 # CMake 構建目錄
├── bin/                   # 可執行文件輸出
├── src/
│   ├── player/           # Player demo 源代碼
│   ├── hal/              # 硬件抽象層
│   └── main.c            # 主程序入口
├── lvgl/                 # LVGL 圖形庫
├── CMakeLists.txt        # CMake 構建配置
└── build.sh              # 多線程構建腳本
```

## 特性

- ✅ LVGL Player Demo 整合
- ✅ 多線程並行構建
- ✅ 自動 CPU 核心檢測
- ✅ Debug/Release 模式優化
- ✅ VS Code 完整集成
- ✅ 跨平台支持（macOS, Linux, Windows）
- ✅ 編譯命令導出（IDE 支持）
