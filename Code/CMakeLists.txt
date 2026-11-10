cmake_minimum_required(VERSION 3.20)

project(RockBurst C ASM)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(IS_ABSOLUTE "${CMAKE_C_COMPILER}")
    get_filename_component(ROCKBURST_TOOLCHAIN_BIN
                           "${CMAKE_C_COMPILER}"
                           DIRECTORY)
    if(CMAKE_HOST_WIN32)
        set(ROCKBURST_OBJCOPY
            "${ROCKBURST_TOOLCHAIN_BIN}/arm-none-eabi-objcopy.exe")
        set(ROCKBURST_SIZE
            "${ROCKBURST_TOOLCHAIN_BIN}/arm-none-eabi-size.exe")
    else()
        set(ROCKBURST_OBJCOPY
            "${ROCKBURST_TOOLCHAIN_BIN}/arm-none-eabi-objcopy")
        set(ROCKBURST_SIZE
            "${ROCKBURST_TOOLCHAIN_BIN}/arm-none-eabi-size")
    endif()
    if(EXISTS "${ROCKBURST_OBJCOPY}" AND EXISTS "${ROCKBURST_SIZE}")
        set(CMAKE_OBJCOPY "${ROCKBURST_OBJCOPY}")
        set(CMAKE_SIZE "${ROCKBURST_SIZE}")
    endif()
endif()

set(TARGET_NAME rock_burst)
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/ld/STM32F407VE_FLASH.ld)

set(ROCKBURST_APP "ROBOT" CACHE STRING "Firmware application")
set_property(CACHE ROCKBURST_APP PROPERTY STRINGS ROBOT TEST_VEHICLE_LIGHT TEST_LED_BLINK TEST_THROTTLE_PWM)

set(ROCKBURST_NETWORK_PROFILE "ANDROID_DIRECT" CACHE STRING "Static network profile")
set_property(CACHE ROCKBURST_NETWORK_PROFILE PROPERTY STRINGS PC_DIRECT ANDROID_DIRECT)

if((NOT ROCKBURST_APP STREQUAL "ROBOT") AND
   (NOT ROCKBURST_APP STREQUAL "TEST_VEHICLE_LIGHT") AND
   (NOT ROCKBURST_APP STREQUAL "TEST_LED_BLINK") AND
   (NOT ROCKBURST_APP STREQUAL "TEST_THROTTLE_PWM"))
    message(FATAL_ERROR "ROCKBURST_APP must be ROBOT, TEST_VEHICLE_LIGHT, TEST_LED_BLINK, or TEST_THROTTLE_PWM")
endif()

if(ROCKBURST_NETWORK_PROFILE STREQUAL "PC_DIRECT")
    set(ROCKBURST_NET_CONFIG_PROFILE_VALUE 1)
elseif(ROCKBURST_NETWORK_PROFILE STREQUAL "ANDROID_DIRECT")
    set(ROCKBURST_NET_CONFIG_PROFILE_VALUE 2)
else()
    message(FATAL_ERROR "ROCKBURST_NETWORK_PROFILE must be PC_DIRECT or ANDROID_DIRECT")
endif()

set(COMMON_SOURCES
    Core/startup_stm32f407ve_gcc.S

    User/main.c
    User/syscalls.c
    User/stm32f4xx_it.c
    User/system_stm32f4xx.c

    System/delay.c

    Hardware/board_led.c
    Hardware/max22216.c
    Hardware/lan8720.c
    Hardware/rs485.c
    Hardware/GP8403.c
    Hardware/relay.c
    Hardware/vehicle_light.c
    Hardware/throttle_pwm.c
    Hardware/button.c
    Hardware/hall_sensor.c

    Network/network.c
    Network/ethernetif.c

    Network/lwip/src/core/init.c
    Network/lwip/src/core/def.c
    Network/lwip/src/core/inet_chksum.c
    Network/lwip/src/core/ip.c
    Network/lwip/src/core/mem.c
    Network/lwip/src/core/memp.c
    Network/lwip/src/core/netif.c
    Network/lwip/src/core/pbuf.c
    Network/lwip/src/core/stats.c
    Network/lwip/src/core/timeouts.c
    Network/lwip/src/core/tcp.c
    Network/lwip/src/core/tcp_in.c
    Network/lwip/src/core/tcp_out.c
    Network/lwip/src/core/ipv4/etharp.c
    Network/lwip/src/core/ipv4/icmp.c
    Network/lwip/src/core/ipv4/ip4.c
    Network/lwip/src/core/ipv4/ip4_addr.c
    Network/lwip/src/netif/ethernet.c
)

