/**
 * @file uart_port.h
 * @brief 定义 ESP32-S3 UART 与 RS485 半双工平台端口接口及可定位错误查询。
 */
#ifndef AD_PLATFORM_MCU_UART_INC_UART_PORT_H
#define AD_PLATFORM_MCU_UART_INC_UART_PORT_H

#include "fw_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 逻辑 UART 通道标识枚举，硬件映射由 AF_config/board_config.h 定义。 */
typedef enum
{
    CORE_UART_ID_1 = 0, /**< 逻辑串口通道1（默认绑定 UART1）。 */
    CORE_UART_ID_2 = 1, /**< 逻辑串口通道2（默认绑定 UART2）。 */
    CORE_UART_ID_MAX    /**< 逻辑串口通道数量上界。 */
} core_uart_id_t;

/** @brief UART 物理收发工作模式枚举。 */
typedef enum
{
    CORE_UART_MODE_NORMAL = 0,       /**< 标准全双工 UART 模式。 */
    CORE_UART_MODE_RS485_HALF_DUPLEX /**< 硬件自动控制 RTS(DE/RE) 的 RS485 半双工模式。 */
} core_uart_mode_t;

/** @brief UART 奇偶校验位配置枚举。 */
typedef enum
{
    CORE_UART_PARITY_NONE = 0, /**< 无校验（8N1/8N2）。 */
    CORE_UART_PARITY_EVEN,     /**< 偶校验（8E1）。 */
    CORE_UART_PARITY_ODD       /**< 奇校验（8O1）。 */
} core_uart_parity_t;

/** @brief UART 停止位配置枚举。 */
typedef enum
{
    CORE_UART_STOP_BITS_1 = 0, /**< 1位停止位。 */
    CORE_UART_STOP_BITS_2      /**< 2位停止位。 */
} core_uart_stop_bits_t;

/** @brief UART 通道主生命周期状态枚举，仅 CORE_UART_STATE_FAULT 表示终态故障。 */
typedef enum
{
    CORE_UART_STATE_UNINITIALIZED = 0, /**< 尚未初始化或已反初始化。 */
    CORE_UART_STATE_READY,             /**< 驱动已安装并就绪，可正常收发。 */
    CORE_UART_STATE_FAULT              /**< 发生硬件或驱动故障，已锁存首个失败记录。 */
} core_uart_state_t;

/** @brief UART 错误定位中的操作阶段枚举。 */
typedef enum
{
    CORE_UART_STAGE_INIT = 1, /**< 参数配置、引脚绑定与驱动安装阶段。 */
    CORE_UART_STAGE_CONFIG,   /**< 运行期修改波特率或模式阶段。 */
    CORE_UART_STAGE_TX,       /**< 发送数据或等待发送完成阶段。 */
    CORE_UART_STAGE_RX,       /**< 接收读取、查询缓存或清空缓冲阶段。 */
    CORE_UART_STAGE_DEINIT,   /**< 驱动卸载与反初始化阶段。 */
    CORE_UART_STAGE_VALIDATE  /**< 内部状态机合法性校验阶段。 */
} core_uart_stage_t;

/** @brief UART 通道初始化配置参数结构体。 */
typedef struct
{
    uint32_t baudrate;               /**< 波特率，单位bps，大于0。 */
    core_uart_mode_t mode;           /**< 普通全双工或 RS485 半双工模式。 */
    core_uart_parity_t parity;       /**< 奇偶校验模式。 */
    core_uart_stop_bits_t stop_bits; /**< 停止位数量。 */
    int32_t tx_pin;                  /**< TX 物理引脚号，必须为有效非负引脚。 */
    int32_t rx_pin;                  /**< RX 物理引脚号，必须为有效非负引脚。 */
    int32_t rts_de_pin;              /**< RS485 DE/RE 控制引脚号；不使用时为 BOARD_GPIO_PIN_NC(-1)。 */
    uint32_t rx_buf_bytes;           /**< 驱动接收环形缓冲区大小，单位bytes，必须大于128。 */
    uint32_t tx_buf_bytes;           /**< 驱动发送环形缓冲区大小，单位bytes（0表示阻塞写FIFO）。 */
} core_uart_config_t;

/**
 * @brief 读取指定逻辑串口的板级默认配置参数。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[out] config 输出默认配置结构体，不得为NULL；失败时不修改。
 * @retval FW_OK 默认配置已填充。
 * @retval FW_ERR_UART_INVALID_ARG id 越界或 config 为NULL。
 * @note 纯配置查询，不访问硬件，可在任意上下文调用。
 */
