/*
 * mfg_task.c
 *
 *  Created on: 2020年6月4日
 *      Author: chlu
 */

/*******************************************************************************
 * Header file includes
 ******************************************************************************/
#include "cybsp.h"
#include "cyhal.h"

/* FreeRTOS header file.*/
#include "FreeRTOS.h"

/*Wi-Fi Connection Manager header file.*/
#include "cy_wcm.h"

/*Task header file*/
#include "mfg_task.h"

#include <stdlib.h>
#include "whd_version.h"
#include "whd_chip_constants.h"
#include "whd_cdc_bdc.h"
#include "whd_thread_internal.h"
#include "whd_debug.h"
#include "whd_utils.h"
#include "whd_wifi_api.h"
#include "whd_buffer_api.h"
#include "whd_wlioctl.h"
#include "whd_types.h"
#include "whd_types_int.h"

#include "cy_retarget_io.h"

/******************************************************
 *                    Constants
 ******************************************************/
#define IOCTL_MED_LEN     (8192)

/*******************************************************************************
 * Global Variables
 ******************************************************************************/
TaskHandle_t mfg_task_handle;

static rem_ioctl_t rem_cdc;
char g_rem_ifname[32] = "wl";
unsigned char buf[IOCTL_MED_LEN] = {0};

/* This variable holds the value of the total number of the scan results that is
 * available in the scan callback function in the current scan.
 */
//uint32_t num_scan_result;
//extern whd_interface_t sta_interface;
#define MAX_WHD_INTERFACE                           (2)
extern whd_interface_t whd_ifs[MAX_WHD_INTERFACE];
#define sta_interface whd_ifs[CY_WCM_INTERFACE_TYPE_STA]

uint8_t mfg_retarget_io_getchar(void)
{
    uint8 c = 0;
    cyhal_uart_getc(&cy_retarget_io_uart_obj, &c, 0);
    return c;
}

uint32_t mfg_retarget_io_readable(void)
{
    return cyhal_uart_readable(&cy_retarget_io_uart_obj);
}

void mfg_retarget_io_putchar(char c)
{
    cyhal_uart_putc(&cy_retarget_io_uart_obj, c);
}

/*
 *
 *
 */
void mfg_task(void *arg)
{
    cy_wcm_scan_filter_t scan_filter;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_config_t wcm_config = { .interface = CY_WCM_INTERFACE_TYPE_STA };

#ifdef SUPPORT_SEGGER
    SEGGER_RTT_printf(0, "MFG task start \n");
#endif

#ifdef SUPPORT_BT_TEST
    //Init BT UART
    mfg_tran_init();
#else
    result = cy_wcm_init(&wcm_config);
    CY_ASSERT(result == NULL);
#endif

#if 0
    /* Disable WIFI sleeping */
    cy_wcm_disable_sleep();
    rlt = wlu_server_serial_start();
    error_handler((WICED_SUCCESS!=rlt),"wlu_server already started");
#endif

#ifdef SUPPORT_SEGGER
    SEGGER_RTT_printf(0, "MFG task receive message \n");
#endif


    while (true)
    {
#ifndef SUPPORT_BT_TEST
        memset(buf, 0, sizeof(buf));
        wl_remote_command_handler( buf);
#else
        uint8_t in_byte;

        if(mfg_retarget_io_readable())
        {
        	in_byte = mfg_retarget_io_getchar();
        	mfg_bt_putchar(in_byte);
        }

        if(mfg_bt_readable())
        {
        	in_byte = mfg_bt_getchar();
        	mfg_retarget_io_putchar(in_byte);
        }
#endif
    } /* end of while(true) */

//    return result;

}


