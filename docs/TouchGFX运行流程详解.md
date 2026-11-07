# TouchGFX 运行流程详解

> 以「远程数据采集系统接收端」工程为实例，逐层拆解 TouchGFX 从上电到每帧刷新的完整调用链。
> 文中路径均相对工程根目录 `d:\receive - trae`，行号对应当前代码。

---

## 0. 文档说明

| 项 | 值 |
|---|---|
| MCU | STM32F746NGH6（Cortex-M7，108MHz） |
| 显示屏 | 480×272，RGB565，LTDC + SDRAM 帧缓冲 |
| GUI 框架 | TouchGFX 4.27.0（核心为预编译库 `libtouchgfx.a`） |
| 工具链 | IAR EWARM 8.32 |
| 触摸 | FT5336，I2C3 轮询 |
| RTOS | 无（裸机单线程 + 中断打拍） |
| 界面实现 | 100% 手写 Widget，Designer 中 `Components: []` |

阅读前提：知道什么是 LTDC 扫描、知道 `while(1)` 主循环。其余概念本文都会解释。




























































































-












---

## 1. 一张图看懂

```
                    ┌─────────────────────────────────────────┐
                    │  LTDC 硬件扫描 SDRAM 中的帧缓冲           │
                    │  每帧扫到第 10 行 → 触发行中断             │
                    └───────────────┬─────────────────────────┘
                                    │ 中断（只置标志）
                                    ▼
                    OSWrappers::signalVSync()  →  vsync_sem = 1
                                    │
                                    ▼
   main while(1) ──► MX_TouchGFX_Process() ──► touchgfx_taskEntry()
                                                    │
                                    有 VSync 吗? ────┤ 否 → 立即返回（空转）
                                                    │ 是
                                                    ▼
                                        HAL::backPorchExited()
                                                    │
                              ┌─────────────────────┴──────────────────────┐
                              ▼                                            ▼
                     sampleTouch() 读 FT5336                   Application::handleTickEvent()
                     → 生成 ClickEvent                                │
                              │                                        ▼
                              │                              FrontendApplication::handleTickEvent()
                              │                                        │
                              │                          ┌─────────────┴─────────────┐
                              │                          ▼                           ▼
                              │                   Model::tick()             screenView::handleTickEvent()
                              │                  （解析串口、测量）          （比较 refreshCount、
                              │                          │                   invalidate 脏区）
                              │                          └─────────────┬─────────────┘
                              │                                        ▼
                              └──────────► 事件派发 View::handleClickEvent
                                                    │
                                                    ▼
                                       遍历脏区 → Widget::draw(invalidatedArea)
                                                    │
                                                    ▼
                                       HAL::endFrame() → vsync_sem = 0（封住下次 VSync）
```

核心认知：**中断只负责"报时"，所有逻辑与绘制都在主循环里做**。

---

## 2. 关键文件与职责

| 文件 | 生成方式 | 职责 |
|---|---|---|
| `Core/Src/main.c` | CubeMX | 时钟、外设初始化、`while(1)` 主循环 |
| `TouchGFX/App/app_touchgfx.c` | 生成一次 | C 与 C++ 的桥接：`MX_TouchGFX_Init/Process` |
| `TouchGFX/target/generated/TouchGFXConfiguration.cpp` | 生成，勿改 | 静态对象定义、`touchgfx_init()`、`touchgfx_taskEntry()` |
| `TouchGFX/target/generated/TouchGFXGeneratedHAL.cpp` | 生成，勿改 | 帧缓冲定义、LTDC 行中断配置、帧缓冲指针读写 |
| `TouchGFX/target/TouchGFXHAL.cpp` | 生成一次，可改 | 用户可覆盖的 HAL 钩子、**LTDC 行中断回调** |
| `TouchGFX/target/generated/OSWrappers.cpp` | 生成，勿改 | VSync / 帧缓冲信号量（裸机用两个 `volatile` 变量实现） |
| `TouchGFX/target/STM32TouchController.cpp` | 生成一次，可改 | 读 FT5336，返回屏幕坐标 |
| `TouchGFX/target/generated/STM32DMA.cpp` | 生成，勿改 | DMA2D 抽象（本工程未启用 blit） |
| `TouchGFX/generated/gui_generated/**` | Designer 生成，勿改 | 屏幕基类、FrontendApplicationBase、FrontendHeapBase |
| `TouchGFX/gui/include/gui/common/FrontendHeap.hpp` | 用户 | 静态内存池 + Model/Application 实例 |
| `TouchGFX/gui/src/common/FrontendApplication.cpp` | 用户 | 应用入口，可插入全局 tick 逻辑 |
| `TouchGFX/gui/src/model/Model.cpp` | 用户 | 业务数据：串口解析、测量、导出 |
| `TouchGFX/gui/src/screen_screen/screenView.cpp` | 用户 | 事件处理、命中测试、脏区标记 |
| `TouchGFX/gui/include/gui/widgets/ScopeWidget.hpp` | 用户 | 全部像素绘制 |

