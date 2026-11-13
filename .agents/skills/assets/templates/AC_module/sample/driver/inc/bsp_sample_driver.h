/**
 * @file bsp_sample_driver.h
 * @brief 提供独立实例、注入式IO和可定位失败的分步采样驱动。
 * @details 此模板协议为示例：ID寄存器0x00、触发寄存器0x01、样本寄存器0x02；
 * 样本为大端有符号16位值加CRC8(多项式0x31、初值0xFF)，每计数10毫摄氏度。
 * 真实设备移植必须替换协议，不得直接将示例地址当作真实芯片配置。
 */
#ifndef AC_MODULE_SAMPLE_DRIVER_H
#define AC_MODULE_SAMPLE_DRIVER_H

#include "fw_error.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 读取寄存器的类型化端口。 */
typedef struct
{
    /**
     * @brief 执行一次有界寄存器读取，不得超过传入的等待预算。
     * @param[in] context 平台实例，寿命覆盖驱动；可按实现约定为NULL。
     * @param[in] reg 寄存器地址。
     * @param[out] data 接收size字节，非NULL；失败时不得供业务消费。
     * @param[in] size 读取字节数，本模板为1或3。
     * @param[in] timeout_ms 本次事务剩余预算，单位ms，大于0。
     * @param[out] cause 可填写底层故障位置和原生码；未提供详细记录时保持清零。
     * @retval FW_OK 全部数据有效。
     * @retval FW_ERR_PORT_BUSY 未接受事务，可在总预算内重试。
     * @return 其他平台具体错误由驱动记录为cause_code；禁止吞掉错误。
     * @note 仅任务上下文，同实例由调用者串行化；不持有data/cause指针。
     */
    fw_error_t (*read_reg)(void *context, uint8_t reg, uint8_t *data,
                           size_t size, uint32_t timeout_ms, fw_error_record_t *cause);
    /**
     * @brief 执行一次有界寄存器写入，接口返回前消费或复制数据。
     * @param[in] context 平台实例，与read_reg共用。
     * @param[in] reg 寄存器地址。
     * @param[in] data 待写入size字节，非NULL。
     * @param[in] size 写入字节数，本模板为1。
     * @param[in] timeout_ms 本次事务最大等待，单位ms，大于0。
     * @param[out] cause 可填写底层故障详情；不保存指针。
     * @retval FW_OK 本次写入按端口契约已完成。
     * @retval FW_ERR_PORT_BUSY 确认没有接受/发出写操作。
     * @return 其他平台错误可能发生在已写入之后，驱动不会自动重发触发命令。
     * @note 仅任务上下文；返回成功不代表传感器转换已完成。
     */
    fw_error_t (*write_reg)(void *context, uint8_t reg, const uint8_t *data,
                            size_t size, uint32_t timeout_ms, fw_error_record_t *cause);
    void *context; /**< 借用的平台上下文，寿命覆盖整个驱动实例。 */
} bsp_sample_ops_t;

/** @brief 生命周期和采样阶段，只有FAULT表示操作失败。 */
typedef enum
{
    BSP_SAMPLE_UNINITIALIZED = 0, /**< 未完成初始化。 */
    BSP_SAMPLE_IDLE,              /**< 初始化完成，等待请求。 */
    BSP_SAMPLE_WAITING,           /**< 已触发转换，等待并读取样本。 */
    BSP_SAMPLE_DONE,              /**< 样本已验证，可读取结果。 */
    BSP_SAMPLE_CANCELLED,         /**< 本地软件操作已取消。 */
    BSP_SAMPLE_FAULT              /**< 已保留首个具体失败原因。 */
} bsp_sample_state_t;

