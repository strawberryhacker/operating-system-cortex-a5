// Copyright (C) strawberryhacker

#include <citrus/spi.h>
#include <citrus/print.h>
#include <citrus/regmap.h>

void spi_init(void)
{
    // Master mode
    SPI1->MR = (1 << 0);
    SPI1->chip_select[0].CSR = (255 << 8) | (1 << 1) | (10 << 24) | (10 << 16);
    SPI1->CR = (1 << 0);
}

void spi_out(u8 data)
{
    while (!(SPI1->SR & (1 << 1)));
    SPI1->TDR = data;
}