> 规则：标"勿改"的文件在 Designer/CubeMX 重新生成时会被覆盖；需要改行为时去覆盖"可改"的文件。

---

## 3. 阶段一：上电初始化

这一阶段的目标：**让硬件开始扫描、让框架就绪、把起始屏幕挂起来**。此时屏幕仍是黑的，因为一次 tick 都还没发生。

### 3.1 时钟与外设（`main.c`）

```c
HAL_Init();
SystemClock_Config();      // SYSCLK = 108MHz
MX_GPIO_Init();
MX_I2C3_Init();            // 触摸
MX_USART1_UART_Init();     // CSV/BMP 导出
MX_DMA2D_Init();
MX_FMC_Init();             // SDRAM 控制器
MX_LTDC_Init();            // 显示控制器
MX_TouchGFX_Init();        // 进入 C++ 世界
```

顺序有依赖：`MX_FMC_Init()` 必须先于 LTDC 和 TouchGFX，因为帧缓冲放在 SDRAM 里；LTDC 初始化时 `FBStartAdress_L0 = 0xC0000000` 就已经指向 SDRAM 首地址（见 `receive.ioc`）。

### 3.2 C → C++ 桥接（`TouchGFX/App/app_touchgfx.c`）

```c
void MX_TouchGFX_Init(void) {
    touchgfx_components_init();   // 本工程为空
    touchgfx_init();              // C++ 域
}
void MX_TouchGFX_Process(void) {
    touchgfx_taskEntry();         // 主循环里反复调用
}
```

CubeMX 生成的 `.c` 不能直接包含 C++ 头文件，所以用这两个 `extern "C"` 函数做边界。

### 3.3 `touchgfx_init()`：注册资源（`TouchGFXConfiguration.cpp:42-60`）

```cpp
Bitmap::registerBitmapDatabase(...);  // 图片表（本工程为空）
TypedText::registerTexts(&texts);     // 文本表
Texts::setLanguage(0);                // GB（本工程未使用生成文本）
FontManager::setFontProvider(&fontProvider);
FrontendHeap& heap = FrontendHeap::getInstance();  // ← 触发静态构造
hal.initialize();
```

`getInstance()` 是函数内 `static` 单例，第一次调用触发构造，**整个 UI 的对象树都在这一刻建好**：

```
FrontendHeap 构造
  ├─ Partition<presenters>   ← 放 presenter 的内存（placement new）
  ├─ Partition<views>        ← 放 view 的内存
  ├─ Partition<transitions>
  ├─ Model model
  └─ FrontendApplication app(model, *this)
        └─ FrontendApplicationBase(...)
              ├─ setDisplayOrientation(LANDSCAPE)
              └─ enableTextureMapperAll() 等（LCD16bpp 能力开关）
      末尾：gotoStartScreen(app) → gotoscreenScreenNoTransition()
```

`gotoscreenScreenNoTransition()` **只挂回调，不创建屏幕**：

```cpp
transitionCallback = Callback(this, &...gotoscreenScreenNoTransitionImpl);
pendingScreenTransitionCallback = &transitionCallback;
```

真正的 `makeTransition<screenView, screenPresenter, NoTransition, Model>` 发生在第一次 `handleTickEvent` 时（见 6.1）。

### 3.4 `hal.initialize()`：框架与硬件握手