/** @brief 错误定位中的操作阶段。 */
typedef enum
{
    BSP_SAMPLE_STAGE_INIT = 1, /**< 初始化或读取设备ID。 */
    BSP_SAMPLE_STAGE_TRIGGER,  /**< 写入转换触发命令。 */
    BSP_SAMPLE_STAGE_WAIT,     /**< 等待转换时间或重试样本读取。 */
    BSP_SAMPLE_STAGE_READ,     /**< 读取样本数据。 */
    BSP_SAMPLE_STAGE_VALIDATE  /**< 校验或检查非法内部状态。 */
} bsp_sample_stage_t;

/** @brief 初始化时复制的设备与时间配置。 */
typedef struct
{
    uint8_t expected_id;       /**< 预期设备ID，示例测试使用0x42。 */
    uint32_t conversion_ms;    /**< 触发后等待时间，单位ms。 */
    uint32_t timeout_ms;       /**< 整次请求预算，范围1..INT32_MAX，须大于conversion_ms。 */
    uint32_t io_timeout_ms;    /**< 单次IO上界，须在转换后的剩余预算之内且大于0。 */
} bsp_sample_config_t;

/** @brief 值类型实例；调用者可静态分配，字段只能由驱动API修改。 */
typedef struct
{
    bsp_sample_ops_t ops;       /**< 驱动私有端口副本，不直接改写。 */
    bsp_sample_config_t config; /**< 驱动私有配置副本，不直接改写。 */
    bsp_sample_state_t state;   /**< 驱动私有主状态，通过快照查询。 */
    bool initialized;          /**< 驱动私有初始化标志，属于本实例。 */
    uint32_t request_id;       /**< 当前或最后一次请求ID。 */
    uint32_t started_ms;       /**< 请求总预算起点，重试不更新。 */
    int32_t temperature_mc;    /**< 最近成功样本，单位毫摄氏度。 */
    fw_error_record_t error;   /**< 首个失败快照，FAULT轮询不会覆盖。 */
} bsp_sample_t;

/**
 * @brief 复制端口和配置并读取设备ID，失败时留下可查询故障。
 * @param[in,out] self 零初始化或非活动的实例，不得为NULL；禁止在WAITING中调用。
 * @param[in] ops 完整端口表，不得为NULL；表被复制，context仍借用。
 * @param[in] config 有效配置，不得为NULL；配置会被复制。
 * @param[in] now_ms 本地单调时间，单位ms，允许uint32回绕。
 * @retval FW_OK 设备ID通过，实例进入IDLE。
 * @retval FW_ERR_SENSOR_INVALID_ARG 参数无效，非NULL实例保存初始化失败位置。
 * @retval FW_ERR_SENSOR_BUSY 存在活动请求，实例不变。
 * @retval FW_ERR_SENSOR_ID_READ_FAILED 底层读失败，具体原因保存在快照。
 * @retval FW_ERR_SENSOR_ID_MISMATCH ID不符。
 * @note 任务上下文、有界阻塞最多io_timeout_ms加本地处理；调用者串行化。
 * FAULT或CANCELLED可显式重新初始化；初始化不创建任务、不使能执行器。
 */
fw_error_t bsp_sample_init(bsp_sample_t *self, const bsp_sample_ops_t *ops,
                           const bsp_sample_config_t *config, uint32_t now_ms);

/**
 * @brief 触发一次转换并进入WAITING，结果由后续step读取。
 * @param[in,out] self 已初始化且无活动请求的实例，非NULL。
 * @param[in] request_id 非零请求身份，由调用者保证当前范围内不冲突。
 * @param[in] now_ms 请求起点的本地毫秒时间。
 * @retval FW_OK 请求新接受，或当前同ID请求已接受且未重发；不代表采样完成。
 * @retval FW_ERR_SENSOR_INVALID_ARG 参数无效，未接受新请求。
 * @retval FW_ERR_SENSOR_NOT_READY 尚未初始化。
 * @retval FW_ERR_SENSOR_BUSY 本实例忙或平台明确未接受写入，未替换原请求。
 * @retval FW_ERR_SENSOR_TRIGGER_UNKNOWN 写失败且触发结果未知，不自动重发。
 * @return 若实例处于FAULT，返回保留的具体故障码。
 * @note 任务上下文，最多等待io_timeout_ms；同实例调用者串行化。
 * 本实例只保留最近一次请求；该ID的重复提交不重发命令，状态和结果保持。
 * 更早请求/跨重新初始化的去重由调用者处理，新请求必须使用新的非零ID。
 */
