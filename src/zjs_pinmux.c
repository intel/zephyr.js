// Copyright (c) 2017, Intel Corporation.

// This file is currently meant only for FRDM_K64F board

// C includes
#include <stdio.h>

// Zephyr includes
#include <kernel.h>
#include <init.h>
#include <pinmux.h>
#include <pinmux/pinmux.h>
#include <fsl_port.h>

// CONFIG_INIT_PINMUX_PRIORITY + 1 to override default pinmux settings
#define CONFIG_K64F_PINMUX_PRIORITY 46

int frdm_k64f_pinmux_setup(struct device *unused)
{
    // NOTE: Here we enable all the controllers but in a real design you would
    //   probably try to optimize for the minimum number of GPIO controllers
    //   turned on to save power. See zephyr/boards/arm/frdm_k64f/pinmux.c for
    //   defaults.

    // TODO: eventually we maybe need to analyze script to decide how to do
    //   pinmux, or have user provide a static configuration
    struct device *porta = device_get_binding(CONFIG_PINMUX_MCUX_PORTA_NAME);
    pinmux_pin_set(porta, 0, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(porta, 1, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(porta, 2, PORT_PCR_MUX(kPORT_MuxAsGpio));

    struct device *portb = device_get_binding(CONFIG_PINMUX_MCUX_PORTB_NAME);
    pinmux_pin_set(portb,  9, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portb, 23, PORT_PCR_MUX(kPORT_MuxAsGpio));

    struct device *portc = device_get_binding(CONFIG_PINMUX_MCUX_PORTC_NAME);
    pinmux_pin_set(portc,  2, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portc,  3, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portc,  4, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portc, 16, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portc, 17, PORT_PCR_MUX(kPORT_MuxAsGpio));
#ifndef CONFIG_SPI_0
    /* SPI0 CS0, SCK, SOUT, SIN */
    struct device *portd = device_get_binding(CONFIG_PINMUX_MCUX_PORTD_NAME);
    pinmux_pin_set(portd, 0, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portd, 1, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portd, 2, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(portd, 3, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif
    struct device *porte = device_get_binding(CONFIG_PINMUX_MCUX_PORTE_NAME);
#if CONFIG_I2C_0
    pinmux_pin_set(porte, 24, PORT_PCR_MUX(kPORT_MuxAlt5)
                            | PORT_PCR_ODE_MASK);
    pinmux_pin_set(porte, 25, PORT_PCR_MUX(kPORT_MuxAlt5)
                            | PORT_PCR_ODE_MASK);
#else
    pinmux_pin_set(porte, 24, PORT_PCR_MUX(kPORT_MuxAsGpio));
    pinmux_pin_set(porte, 25, PORT_PCR_MUX(kPORT_MuxAsGpio));
#endif
    return 0;
}

SYS_INIT(frdm_k64f_pinmux_setup, POST_KERNEL, CONFIG_K64F_PINMUX_PRIORITY);
