/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
#define NBR_TABLE_CONF_MAX_NEIGHBORS 32
#define ENERGEST_CONF_ON              1
/* Disable button shutdown functionality */
#define BUTTON_SENSOR_CONF_ENABLE_SHUTDOWN    0
/*---------------------------------------------------------------------------*/
/* Enable the ROM bootloader */
#define ROM_BOOTLOADER_ENABLE                 1
/*---------------------------------------------------------------------------*/
/* Change to match your configuration */
#define IEEE802154_CONF_PANID            0xABCD
#define RF_CORE_CONF_CHANNEL                 26
#define RF_BLE_CONF_ENABLED                   0
/*---------------------------------------------------------------------------*/
#define RDC_NULLRDC 0
#define RDC_CONTIKIMAC 1
#undef RDC_MODE

#undef NETSTACK_CONF_RDC
//#define NETSTACK_CONF_RDC nullrdc_driver
//#define RDC_MODE RDC_NULLRDC

#define NETSTACK_CONF_RDC contikimac_driver
#define RDC_MODE RDC_CONTIKIMAC

#if NETSTACK_CONF_RDC == contikimac_driver
    #undef NETSTACK_RDC_CHANNEL_CHECK_RATE
    #define NETSTACK_RDC_CHANNEL_CHECK_RATE 32  // wake up every 31.25ms (1/CHECK_RATE)
    #define CHANNEL_CHECK_INTERVAL_TICKS ((CLOCK_SECOND / NETSTACK_RDC_CHANNEL_CHECK_RATE) + ((CLOCK_SECOND/200))) //add 5ms to the check interval
#endif

/*-------------------------------DEBUG------------------------------------*/
#define USR_DEBUG 1


/*---------------------------------------------------------------------------*/
#define NULLRDC_CONF_802154_AUTOACK           1
/*---------------------------------------------------------------------------*/
#if CONTIKI_TARGET_ZOUL
/* System clock runs at 32 MHz */
#define SYS_CTRL_CONF_SYS_DIV         SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

/* IO clock runs at 32 MHz */
#define SYS_CTRL_CONF_IO_DIV          SYS_CTRL_CLOCK_CTRL_IO_DIV_32MHZ

#define NETSTACK_CONF_WITH_IPV6       0

#define CC2538_RF_CONF_CHANNEL        26

#define COFFEE_CONF_SIZE              0

#define LPM_CONF_MAX_PM               LPM_PM0
#endif
/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
