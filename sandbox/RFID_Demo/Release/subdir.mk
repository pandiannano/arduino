################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Menu.cpp \
../RFID_Demo.cpp 

OBJS += \
./Menu.o \
./RFID_Demo.o 

CPP_DEPS += \
./Menu.d \
./RFID_Demo.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: AVR C++ Compiler'
	avr-g++ -I"/Users/shawn/Arduino/developer/Arduino.app/Contents/Resources/Java/libraries/SPI" -I"/Users/shawn/Arduino/developer/Arduino.app/Contents/Resources/Java/hardware/arduino/cores/arduino" -I"/Users/shawn/Arduino/developer/Arduino.app/Contents/Resources/Java/hardware/arduino/variants/mega" -I"/Users/shawn/works/tb-joylabs/arduino.git/sandbox/RFID_Demo/../../libraries" -D__IN_ECLIPSE__=1 -DARDUINO=103 -DUSB_PID= -DUSB_VID= -Wall -Os -ffunction-sections -fdata-sections -fno-exceptions -g -mmcu=atmega2560 -DF_CPU=16000000UL -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)"  -c -o "$@" -x c++ "$<"
	@echo 'Finished building: $<'
	@echo ' '


