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

#include <boot_service_discovery.hpp>
#include <filesystem/file.hpp>
#include <filesystem/filesystem.hpp>
#include <robot.hpp>
#include <xbot-service/Io.hpp>
#include <xbot-service/RemoteLogging.hpp>
#include <xbot-service/portable/system.hpp>

#include "../services/service_ids.h"
#include "globals.hpp"
#include "heartbeat.h"
#include "id_eeprom.h"
#include "services.hpp"
#include "status_led.h"

extern "C" {
#include <lwip/apps/tftp_server.h>
}

static void DispatchEvents();

File ftp_file{};

static void* ftp_open(const char* name, const char* mode, uint8_t write) {
  (void)name;
  (void)mode;
  (void)write;
  if (ftp_file.isOpen()) {
    return nullptr;
  }
  if (ftp_file.open(name) == LFS_ERR_OK) {
    return &ftp_file;
  }
  return nullptr;
}

static void ftp_close(void* handle) {
  (void)handle;
  ftp_file.close();
}

static int ftp_read(void* handle, void* buf, int bytes) {
  (void)handle;
  (void)buf;
  (void)bytes;
  return ftp_file.read(buf, bytes);
}

static int ftp_write(void* handle, struct pbuf* p) {
  (void)handle;
  (void)p;
  if (!ftp_file.isOpen()) {
    return -1;
  }

  for (pbuf* pbuf = p; pbuf != nullptr; pbuf = pbuf->next) {
    if (ftp_file.write(pbuf->payload, pbuf->len) < 0) {
      return -1;
    }
  }

  return 0;
}

static void ftp_error(void* handle, int err, const char* msg, int size) {
  (void)handle;
  (void)err;
  (void)msg;
  (void)size;
}

static tftp_context ftp_ctx{
    .open = ftp_open, .close = ftp_close, .read = ftp_read, .write = ftp_write, .error = ftp_error};

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

  auto error = tftp_init_server(&ftp_ctx);
  (void)error;

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
  const eventid_t MOWER_EVENTS_ID = 1;
  chEvtRegister(&mower_events, &event_listener, MOWER_EVENTS_ID);
  while (1) {
    uint32_t event = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_INFINITE);
    if (event == MOWER_EVENTS_ID) {
      // Get the flags provided by the event
      uint32_t flags = chEvtGetAndClearFlags(&event_listener);
      if (flags & MowerEvents::EMERGENCY_CHANGED) {
        diff_drive.OnEmergencyChangedEvent();
        if (robot->NeedsService(xbot::service_ids::MOWER)) {
          mower_service.OnEmergencyChangedEvent();
        }
      }
    }
  }
}
