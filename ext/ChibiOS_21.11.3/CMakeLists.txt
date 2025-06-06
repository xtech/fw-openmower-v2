set(LWIP_DIR ext/lwip)
include(ext/lwip/src/Filelists.cmake)
add_library(
        ChibiOS OBJECT
        #startup
        os/common/startup/ARMCMx/compilers/GCC/crt1.c
        os/common/startup/ARMCMx/compilers/GCC/crt0_v7m.S
        os/common/startup/ARMCMx/compilers/GCC/vectors.S
        #hal
        os/hal/src/hal.c
        os/hal/src/hal_st.c
        os/hal/src/hal_buffers.c
        os/hal/src/hal_queues.c
        os/hal/src/hal_flash.c
        os/hal/src/hal_mmcsd.c
        os/hal/src/hal_adc.c
        os/hal/src/hal_can.c
        os/hal/src/hal_crypto.c
        os/hal/src/hal_dac.c
        os/hal/src/hal_efl.c
        os/hal/src/hal_gpt.c
        os/hal/src/hal_i2c.c
        os/hal/src/hal_i2s.c
        os/hal/src/hal_icu.c
        os/hal/src/hal_mac.c
        os/hal/src/hal_mmc_spi.c
        os/hal/src/hal_pal.c
        os/hal/src/hal_pwm.c
        os/hal/src/hal_rtc.c
        os/hal/src/hal_sdc.c
        os/hal/src/hal_serial.c
        os/hal/src/hal_serial_usb.c
        os/hal/src/hal_sio.c
        os/hal/src/hal_spi.c
        os/hal/src/hal_trng.c
        os/hal/src/hal_uart.c
        os/hal/src/hal_usb.c
        os/hal/src/hal_wdg.c
        os/hal/src/hal_wspi.c
        os/hal/lib/streams/chprintf.c
        os/hal/lib/streams/chscanf.c
        os/hal/lib/streams/memstreams.c
        os/hal/lib/streams/nullstreams.c
        os/hal/lib/streams/bufstreams.c
        os/hal/ports/common/ARMCMx/nvic.c
        os/hal/ports/STM32/STM32H7xx/stm32_isr.c
        os/hal/ports/STM32/STM32H7xx/hal_lld.c
        os/hal/ports/STM32/LLD/ADCv4/hal_adc_lld.c
        os/hal/ports/STM32/LLD/BDMAv1/stm32_bdma.c
        os/hal/ports/STM32/LLD/DMAv2/stm32_dma.c
        os/hal/ports/STM32/LLD/EXTIv1/stm32_exti.c
        os/hal/ports/STM32/LLD/GPIOv2/hal_pal_lld.c
        os/hal/ports/STM32/LLD/I2Cv3/hal_i2c_lld.c
        os/hal/ports/STM32/LLD/MACv2/hal_mac_lld.c
        os/hal/ports/STM32/LLD/MDMAv1/stm32_mdma.c
        os/hal/ports/STM32/LLD/OCTOSPIv2/hal_wspi_lld.c
        os/hal/ports/STM32/LLD/SPIv3/hal_spi_v2_lld.c
        os/hal/ports/STM32/LLD/RTCv2/hal_rtc_lld.c
        os/hal/ports/STM32/LLD/SYSTICKv1/hal_st_lld.c
        os/hal/ports/STM32/LLD/TIMv1/hal_gpt_lld.c
        os/hal/ports/STM32/LLD/TIMv1/hal_icu_lld.c
        os/hal/ports/STM32/LLD/TIMv1/hal_pwm_lld.c
        os/hal/ports/STM32/LLD/USARTv3/hal_serial_lld.c
        os/hal/ports/STM32/LLD/USARTv3/hal_sio_lld.c
        os/hal/ports/STM32/LLD/USARTv3/hal_uart_lld.c
        os/hal/ports/STM32/LLD/xWDGv1/hal_wdg_lld.c
        os/hal/ports/STM32/LLD/FDCANv1/hal_can_lld.c
        os/hal/ports/STM32/LLD/SDMMCv2/hal_sdc_lld.c
        # Flash Device
        os/hal/lib/complex/serial_nor/hal_serial_nor.c
        #osal
        os/hal/osal/rt-nil/osal.c
        #RT
        os/rt/src/chsys.c
        os/rt/src/chrfcu.c
        os/rt/src/chdebug.c
        os/rt/src/chtrace.c
        os/rt/src/chvt.c
        os/rt/src/chschd.c
        os/rt/src/chinstances.c
        os/rt/src/chthreads.c
        os/rt/src/chtm.c
        os/rt/src/chstats.c
        os/rt/src/chregistry.c
        os/rt/src/chsem.c
        os/rt/src/chmtx.c
        os/rt/src/chcond.c
        os/rt/src/chevents.c
        os/rt/src/chmsg.c
        os/rt/src/chdynamic.c
        os/common/ports/ARMv7-M/chcore.c
        os/common/ports/ARMv7-M/compilers/GCC/chcoreasm.S
        #oslib
        os/oslib/src/chmboxes.c
        os/oslib/src/chmemcore.c
        os/oslib/src/chmemheaps.c
        os/oslib/src/chmempools.c
        os/oslib/src/chpipes.c
        os/oslib/src/chobjcaches.c
        os/oslib/src/chdelegates.c
        os/oslib/src/chfactory.c
        # board
        ${BOARD_DIR}/board.c
        ${BOARD_DIR}/board_ex.c
        ${BOARD_DIR}/hal_flash_device.c
        # LwIP
        os/various/lwip_bindings/lwipthread.c
        os/various/lwip_bindings/arch/sys_arch.c
        os/various/evtimer.c
        ${lwipnoapps_SRCS}
)


