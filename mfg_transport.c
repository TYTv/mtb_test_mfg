/*
 * mfg_transport.c
 *
 *  Created on: 2020年6月12日
 *      Author: chlu
 */
#include <stdlib.h>
#include "FreeRTOS.h"
#include "cybsp.h"
#include "cyhal.h"
#include "cy_retarget_io.h"
#include "cyhal_hw_types.h"
#include "cyhal_uart.h"
#include "queue.h"
#include "semphr.h"
#include "stream_buffer.h"
#include "task.h"
#include "cy_syslib.h"
/* Task header files */
#include "mfg_task.h"

#define   RX_FIFO_SIZE                      1500
#define   TX_FIFO_SIZE                      1500

StreamBufferHandle_t  mfg_rx_fifo;
cyhal_uart_t mfg_handler;

uint32_t mfg_bt_readable(void)
{
    return cyhal_uart_readable(&mfg_handler);
}

uint8_t mfg_bt_getchar(void)
{
    uint8 c = 0;
    cyhal_uart_getc(&mfg_handler, &c, 0);
    return c;
}

void mfg_bt_putchar(char c)
{
    cyhal_uart_putc(&mfg_handler, c);
}

void mfg_tx_irq(void)
{

}

/*
 * RX interrupt.
 */
void mfg_rx_irq(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t uart_byte;

       while(cyhal_uart_readable(&mfg_handler)){
            cyhal_uart_getc(&mfg_handler,&uart_byte,0);
            xStreamBufferSendFromISR(mfg_rx_fifo,&uart_byte,sizeof(uint8_t),&xHigherPriorityTaskWoken);
            taskYIELD();
       }

    return;
}

/*
 * UART event handler/In IRQ.
 */
void mfg_io_event_cb(void *arg,cyhal_uart_event_t event)
{

#ifdef  SUPPORT_SEGGER
     SEGGER_RTT_printf(0, "mfg_io_event_cb event:0x%x\n",event);
#endif

    if(CYHAL_UART_IRQ_RX_NOT_EMPTY&event){
    	mfg_rx_irq();
    }

    if(CYHAL_UART_IRQ_TX_DONE&event){
    	mfg_tx_irq();
    }
}


/*
 * RX thread.
 */
void mfg_rx_thread_proc(void *ptr)
{
    uint32_t head=0,payload_len=0,data_counter=0;
    uint16_t opcode=0;
    size_t   xReceivedBytes;
    char     in_data=0;


#ifdef  SUPPORT_SEGGER
    SEGGER_RTT_printf(0, "mfg_rx_thread_proc \n");
#endif

    while(1)
    {
        xReceivedBytes = xStreamBufferReceive( mfg_rx_fifo, &in_data, sizeof(uint8_t), portMAX_DELAY);
        if(0==xReceivedBytes)
            continue;
#ifdef  SUPPORT_SEGGER
        SEGGER_RTT_printf(0, "mfg_rx_thread_proc rx:0x%x\n",in_data);
#endif

    }
}

void mfg_tran_init(void)
{

    const cyhal_uart_cfg_t uart_config =
    {
        .data_bits = 8,
        .stop_bits = 1,
        .parity = CYHAL_UART_PARITY_NONE,
        .rx_buffer = NULL,
        .rx_buffer_size = 0,
    };

    cy_rslt_t result = cyhal_uart_init(&mfg_handler, CYBSP_BT_UART_TX, CYBSP_BT_UART_RX, NULL, &uart_config);
    if(result != CY_RSLT_SUCCESS){
        Cy_SysLib_AssertFailed(__FILE__,__LINE__);
        return;
    }

    result = cyhal_uart_set_baud(&mfg_handler, 115200, NULL);
    if(result != CY_RSLT_SUCCESS){
        Cy_SysLib_AssertFailed(__FILE__,__LINE__);
        return;
    }

    result = cyhal_uart_set_flow_control(&mfg_handler,CYBSP_BT_UART_CTS,CYBSP_BT_UART_RTS);
    if(result != CY_RSLT_SUCCESS){
        Cy_SysLib_AssertFailed(__FILE__,__LINE__);
        return;
    }

    //Power BT up.
    result = cyhal_gpio_init(CYBSP_BT_POWER, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, 1);
    CY_ASSERT(result == NULL);

#if 0
    cyhal_uart_register_callback(&mfg_handler,mfg_io_event_cb,NULL);
    cyhal_uart_enable_event(&mfg_handler,
                            (cyhal_uart_event_t)
                            ( CYHAL_UART_IRQ_RX_DONE|
                              CYHAL_UART_IRQ_TX_DONE|
                              CYHAL_UART_IRQ_RX_NOT_EMPTY),
                              CYHAL_ISR_PRIORITY_DEFAULT,
                              1);

    mfg_rx_fifo = xStreamBufferCreate(RX_FIFO_SIZE,1);
    if(NULL == mfg_rx_fifo){
        Cy_SysLib_AssertFailed(__FILE__,__LINE__);
        return;
    }

    if(xTaskCreate(mfg_rx_thread_proc,"rx_thread_proc",1024,NULL,1,NULL)!=pdPASS){
        Cy_SysLib_AssertFailed(__FILE__,__LINE__);
        return;
    }
#endif
}
