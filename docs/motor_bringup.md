# 电机台架接线与验证

本页记录电机台架。当前默认使用[液滴计数台架模式](drop_count_bringup.md)，
要运行电机台架须先将 `APP_DROP_COUNT_TEST_ENABLE` 设为 0。

## 当前引脚

| 用途 | ESP32-S3 | 外部连接 | 本期状态 |
| --- | --- | --- | --- |
| 电机UART1 TX | GPIO17 | 自动收发模块TTL RX/DI | 已配置 |
| 电机UART1 RX | GPIO16 | 自动收发模块TTL TX/RO | 已配置 |
| 参考地 | GND | 模块参考地；485侧按隔离/非隔离模块说明连接 | 接线确认 |
| DE/RE | 不使用 | 模块自动收发 | 不配置方向GPIO |
| 液滴计数 | GPIO4 | QLT1616经电平适配后的信号 | 计数台架用PCNT采集 |
| 气泡输入 | GPIO5候选 | OGR350经确认/适配后的逻辑A或B | 仅预留，不初始化 |

引脚基于ESP32-S3可用IO选择，具体底板未给出完整型号，仍须核对17/16/4/5未接其他外设。UART0留作默认日志口；应用不初始化UART2，避免占用GPIO19/20的USB功能。[Espressif GPIO说明](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/api-reference/peripherals/gpio.html)。

模块TTL侧必须适配3.3V逻辑。模块RS485 A接驱动板A(D+)，B接B(D-)；驱动板P14/P15并联，不能把TTL接P13（P13为预留RS232口）。

QLT1616棕色为电源正、蓝色为电源负、黑色为信号，手册标4..30V供电及NPN/PNP可切换。先确认输出模式和电平适配电路，再接GPIO4；工业供电电平不能直接送入ESP32-S3 GPIO。OGR350本期不接入软件判定。

## 驱动板拨码

按Kamoer手册，断电设置并核对实物标识：

1. S1第1位OFF，启用RS485控制。
2. 初次测试S1第2位ON，使上电使用9600bps、地址1。
3. S1第5位按总线末端需求接入120欧终端电阻，不能无条件对每个节点都开启。
4. 电机接P1，驱动板电源接P10；手册推荐DC24V，需按实际泵型与供电条件核对。

## 应用入口与默认动作

配置源：`AF_config/motor_test_config.h`。默认`APP_MOTOR_TEST_ENABLE=0`，正常启动不会探测、配置或启动电机。

完成接线和供电确认后，将该宏设为1，重新构建并人工烧录，即会在每次上电后执行一次台架测试：

1. 读取电机反馈、固件/硬件版本与序列号。
2. 发送STOP，设置顺时针和速度模式。
3. 设置起始速度10rpm、运行速度10rpm。
4. 发送一次START，在收到ACK后计时3秒，并周期读取电机反馈。
5. 到期发送STOP；进入`APP_MOTOR_STOP_ACKED`后不再发周期命令。

测试方向、速度和持续时间均集中在该配置文件。测试后应关闭宏，避免后续上电再次执行。起始速度自动限制为不高于本次运行速度；不改变驱动板加减速或细分等已有参数。

应用代码入口为`AA_app/app_motor_test.c/.h`：`init/start/step/cancel/get_result`。组合根静态实例为`AA_app/app_main.c`的`s_motor_test`；本期没有控制台命令解析器，可用调试器查看该实例，或在bootstrap所有者任务调用`app_motor_test_get_result()`取得快照。其他任务不能直接改实例或并发调用驱动。

诊断主要看`state`、`feedback_valid`、`feedback.timestamp_ms`、`stop_acked`及`error`；错误可用`fw_error_code_name()`与`fw_error_code_text_zh()`查询，底层驱动错误见`cause_code/origin_*`。普通错误不自动重发START；FAULT时step不再输出，显式cancel允许再尝试一次STOP并保留首错。

`app_deinit()`属于已静止、已停止所有者任务后的维护清理接口；不得用它跨任务删除正在驱动电机的任务来代替停车。运行期停车应由同一所有者调用cancel。

## 时间与结果语义

单次串口事务总预算250ms，包括RTU静默、短帧发送和接收；9600bps静默按5ms保守取整。发送使用无软件TX环形缓冲的短帧FIFO入口，容量最多64字节，本协议最长13字节。

step每次最多一次普通事务，失败可再执行一次有界STOP清理，因此最坏约500ms加调度误差。测试任务100ms调度，事务超过周期时重新锚定，不连续追赶。运行期限在发起下一次反馈读取前检查，STOP发起可能受到一次在途事务和调度的延迟；3秒不是严格物理停转时间，需实测减速和余液。台架参数不能替代正式滴定的实时停泵设计。

`FW_OK`只表示当前API契约完成；START/STOP写应答只表示收到设备ACK。位置脉冲为驱动板报告值，不能证明泵无失步或液滴已送达。

STOP通信失败时记录`stop_acked=false`和cleanup错误，无法保证物理停车。恢复应先现场确认状态/停车、恢复串口，再显式重建实例；不能仅清除FAULT后重复原投药请求。

## 构建与验证

主机回归：在工程根目录运行`./tools/run_host_tests.sh`。脚本链接真实生产驱动、应用FSM、adapter及平台端口，开启ASan/UBSan和编译告警即错误。

本机ESP-IDF构建环境：

```bash
export IDF_PATH=/home/crowiehuang/.espressif/v6.1/esp-idf
export ESP_ROM_ELF_DIR=/home/crowiehuang/.espressif/tools/esp-rom-elfs/20241011
export PATH=/home/crowiehuang/.espressif/tools/python/v6.1/venv/bin:/home/crowiehuang/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin:$PATH
cmake --build build
```

当前已验证：手册START/STOP/300rpm示例完整字节帧、大小端、反馈解析、CRC/地址/回显/长度错误、设备异常、发送未知结果、首错及底层origin、STOP清理失败、FAULT无重复输出、取消、超时回绕、真实adapter分片收帧、组合根默认无运动及自动收发配置。公共头独立编译通过。

尚未验证：TTL模块方向切换与是否回显、真实总线干扰、RTOS最长调度延迟、任务栈最低余量、轴实际转向/速度/停车时间、液滴计数及药量标定。本次未执行烧录、复位或实际电机运动。

## 下一次实测记录

先记录设备版本/序列号与通讯是否稳定，再用清水观察10rpm下正反转及STOP响应，测量停止后的尾滴。随后确认QLT1616的NPN/PNP、有效边沿与MODE设置，接入PCNT/ISR，做已知滴数对比。最后接入标定后的OGR350与滴定服务。