int wl_remote_command_handler( unsigned char *buf )
{
    int result = 0;
    unsigned char *buf_ptr = NULL;
    memset(&rem_cdc, 0, sizeof(rem_cdc));

    /* Receive the CDC header */
    if ((wl_remote_rx_header(&rem_cdc)) < 0 )
    {
        MFG_DPRINT_DBG(OUTPUT, "\n Waiting for client to transmit command\n");
        return -1;
    }
    MFG_DPRINT_INFO(OUTPUT, "REC : cmd %d\t msg len %d  msg flag %d\t msg status %d\n",
                    rem_cdc.msg.cmd, rem_cdc.msg.len,
                    rem_cdc.msg.flags, rem_cdc.msg.status);
   /*
    * Allocate buffer only if there is a response message expected.
    * Some commands such as up/down do not output anything.
    */
    if (rem_cdc.msg.len)
    {
        if ((buf_ptr = (unsigned char *)malloc(rem_cdc.msg.len)) == NULL)
        {
            MFG_DPRINT_ERR(ERR, "malloc of %d bytes failed\n", rem_cdc.msg.len);
            return -1;
        }
        memset(buf_ptr, 0, rem_cdc.msg.len);
        /* Receive the data */
        if ((result = wl_remote_rx_data(buf_ptr)) == -1 )
        {
           rem_cdc.msg.status = result;
           if (buf_ptr)
           {
              free(buf_ptr);
           }
           return -1;
        }
        if (( result = wl_remote_tx_response(buf_ptr, 0)) != 0)
        {
            rem_cdc.msg.status = result;
            MFG_DPRINT_ERR(ERR, "\nReturn results failed\n");
        }
        if (buf_ptr)
        {
           free(buf_ptr);
        }
    }
    else
    {
       if (( result = wl_remote_tx_response(buf_ptr, 0)) != 0)
       {
          rem_cdc.msg.status = result;
          MFG_DPRINT_ERR(ERR, "\nReturn results failed\n");
       }
    }
    return result;
}


/* Function: wl_remote_rx_data
 * This function will receive the data from client
 * for different transports
 * In case of socket the data comes from a open TCP socket
 * However in case of dongle UART or wi-fi the data is accessed
 * from the driver buffers.
 */
int wl_remote_rx_data(void* buf_ptr)
{
    if ((wl_remote_CDC_rx( &rem_cdc, (unsigned char *)buf_ptr, rem_cdc.msg.len, 0)) == -1)
    {
        MFG_DPRINT_ERR(ERR, "Reading CDC %d data bytes failed\n", rem_cdc.msg.len);
        return -1;
    }
    return 0;
}

/* Return a CDC type buffer */
int wl_remote_CDC_rx( rem_ioctl_t *rem_ptr, unsigned char *readbuf, uint32_t buflen, int debug)
{
    uint32_t numread = 0;
    int result = 0;

    if (rem_ptr->data_len > rem_ptr->msg.len)
    {
       MFG_DPRINT_ERR(ERR, "wl_remote_CDC_rx: remote data len (%d) > msg len (%d)\n",
                      rem_ptr->data_len, rem_ptr->msg.len);
       return -1;
    }

    if (wl_read_serial_data( readbuf, rem_ptr->data_len, &numread) < 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_read_serial_data: Data Receive failed \n");
        return -1;
    }
    return result;
}

int wl_remote_CDC_rx_hdr( rem_ioctl_t *rem_ptr )
{
    uint32_t numread = 0;
    int result = 0;
    uint32_t len;
    len = sizeof(rem_ioctl_t);

    if (wl_read_serial_data( (unsigned char *)rem_ptr, len,	&numread) < 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_remote_CDC_rx_hdr: Header Read failed \n");
        return -1;
    }
    return result;
}