fw_error_t bsp_sample_start(bsp_sample_t *self, uint32_t request_id, uint32_t now_ms);

/**
 * @brief 推进一次采样状态，不休眠，必要时只进行一次有界IO。
 * @param[in,out] self 已调用初始化的实例，非NULL。
 * @param[in] now_ms 本地毫秒时间，与start时间源一致，间隔须小于2^31 ms。
 * @retval FW_OK 本周期正常，可能仍在等待，完成依据查看state。
 * @retval FW_ERR_SENSOR_INVALID_ARG self为NULL。
 * @retval FW_ERR_SENSOR_NOT_READY 未初始化。
 * @retval FW_ERR_SENSOR_SAMPLE_TIMEOUT 总预算耗尽。
 * @retval FW_ERR_SENSOR_READ_FAILED 底层样本读取失败。
 * @retval FW_ERR_SENSOR_CRC 样本CRC错误。
 * @retval FW_ERR_SENSOR_INVALID_STATE 内部状态值非法。
 * @return 已在FAULT时原样返回首个故障码，不发新IO。
 * @note 任务上下文，同实例调用者串行化；IO等待不超过剩余总预算。
 */
fw_error_t bsp_sample_step(bsp_sample_t *self, uint32_t now_ms);

/**
 * @brief 取消本地采样流程，不复位设备，不撤销已发出的转换命令。
 * @param[in,out] self 已初始化实例，不得为NULL。
 * @param[in] now_ms 本地毫秒时间，用于记录非法状态发生位置。
 * @retval FW_OK 已取消或此前已取消，后续step不再发IO。
 * @retval FW_ERR_SENSOR_INVALID_ARG self为NULL。
 * @retval FW_ERR_SENSOR_NOT_READY 未初始化。
 * @return FAULT时保留原错误；非法状态通过统一失败入口记录。
 * @note 任务上下文，不阻塞；物理转换可仍在设备内部继续。
 */
fw_error_t bsp_sample_cancel(bsp_sample_t *self, uint32_t now_ms);

/**
 * @brief 获取已验证样本，失败时不改输出。
 * @param[in] self 已初始化实例，不得为NULL。
 * @param[out] temperature_mc 接收毫摄氏度温度，不得为NULL。
 * @retval FW_OK 当前状态DONE且输出有效。
 * @retval FW_ERR_SENSOR_INVALID_ARG 参数无效。
 * @retval FW_ERR_SENSOR_BUSY 尚在测量。
 * @retval FW_ERR_SENSOR_NOT_READY 未初始化或当前没有完成样本。
 * @return FAULT时返回原错误码。
 * @note 不阻塞、仅复制数据；调用者须避免与step并发。
 */
fw_error_t bsp_sample_get_result(const bsp_sample_t *self, int32_t *temperature_mc);

/**
 * @brief 读取主状态和错误快照，初始化失败后也能查询。
 * @param[in] self 零初始化或已使用的实例，不得为NULL。
 * @param[out] state 接收主状态，不得为NULL。
 * @param[out] error 接收完整失败快照，不得为NULL。
 * @retval FW_OK 快照已复制，不代表设备健康。
 * @retval FW_ERR_SENSOR_INVALID_ARG 参数无效，输出不变。
 * @note 不阻塞；跨任务查询须由调用者串行化，函数不创建锁。
 */
fw_error_t bsp_sample_get_status(const bsp_sample_t *self, bsp_sample_state_t *state,
                                 fw_error_record_t *error);

#ifdef __cplusplus
}
#endif
#endif
