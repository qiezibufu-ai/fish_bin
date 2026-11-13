# 分层与命名

## 目录与依赖

目标布局如下。业务目录和文件使用小写snake_case；六个顶层目录按固定名称保留排序。`Core/Drivers/Middlewares`等厂商/生成目录保持原名，不复制修改厂商代码来符合命名。

```text
AA_app/                         应用、初始化组合根、命令适配和业务动作
  app_main.c/.h
  task/                         任务入口与任务配置
  actions/<action>/inc,src/      可独立组合的业务动作
AB_service/<capability>/inc,src/ 能力服务、控制权、状态、调度
AC_module/<device>/driver/inc,src/ 设备协议、寄存器、实例
AC_module/<capability>/wrapper/inc,src/ 稳定的设备能力接口
AC_module/<device>/handler/inc,src/ 可选的设备组合处理
AC_module/adapter/inc,src/       驱动、wrapper与平台端口的静态绑定
AD_platform/mcu/<peripheral>/inc,src/ MCU端口与HAL适配
AD_platform/os/inc,src/          OSAL与RTOS/裸机实现
AE_common/                      基础类型、错误码、算法和工具
AF_config/                      板级、器件、标定、控制和任务配置
tests/ tools/ docs/             测试、生成工具、工程文档
```

图中`inc,src/`表示两个子目录，不是带逗号的实际目录。简单模块可将一对.c/.h直接放在模块目录，不能同一模块一半扁平一半inc/src且没有原因。未用的业务层不创建占位实现。

已有外部API/打包路径需要兼容时，先保留薄入口并登记映射，再逐模块归位；新代码按上述布局。必须迁移的范围由用户任务决定，不能一次搬动未授权的所有模块。

允许依赖：

| 层 | 可依赖 |
|---|---|
| AA_app | AB/AC/AD/AE/AF；组合根装配实际实现 |
| AB_service | 同层公开能力、AC、AD的OS/时间端口、AE、AF |
| AC纯驱动/算法 | 标准库、AE、设备专用寄存器/常量；硬件调用经注入端口 |
| AC wrapper/handler | AC公开接口、AE；handler如需OS则由端口注入 |
| AC adapter | AC驱动/wrapper、AD公开端口、AE、AF；不含业务任务 |
| AD_platform | 标准库、HAL/RTOS、AE、必要AF板级资源；不调用AB/AA业务 |
| AE_common | 标准库和同层基础能力；不包含HAL/RTOS/业务头 |
| AF_config | 常量与配置类型；板级配置可含板级定义，不向纯算法传递HAL头 |

下层需要上层提供数据时，在使用者边界定义port接口，通过组合根绑定。禁止用extern全局业务函数或循环include隐藏反向依赖。纯计算可留在所属模块，不必全部搬到AE。

## 文件与API名

| 对象 | 规则 | 示例 |
|---|---|---|
| 应用文件/API | `app_<name>`或明确业务前缀 | `app_main.c`、`app_init()` |
| 服务文件/API | `service_<capability>` | `service_power.c`、`service_power_request()` |
| 设备驱动 | `bsp_<chip>_driver.c/.h` | `bsp_aht21_driver.h`、`bsp_aht21_read()` |
| 寄存器定义 | `bsp_<chip>_reg.h` | `bsp_aht21_reg.h` |
| 稳定能力wrapper | `bsp_wrapper_<capability>` | `bsp_wrapper_temperature_read()` |
| 绑定adapter | `bsp_adapter_port_<device>` | `bsp_adapter_port_aht21_register()` |
| MCU端口 | 文件`<bus>_port`，API`core_<bus>_*` | `i2c_port.h`、`core_i2c_read()` |
| OS端口 | `osal_<resource>_*` | `osal_mutex_take()` |
| 公共能力 | 明确能力前缀 | `fw_error_code_name()`、`crc8_compute()` |
| 配置 | `<scope>_config.h` | `board_config.h`、`arm_control_config.h` |

新的`AB_service`模块使用`service_*`前缀；已有`chassis_*`等对外API先适配，避免一次破坏所有调用者。项目选择的兼容别名要文档化，禁止新增另一套混用前缀。

- 函数/变量：`lower_snake_case`；类型：`<module>_<meaning>_t`；枚举项/宏：`<MODULE>_<MEANING>`。
- 模块私有静态变量`s_`前缀；结构体实例字段不叠加Hungarian前缀；公开可写全局变量原则上禁止，确需共享由单一模块拥有并提供同步接口。
- 使用`config/command/status/result/ops/context`区分配置、请求、状态、结果和注入接口。不能同名`status`既表示数据又表示错误码。
- 函数指针字段使用动作名如`read_reg/now_ms`，类型完整；`void *context`只传上下文，不通过强制转换修补签名。
- 单位使用`_ms/_us/_mm/_deg/_rad/_rpm/_raw/_pulse/_bytes`。时间tick与毫秒不同，原始编码器计数与命令脉冲不同。
- include guard采用完整路径派生的大写名，例如`AC_MODULE_AHT21_DRIVER_H`；不使用双下划线或实现保留标识符。

## 驱动组成

1. **Driver**：寄存器/协议、时序、校验、设备状态；实例持有ops、context与私有状态。不使用跨实例共享的初始化标志。
2. **Wrapper**：对上层提供读温度/擦Flash等稳定能力；保留状态码与输出有效性。不能用void回调把所有失败吃掉。
3. **Adapter**：将真实总线、时钟、设备实例装配到wrapper，并显式映射平台错误。必需回调缺失时注册失败，不能静默安装空操作。
4. **Handler**：有组合事务、事件或采样流程时才建立；不能只为同签名转发创建handler。

纯驱动统一接受事务级接口。软件/硬件I2C差异优先在端口中处理；确需按位时序接口时独立描述，不让宏切换改变公共结构ABI。实例使用调用者存储或受控静态存储，默认不在控制热路径malloc。

## 初始化与构建

组合根依次准备平台资源、注册绑定、初始化服务、注册任务。注册只绑定能力，初始化只做声明过的硬件操作；不能隐式使能运动执行器。所有初始化失败有原原因、资源回收和可读取状态。

更新目标构建的源文件、模块include可见性、头文件引用及生成钩子。公共头与私有头分开；旧实现不再参与同一资源的运行。没有目标工具链时完成可做的主机验证并明确未构建部分，不能宣称移植已验证。