分两段：`HAL::initialize()`（库实现）和生成 HAL 追加的配置（`TouchGFXGeneratedHAL.cpp:39-51`）。

`HAL::initialize()` 内部主要做：

1. `OSWrappers::initialize()` —— 两个信号量清零（`OSWrappers.cpp:33-37`）
2. `lcd().initialize()` —— LCD16bpp 驱动准备
3. `touchController.init()` —— 本工程为空实现
4. 建立帧缓冲地址

生成 HAL 随后追加：

```cpp
registerEventListener(*(Application::getInstance()));  // Application 成为事件接收者
registerTaskDelayFunction(&OSWrappers::taskDelay);     // 单缓冲策略必需
setFrameRefreshStrategy(REFRESH_STRATEGY_OPTIM_SINGLE_BUFFER_TFT_CTRL);
enableLCDControllerInterrupt();                        // 配 LIPCR + 开 LIE
setFrameBufferStartAddresses((void*)frameBuf, 0, 0);   // 单缓冲：只给 0 号地址
```

`setFrameRefreshStrategy` 有三条自检，任一不满足就 `while(1)` 死等（`TouchGFXGeneratedHAL.cpp:44-47`）：

- 已注册 `taskDelay` 函数
- `getTFTCurrentLine()` 返回了有效值（不是 `0xFFFF`）
- 没有使用双缓冲

这是移植时最容易卡住的地方：**上电就死机且没画面，先查这里**。

### 3.5 LTDC 行中断的"武装"（`TouchGFXGeneratedHAL.cpp:68-77`）

```cpp
lcd_int_active_line = (LTDC->BPCR & LTDC_BPCR_AVBP_Msk) - 1;   // = 10
lcd_int_porch_line  = (LTDC->AWCR & LTDC_AWCR_AAH_Msk) - 1;
LTDC->LIPCR = lcd_int_active_line;   // 行中断位置
LTDC->IER |= LTDC_IER_LIE;           // 使能行中断
```

本工程 LTDC 时序（`Core/Src/ltdc.c:47-54`）：`AccumulatedVBP = 11`，有效区从第 11 行开始。中断设在第 10 行，即**进入有效显示区的前一行**，这样 tick 一开始就拥有完整一帧的绘制时间。

`NVIC_SetPriority(LTDC_IRQn, 9)`（`TouchGFXGeneratedHAL.cpp:53-56`）：优先级较低，不会抢占串口 DMA 等实时任务。

### 3.6 帧缓冲放在哪（`TouchGFXGeneratedHAL.cpp:33-34` + `EWARM/stm32f746xx_flash.icf:31,38`）

```cpp
LOCATION_PRAGMA_NOLOAD("TouchGFX_Framebuffer")
uint32_t frameBuf[(480 * 272 * 2 + 3) / 4] LOCATION_ATTRIBUTE_NOLOAD("TouchGFX_Framebuffer");
```

- 大小 = 480×272×2 = 261,120 B ≈ 255 KB
- 链接脚本：`place in SDRAM_region { section TouchGFX_Framebuffer }`
- `do not initialize { section .noinit, section TouchGFX_Framebuffer }` —— **启动时不清零**，省下 255KB 的拷贝时间

### 3.7 为什么此刻没画面

`vsync_sem = 0`，`isVSyncAvailable()` 恒为假，`tick()` 从未被调用 → 没有任何 `draw()` → 帧缓冲还是 SDRAM 的随机/上一版内容。

---

## 4. 阶段二：VSync 驱动（整个系统的心跳）

### 4.1 中断侧（`TouchGFXHAL.cpp:36-42`）

```cpp
extern "C" void HAL_LTDC_LineEventCallback(LTDC_HandleTypeDef *hltdc)
{
    touchgfx::OSWrappers::signalVSync();   // vsync_sem = 1
    LTDC->IER |= LTDC_IER_LIE;             // 重新使能行中断（HAL 会清掉标志）
}
```

调用链：

```
LTDC 扫描到第 10 行
  → LTDC_IRQHandler            (Core/Src/stm32f7xx_it.c:205)
  → HAL_LTDC_IRQHandler(&hltdc)
  → HAL_LTDC_LineEventCallback()   ← 必须自己实现
  → signalVSync()
```

