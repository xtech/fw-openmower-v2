// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#ifdef USE_SEGGER_SYSTEMVIEW
#include <SEGGER_RTT.h>
#include <SEGGER_RTT_streams.h>
#endif
#include <boot_service_discovery.h>
#include <drivers/vesc/VescUart.h>
#include <heartbeat.h>
#include <id_eeprom.h>
#include <lwipthread.h>
#include <status_led.h>

#include <globals.hpp>
#include <xbot-service/Io.hpp>
#include <xbot-service/portable/system.hpp>

#include "services/emergency_service/emergency_service.hpp"
#include "services/imu_service/imu_service.hpp"
#include "services/power_service/power_service.hpp"
EmergencyService emergency_service{1};
ImuService imu_service{4};
PowerService power_service{5};

static THD_WORKING_AREA(waTestThread, 500);

static void test_thread(void *p) {
  static VescUart vesc_uart(&UARTD2);
  while (1) {
#ifdef USE_SEGGER_SYSTEMVIEW
    SEGGER_SYSVIEW_Print("keepalive");
    vesc_uart.sendKeepalive();
    SEGGER_SYSVIEW_Print("request");
    vesc_uart.requestVescValues();
    SEGGER_SYSVIEW_Print("parse");
    vesc_uart.parseVescValues();
    SEGGER_SYSVIEW_Print("wait");
#endif
    chThdSleepMilliseconds(1000);
  }
}

/*
 * Application entry point.
 */
int main(void) {
#ifdef RELEASE_BUILD
  // Reset the boot register for a release build, so that the chip
  // resets to bootloader
  MODIFY_REG(SYSCFG->UR2, SYSCFG_UR2_BOOT_ADD0,
             (0x8000000 >> 16) << SYSCFG_UR2_BOOT_ADD0_Pos);
#endif

  // Need to disable D-Cache for the Ethernet MAC to work properly
  SCB_DisableDCache();
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();
#ifdef USE_SEGGER_RTT
  rttInit();
#endif
#ifdef USE_SEGGER_SYSTEMVIEW
  SYSVIEW_ChibiOS_Start(STM32_SYS_CK, STM32_SYS_CK, "I#15=SysTick");
#endif

  /*
   * InitGlobals() sets up global variables shared by threads. (e.g. mutex)
   * InitHeartbeat() starts the heartbeat timer to blink an LED as long as the
   * RTOS is running.
   */
  InitGlobals();
  InitHeartbeat();
  InitStatusLed();

  SetStatusLedMode(LED_MODE_ON);

  /*
   * Setup LWIP stack using the MAC address provided by the EEPROM.
   */
  uint8_t mac_address[6] = {0};
  if (!ID_EEPROM_GetMacAddress(mac_address, sizeof(mac_address))) {
    while (1)
      ;
  }
  lwipthread_opts_t lwipconf_opts = {0};
  lwipconf_opts.addrMode = NET_ADDRESS_DHCP;
  lwipconf_opts.address = 0;
  lwipconf_opts.gateway = 0;
  lwipconf_opts.netmask = 0;
  lwipconf_opts.macaddress = mac_address;
  lwipInit(&lwipconf_opts);

  InitBootloaderServiceDiscovery();

  xbot::service::system::initSystem();
  xbot::service::Io::start();
  emergency_service.start();
  imu_service.start();
  power_service.start();

  chThdCreateStatic(waTestThread, sizeof(waTestThread), NORMALPRIO, test_thread,
                    NULL);

  // Subscribe to global events and dispatch to our services
  event_listener_t event_listener;
  chEvtRegister(&mower_events, &event_listener, 1);
  while (1) {
    uint32_t event = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_INFINITE);
    // event no 1 == "mower_events"
    if (event == 1) {
      // Get the flags provided by the event
      uint32_t flags = chEvtGetAndClearFlags(&event_listener);
      if (flags & MOWER_EVT_EMERGENCY_CHANGED) {
        // Get the new emergency value
        chMtxLock(&mower_status_mutex);
        uint32_t status_copy = mower_status;
        chMtxUnlock(&mower_status_mutex);
        // Notify services
      }
    }
  }

  chThdSleep(TIME_INFINITE);
}
