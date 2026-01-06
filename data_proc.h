/* data_proc.h - 数据处理模块头文件 */

#ifndef DATA_PROC_H
#define DATA_PROC_H

#include "xil_types.h"

/* ========== 1. 数据结构定义 ========== */
/* 将原来在 axi_tcp_app.c 里的结构体移动到这里 */

/* 原始输入数据 */
typedef struct {
    u32 d1;
    u32 d2;
    u32 d3;
    u32 d4;
} RawData;

/* 处理后的输出帧 */
typedef struct {
    u8      header[2];  /* 0xAA, 0x55 */
    double  ch1;
    double  ch2;
    double  ch3;
    double  ch4;
    double  ch5;
    double  ch6;
    double  ch7;
    double  ch8;
    double  ch9;
} __attribute__((packed)) ProcessedFrame;

/* ========== 2. 接口函数声明 ========== */

/**
 * @brief 初始化处理模块（如果将来需要滤波器状态或加载矩阵参数，可在此初始化）
 */
void DataProc_Init(void);

/**
 * @brief 核心处理函数：将原始数据转换为工程量
 * @param input  指向原始数据的指针
 * @param output 指向输出帧的指针
 */
void DataProc_Process(const RawData *input, ProcessedFrame *output);

#endif /* DATA_PROC_H */