**如果忘了实现这个回调**，`vsync_sem` 永远是 0，主循环空转，画面永远不刷新——这是新手最常遇到的"白屏/黑屏"原因。

### 4.2 任务侧（`TouchGFXConfiguration.cpp:66-77`）

```cpp
void touchgfx_taskEntry() {
    if (OSWrappers::isVSyncAvailable()) {
        hal.backPorchExited();
    }
}
```

`backPorchExited()` 是 HAL 头文件的内联实现（`HAL.hpp:691-695`）：

```cpp
virtual void backPorchExited() {
    swapFrameBuffers();   // 单缓冲：无缓冲可换，空操作
    tick();               // 每帧真正的工作
}
```

### 4.3 自节流：`signalRenderingDone`

`TouchGFXGeneratedHAL::endFrame()`（`TouchGFXGeneratedHAL.cpp:84-88`）：

```cpp
void endFrame() {
    HAL::endFrame();
    OSWrappers::signalRenderingDone();   // vsync_sem = 0
}
```

含义：**一帧没画完，下一个 VSync 会被丢弃**。所以绘制慢不会导致事件堆积，只会降帧率。这就是"从不让画面处于半更新状态"的保证机制。

### 4.4 帧率推导

| 参数 | 值 |
|---|---|
| LTDC 像素时钟 | 9.6 MHz（PLLSAI: 384/5/8） |
| 总扫描像素 | 565 × 285 = 161,025 |
| 理论帧率 | 9.6e6 / 161,025 ≈ **59.6 Hz** |
| 每帧预算 | 约 16.8 ms |

注意：单缓冲策略下，若一帧绘制跨过多个扫描带，实际视觉更新可能只有 30Hz。这是正常的，不是 bug。

---

## 5. 阶段三：`HAL::tick()` 一帧内做什么

框架核心是预编译库，源码在 TouchGFX 安装目录。按 HAL 的公开语义，一次 tick 依次完成：

| # | 动作 | 说明 |
|---|---|---|
| 1 | 检查 VSync 计数 | 没有新 VSync 直接返回 |
| 2 | `sampleTouch(x, y)` | 轮询触摸控制器，拿原始坐标 |
| 3 | 触摸状态机 | 比较"上次/这次"是否按下，推导 PRESSED / DRAG / RELEASED |
| 4 | 事件入队 | 生成 `ClickEvent`，暂存待派发 |
| 5 | `Application::handleTickEvent()` | 进入 MVP（见第 6 节） |
| 6 | 派发事件 | `currentScreen->handleClickEvent(event)` |
| 7 | 绘制脏区 | 对每个失效矩形调用 `Widget::draw(invalidatedArea)` |
| 8 | `endFrame()` | 清 VSync 标志，等待下一帧 |

### 5.1 触摸采样（`TouchGFX/target/STM32TouchController.cpp:26-59`）

```cpp
bool STM32TouchController::sampleTouch(int32_t& x, int32_t& y)
{
    // 从寄存器 0x02 一次读 5 字节：状态 + X(2) + Y(2)
    HAL_I2C_Mem_Read(&hi2c3, 0x70, 0x02, I2C_MEMADD_SIZE_8BIT, buf, 5, 10);

    if (buf[0] == 0xFF || (buf[0] & 0x0F) == 0) {
        g_touch_pressed = 0;
        return false;          // 无触摸点
    }
    rawX = ((buf[1] & 0x0F) << 8) | buf[2];
    rawY = ((buf[3] & 0x0F) << 8) | buf[4];

    x = rawY;   // ← 轴交换：FT5336 按竖屏原生方向上报
    y = rawX;
    g_touch_pressed = 1;
    return true;
}
```

三个要点：

1. **返回值语义**：`true` = 当前有手指按下，`false` = 无。框架据此判断 press/release 沿。
2. **轴交换**：面板原生是 480 长边为 X，但横屏使用时 X/Y 要互换。注释里已说明。
3. **10ms 超时**：I2C 异常时不会死等，最坏多花 10ms（会挤占一帧预算，但不会卡死）。

### 5.2 事件派发

框架把 `ClickEvent` 交给 `Application`，由 `MVPApplication` 转发给当前屏幕：

