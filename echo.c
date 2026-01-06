/* echo.c */

#include <stdio.h>
#include <string.h>
#include "axi_tcp_app.h"
#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

/* 给 axi_tcp_app.c 使用的全局连接句柄 */
struct tcp_pcb *g_client_pcb = NULL;

/* 这三个函数给 main.c 调用 */
int transfer_data(void);
void print_app_header(void);
int start_application(void);

/* ========== 1. 应用层打印信息 ========== */

void print_app_header()
{
    xil_printf("\n\r\n\r----- AXI -> TCP sender demo ------\n\r");
    xil_printf("Board will listen TCP port 7.\n\r");
    xil_printf("PL sampled data goes to PS via AXI-Lite interrupt.\n\r");
    xil_printf("PS sends processed data to the connected PC via TCP.\n\r");
}


/* ========== 2. 收到 PC 发来的数据（直接丢弃） ========== */

static err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                               struct pbuf *p, err_t err)
{
	/* 对端关闭连接的情况 */
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		g_client_pcb = NULL;  /* 当前连接失效 */
		xil_printf("TCP connection closed by remote.\n\r");
		return ERR_OK;
	}

	/* 告诉 TCP 栈：我已经收到了这么多数据 */
	tcp_recved(tpcb, p->len);

	/* 这里不需要用这些数据，直接丢掉 */
	pbuf_free(p);

	return ERR_OK;
}

/* 连接错误回调，例如对端异常断开时会触发 */
static void error_callback(void *arg, err_t err)
{
	/* 出错时，连接无效了 */
	g_client_pcb = NULL;
	xil_printf("TCP error callback, err = %d\n\r", err);
}

/* ========== 3. 有新的 TCP 连接进来时 ========== */

static err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	xil_printf("New TCP connection accepted, id = %d\r\n", connection);

	/* 保存当前连接的 pcb，供 transfer_data() 发送用 */
	g_client_pcb = newpcb;

	tcp_nagle_disable(newpcb);
	//ResetRingBuffer();
	/* 设置接收和错误回调 */
	tcp_recv(newpcb, recv_callback);
	tcp_err(newpcb, error_callback);

	/* 把一个简单的数字当作回调参数（可选） */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	connection++;

	return ERR_OK;
}

/* ========== 4. 启动 TCP 服务器，只负责监听 ========== */

int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;   /* 你可以改成自己想要的端口 */

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP server started @ port %d\n\r", port);

	return 0;
}
