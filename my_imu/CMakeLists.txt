cmake_minimum_required(VERSION 3.20)

project(my_imu LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(LINKER_SCRIPT "${PROJECT_DIR}/cmake/gcc/STM32H723VGTx_FLASH.ld")
set(STARTUP_FILE "${PROJECT_DIR}/cmake/gcc/startup_stm32h723xx.S")

set(APP_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/dma.c
    Core/Src/spi.c
    Core/Src/stm32h7xx_hal_msp.c
    Core/Src/stm32h7xx_it.c
    Core/Src/system_stm32h7xx.c
    Bsp/BMI088/BMI088driver.c
    Bsp/BMI088/BMI088Middleware.c
    Bsp/WS2812/ws2812.c
    App/usb_bsp.c
    USB_DEVICE/App/usb_device.c
    USB_DEVICE/App/usbd_cdc_if.c
    USB_DEVICE/App/usbd_desc.c
    USB_DEVICE/Target/usbd_conf.c
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
)

set(HAL_SOURCES
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c
    Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c
)

list(TRANSFORM APP_SOURCES PREPEND "${PROJECT_DIR}/")
list(TRANSFORM HAL_SOURCES PREPEND "${PROJECT_DIR}/")

add_executable(my_imu.elf
    ${APP_SOURCES}
    ${HAL_SOURCES}
    ${STARTUP_FILE}
)

target_compile_definitions(my_imu.elf PRIVATE
    USE_HAL_DRIVER
    STM32H723xx
)

target_include_directories(my_imu.elf PRIVATE
    Core/Inc
    Bsp/BMI088/inc
    Bsp/BMI088
    Bsp/WS2812/inc
    App
    Drivers/STM32H7xx_HAL_Driver/Inc
    Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32H7xx/Include
    Drivers/CMSIS/Include
    USB_DEVICE/App
    USB_DEVICE/Target
    Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
)

set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(my_imu.elf PRIVATE
    ${MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_options(my_imu.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/my_imu.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
)

target_link_libraries(my_imu.elf PRIVATE m c nosys)

add_custom_command(TARGET my_imu.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:my_imu.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/my_imu.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:my_imu.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/my_imu.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:my_imu.elf>
    BYPRODUCTS
        ${CMAKE_CURRENT_BINARY_DIR}/my_imu.hex
        ${CMAKE_CURRENT_BINARY_DIR}/my_imu.bin
    COMMENT "Generating my_imu.hex and my_imu.bin"
)

set_target_properties(my_imu.elf PROPERTIES
    LINK_DEPENDS "${LINKER_SCRIPT}"
)
