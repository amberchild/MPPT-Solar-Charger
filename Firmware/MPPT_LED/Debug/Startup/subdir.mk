################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Startup/startup_stm32l072kztx.s 

OBJS += \
./Startup/startup_stm32l072kztx.o 

S_DEPS += \
./Startup/startup_stm32l072kztx.d 


# Each subdirectory must supply rules for building sources it contributes
Startup/startup_stm32l072kztx.o: ../Startup/startup_stm32l072kztx.s
	arm-none-eabi-gcc -mcpu=cortex-m0plus -g3 -c -x assembler-with-cpp -MMD -MP -MF"Startup/startup_stm32l072kztx.d" -MT"$@"  -mfloat-abi=soft -mthumb -o "$@" "$<"

