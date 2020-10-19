/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the Empty PSoC6 Application
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* (c) 2019-2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

#include "cy_retarget_io.h"

/* FreeRTOS header file */
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

/* Task header files */
#include "mfg_task.h"

#define GPIO_INTERRUPT_PRIORITY             (7)

/* This enables RTOS aware debugging */
volatile int uxTopUsedPriority;

int main(void)
{
    cy_rslt_t result;

    /* This enables RTOS aware debugging in OpenOCD. */
    uxTopUsedPriority = configMAX_PRIORITIES - 1;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    CY_ASSERT(result == NULL);

    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
    CY_ASSERT(result == NULL);

    /* Configure GPIO interrupt. */
    //cyhal_gpio_register_callback(CYBSP_USER_BTN, gpio_interrupt_handler, NULL);
    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL, GPIO_INTERRUPT_PRIORITY, true);

    __enable_irq();

#ifdef SUPPORT_SEGGER
    SEGGER_RTT_Init();
    {
        uint8_t segger_rx_buf[4];
            while(!SEGGER_RTT_HasData(0));
        SEGGER_RTT_printf(0, "go \n" );
        SEGGER_RTT_Read(0,segger_rx_buf,1);
    }

#endif

    /* Initialize retarget-io to use the debug UART port. */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    CY_ASSERT(result == NULL);

    SUPPORT_MFG_TEST = cyhal_gpio_read(CYBSP_USER_BTN1);		// P0_4

#ifndef  SUPPORT_SEGGER
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen. */
    printf("\x1b[2J\x1b[;H");
    if( SUPPORT_MFG_TEST == MFG_WIFI_TEST ){
        printf("============================================================\n");
    	printf("              MTB TEST MFG ( WIFI )\n");
        printf("============================================================\n");
        printf("Please press USER_BTN1(P0_4) and reset the development board to change to bluetooth mode.\n\n");
    }else{
        printf("============================================================\n");
    	printf("              MTB TEST MFG ( BLUETOOTH )\n");
        printf("============================================================\n");
        printf("Please reset the development board to change to wifi mode.\n\n");
    }

#else
    SEGGER_RTT_printf(0, "\x1b[2J\x1b[;H");
    SEGGER_RTT_printf(0, "MFG task \n");
#endif
    /* Create the tasks. */
    xTaskCreate(mfg_task, "MFG task", MFG_TASK_STACK_SIZE, NULL, MFG_TASK_PRIORITY, &mfg_task_handle);

    /* Start the FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never get here. */
    CY_ASSERT(0);

//    for (;;)
//    {
//    }
}

/* [] END OF FILE */
