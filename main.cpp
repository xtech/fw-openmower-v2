// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#include <SEGGER_RTT.h>
#include <SEGGER_RTT_streams.h>
#include <boot_service_discovery.h>
#include <chprintf.h>
#include <globals.h>
#include <heartbeat.h>
#include <id_eeprom.h>
#include <lwipthread.h>
#include <status_led.h>

#include <xbot-service/Io.hpp>
#include <xbot-service/portable/system.hpp>

#include "services/imu_service/imu_service.hpp"
#include "services/power_service/power_service.hpp"
ImuService imu_service{4};
PowerService power_service{5};
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
  imu_service.start();
  power_service.start();
  chThdSleep(TIME_INFINITE);
}
