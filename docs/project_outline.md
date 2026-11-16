# fish_bin 工程结构

ESP32-S3 自动滴定装置：Kamoer 4460.5 控制蠕动泵，QLT1616 计实体液滴，
OGR350 通过 ADC 判断管路状态。目标以滴计，尚未做毫升标定。
接线、构建和操作见 [BOOT 空管标定与串口滴定](dosing_control_bringup.md)。

| 层 | 当前职责 |
| --- | --- |
| AA_app | 组合根初始化资源，唯一控制任务处理 BOOT、标定、USB/UART0 命令和灯态。 |
| AB_service | `service_bubble` 校验标定窗口并计算电压差；`service_dosing` 管理预停、正向滴定、尾滴、反向回收和终态。 |
| AC_module | Kamoer Modbus RTU 协议及 UART 绑定；GPIO4 的 PCNT 读数通过适配器转成滴定输入快照。 |
| AD_platform | GPIO、UART、ADC、PCNT、面板端口和 OSAL；GPIO48 仅由面板 RMT 使用。 |
| AE_common | 通用错误码、诊断记录、类型和缓冲工具。 |
| AF_config | 板级引脚、电机总线、液滴滤波、标定和滴定控制参数。 |

上电初始化只绑定端口；控制任务先等待空管 BOOT 标定，再接受完整的串口滴数命令。
PCNT 的液滴快照用于到量和超时，ADC 用于在正向运行超时时区分「确认空管且零滴」
与一般故障。达到滴数后停泵，等尾滴安静，再反向运行 500 ms；回收 STOP 得到
应答才报告 DONE。正向 STOP 前计数与包含尾滴的最终计数分别报告。
STOP 应答不能代替物理停稳反馈。

现行固件只有这条控制路径。旧的自动运行和单项连续打印属于早期台架。
主机回归由 `./tools/run_host_tests.sh` 运行，覆盖协议、错误传播、GPIO/PCNT、
标定与双串口控制，以及从 BOOT 到回收完成的应用联调；不能替代清水实测。

器件资料：Kamoer 4460.5 Modbus RTU 手册（RS485.pdf）、QILY QLT1616 计数手册、
SAMOTEY OGR350 使用说明。
