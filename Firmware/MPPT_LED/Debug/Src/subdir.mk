################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/StringCommandParser.c \
../Src/eeprom.c \
../Src/freertos.c \
../Src/indication_task.c \
../Src/led_control_task.c \
../Src/main.c \
../Src/management_task.c \
../Src/modem.c \
../Src/monitor_task.c \
../Src/stm32l0xx_hal_msp.c \
../Src/stm32l0xx_hal_timebase_tim.c \
../Src/stm32l0xx_it.c \
../Src/syscalls.c \
../Src/sysmem.c \
../Src/system_stm32l0xx.c 

OBJS += \
./Src/StringCommandParser.o \
./Src/eeprom.o \
./Src/freertos.o \
./Src/indication_task.o \
./Src/led_control_task.o \
./Src/main.o \
./Src/management_task.o \
./Src/modem.o \
./Src/monitor_task.o \
./Src/stm32l0xx_hal_msp.o \
./Src/stm32l0xx_hal_timebase_tim.o \
./Src/stm32l0xx_it.o \
./Src/syscalls.o \
./Src/sysmem.o \
./Src/system_stm32l0xx.o 

C_DEPS += \
./Src/StringCommandParser.d \
./Src/eeprom.d \
./Src/freertos.d \
./Src/indication_task.d \
./Src/led_control_task.d \
./Src/main.d \
./Src/management_task.d \
./Src/modem.d \
./Src/monitor_task.d \
./Src/stm32l0xx_hal_msp.d \
./Src/stm32l0xx_hal_timebase_tim.d \
./Src/stm32l0xx_it.d \
./Src/syscalls.d \
./Src/sysmem.d \
./Src/system_stm32l0xx.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32L072xx -DDEBUG -c -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Inc -I../Drivers/CMSIS/Include -I../Drivers/STM32L0xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../Drivers/STM32L0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@"