```
Application::handleClickEvent
  → View::handleClickEvent(touchgfx::ClickEvent&)
       event.getType() == PRESSED / DRAG / RELEASED
       event.getX(), event.getY()
```

本工程在 `screenView::handleClickEvent` 里自己做命中测试（下一节）。

### 5.3 绘制

见第 7 节。

---

## 6. 阶段四：MVP 应用层

### 6.1 tick 的传递链

`TouchGFX/gui/include/gui/common/FrontendApplication.hpp:16-20`：

```cpp
virtual void handleTickEvent() {
    model.tick();                                // ① 先更新数据
    FrontendApplicationBase::handleTickEvent();  // ② 再跑 MVP 基类
}
```

`FrontendApplicationBase::handleTickEvent()`（库实现，Designer 生成）做两件事：

1. 若有 `pendingScreenTransitionCallback`，执行它 → `makeTransition<screenView, screenPresenter, NoTransition, Model>`，在 FrontendHeap 的 Partition 里 placement-new 出 view/presenter，调 `presenter->activate()`、`view->setupScreen()`
2. `currentScreen->handleTickEvent()` → **本工程是 `screenView::handleTickEvent`**

**顺序的意义**：Model 先更新，View 后读取，所以本帧新数据在同一帧内就能反映到屏幕。

### 6.2 `Model::tick()`：业务数据（`TouchGFX/gui/src/model/Model.cpp:109-179`）

```cpp
void Model::tick()
{
    tickCount++;

    // ① 导出优先：占满本次 tick，结束后 return
    if (scope.exportActive) {
        scope.exportBmp ? pumpBmp() : pumpExport();
        return;
    }

    // ② 自测（宏 SCOPE_LINK_SELFTEST = 0，编译为空）
    ScopeLink::instance().selfTestTick(scope);

    // ③ 串口链路：收到完整帧就更新数据
    if (ScopeLink::instance().poll(scope)) {
        hasRealData = true;
        if (scope.triggerMode == TRIG_AUTO) {
            scope.measure();
            scope.refreshCount++;
        } else if (armed && triggerReached()) {
            scope.measure();
            scope.refreshCount++;
            if (triggerMode == TRIG_SINGLE) armed = false;
        }
    }

    // ④ 未联机时每 3 tick 造一屏假数据（约 20Hz）
    if (!hasRealData && (tickCount % 3u) == 0u) { ... }
}
```

三条硬规则：

- **不阻塞**：这里的任何耗时都会直接降帧率。曾经的自测模式阻塞发送 1706B（18.5ms）超过整帧预算，表现为"切时基卡顿"。
- **不碰界面**：Model 不知道任何 Widget 存在，只改 `ScopeData`。
- **不调 invalidate**：脏区标记是 View 的责任，Model 只负责 `refreshCount++`。

`refreshCount` 是 Model 与 View 之间唯一的"有新数据"信号。

### 6.3 `screenView::handleTickEvent()`：把数据变化翻译成脏区

`TouchGFX/gui/src/screen_screen/screenView.cpp:34-77`：

```cpp
if (data.refreshCount != lastRefresh) {
    lastRefresh = data.refreshCount;
    scope.invalidatePlot();                     // 波形区：每次新数据
    if ((lastRefresh % 3u) == 0u)
        scope.invalidateReadout();              // 读数区：约 7Hz
}
if (data.exportActive) scope.invalidateHeader();
if (!data.exportActive && (linkOk 变化 || linkFrames/20 变化))
    scope.invalidateHeader();                   // 标题栏：约 3Hz
```

**降频策略是性能关键**：波形区必须每帧刷，读数/标题栏按需刷。全屏每帧重绘会直接吃掉一帧预算。

### 6.4 点击处理（`screenView.cpp:79-178`）

```cpp
if (event.getType() == ClickEvent::PRESSED) {
    int8_t button = hitTest(event.getX(), event.getY());   // 自己算
    scope.setPressedButton(button);                        // 按下高亮
    switch (button) {
        case BTN_TB0 ... BTN_TB4:
            presenter->setTimebase(button - BTN_TB0);
            scope.invalidatePlot();
            break;
        ...
    }
    scope.invalidatePanel();
}
else {  // RELEASED：清高亮
    pressedButton = BTN_NONE;
    scope.setPressedButton(BTN_NONE);
    scope.invalidatePanel();
}
screenViewBase::handleClickEvent(event);   // 交回基类
```

