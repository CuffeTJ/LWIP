/* data_proc.c - 数据处理算法实现 */

#include "data_proc.h"
#include <math.h> // 如果将来需要 sin, cos, pow 等函数

/* ========== 私有参数定义 ========== */
/* 这里的参数对外不可见，只有算法内部知道 */
static const double kFactor = 83.14159;
static const double kInvFactor = 1.0 / 83.14159; // 预计算倒数优化除法

/* 如果将来做多项式，可以在这里定义系数 */
// static const double kPolyCoeff[3] = {0.5, 1.2, -0.01};

/* ========== 函数实现 ========== */

void DataProc_Init(void)
{
    /* 目前没有状态需要复位，留空即可 */
    /* 如果将来有 IIR/FIR 滤波器，在这里清零历史 buffer */
}

void DataProc_Process(const RawData *input, ProcessedFrame *output)
{
    /* 1. 填充协议头 (也可以放在 app 层，但放在这里可以让 output 完全就绪) */
    output->header[0] = 0xAA;
    output->header[1] = 0x55;

    /* 2. 核心数学运算 */

    /* 简单的线性变换 */
    output->ch1 = (double)input->d1;
    output->ch2 = (double)input->d2;

    /* 乘法运算 */
    output->ch3 = (double)input->d1 * kFactor;

    output->ch4 = (double)input->d1;
    output->ch5 = (double)input->d1; // 重复数据
    output->ch6 = (double)input->d2;

    /* 除法运算 (使用预计算的倒数进行乘法优化) */
    output->ch7 = (double)input->d1 * kInvFactor;

    output->ch8 = (double)input->d1;
    output->ch9 = (double)input->d1;

    /* --- 扩展示例：如果你将来想做多项式运算 --- */
    /* double x = (double)input->d1;
    output->ch1 = kPolyCoeff[0] + kPolyCoeff[1]*x + kPolyCoeff[2]*x*x;
    */
}
