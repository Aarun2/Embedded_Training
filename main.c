// C standard libs, needed for compilation. Put in every file!
#include <stdint.h>
#include <stdbool.h>

//needed to initialize launchpad clock/interrupts
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "hardware_abstraction_layer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "config_udp.h"
#include "bcl_port.h"

#include "ControlServicePackets.h"

#define INITIAL_DELAY   500
#define MAX_DELAY       2000

extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
static void InitBCLUDP(int bcl_inst);
static void example_task(void *args);
static BCL_STATUS callback(int bcl_inst, BclPayloadPtr payload);

static int time_delay = INITIAL_DELAY;

static void InitBCLUDP(int bcl_inst)
{

    {
        static const uint8_t Ip_Address[ipIP_ADDRESS_LENGTH_BYTES]      = {192,168,1,10};
        static const uint8_t Net_Mask[ipIP_ADDRESS_LENGTH_BYTES]        = {255,255,255,0};
        static const uint8_t Gateway_Address[ipIP_ADDRESS_LENGTH_BYTES] = {192,168,1,1};
        static const uint8_t DNS_Address[ipIP_ADDRESS_LENGTH_BYTES]     = {8,8,8,8};
        static       uint8_t Mac_Address[6];
        eth_read_mac_addr(Mac_Address);
        FreeRTOS_IPInit(
            Ip_Address,
            Net_Mask,
            Gateway_Address,
            DNS_Address,
            Mac_Address);
    }

    BCL_configUDP(bcl_inst, 10000, 10000, FreeRTOS_inet_addr_quick(192,168,1,20)); // Sam: these arguments will almost certainly have to change

}

static void example_task(void *args)
{
    uint8_t val = GPIO_PIN_0;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD))
    {
    }
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0);

    while(1) {
        GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, val);
        val ^= GPIO_PIN_0;
        vTaskDelay(time_delay);
    }
}

static BCL_STATUS callback(int bcl_inst, BclPayloadPtr payload)
{
    time_delay += 500;
    if(time_delay >= MAX_DELAY)
        time_delay = INITIAL_DELAY;
}

int main(void)
{
    global_sys_clock = SysCtlClockFreqSet(
            SYSCTL_XTAL_25MHZ|SYSCTL_OSC_MAIN|SYSCTL_USE_PLL|SYSCTL_CFG_VCO_480,
            120000000
    );

    //Wait for external peripherals to initialize
    {
        uint32_t i = 0;
        while (i++ < 1000000);
    }

    // Register FreeRTOS's Supervisor Call handler
    IntRegister(FAULT_SVCALL, vPortSVCHandler);
    // Register FreeRTOS's Pend Supervisor Call handler
    IntRegister(FAULT_PENDSV, xPortPendSVHandler);
    // Register FreeRTOS's SysTick handler
    IntRegister(FAULT_SYSTICK, xPortSysTickHandler);

    int main_bcl_service;
    main_bcl_service = BCL_initService();

    InitBCLUDP(main_bcl_service);

    xTaskCreate(
            example_task,
            "ex",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY+1,
            NULL
    );

    BCL_pktCallbackRegister(callback, QUERY_HEARTBEAT_OPCODE);

    vTaskStartScheduler();

	return 0;
}
