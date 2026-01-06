/* axi_tcp_app.h */

#ifndef AXI_TCP_APP_H
#define AXI_TCP_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xil_types.h"
#include "xparameters.h"

/* ========== 1. 硬件配置 ========== */
/* 这里的 Device ID 和中断号保持不变 */
#define INTC_DEVICE_ID   XPAR_PS7_SCUGIC_0_DEVICE_ID
#define SW1_INT_ID       61    /* PL->PS 中断号 */

/* ========== 2. 函数声明 ========== */

/* 初始化 PL->PS 中断（配置 GIC 和 RingBuffer） */
int AXI_IntcInit(u16 DeviceId);

/* 主循环调用：检查缓冲区，计算浮点，批量发送 TCP */
int transfer_data(void);
void ResetRingBuffer(void);
/* [新增] 获取缓冲区溢出计数 */
/* 如果返回值在不断增加，说明主循环处理太慢，或者网络堵塞了 */
u32 get_overflow_count(void);

#ifdef __cplusplus
}
#endif

#endif /* AXI_TCP_APP_H */
