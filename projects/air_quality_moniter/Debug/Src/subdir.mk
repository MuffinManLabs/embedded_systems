################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/bme280.c \
../Src/main.c \
../Src/rcc.c \
../Src/spi2.c \
../Src/syscalls.c \
../Src/sysmem.c \
../Src/systick.c \
../Src/usart2.c 

OBJS += \
./Src/bme280.o \
./Src/main.o \
./Src/rcc.o \
./Src/spi2.o \
./Src/syscalls.o \
./Src/sysmem.o \
./Src/systick.o \
./Src/usart2.o 

C_DEPS += \
./Src/bme280.d \
./Src/main.d \
./Src/rcc.d \
./Src/spi2.d \
./Src/syscalls.d \
./Src/sysmem.d \
./Src/systick.d \
./Src/usart2.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DSTM32 -DSTM32F407G_DISC1 -DSTM32F4 -DSTM32F407VGTx -c -I../Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/bme280.cyclo ./Src/bme280.d ./Src/bme280.o ./Src/bme280.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/rcc.cyclo ./Src/rcc.d ./Src/rcc.o ./Src/rcc.su ./Src/spi2.cyclo ./Src/spi2.d ./Src/spi2.o ./Src/spi2.su ./Src/syscalls.cyclo ./Src/syscalls.d ./Src/syscalls.o ./Src/syscalls.su ./Src/sysmem.cyclo ./Src/sysmem.d ./Src/sysmem.o ./Src/sysmem.su ./Src/systick.cyclo ./Src/systick.d ./Src/systick.o ./Src/systick.su ./Src/usart2.cyclo ./Src/usart2.d ./Src/usart2.o ./Src/usart2.su

.PHONY: clean-Src