命中测试是纯几何计算（`hitTest`，`screenView.cpp:180-271`），因为整个界面只有一个 Widget，框架不知道"按钮"的概念：

```
PRESSED(x,y) → 遍历当前页面的按钮矩形 → 返回 BTN_* 编号
→ presenter 改 Model 数据
→ Model::pushTimebase() 下发命令帧给采集端
→ invalidate 立即反馈（不等下一帧数据）
```

### 6.5 三层的职责边界

| 层 | 允许做 | 禁止做 |
|---|---|---|
| Model | 解析数据、测量、状态机、下发命令 | 直接改 Widget、调 invalidate、阻塞等待 |
| Presenter | 转调用、Model 与 View 的解耦 | 存业务数据 |
| View | 事件处理、命中测试、invalidate | 解析协议、长时间计算 |

---

## 7. 阶段五：绘制层

### 7.1 `invalidate` 的语义

```cpp
widget.invalidate();                     // 整个 widget 矩形变脏
widget.invalidateRect(rect);             // 指定矩形变脏
```

做的事：把矩形做裁剪/合并后加入脏区列表，**并把失效向上传播到父容器**。不做绘制。

真正绘制统一发生在 `HAL::tick` 第 7 步。

### 7.2 `draw()` 必须按 `invalidatedArea` 裁剪

```cpp
virtual void draw(const touchgfx::Rect& invalidatedArea) const;
```

`ScopeWidget::draw` 的正确写法：

```cpp
void draw(const Rect& invalidatedArea) const override
{
    Rect me(0, 0, getWidth(), getHeight());
    Rect dirty = invalidatedArea & me;       // 先求交集
    if (dirty.isEmpty()) return;

    fillRectClip(dirty, COLOR_BG);           // 所有填充都带 clip
    drawGrid(dirty);
    drawAnalog(dirty);
    ...
}
```

不裁剪会有两个后果：

1. 覆盖上层/下层控件本该显示的内容（控件重叠时花屏）
2. 单缓冲策略下，每次 `draw` 可能只针对一小段"安全带"，全画一遍等于重复刷整屏，帧率暴跌且出现撕裂

### 7.3 单缓冲为什么能避免撕裂

官方说明（`HAL.hpp:1085-1091`）：

> TouchGFX will carefully track the progress of the TFT controller, and draw parts of the framebuffer whenever possible. The effect is that the risk of tearing is much reduced compared to the default single buffer strategy of only drawing in porch areas.

机制：

1. `getTFTCurrentLine()`（`TouchGFXGeneratedHAL.cpp:113-125`）读 `LTDC->CPSR`，减去 back porch，得到 LTDC 当前扫描行
2. HAL 只绘制**不会被本帧再次读取**的区域
3. 绘制速度必须快于扫描速度，否则本次 tick 画不完 → 下一个 VSync 被丢弃 → 视觉降帧

本工程波形区约 340×190，加上网格与数字通道，是整帧最重的部分。如果未来加双层波形或更多通道，优先考虑：

- 缩小 `invalidate` 区域（只刷变化的采样段）
- 改用 DMA2D 加速填充（当前 `STM32DMA::getBlitCaps()` 返回 0，未启用）
- 提高到双缓冲（需要额外 255KB SDRAM）

---

## 8. 一次点击的完整调用栈

以"点击 1ms 时基按钮"为例：

