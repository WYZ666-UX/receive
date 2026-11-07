# 远程数据采集系统 · 显示端

基于 STM32F746NGH6 + TouchGFX 的示波器式显示端：通过以太网（USART6 可选备份）接收采集端的 500 点波形数据，完成波形显示、参数测量、触发控制、CSV/BMP 导出。

## 功能

- 2 路模拟通道（CH1/CH2）+ 3 路数字通道（D0/D1/D2）
- 五档时基：10 ms / 5 ms / 1 ms / 500 µs / 100 µs 每格
- 模拟测量：峰峰值（99.5%/0.5% 分位）、最大值、频率
- 数字测量：频率、周期、占空比（慢信号按对称方波估算，界面以 `T~` 标注）
- 触发：AUTO / NORMAL / SINGLE，支持 CH1、CH2、D0~D2 触发源，上升/下降沿
- 菜单：触发设置（TRIG）、导出（SAVE），支持通道开关与时基切换
- 导出：USART1 输出 CSV（500 行）与 BMP（整屏 480×272）
- 双帧缓冲 + 局部刷新，波形滚动无撕裂；静态背景缓存降低绘制负载

## 目录结构

```
Core/                 CubeMX 生成的外设初始化、LwIP、USART6 组帧
Drivers/              STM32 HAL 与 CMSIS
Middlewares/          LwIP 与 TouchGFX
TouchGFX/gui/         界面与业务代码
  include/gui/model/      ScopeData（数据+测量）、LinkUdp（协议）、Model
  include/gui/widgets/    ScopeWidget（全部界面绘制）
  src/model/              Model 实现（触发门控、设置下发、导出）
  src/screen_screen/      screenView（刷新调度、触摸命中）
EWARM/                IAR 工程（receive.ewp）与链接脚本
docs/                 协议规范、学习文档
tools/                PC 侧测试与导出工具
```

## 构建与烧录

- IDE：IAR EWARM 8.32，打开 `EWARM/receive.ewp`，配置 `receive`
- 命令行编译（项目路径需用绝对路径；相对路径在全新克隆上会解析不到 `TouchGFX` 源文件）：

```
IarBuild.exe <仓库路径>\EWARM\receive.ewp -build receive
```

- 烧录（ST-Link，SWD）：

```
cspybat -f EWARM/settings/receive.receive.general.xcl --backend -f EWARM/settings/receive.receive.driver.xcl
```

`EWARM/settings/*.xcl` 由 IAR 自动生成，首次打开工程后会重新产生。

## 网络

| 项 | 值 |
|---|---|
| 显示端 IP | 192.168.9.11 |
| 采集端 IP | 192.168.9.10 |
| UDP 端口 | 5000（双方相同） |
| 子网掩码 | 255.255.255.0 |

协议详见 [docs/双端通信协议规范-20261105.md](docs/双端通信协议规范-20261105.md)。

## 使用

1. 网线直连采集端，上电后界面自动建立链路；
2. 顶栏切换时基、通道；`TRIG` 打开触发菜单；`SAVE` 导出；
3. 导出：USART1（PA9 → ST-Link 虚拟串口）在导出期间为 460800；
   - 单次抓取：`python tools/serial_export_capture.py --port COM3`
   - 常驻自动保存：双击 `tools/启动导出监听.bat`，之后点 SAVE 即落盘到 `exports/`

## 工具

| 文件 | 用途 |
|---|---|
| `tools/udp_test.py` | PC 模拟采集端，便于脱离真机调试界面 |
| `tools/testpat_check.py` | 校验协议测试图案的标准答案（CRC32） |
| `tools/serial_export_capture.py` | 单次抓取 CSV/BMP |
| `tools/export_watcher.py` | 后台常驻，自动保存每次导出 |

## 文档

- [双端通信协议规范（v1.2）](docs/双端通信协议规范-20261105.md)
- [系统学习：通信、显示与测量实现](docs/系统学习-通信显示与测量实现.md)
- [TouchGFX 运行流程详解](docs/TouchGFX运行流程详解.md)
- [学习路线](docs/学习路线-STM32示波器项目.md)

## 说明

- 显示端测量口径遵循协议 v1.2：电压 `code × 3.3 / 4095`，峰峰值取分位而非极值；
- 触发对齐由采集端完成，显示端只按帧头 `TRIG_POS` 决定是否上屏并绘制标记；
- USART6 为备份链路，默认使用以太网。
