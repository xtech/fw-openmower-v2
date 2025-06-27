// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on

#ifdef USE_SEGGER_RTT
#include <SEGGER_RTT.h>
#include <SEGGER_RTT_streams.h>
#endif
#include <etl/to_string.h>
#include <lvgl.h>
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

#define LINE_DISPLAY_A0 LINE_GPIO4
#define LINE_DISPLAY_RESET LINE_GPIO3
#define LINE_DISPLAY_CS LINE_GPIO2
#define LINE_DISPLAY_BACKLIGHT LINE_SPI2_MISO
#define SPID_DISPLAY SPID2

#define CMD_DISPLAY_OFF 0xAE
#define CMD_DISPLAY_ON 0xAF

#define CMD_SET_DISP_START_LINE 0x40
#define CMD_SET_PAGE 0xB0

#define CMD_SET_COLUMN_UPPER 0x10
#define CMD_SET_COLUMN_LOWER 0x00

#define CMD_SET_ADC_NORMAL 0xA0
#define CMD_SET_ADC_REVERSE 0xA1

#define CMD_SET_DISP_NORMAL 0xA6
#define CMD_SET_DISP_REVERSE 0xA7

#define CMD_SET_ALLPTS_NORMAL 0xA4
#define CMD_SET_ALLPTS_ON 0xA5
#define CMD_SET_BIAS_9 0xA2
#define CMD_SET_BIAS_7 0xA3

#define CMD_RMW 0xE0
#define CMD_RMW_CLEAR 0xEE
#define CMD_INTERNAL_RESET 0xE2
#define CMD_SET_COM_NORMAL 0xC0
#define CMD_SET_COM_REVERSE 0xC8
#define CMD_SET_POWER_CONTROL 0x28
#define CMD_SET_RESISTOR_RATIO 0x20
#define CMD_SET_VOLUME_FIRST 0x81
#define CMD_SET_VOLUME_SECOND 0x00
#define CMD_SET_STATIC_OFF 0xAC
#define CMD_SET_STATIC_ON 0xAD
#define CMD_SET_STATIC_REG 0x00
#define CMD_SET_BOOSTER_FIRST 0xF8
#define CMD_SET_BOOSTER_234 0x00
#define CMD_SET_BOOSTER_5 0x01
#define CMD_SET_BOOSTER_6 0x03
#define CMD_NOP 0xE3
#define CMD_TEST 0xF0

static void Send(size_t n, const void* txbuf, bool cmd = true) {
  palWriteLine(LINE_DISPLAY_A0, cmd ? PAL_LOW : PAL_HIGH);
  spiSelect(&SPID_DISPLAY);
  spiSend(&SPID_DISPLAY, n, txbuf);
  spiUnselect(&SPID_DISPLAY);
}

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  constexpr uint8_t H_RES = 128;

  // As per documentation, 2x4 bytes are reserved for a palette and should be skipped.
  px_map += 8;

  // The display is divided into pages of 8 pixels height.
  // Within a page, there are 128 visible columns, represented by one byte each (LSB = top).
  // See https://edeca.net/page/the-st7565-display-controller/ for excellent descriptions and pictures.

  // We need to transform the data from horizontal representation.
  const uint8_t width = lv_area_get_width(area);
  const uint8_t first_column = area->x1 + 4;
  for (uint8_t page = area->y1 / 8; page <= area->y2 / 8; page++) {
    static uint8_t cmd[3];
    cmd[0] = CMD_SET_PAGE | page;
    cmd[1] = CMD_SET_COLUMN_UPPER | ((first_column >> 4) & 0x0f);
    cmd[2] = CMD_SET_COLUMN_LOWER | (first_column & 0x0f);
    Send(sizeof(cmd), cmd, true);

    static uint8_t data[H_RES];
    lv_memzero(data, sizeof(data));
    for (uint8_t row = 0; row < 8; row++) {
      for (uint8_t x = 0; x < width; x++) {
        const uint8_t src_bit = 7 - x % 8;
        // LVGL considers white as active, black as inactive, see LV_DRAW_SW_I1_LUM_THRESHOLD.
        if (((*px_map >> src_bit) & 1) == 0) {
          data[x] |= 1 << row;
        }
        if (src_bit == 0) px_map++;
      }
    }
    Send(width, data, false);
  }

  lv_display_flush_ready(disp);
}

typedef struct {
  lv_obj_t* obj;
  int dx;
  int dy;
} label_anim_data_t;

static void bounce_anim_cb(lv_timer_t* timer) {
  label_anim_data_t* data = (label_anim_data_t*)lv_timer_get_user_data(timer);
  lv_obj_t* obj = data->obj;

  // Get current position
  lv_coord_t x = lv_obj_get_x(obj);
  lv_coord_t y = lv_obj_get_y(obj);

  // Get object size and screen size
  lv_coord_t w = lv_obj_get_width(obj);
  lv_coord_t h = lv_obj_get_height(obj);
  lv_coord_t sw = lv_obj_get_width(lv_screen_active());
  lv_coord_t sh = lv_obj_get_height(lv_screen_active());

  // Update position
  x += data->dx;
  y += data->dy;

  // Bounce from edges
  if (x <= 0 || (x + w) >= sw) {
    data->dx = -data->dx;
    x += data->dx;
  }

  if (y <= 0 || (y + h) >= sh) {
    data->dy = -data->dy;
    y += data->dy;
  }

  // Set new position
  lv_obj_set_pos(obj, x, y);
}

