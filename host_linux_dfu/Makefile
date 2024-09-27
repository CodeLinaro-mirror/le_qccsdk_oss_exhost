# Makefile for compiling hello.c on Raspberry Pi 4

#HOSTCC = ~/tools/raspiberry/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-gcc
CC = gcc


TARGET=output/qcc730_main
CC = gcc

SOURCES = Service/qcspi_driver.c Service/qcspi_hal.c linux_app_dfu.c

OBJECTS = $(addprefix output/, $(notdir $(SOURCES:.c=.o)))
INCLUDE_DIRS = ./Service


all: output/qcc730_main

output/%.o: Service/%.c
	@mkdir -p output
	$(CC) -I$(INCLUDE_DIRS) -c $< -g -o $@

output/%.o: %.c
	$(CC) -I$(INCLUDE_DIRS) -c $< -g -o $@


output/qcc730_main: $(OBJECTS)
	$(CC) $^ -g -o $@

clean:
	rm -rf output