int wl_read_serial_data( unsigned char *readbuf, uint32_t len, uint32_t *numread )
{
    int result = 0;
    int c;
    int i = 0;

#ifdef SUPPORT_SEGGER
    SEGGER_RTT_printf(0, "wl_read_serial_data: len:%d ",len);
#endif

    memset(buf, 0, sizeof(buf));
    while ( i < (int)len )
    {
        //scanf("%c", &c);
    	c = mfg_retarget_io_getchar();
        buf[i++] = c;
#ifdef SUPPORT_SEGGER
        SEGGER_RTT_printf(0, "0x%x ",c);
#endif
    } /* end of while ( i < (int)len ) */

    if ( i > 0 )
    {
        memcpy(readbuf, buf, i);
        *numread = i;
    } /* end of if */

    return result;
}

int wl_remote_rx_header( rem_ioctl_t *rem_ptr )
{
   if ((wl_remote_CDC_rx_hdr( rem_ptr)) < 0)
   {
       MFG_DPRINT_DBG(OUTPUT, "\n Waiting for client to transmit command\n");
       return -1;
   }

   MFG_DPRINT_INFO(OUTPUT, "%d %d %d %d\r\n", rem_ptr->msg.cmd,
                   rem_ptr->msg.len, rem_ptr->msg.flags, rem_ptr->data_len);
   return 0;
}

/*
 * This function is used for transmitting the response over UART transport.
 * The serial the data is sent to the driver using remote_CDC_dongle_tx function
 * which in turn may fragment the data and send it in chunks to the client.
 *
 * Arguments: buf: Buffer pointer
 *            cmd: ioctl command.
 */
int wl_remote_tx_response( void* buf_ptr, int cmd)
{
    int error = 0;
    int outlen = 0;

    if ( rem_cdc.msg.flags & REMOTE_GET_IOCTL )
    {
       error =  wl_ioctl( rem_cdc.msg.cmd, buf_ptr, rem_cdc.data_len, 0, &outlen);
    }
    else
    {
       error = wl_ioctl( rem_cdc.msg.cmd, buf_ptr, rem_cdc.data_len, true, &outlen);
    }

    if ( error != 0 )
    {
       rem_cdc.msg.status = error;
    }

    if ((error = wl_remote_CDC_tx( cmd, (unsigned char *)buf_ptr, outlen,
	                               outlen, REMOTE_REPLY, 0)) != 0)
    {
        MFG_DPRINT_ERR(ERR, "wl_server: Return results failed\n");
    }
    return error;
}

int wl_remote_CDC_tx( uint32_t cmd, unsigned char *buf, uint32_t buf_len, uint32_t data_len, uint32_t flags, int debug)
{
    unsigned long numwritten = 0;
    int result = 0;
    rem_ioctl_t *rem_ptr = &rem_cdc;
    int ret;

    memset(rem_ptr, 0, sizeof(rem_ioctl_t));
    rem_ptr->msg.cmd = cmd;
    rem_ptr->msg.len = data_len;
    rem_ptr->msg.flags = flags;
    rem_ptr->data_len = data_len;

    if (strlen(g_rem_ifname) != 0)
    {
       strncpy(rem_ptr->intf_name, g_rem_ifname, (int)INTF_NAME_SIZ);
    }

    if (data_len > buf_len)
    {
        MFG_DPRINT_ERR(ERR, "wl_remote_CDC_tx: data_len (%d) > buf_len (%d)\n", data_len, buf_len);
        return -1;
    }

    /* Send CDC header first */
    if ((ret = wl_write_serial_data((unsigned char *)rem_ptr, REMOTE_SIZE, &numwritten)) == -1)
    {
        MFG_DPRINT_ERR(ERR, "CDC_Tx: Data: Write failed \n");
        return -1;
    }

    if ( data_len > 0 )
    {
       /* Send data second */
       if ((ret = wl_write_serial_data( (unsigned char*)buf, data_len, &numwritten)) == -1)
       {
          MFG_DPRINT_ERR(ERR, "CDC_Tx: Data: Write failed \n");
          return -1;
       }
    }
    return result;
}

int wl_write_serial_data( unsigned char* write_buf, unsigned long size, unsigned long *numwritten)
{
    int result = 0;

    if ( size == 0 )
    {
       return 0;
    }
    result = wl_put_bytes ( write_buf, size );
    *numwritten = size;
    return result;
}