void create_bouncing_label() {
  lv_obj_t* label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "OpenMower");
  lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);

  // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  // Create animation data
  static label_anim_data_t anim_data = {.obj = label, .dx = 1, .dy = 1};

  // Create timer to update position
  lv_timer_create(bounce_anim_cb, 1, &anim_data);
}

static void my_rounder_cb(lv_event_t* e) {
  lv_area_t* area = (lv_area_t*)lv_event_get_param(e);

  /* Round the height to the nearest multiple of 8 */
  area->y1 = (area->y1 & ~0x7);
  area->y2 = (area->y2 | 0x7);
}

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

  palSetLineMode(LINE_DISPLAY_A0, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_DISPLAY_CS, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_DISPLAY_RESET, PAL_MODE_OUTPUT_PUSHPULL);
  palSetLineMode(LINE_DISPLAY_BACKLIGHT, PAL_MODE_OUTPUT_PUSHPULL);

  static const SPIConfig spi_config = {
      .circular = false,
      .slave = false,
      .data_cb = nullptr,
      .error_cb = nullptr,
      .ssline = LINE_DISPLAY_CS,
      .cfg1 = SPI_CFG1_MBR_DIV16 | SPI_CFG1_DSIZE_0 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_2,
      .cfg2 = SPI_CFG2_COMM_TRANSMITTER | SPI_CFG2_CPHA | SPI_CFG2_CPOL};

  spiStart(&SPID_DISPLAY, &spi_config);
  spiAcquireBus(&SPID_DISPLAY);
  spiUnselect(&SPID_DISPLAY);

  palSetLine(LINE_DISPLAY_BACKLIGHT);

  palClearLine(LINE_DISPLAY_RESET);
  chThdSleepMilliseconds(1);
  palSetLine(LINE_DISPLAY_RESET);
  chThdSleepMilliseconds(1);

  static const uint8_t init_cmds[] = {

      0xe2, /* soft reset */
      0xae, /* display off */
      0x40, /* set display start line to 0 */

      0xa1, /* ADC set to reverse */
      0xc0, /* common output mode */
      // Flipmode
      // 0xa0,		                /* ADC set to reverse */
      // 0xc8,		                /* common output mode */

      0xa6,          /* display normal, bit val 0: LCD pixel off. */
      0xa2,          /* LCD bias 1/9 */
      0x2f,          /* all power  control circuits on */
      0xf8, 0x00,    /* set booster ratio to 4x */
      0x25,          /* set V0 voltage resistor ratio to large,  issue 1678: changed from 0x23 to 0x25 */
      0x81, 55 >> 2, /* set contrast, contrast value NHD C12864, see issue 186, increased contrast to 180 (issue
                             219), reduced to 170 (issue 1678) */

      // 0xae, /* display off */
      0xaf, /* display on */
      0xa4, /* enter powersafe: all pixel on, issue 142 */
            // 0xa5, /* enter powersafe: all pixel on, issue 142 */

  };

  Send(sizeof(init_cmds), init_cmds);

  static uint8_t buf1[(128 * 64 / 8) + 8];
  static uint8_t buf2[(128 * 64 / 8) + 8];
  lv_init();
  lv_display_t* display1 = lv_display_create(128, 64);
  lv_display_add_event_cb(display1, my_rounder_cb, LV_EVENT_INVALIDATE_AREA, display1);
  lv_display_set_flush_cb(display1, flush_cb);
  lv_display_set_buffers(display1, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  // /*Change the active screen's background color*/
  // lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

  /*Create a white label, set its text and align it to the center*/
  // lv_obj_t* label = lv_label_create(lv_screen_active());
  // lv_label_set_text(label, "Hello world");
  // lv_obj_set_style_text_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
  // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  create_bouncing_label();

  // lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), LV_PART_MAIN);
  // lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

  // /*Create an array for the points of the line*/
  // static lv_point_precise_t line_points[] = {{5, 12}, {30, 12}};

  // /*Create style*/
  // static lv_style_t style_line;
  // lv_style_init(&style_line);
  // lv_style_set_line_width(&style_line, 1);
  // lv_style_set_line_color(&style_line, lv_color_black());

  // /*Create a line and apply the new style*/
  // lv_obj_t* line1;
  // line1 = lv_line_create(lv_screen_active());
  // lv_line_set_points(line1, line_points, 2); /*Set the points*/
  // lv_obj_add_style(line1, &style_line, 0);
  // // lv_obj_center(line1);

  while (true) {
    lv_task_handler();
    lv_tick_inc(1);
    chThdSleepMilliseconds(7);
  }

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
        if (robot->NeedsService(xbot::service_ids::MOWER)) {
          mower_service.OnEmergencyChangedEvent();
        }
      }
      if (flags & MowerEvents::INPUTS_CHANGED) {
        input_service.OnInputsChangedEvent();
        emergency_service.CheckInputs(xbot::service::system::getTimeMicros());
      }
    }
  }
}
