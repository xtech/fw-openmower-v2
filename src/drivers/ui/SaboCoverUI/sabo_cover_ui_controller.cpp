//
// Created by Apehaenger on 4/20/25.
//
#include "sabo_cover_ui_controller.hpp"

#include <ulog.h>

#include <services.hpp>

#include "hal.h"
#include "sabo_cover_ui_display.hpp"
#include "sabo_cover_ui_display_driver_uc1698.hpp"

namespace xbot::driver::ui {

using namespace sabo;

void SaboCoverUIController::Configure(const CoverUICfg& cui_cfg) {
  /* The code snippet you provided is initializing the SPI pins for communication. It is setting the mode
  of the pins `sck`, `miso`, and `mosi` to alternate function mode 5, with a medium speed setting and
  pull-up resistors enabled. This configuration is necessary to establish communication over SPI
  (Serial Peripheral Interface) protocol. */
  // Init SPI pins
  /*palSetLineMode(cui_cfg.spi_cfg.pins.sck, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
  palSetLineMode(cui_cfg.spi_cfg.pins.miso, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);
  palSetLineMode(cui_cfg.spi_cfg.pins.mosi, PAL_MODE_ALTERNATE(5) | PAL_STM32_OSPEED_MID2 | PAL_STM32_PUPDR_PULLUP);*/

  // SPI
  /*spi_instance_ = cui_cfg.spi_cfg.instance;
  spi_config_ = {
      .circular = false,
      .slave = false,
      .data_cb = NULL,
      .error_cb = NULL,
      .ssline = LINE_GPIO5,
      // HEF4794BT is the slowest device on SPI bus. F_clk(max)@5V: Min=5MHz, Typ=10MHz
      // Worked, but let's be save within the limits of the HEF4794BT
      //.cfg1 = SPI_CFG1_MBR_0 | SPI_CFG1_MBR_1 |  // Baudrate = FPCLK/16 (12.5 MHz @ 200 MHz PLL2_P)
      //        SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)
      .cfg1 = SPI_CFG1_MBR_2 | SPI_CFG1_MBR_0 |  // Baudrate = FPCLK/32 (6.25 MHz @ 200 MHz PLL2_P)
              SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)
      // For testing/debugging
      //.cfg1 = SPI_CFG1_MBR_2 | SPI_CFG1_MBR_1 | SPI_CFG1_MBR_0 |       // Baudrate = FPCLK/256 (0.78 MHz @ 200 MHz)
      //        SPI_CFG1_DSIZE_2 | SPI_CFG1_DSIZE_1 | SPI_CFG1_DSIZE_0,  // 8-Bit (DS = 0b111)
      .cfg2 = SPI_CFG2_MASTER  // Master, Mode 0 (CPOL=0, CPHA=0) = Data on rising edge
  };*/

  // Start SPI
  /*if (spiStart(cui_cfg.spi_cfg.instance, &spi_config_) != MSG_OK) {
    ULOG_ERROR("CoverUI SPI init failed");
    return;
  }*/

  // Select the CoverUI driver based on the carrier board version and/or CoverUI Series
  if (carrier_board_info.version_major == 0 && carrier_board_info.version_minor == 1) {
    // Mobo v0.1 has only CoverUI-Series-II support and no CoverUI-Series detection
    static SaboCoverUICaboDriverV01 driver_v01(cui_cfg.cabo_cfg);
    driver_ = &driver_v01;
  } else {
    // Mobo v0.2 and later support both CoverUI-Series (I & II) as well as it has CoverUI-Series detection
    static SaboCoverUICaboDriverV02 driver_v02(cui_cfg.cabo_cfg);
    driver_ = &driver_v02;
    static SaboCoverUIDisplay display(cui_cfg.lcd_cfg);
    display_ = &display;
  }

  auto* lcd_drv = SaboCoverUIDisplayDriverUC1698::InstancePtr();
  if (!lcd_drv) {
    ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 not initialized!");
    return;
  }

  configured_ = true;
}

void SaboCoverUIController::Start() {
  if (thread_ != nullptr) {
    ULOG_ERROR("Sabo CoverUI Controller started twice!");
    return;
  }

  if (!configured_) {
    ULOG_ERROR("Sabo CoverUI Controller not configured!");
    return;
  }

  if (driver_ == nullptr) {
    ULOG_ERROR("Sabo CoverUI Driver not set!");
    return;
  }
  if (!driver_->Init()) {
    ULOG_ERROR("Sabo CoverUI Driver initialization failed!");
    return;
  }

  if (display_ && !display_->Init()) {
    ULOG_ERROR("Sabo CoverUI Display initialization failed!");
    return;
  }

  thread_ = chThdCreateStatic(&wa_, sizeof(wa_), NORMALPRIO, ThreadHelper, this);
#ifdef USE_SEGGER_SYSTEMVIEW
  processing_thread_->name = "SaboCoverUIController";
#endif
}

void SaboCoverUIController::ThreadHelper(void* instance) {
  auto* i = static_cast<SaboCoverUIController*>(instance);
  i->ThreadFunc();
}

void SaboCoverUIController::UpdateStates() {
  if (!driver_->IsReady()) return;

  // const auto high_level_state = mower_ui_service.GetHighLevelState();

  // Start LEDs
  // For identification purposes, Red-Start-LED get handled exclusively with high priority before Green-Start-LED
  if (emergency_service.GetEmergency()) {
    driver_->SetLED(LEDID::PLAY_RD, LEDMode::BLINK_FAST);  // Emergency
    driver_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
    /* FIXME: Add/Enable once mower_ui_service is working
  } else if (high_level_state ==
             MowerUiService::HighLevelState::MODE_UNKNOWN) { // Waiting for ROS
    driver_->SetLED(LEDID::PLAY_RD, LEDMode::BLINK_SLOW);              */
    driver_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
  } else {
    driver_->SetLED(LEDID::PLAY_RD, LEDMode::OFF);

    // Green-Start-LED
    if (power_service.GetAdapterVolts() > 26.0f) {    // Docked
      if (power_service.GetBatteryVolts() < 20.0f) {  // No (or dead) battery
        driver_->SetLED(LEDID::PLAY_GN, LEDMode::BLINK_FAST);
      } else if (power_service.GetChargeCurrent() > 0.1f) {  // Battery charging
        driver_->SetLED(LEDID::PLAY_GN, LEDMode::BLINK_SLOW);
      } else {  // Battery charged
        driver_->SetLED(LEDID::PLAY_GN, LEDMode::ON);
      }
    } else {
      // TODO: Handle high level states like "Mowing" or "Area Recording"
      driver_->SetLED(LEDID::PLAY_GN, LEDMode::OFF);
    }
  }
}

bool SaboCoverUIController::IsButtonPressed(const ButtonID button) const {
  if (!driver_->IsReady()) return false;
  return driver_->IsButtonPressed(button);
}

const char* SaboCoverUIController::ButtonIDToString(const ButtonID id) {
  switch (id) {
    case ButtonID::UP: return "Up";
    case ButtonID::DOWN: return "Down";
    case ButtonID::LEFT: return "Left";
    case ButtonID::RIGHT: return "Right";
    case ButtonID::OK: return "OK";
    case ButtonID::PLAY: return "Play";
    case ButtonID::S1_SELECT: return "Select (S1)";
    case ButtonID::MENU: return "Menu";
    case ButtonID::BACK: return "Back";
    case ButtonID::S2_AUTO: return "Auto (S2)";
    case ButtonID::S2_MOW: return "Mow (S2)";
    case ButtonID::S2_HOME: return "Home (S2)";
    default: return "Unknown";
  }
}

void SaboCoverUIController::ThreadFunc() {
  auto* lcd_drv = SaboCoverUIDisplayDriverUC1698::InstancePtr();
  if (!lcd_drv) {
    ULOG_ERROR("SaboCoverUIDisplayDriverUC1698 not initialized!");
    return;
  }

  event_listener_t el_fill;
  chEvtRegister(&lcd_drv->spi_fill_event, &el_fill, 0);
  // chEvtRegister(&lcd_drv->spi_flush_event, &el_flush, 1);

  while (true) {
    driver_->Tick();
    UpdateStates();
    display_->Tick();

    static bool display_tested = false;
    if (display_ && !display_tested) {
      driver_->SetLED(LEDID::AUTO, LEDMode::ON);
      // driver_->Tick();
      //  display_->Test();
      driver_->SetLED(LEDID::AUTO, LEDMode::OFF);
      // driver_->Tick();

      display_tested = true;
    }

    eventmask_t events = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_IMMEDIATE);
    if (events & EVENT_MASK(0)) {
      // eventflags_t flags = chEvtGetAndClearFlags(&el_fill);
      lcd_drv->FillScreenFast(false, false);
    }
    /*if (mask & EVENT_MASK(1)) {
      lcd_drv->OnSpiFlushTransferDone();
    }*/

    // ----- Debug -----
    static uint32_t last_debug_time = 0;
    uint32_t now = chVTGetSystemTimeX();
    if (TIME_I2MS(now - last_debug_time) >= 500) {
      last_debug_time = now;
      // Check all buttons and log their state
      for (int btn = static_cast<int>(ButtonID::_FIRST); btn <= static_cast<int>(ButtonID::_LAST); ++btn) {
        ButtonID button = static_cast<ButtonID>(btn);
        if (driver_->IsButtonPressed(button)) {
          ULOG_INFO("Button [%s] pressed", ButtonIDToString(static_cast<ButtonID>(btn)));
        }
      }
    }

    // Sleep for max. 1ms for reliable button debouncing (and future LCD buffer updates (LVGL))
    chThdSleep(TIME_MS2I(1));
  }
}

}  // namespace xbot::driver::ui