target_include_directories(ChibiOS PUBLIC
        os/license
        # startup
        os/common/portability/GCC
        os/common/startup/ARMCMx/compilers/GCC
        os/common/startup/ARMCMx/devices/STM32H7xx
        os/common/ext/ARM/CMSIS/Core/Include
        os/common/ext/ST/STM32H7xx
        #hal
        os/hal/include
        os/hal/lib/streams
        os/hal/ports/common/ARMCMx
        os/hal/ports/STM32/LLD/ADCv4
        os/hal/ports/STM32/STM32H7xx
        os/hal/ports/STM32/LLD/BDMAv1
        os/hal/ports/STM32/LLD/DMAv2
        os/hal/ports/STM32/LLD/EXTIv1
        os/hal/ports/STM32/LLD/GPIOv2
        os/hal/ports/STM32/LLD/I2Cv3
        os/hal/ports/STM32/LLD/MACv2
        os/hal/ports/STM32/LLD/MDMAv1
        os/hal/ports/STM32/LLD/OCTOSPIv2
        os/hal/ports/STM32/LLD/SPIv3
        os/hal/ports/STM32/LLD/RTCv2
        os/hal/ports/STM32/LLD/SYSTICKv1
        os/hal/ports/STM32/LLD/TIMv1
        os/hal/ports/STM32/LLD/USART
        os/hal/ports/STM32/LLD/USARTv3
        os/hal/ports/STM32/LLD/xWDGv1
        os/hal/ports/STM32/LLD/FDCANv1
        os/hal/ports/STM32/LLD/SDMMCv2
        # Flash Device
        os/hal/lib/complex/serial_nor
        #osal
        os/hal/osal/rt-nil
        #RT
        os/rt/include
        os/common/portability/GCC
        os/common/ports/ARM-common
        os/common/ports/ARMv7-M
        os/oslib/include
        #board and config
        ${BOARD_DIR}
        ${CONFIG_DIR}
        os/various/lwip_bindings
        os/various
        ${LWIP_DIR}/src/include
)


if (USE_SEGGER_RTT OR USE_SEGGER_SYSTEMVIEW)
    message("Using Segger RTT")
    target_compile_definitions(ChibiOS PUBLIC USE_SEGGER_RTT)
    target_sources(ChibiOS PRIVATE
            # Segger RTT
            ext/SEGGER/SEGGER_RTT_ASM_ARMv7M.S
            ext/SEGGER/SEGGER_RTT.c
            ext/SEGGER/SEGGER_RTT_printf.c
            os/various/segger_bindings/RTT/SEGGER_RTT_streams.c
    )
    target_include_directories(ChibiOS PUBLIC
            # Segger RTT
            ext/SEGGER
            os/various/segger_bindings/RTT
    )
endif ()

if(${USE_SEGGER_SYSTEMVIEW})
    message("Using Segger SystemView")
    target_compile_definitions(ChibiOS PUBLIC USE_SEGGER_SYSTEMVIEW)
    target_sources(ChibiOS PRIVATE
            # Segger SystemView
            ext/SEGGER/SEGGER_SYSVIEW.c
            os/various/segger_bindings/SYSTEMVIEW/SEGGER_SYSVIEW_ChibiOS.c
    )
    target_include_directories(ChibiOS PUBLIC
            # Segger SystemView
            os/various/segger_bindings/SYSTEMVIEW
    )
endif ()