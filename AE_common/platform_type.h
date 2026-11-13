/**
 * @file platform_type.h
 * @brief 提供跨模块基础C标准整数类型与历史兼容类型别名。
 */
#ifndef AE_COMMON_PLATFORM_TYPE_H
#define AE_COMMON_PLATFORM_TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 兼容旧模块的字符别名。 */
typedef char char_t;
/** @brief 兼容旧模块的无符号字节别名。 */
typedef uint8_t uchar_t;
/** @brief 兼容旧模块的8位布尔值别名。 */
typedef uint8_t bool_t;

#ifdef __cplusplus
}
#endif
#endif /* AE_COMMON_PLATFORM_TYPE_H */