int wl_put_bytes ( unsigned char *buf, int size )
{
   int i;

#ifdef SUPPORT_SEGGER
    SEGGER_RTT_printf(0, "wl_put_bytes s:0x%x \n",size);
#endif

   for ( i = 0; i < size; i ++ )
   {
#ifdef SUPPORT_SEGGER
     SEGGER_RTT_printf(0, "0x%x ",buf[i]);
#endif
	  //putchar(buf[i]);
     mfg_retarget_io_putchar(buf[i]);
   }

#ifdef SUPPORT_SEGGER
   SEGGER_RTT_printf(0, "wl_put_bytes done\n");
#endif

   return 0;
}


/*
 * This function is used for sending wl_ioctl/iovar to WHD driver
 *
 * @ param1 int  cmd : cmd for IOCTL
 * @ param2 void *buf: Buffer to fill for iovar set/get request/response
 * @ param3 int  len : Length of Buffer
 * @ param4 bool set : GET/SET GET is zero/SET is 1.
 * @ param5 int  outlen: output data length
 * @ return int: 0 SUCCESS
 *               -1 ERROR
 */
int wl_ioctl( int cmd, void *buf, int len, bool set, int *outlen)
{
    uint32_t value = 0;
    int result = 0;
    char iovar[256] = {0};
    char *token = NULL;
    char *saveptr = (char *)buf;
    int datalen = len;
#ifdef SUPPORT_SEGGER
        SEGGER_RTT_printf(0, "wl_ioctl: cmd:0x%x len:0x%x set:0x%x\n",cmd,len,set);
#endif

    if ( ( cmd != WLC_GET_VAR) && ( cmd != WLC_SET_VAR ))
    {
        if  ( ( len <= 4 ) && (set ))
        {
           if ( len > 0 )
           {
             memcpy( &value, buf, len );
           }
           whd_wifi_set_ioctl_value(sta_interface, cmd, value);
        }
        else if ( ( len <= 4) )
        {
     	   result = whd_wifi_get_ioctl_value( sta_interface, cmd, &value);
     	   memcpy(buf, &value, len );
        }
        else if ( ( len > 4) && ( set )  )
        {
           result = whd_wifi_set_ioctl_buffer( sta_interface, cmd, (unsigned char *)buf, len );
        }
        else if ( ( len > 4 ) && ( !set ))
        {
          	result = whd_wifi_get_ioctl_buffer( sta_interface, cmd, (unsigned char *)buf, len );
        }
   }
   else
   {
       token = strtok_r((char *)buf, "'\0'", &saveptr);
       strncpy(iovar, token, strlen(token));
       iovar[strlen(token)] = '\0';
       datalen = len - (strlen(token) + 1);

       if ( ( datalen <= 4 ) && ( cmd == WLC_SET_VAR ) )
       {
    	  memcpy(&value, (char *)(((char *)buf) + strlen(token) + 1)  , sizeof(value));
          result = whd_wifi_set_iovar_value( sta_interface, (const char *)buf, value);
       }
       else if  (( datalen <= 4 ) && ( cmd == WLC_GET_VAR ) )
       {
          result = whd_wifi_get_iovar_value( sta_interface, (const char *)buf, &value );
          memcpy(buf, &value, len );
       }
       else if ( ( datalen > 4) && ( set )  )
       {
          result = whd_wifi_set_ioctl_buffer( sta_interface, cmd, (unsigned char *)buf, len );
       }
       else if ( ( datalen > 4 ) && ( !set ))
       {
         result = whd_wifi_get_ioctl_buffer( sta_interface, cmd, (unsigned char *)buf, len );
       }
   }

   if ( set )
   {
      *outlen = 0;
   }
   else
   {
      *outlen = len;
   }
   return result;
}
