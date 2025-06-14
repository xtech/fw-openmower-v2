// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#ifdef USE_SEGGER_RTT
#include <SEGGER_RTT.h>
#include <SEGGER_RTT_streams.h>
#endif
#include <etl/to_string.h>
#include <lwipthread.h>
#include <service_ids.h>

#include <boot_service_discovery.hpp>
#include <filesystem/file.hpp>
#include <filesystem/filesystem.hpp>
#include <robot.hpp>
#include <xbot-service/Io.hpp>
#include <xbot-service/RemoteLogging.hpp>
#include <xbot-service/portable/system.hpp>

#include "globals.hpp"
#include "heartbeat.h"
#include "id_eeprom.h"
#include "services.hpp"
#include "status_led.h"

static void DispatchEvents();

/*
 * Application entry point.
 */
int main() {
#ifdef RELEASE_BUILD
  // Reset the boot register for a release build, so that the chip
  // resets to bootloader
  MODIFY_REG(SYSCFG->UR2, SYSCFG_UR2_BOOT_ADD0, (0x8000000 >> 16) << SYSCFG_UR2_BOOT_ADD0_Pos);
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
  SetStatusLedColor(RED);

  /*
   * Setup LWIP stack using the MAC address provided by the EEPROM.
   */
  uint8_t mac_address[6] = {0};
  if (!ID_EEPROM_GetMacAddress(mac_address, sizeof(mac_address))) {
    while (1)
      ;
  }
  lwipthread_opts_t lwipconf_opts{};
  lwipconf_opts.addrMode = NET_ADDRESS_DHCP;
  lwipconf_opts.address = 0;
  lwipconf_opts.gateway = 0;
  lwipconf_opts.netmask = 0;
  lwipconf_opts.macaddress = mac_address;
  lwipInit(&lwipconf_opts);

  InitBootloaderServiceDiscovery();

  // Safe to do before checking the carrier board, needed for logging
  xbot::service::system::initSystem();
  xbot::service::startRemoteLogging();

  // Try opening the filesystem, on error fail
  if (!InitFS()) {
    SetStatusLedMode(LED_MODE_BLINK_SLOW);
    SetStatusLedColor(RED);
    while (true) {
      ULOG_ERROR("Error mounting filesystem!");
      chThdSleep(TIME_S2I(1));
    }
  }

  robot = GetRobot();
  if (!robot->IsHardwareSupported()) {
    SetStatusLedMode(LED_MODE_BLINK_FAST);
    SetStatusLedColor(RED);

    etl::string<100> info{};
    info += "Carrier board not supported for this firmware: ";
    info += carrier_board_info.board_id;
    info += " v";
    etl::to_string(carrier_board_info.version_major, info, true);
    info += ".";
    etl::to_string(carrier_board_info.version_minor, info, true);
    info += ".";
    etl::to_string(carrier_board_info.version_patch, info, true);

    while (true) {
      ULOG_ERROR(info.c_str());
      chThdSleep(TIME_S2I(1));
    }
  }

  robot->InitPlatform();
  xbot::service::Io::start();
  StartServices();
  SetStatusLedColor(GREEN);
  DispatchEvents();
}

static void DispatchEvents() {
  // Subscribe to global events and dispatch to our services
  event_listener_t event_listener;
  chEvtRegister(&mower_events, &event_listener, Events::GLOBAL);
  while (1) {
    eventmask_t events = chEvtWaitAnyTimeout(Events::ids_to_mask({Events::GLOBAL}), TIME_INFINITE);
    if (events & EVENT_MASK(Events::GLOBAL)) {
      // Get the flags provided by the event
      eventflags_t flags = chEvtGetAndClearFlags(&event_listener);
      if (flags & MowerEvents::EMERGENCY_CHANGED) {
        diff_drive.OnEmergencyChangedEvent();
#ifndef NO_MOWER_SERVICE
        mower_service.OnEmergencyChangedEvent();
#endif
      }
      if (flags & MowerEvents::INPUTS_CHANGED) {
        input_service.OnInputsChangedEvent();
        emergency_service.CheckInputs(xbot::service::system::getTimeMicros());
      }
    }
  }
}