```
[LTDC 行中断]  signalVSync()                       vsync_sem = 1
[主循环]       MX_TouchGFX_Process()
               → touchgfx_taskEntry()
               → isVSyncAvailable() == true
               → HAL::backPorchExited()
               → HAL::tick()
                   ├ sampleTouch()               读取 FT5336 → (x, y)
                   ├ 触摸状态机                    → ClickEvent(PRESSED, x, y)
                   ├ Application::handleTickEvent()
                   │   ├ Model::tick()             解析串口/生成假数据
                   │   └ screenView::handleTickEvent()
                   │       └ 比较 refreshCount → invalidatePlot()
                   ├ currentScreen->handleClickEvent(event)
                   │   └ screenView::handleClickEvent()
                   │       ├ hitTest(x, y) → BTN_TB2
                   │       ├ presenter->setTimebase(2)
                   │       │   └ Model::pushTimebase()
                   │       │       └ ScopeLink::sendCommand(CMD_TIMEBASE, 2, 0)
                   │       ├ scope.invalidatePlot()   ← 立即让波形按新时基重画
                   │       └ scope.invalidatePanel()  ← 按钮高亮
                   ├ 遍历脏区 → ScopeWidget::draw(part)
                   │   ├ drawGrid → drawAnalog → drawDigital → drawPanel
                   └ endFrame()                    vsync_sem = 0
```

总耗时若小于 16.8ms，用户看到的是即点即应；超过则下一次 VSync 被丢弃，出现"点击后稍迟一拍"。

---

## 9. 一次数据刷新的完整时序

串口链路（USART6 + DMA 循环接收）与 GUI 的关系：

```
采集端持续发帧（1706B/帧，20~60fps）
        │
        ▼
DMA2 Stream1 Channel5 循环写入 4096B 缓冲（硬件，不占 CPU）
        │
        ▼ 每帧 tick 里
ScopeLink::poll()：__HAL_DMA_GET_COUNTER 推算写指针
        → 逐字节状态机（找 A5 / 找 5A / 收 12B 头 / 收 1692B 载荷）
        → CRC16 校验 → 解包 12bit + 位打包
        ▼
ScopeData::ch1/ch2/dig[] 更新
        ▼
measure() 算 Vpp/Vrms/频率/占空比
        ▼
refreshCount++
        ▼
screenView::handleTickEvent 发现计数变化
        ▼
invalidatePlot() → 下一次绘制阶段画新波形
```

关键点：**DMA 负责搬运，tick 负责解析，两者解耦**。解析一帧 <1ms，不会拖垮帧率。

---

## 10. 时间预算与测量

每帧 16.8ms 的分配参考：

| 环节 | 典型值 | 备注 |
|---|---|---|
| 触摸 I2C 读 | 0.05~0.3ms | 异常时最多 10ms |
| `Model::tick`（解析 + 测量） | <1.5ms | 依赖帧率与触发扫描 |
| 事件派发 | 微秒级 | 只在点击时 |
| 波形绘制 | 2~6ms | 最大项，与脏区面积近似线性 |
| 面板/文字绘制 | 1~2ms | 已降频到 3~7Hz |
| **合计** | **<10ms 为健康** | 留 40% 余量 |

测量方法（不依赖调试器）：

```cpp
// Model::tick 开头/结尾翻转一个空闲 GPIO
HAL_GPIO_WritePin(GPIOX, PIN, GPIO_PIN_SET);
... 被测代码 ...
HAL_GPIO_WritePin(GPIOX, PIN, GPIO_PIN_RESET);
```

用示波器看脉宽。分别测 `Model::tick` 和 `ScopeWidget::draw`，就能判断是"数据层超时"还是"绘制层超时"。脉宽超过 16.8ms 就一定掉帧。

---

## 11. 故障对照表

| 现象 | 最可能原因 | 排查位置 |
|---|---|---|
| 上电卡死，无任何输出 | `setFrameRefreshStrategy` 返回 false 后 `while(1)` | `TouchGFXGeneratedHAL.cpp:44-47`；是否注册了 taskDelay、是否实现 `getTFTCurrentLine` |
| 黑屏/白屏，程序正常跑 | 未实现 `HAL_LTDC_LineEventCallback` | `TouchGFXHAL.cpp:36-42`；用逻辑分析仪看 LTDC_IRQ 是否进 |
| 画面完全不刷新 | 帧缓冲地址不对 / LTDC `CFBAR` 未指向 `frameBuf` | `setFrameBufferStartAddresses` 调用顺序 |
| 画面闪烁、控件错位 | `draw` 未按 `invalidatedArea` 裁剪 | `ScopeWidget::draw` 是否用 `dirty = invalidatedArea & me` |
| 点击无响应 | 触摸返回值语义反了 / 坐标轴未交换 | `STM32TouchController::sampleTouch`，`x = rawY` |
| 点击位置偏移 | 分辨率或方向不匹配 | `setDisplayOrientation(LANDSCAPE)`、`480×272` |
| 切页卡顿、点击丢失 | `Model::tick` 内阻塞（UART 发送、长循环） | 检查是否有 `HAL_UART_Transmit` 超时参数过大 |
| 波形撕裂 | 绘制时间超过一帧预算 | 测量 `draw` 脉宽，缩小脏区 |
| 串口一有数据就花屏 | 解析中直接改正在绘制的缓冲 | Model 与 View 必须通过 `refreshCount` 解耦 |