set(APP_SOURCES)
if(ROCKBURST_APP STREQUAL "ROBOT")
    list(APPEND APP_SOURCES
        Robot/robot_app.c
        Robot/robot_model.c
        Robot/control_server.c
        Robot/robot_protocol.c

        Control/control.c
        Control/binding_manager.c
        Control/config_store.c
        Control/drive_mixer.c

        Device/device_hydraulic.c
    )
elseif(ROCKBURST_APP STREQUAL "TEST_VEHICLE_LIGHT")
    list(APPEND APP_SOURCES
        Test/vehicle_light_test_app.c
    )
elseif(ROCKBURST_APP STREQUAL "TEST_LED_BLINK")
    list(APPEND APP_SOURCES
        Test/led_blink_test_app.c
    )
elseif(ROCKBURST_APP STREQUAL "TEST_THROTTLE_PWM")
    list(APPEND APP_SOURCES
        Test/throttle_pwm_test_app.c
    )
else()
    message(FATAL_ERROR "Unexpected ROCKBURST_APP value")
endif()

add_executable(${TARGET_NAME})

target_sources(${TARGET_NAME} PRIVATE
    ${COMMON_SOURCES}
    ${APP_SOURCES}

    Fwlib/src/misc.c
    Fwlib/src/stm32f4xx_adc.c
    Fwlib/src/stm32f4xx_can.c
    Fwlib/src/stm32f4xx_crc.c
    Fwlib/src/stm32f4xx_cryp.c
    Fwlib/src/stm32f4xx_cryp_aes.c
    Fwlib/src/stm32f4xx_cryp_des.c
    Fwlib/src/stm32f4xx_cryp_tdes.c
    Fwlib/src/stm32f4xx_dac.c
    Fwlib/src/stm32f4xx_dbgmcu.c
    Fwlib/src/stm32f4xx_dcmi.c
    Fwlib/src/stm32f4xx_dma.c
    Fwlib/src/stm32f4xx_dma2d.c
    Fwlib/src/stm32f4xx_exti.c
    Fwlib/src/stm32f4xx_flash.c
    Fwlib/src/stm32f4xx_flash_ramfunc.c
    Fwlib/src/stm32f4xx_fsmc.c
    Fwlib/src/stm32f4xx_gpio.c
    Fwlib/src/stm32f4xx_hash.c
    Fwlib/src/stm32f4xx_hash_md5.c
    Fwlib/src/stm32f4xx_hash_sha1.c
    Fwlib/src/stm32f4xx_i2c.c
    Fwlib/src/stm32f4xx_iwdg.c
    Fwlib/src/stm32f4xx_ltdc.c
    Fwlib/src/stm32f4xx_pwr.c
    Fwlib/src/stm32f4xx_rcc.c
    Fwlib/src/stm32f4xx_rng.c
    Fwlib/src/stm32f4xx_rtc.c
    Fwlib/src/stm32f4xx_sai.c
    Fwlib/src/stm32f4xx_sdio.c
    Fwlib/src/stm32f4xx_spi.c
    Fwlib/src/stm32f4xx_syscfg.c
    Fwlib/src/stm32f4xx_tim.c
    Fwlib/src/stm32f4xx_usart.c
    Fwlib/src/stm32f4xx_wwdg.c

)

target_include_directories(${TARGET_NAME} PRIVATE
    Core
    Fwlib/inc
    User
    System
    Hardware
    App
    Control
    Device
    Robot
    Test
    Network
    Network/lwip/src/include
)

target_link_libraries(${TARGET_NAME} PRIVATE
    m
)

target_compile_definitions(${TARGET_NAME} PRIVATE
    STM32F40_41xxx
    USE_STDPERIPH_DRIVER
    NET_CONFIG_PROFILE=${ROCKBURST_NET_CONFIG_PROFILE_VALUE}
)

target_compile_options(${TARGET_NAME} PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -Og
    -g3
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_options(${TARGET_NAME} PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    --specs=nosys.specs
)

set(ELF_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.elf)
set(HEX_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex)
set(BIN_FILE ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME ${TARGET_NAME}
    SUFFIX ".elf"
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${HEX_FILE}
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${BIN_FILE}
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    BYPRODUCTS ${HEX_FILE} ${BIN_FILE}
    COMMENT "Generating HEX/BIN and printing image size"
)