fw_error_t core_uart_get_default_config(core_uart_id_t id,
                                        core_uart_config_t *config);

/**
 * @brief 初始化指定逻辑 UART 通道并安装底层驱动。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[in] config 串口配置指针；传NULL时自动使用板级默认配置。
 * @retval FW_OK 串口配置与驱动安装成功，通道进入 CORE_UART_STATE_READY。
 * @retval FW_ERR_UART_INVALID_ARG id 越界或 config 参数非法（非越界实例记录初始化失败点）。
 * @retval FW_ERR_UART_CONFIG_FAILED 底层参数、引脚或模式配置失败。
 * @retval FW_ERR_UART_DRIVER_FAILED 底层驱动安装失败。
 * @note 仅任务上下文调用；支持在 FAULT 或 UNINITIALIZED 状态下显式重新初始化。
 */
fw_error_t core_uart_init(core_uart_id_t id, const core_uart_config_t *config);

/**
 * @brief 反初始化指定逻辑 UART 通道并卸载底层驱动。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @retval FW_OK 驱动已卸载，通道回到 CORE_UART_STATE_UNINITIALIZED。
 * @retval FW_ERR_UART_INVALID_ARG id 越界。
 * @retval FW_ERR_UART_DRIVER_FAILED 底层驱动卸载失败。
 * @note 仅任务上下文调用。
 */
fw_error_t core_uart_deinit(core_uart_id_t id);

/**
 * @brief 动态修改已初始化 UART 通道的通信波特率。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[in] baudrate 新的目标波特率，单位bps，必须大于0。
 * @retval FW_OK 波特率更新成功。
 * @retval FW_ERR_UART_INVALID_ARG id 越界或 baudrate 为0。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_CONFIG_FAILED 底层修改波特率失败。
 * @return 若通道处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_uart_set_baudrate(core_uart_id_t id, uint32_t baudrate);

/**
 * @brief 有界阻塞发送完整字节流，并在返回前等待硬件 TX FIFO（含 RS485 方向切换）发送完毕。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[in] data 待发送字节缓冲区，不得为NULL；返回后调用方即可安全复用。
 * @param[in] len_bytes 待发送字节数，单位bytes，必须大于0。
 * @param[in] timeout_ms 等待发送完成的最大时间预算，单位ms，必须大于0且不为 UINT32_MAX。
 * @retval FW_OK 全部字节已写入并在线上发送完成。
 * @retval FW_ERR_UART_INVALID_ARG 参数为空、长度为0或超时预算非法。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_TIMEOUT 在 timeout_ms 内硬件未能完成发送（非终态，不进入 FAULT）。
 * @retval FW_ERR_UART_TX_FAILED 底层写入或驱动报错，通道进入 FAULT。
 * @return 若通道处于 FAULT，原样返回已锁存的首个故障码，不触发新发送。
 * @note 仅任务上下文调用，同一通道并发发送由上层串行化；最多阻塞 timeout_ms。
 */
fw_error_t core_uart_tx_poll(core_uart_id_t id,
                             const uint8_t *data,
                             size_t len_bytes,
                             uint32_t timeout_ms);

/**
 * @brief 发送一个最多64字节的短帧，直接尝试写FIFO一次并有界等待TX完成。
 * @param[in] id 逻辑串口0..CORE_UART_ID_MAX-1。
 * @param[in] data 帧数据，不得为NULL；返回后可复用。
 * @param[in] len_bytes 1..64字节，不会循环等待FIFO空间。
 * @param[in] timeout_ms 发送完成等待预算，1..UINT32_MAX-1ms。
 * @retval FW_OK 整帧已发出，不代表设备已ACK。
 * @retval FW_ERR_UART_INVALID_ARG 参数无效或通道启用了TX软件环形缓冲。
 * @retval FW_ERR_UART_NOT_READY 未初始化。
 * @retval FW_ERR_UART_TIMEOUT TX完成超时，发送结果未知，不自动重发。
 * @retval FW_ERR_UART_TX_FAILED FIFO只接受部分字节或底层失败，通道进入FAULT。
 * @return FAULT时返回首错，无新IO；非法状态报告FW_ERR_UART_INVALID_STATE。
 * @note 仅任务上下文；该通道必须由一个任务独占且tx_buf_bytes=0，
 * 不得混用异步发送或跨任务配置。FIFO写入不等待空间，TX等待至多timeout_ms加一节拍。
 */
