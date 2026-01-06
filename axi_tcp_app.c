/* axi_tcp_app.c - Float Processing Version */

#include "axi_tcp_app.h"
#include "data_proc.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "xscugic.h"
#include "lwip/tcp.h"
#include "xil_printf.h"
#include "xil_cache.h"

/* ========== 1. 硬件与参数定义 ========== */
#define AXI_LITE_BASEADDR      0x43C00000U
#define AXI_REG_DATA0_OFFSET   0x00U
#define AXI_REG_DATA1_OFFSET   0x04U
#define AXI_REG_DATA2_OFFSET   0x08U
#define AXI_REG_DATA3_OFFSET   0x0CU

#define INTC_DIST_BASE_ADDR    XPAR_SCUGIC_0_DIST_BASEADDR
#define INTC_CPU_BASE_ADDR     XPAR_SCUGIC_0_CPU_BASEADDR
#define INT_CFG0_OFFSET        0x00000C00U

/* 环形缓冲区大小 (2的幂) */
#define RING_BUF_SIZE   1048576
#define RING_BUF_MASK   (RING_BUF_SIZE - 1)

/* 批量处理阈值：积攒 10 个点 (10ms) 发一次 */
/* 注意：不能太大，否则超过 MTU (1500字节) 会导致 TCP 分片，影响效率 */
#define BATCH_SIZE      20

typedef struct {
    ProcessedFrame frames[BATCH_SIZE];
} __attribute__((packed)) TcpBatchPacket;

/* ========== 3. 全局变量 ========== */
extern struct tcp_pcb *g_client_pcb;

/* 环形缓冲区 (存原始 u32) */
static volatile RawData g_ring_buf[RING_BUF_SIZE];
static volatile u32 g_head = 0;
static volatile u32 g_tail = 0;

/* 发送缓冲区 */
static TcpBatchPacket g_tx_pkt;
static volatile u32 g_overflow_cnt = 0;

/* ========== 4. 辅助函数 (GIC配置) ========== */
u32 get_overflow_count(void) { return g_overflow_cnt; }

static void IntcTypeSetup_LowLevel(u32 DistBaseAddr, int intId, int intType) {
    int mask;
    intType &= 0x03;
    mask = XScuGic_ReadReg(DistBaseAddr, INT_CFG0_OFFSET + (intId / 16) * 4);
    mask &= ~(0x03 << (intId % 16) * 2);
    mask |= intType << ((intId % 16) * 2);
    XScuGic_WriteReg(DistBaseAddr, INT_CFG0_OFFSET + (intId / 16) * 4, mask);
}

static void SetIntrPriority_LowLevel(u32 DistBaseAddr, int IntId, int Priority) {
    u32 RegOffset = 0x400 + (IntId / 4) * 4;
    u32 Shift = (IntId % 4) * 8;
    u32 Val = XScuGic_ReadReg(DistBaseAddr, RegOffset);
    Val &= ~(0xFF << Shift);
    Val |= (Priority << Shift);
    XScuGic_WriteReg(DistBaseAddr, RegOffset, Val);
}

/* ========== 5. 中断服务函数 (只存数，不计算) ========== */
static void SW1_intr_Handler(void *CallbackRef)
{
    u32 next_head = (g_head + 1) & RING_BUF_MASK;

    if (next_head != g_tail) {
        g_ring_buf[g_head].d1 = Xil_In32(AXI_LITE_BASEADDR + AXI_REG_DATA0_OFFSET);
        g_ring_buf[g_head].d2 = Xil_In32(AXI_LITE_BASEADDR + AXI_REG_DATA1_OFFSET);
        g_ring_buf[g_head].d3 = Xil_In32(AXI_LITE_BASEADDR + AXI_REG_DATA2_OFFSET);
        g_ring_buf[g_head].d4 = Xil_In32(AXI_LITE_BASEADDR + AXI_REG_DATA3_OFFSET);

        g_head = next_head;
    } else {
        g_overflow_cnt++;
    }
}

/* ========== 6. 初始化 ========== */
int AXI_IntcInit(u16 DeviceId)
{
    g_head = 0; g_tail = 0; g_overflow_cnt = 0;

    XScuGic_RegisterHandler(INTC_CPU_BASE_ADDR, SW1_INT_ID,
                            (Xil_ExceptionHandler)SW1_intr_Handler, NULL);
    IntcTypeSetup_LowLevel(INTC_DIST_BASE_ADDR, SW1_INT_ID, 0x03); // Rising Edge
    SetIntrPriority_LowLevel(INTC_DIST_BASE_ADDR, SW1_INT_ID, 0xA0);
    XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, SW1_INT_ID);

    xil_printf("AXI App: Float Processing Mode. Batch Size: %d\r\n", BATCH_SIZE);

    DataProc_Init();

    return XST_SUCCESS;
}
void ResetRingBuffer(void)
{
    /* 关键：将读写指针归零，相当于清空缓冲区 */
    g_head = 0;
    g_tail = 0;
    g_overflow_cnt = 0;

    xil_printf("TCP Connected: Ring Buffer Reset!\r\n");
}
/* ========== 7. 主循环发送 (负责计算 double) ========== */
int transfer_data(void)
{
    if (g_client_pcb == NULL) return 0;

    /* 循环发送 (Burst Mode) */
    while (1) {

        /* 1. 检查环形缓冲区剩余数据量 */
        u32 head_snap = g_head;
        u32 tail_snap = g_tail;
        u32 available;

        if (head_snap >= tail_snap) available = head_snap - tail_snap;
        else available = RING_BUF_SIZE + head_snap - tail_snap;

        /* 如果凑不齐20个点，就不发，return 0 */
        if (available < BATCH_SIZE) {
            return 0;
        }

        /* 2. 检查 TCP 发送缓冲区空间 */
        u16_t packet_len = sizeof(TcpBatchPacket);

        /* 如果 TCP 堵了，发不出去，就暂停发送 */
        if (tcp_sndbuf(g_client_pcb) < packet_len) {
            tcp_output(g_client_pcb); // 催促网卡赶紧把旧的发走
            return 0;
        }

        /* 3. 搬运数据*/
        u32 temp_tail = tail_snap;

        for (int i = 0; i < BATCH_SIZE; i++) {
            const RawData *p_raw = (const RawData *)&g_ring_buf[temp_tail];
            ProcessedFrame *p_out = &g_tx_pkt.frames[i];

            // 执行算法处理
            DataProc_Process(p_raw, p_out);

            temp_tail = (temp_tail + 1) & RING_BUF_MASK;
        }

        /* 4. 发送  */
        err_t err = tcp_write(g_client_pcb, (const void*)&g_tx_pkt, packet_len, TCP_WRITE_FLAG_COPY);

        if (err == ERR_OK) {
            // 发送成功，更新尾指针
            g_tail = temp_tail;

            // 继续循环，尝试发下一包！
        } else {
            // 发送失败，尝试推送并退出
            tcp_output(g_client_pcb);
            return -1;
        }
    }

    return 0;
}