---

## 12. 关键数字速查

| 项 | 值 |
|---|---|
| 屏幕 | 480 × 272，RGB565（2 B/px） |
| 帧缓冲 | 261,120 B ≈ 255 KB，位于 SDRAM `0xC0000000` 起 |
| LTDC 总时序 | 565 × 285，像素时钟 9.6 MHz，约 59.6 fps |
| 行中断位置 | 第 10 行（有效区前一行），NVIC 优先级 9 |
| 每帧预算 | 16.8 ms |
| SYSCLK | 108 MHz |
| 链路波特率 | USART6，921600 8N1 |
| 数据帧 | 1706 B（头 12 + 载荷 1692 + CRC 2） |
| 采样点 | 500 点/通道，12 bit |
| 时基 | 10ms / 5ms / 1ms / 500µs / 100µs 每格，一屏 10 格 |

---

## 13. 扩展指引

### 13.1 加第二屏

1. Designer 里新建 Screen（哪怕是空屏幕）
2. 生成代码后，新屏幕会出现在 `FrontendApplicationBase` 的 `gotoXXXScreen` 里
3. 在 `FrontendApplication.hpp` 中调用 `application().gotoscreen2ScreenNoTransition()`
4. 新屏幕的 View 同样可以 `add(自定义Widget)`

注意：`FrontendHeap.hpp` 中的 `UserDefinedViewTypes` 需要把你新增的自定义 View 类型加进去，否则静态内存池按最大类型分配时会算漏。

### 13.2 把 `ScopeWidget` 变成 Designer 可见的控件

现在 Designer 画布为空，因为 `receive.touchgfx` 里 `Components: []`。若想让它在 Designer 中可见：

1. 把 `ScopeWidget` 包成 `CustomContainer`（继承 `touchgfx::Container`，并实现 `setupScreen`）
2. 在 Designer 中拖入 Custom Container 并选择该类型

代价：需要改 `.ewp` 添加新 `.cpp`，且容器与现有 `hitTest` 逻辑需要重新对齐。**当前阶段不必做**。

### 13.3 改用双缓冲

- 需要额外 255KB SDRAM（F746NG 有 8MB SDRAM，够用）
- `setFrameBufferStartAddresses(fb0, fb1, 0)`
- 去掉 `REFRESH_STRATEGY_OPTIM_SINGLE_BUFFER_TFT_CTRL`，改回 `REFRESH_STRATEGY_DEFAULT`
- 收益：彻底无撕裂、帧率更稳；代价：SDRAM 带宽翻倍（LTDC 读 + CPU 写不同缓冲）

---

## 附录 A：术语表

| 术语 | 含义 |
|---|---|
| HAL | Hardware Abstraction Layer，TouchGFX 的移植层，负责 VSync、触摸、刷屏 |
| VSync | 垂直同步信号，这里指 LTDC 每帧一次的行中断 |
| 帧缓冲 | LTDC 直接扫描显示的显存区域 |
| 脏区 / invalidate | 需要重绘的矩形集合 |
| invalidatedArea | 传给 `draw()` 的矩形，绘制必须裁剪到它 |
| 单缓冲 | 只有一块帧缓冲，边扫边画，靠跟踪扫描行防撕裂 |
| MVP | Model-View-Presenter，TouchGFX 的应用架构 |
| FrontendHeap | 静态内存池，容纳 view / presenter / transition / model / app |
| 行中断 | LTDC 扫到指定行时触发的中断，本工程用于产生 VSync |
| 预编译库 | `libtouchgfx.a`，框架核心以二进制形式提供，只有头文件可见 |