fw_error_t core_uart_tx_frame(core_uart_id_t id, const uint8_t *data,
                              size_t len_bytes, uint32_t timeout_ms);

/**
 * @brief 非阻塞将数据写入驱动发送环形缓冲区（立即返回，不等待物理发送完成）。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[in] data 待发送字节缓冲区，不得为NULL；驱动复制入队后可立即复用。
 * @param[in] len_bytes 期望入队的字节数，单位bytes，必须大于0。
 * @param[out] queued_bytes 可选输出实际入队字节数，可为NULL。
 * @retval FW_OK 全部 len_bytes 字节均已复制入发送环形缓冲区。
 * @retval FW_ERR_UART_INVALID_ARG 参数无效。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_BUSY 发送缓冲区空间不足，仅入队 *queued_bytes 字节（不进入 FAULT）。
 * @retval FW_ERR_UART_TX_FAILED 底层写入返回负错误码，通道进入 FAULT。
 * @return 若通道处于 FAULT，原样返回首个故障码。
 * @note 仅任务上下文调用，不等待物理线传输结束。
 */
fw_error_t core_uart_tx_async(core_uart_id_t id,
                              const uint8_t *data,
                              size_t len_bytes,
                              size_t *queued_bytes);

/**
 * @brief 在给定时间预算内从 UART 接收缓冲区读取数据。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[out] buf 接收数据缓冲区，不得为NULL。
 * @param[in] max_len_bytes 本次最多读取字节数，单位bytes，必须大于0。
 * @param[out] out_read_bytes 输出实际读取到的字节数，不得为NULL；超时无新数据时置0。
 * @param[in] timeout_ms 最大等待时间，单位ms；0表示非阻塞立即读取已有数据。
 * @retval FW_OK 读取操作正常完成（实际读取长度见 *out_read_bytes，0表示预算内无新数据）。
 * @retval FW_ERR_UART_INVALID_ARG 参数无效，out_read_bytes（若非NULL）置0。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_RX_FAILED 底层驱动读取返回错误，通道进入 FAULT。
 * @return 若通道处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用；无新数据属于正常等待结果，不会将通道置为 FAULT。
 */
fw_error_t core_uart_rx_read(core_uart_id_t id,
                             uint8_t *buf,
                             size_t max_len_bytes,
                             size_t *out_read_bytes,
                             uint32_t timeout_ms);

/**
 * @brief 查询当前 UART 接收环形缓冲区中已缓存的可读字节数。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[out] available_bytes 输出可读字节数，不得为NULL；失败时置0。
 * @retval FW_OK 查询成功。
 * @retval FW_ERR_UART_INVALID_ARG 参数无效。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_RX_FAILED 底层查询失败，通道进入 FAULT。
 * @return 若通道处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用，不阻塞。
 */
fw_error_t core_uart_rx_available(core_uart_id_t id, size_t *available_bytes);

/**
 * @brief 清空指定 UART 通道的接收环形缓冲区与硬件 FIFO。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @retval FW_OK 接收缓冲区已清空。
 * @retval FW_ERR_UART_INVALID_ARG id 越界。
 * @retval FW_ERR_UART_NOT_READY 通道尚未初始化。
 * @retval FW_ERR_UART_RX_FAILED 底层清空失败，通道进入 FAULT。
 * @return 若通道处于 FAULT，原样返回已锁存的首个故障码。
 * @note 仅任务上下文调用。
 */
fw_error_t core_uart_rx_flush(core_uart_id_t id);

/**
 * @brief 读取指定 UART 通道的主状态与首个失败快照（初始化失败后也可查询）。
 * @param[in] id 逻辑串口标识，范围 0..(CORE_UART_ID_MAX-1)。
 * @param[out] state 输出当前主状态，不得为NULL。
 * @param[out] error 输出完整失败快照，不得为NULL。
 * @retval FW_OK 快照复制成功。
 * @retval FW_ERR_UART_INVALID_ARG id 越界或输出指针为NULL。
 * @note 不阻塞，不依赖串口打印。
 */
fw_error_t core_uart_get_status(core_uart_id_t id,
                                core_uart_state_t *state,
                                fw_error_record_t *error);

#ifdef __cplusplus
}
#endif
#endif /* AD_PLATFORM_MCU_UART_INC_UART_PORT_H */
