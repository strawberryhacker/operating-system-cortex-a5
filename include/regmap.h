///  Copyright (C) strawberryhacker 

/// This file contains some of the registers and system definitions for the Atmel
/// SAMA5D27 microprocessor
/// 
/// To add more register definitions do the following. Add a struct with the 
/// mudule name followed by underscore + reg. Add the relevant register entries
/// in the right order (remember to add padding). Use the attributes _r, __wand
/// _rw according to the device datasheet. Define the address of the modules by
/// using the module name in capital letters. It is not necessary to cast to a
/// volatile structure

#ifndef REGMAP_H
#define REGMAP_H

#include <cinnamon/types.h>

// Types used for registers 
#define __r volatile const
#define __w volatile 
#define _rw volatile

// Watchdog registers 
struct wdt_reg {
    __w u32 CR;
    _rw u32 MR;
    __r u32 SR;
};

#define WDT ((struct wdt_reg *)0xF8048040)

// Parallel input output controller registers 
struct gpio_reg {
    _rw u32 MSKR;
    _rw u32 CFGR;
    __r u32 PDSR;
    __r u32 LOCKSR;
    __w u32 SODR;
    __w u32 CODR;
    _rw u32 ODSR;
    __r u32 RESERVED1;
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    __r u32 ISR;
    __r u32 RESERVED2[3];
    __w u32 IOFR;
};

#define GPIOA ((struct gpio_reg *)0xFC038000)
#define GPIOB ((struct gpio_reg *)0xFC038040)
#define GPIOC ((struct gpio_reg *)0xFC038080)
#define GPIOD ((struct gpio_reg *)0xFC0380C0)

// UART registers 
struct uart_reg {
    __w u32 CR;
    _rw u32 MR;  
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    __r u32 SR;
    __r u32 RHR;
    __w u32 THR;
    _rw u32 BRGR;
    _rw u32 CMPR;
    _rw u32 RTOR;
    __r u32 RESERVED[46];
    _rw u32 WPMR;
};

#define UART0 ((struct uart_reg *)0xF801C000)
#define UART1 ((struct uart_reg *)0xF8020000)
#define UART2 ((struct uart_reg *)0xF8024000)
#define UART3 ((struct uart_reg *)0xFC008000)
#define UART4 ((struct uart_reg *)0xFC00C000)

// Power management controller registers 
struct pmc_reg {
    __w u32 SCER;
    __w u32 SCDR;
    __r u32 SCSR;
    __r u32 RESERVED1;
    __w u32 PCER0;
    __w u32 PCDR0;
    __r u32 PCSR0;
    _rw u32 UCKR;
    _rw u32 MOR;
    _rw u32 MCFR;
    _rw u32 PLLAR;
    __r u32 RESERVED2;
    _rw u32 MCKR;
    __r u32 RESERVED3;
    _rw u32 USB;
    __r u32 RESERVED4;
    _rw u32 PCK0;
    _rw u32 PCK1;
    _rw u32 PCK2;
    __r u32 RESERVED5[5];
    __w u32 IER;
    __w u32 IDR;
    __r u32 SR;
    __r u32 IMR;
    _rw u32 FSMR;
    _rw u32 FSPR;
    _rw u32 FOCR;
    __r u32 RESERVED6;
    _rw u32 PLLICPR;
    __r u32 RESERVED7[24];
    _rw u32 WPMR;
    __r u32 WPSR;
    __r u32 RESERVED8[5];
    __w u32 PCER1;
    __w u32 PCDR1;
    __r u32 PCSR1;
    _rw u32 PCR;
    _rw u32 OCR;
    __w u32 SLPWK_ER0;
    __w u32 SLPWK_DR0;
    __r u32 SLPWK_SR0;
    __r u32 SLPWK_ASR0;
    __r u32 RESERVED9[4];
    __w u32 SLPWK_ER1;
    __w u32 SLPWK_DR1;
    __r u32 SLPWK_SR1;
    __r u32 SLPWK_ASR1;
    __r u32 SLPWK_AIPR;
    _rw u32 SLPWKCR;
    _rw u32 AUDIO_PLL0;
    _rw u32 AUDIO_PLL1;
};

#define PMC ((struct pmc_reg *)0xF0014000)

// Peridic interval timer registers 
struct pit_reg {
    _rw u32 MR;
    __r u32 SR;
    __r u32 PIVR;
    __r u32 PIIR;
};

#define PIT ((struct pit_reg *)0xF8048030)

// Advanced interrupt controller registers 
struct apic_reg {
    _rw u32 SSR;
    _rw u32 SMR;
    _rw u32 SVR;
    __r u32 RESERVED0;
    _rw u32 IVR;        // WARNING: dont write unless you know what ur doing 
    __r u32 FVR;
    __r u32 ISR;
    __r u32 RESERVED1;
    __r u32 IPR0;
    __r u32 IPR1;
    __r u32 IPR2;
    __r u32 IPR3;
    __r u32 IMR;
    __r u32 CISR;
    __w u32 EOICR;
    _rw u32 SPU;
    __w u32 IECR;
    __w u32 IDCR;
    __w u32 ICCR;
    __w u32 ISCR;
    __r u32 RESERVED2[7];
    _rw u32 DCR;
    __r u32 RESERVED3[29];
    _rw u32 WPMR;
    __r u32 WPSR;
};

#define APIC ((struct apic_reg *)0xFC020000)
#define SAPIC ((struct apic_reg *)0xF803C000)

// MMC registers 
struct mmc_reg {
    _rw u32 SSAR;
    _rw u16 BSR;
    _rw u16 BCR;
    _rw u32 ARG1R;
    _rw u16 TMR;
    _rw u16 CR;
    __r u32 RR[4];
    _rw u32 BDPR;
    __r u32 PSR;
    _rw u8  HC1R;
    _rw u8  PCR;
    _rw u8  BGCR;
    _rw u8  WCR;
    _rw u16 CCR;
    _rw u8  TCR;
    _rw u8  SRR;
    _rw u16 NISTR;
    _rw u16 EISTR;
    _rw u16 NISTER;
    _rw u16 EISTER;
    _rw u16 NISIER;
    _rw u16 EISIER;
    __r u16 ACESR;
    _rw u16 HC2R;
    __r u32 CA0R;
    _rw u32 CA1R;
    __r u32 MCCAR;
    __r u16 RESERVED0;
    __w u16 FERACES;
    __w u32 FEREIS;
    __r u8  AESR;
    __r u8  RESERVED1[3];
    _rw u32 ASAR;
    __r u32 RESERVED2;
    _rw u16 PVR[8];
    __r u32 RESERVED3[35];
    __r u16 SISR;
    __r u16 HCVR;
    __r u32 RESERVED4[64];
    __r u32 APSR;
    _rw u8  MC1R;
    __w u8  MC2R;
    __r u16 RESERVED5;
    _rw u32 ACR;
    _rw u32 CC2R;
    _rw u8  RTC1R;
    __w u8  RTC2R;
    __r u16 RESERVED6;
    _rw u32 RTCVR;
    _rw u8  RTISTER;
    _rw u8  RTISIER;
    __r u16 RESERVED7;
    _rw u8  RTISTR;
    __r u8  RTSSR;
    __r u16 RESERVED8;
    _rw u32 TUNCR;
    __r u32 RESERVED9[3];
    _rw u32 CACR;
    __r u32 RESERVED10[3];
    _rw u32 CALCR;
};

#define MMC0 ((struct mmc_reg *)0xA0000000)
#define MMC1 ((struct mmc_reg *)0xB0000000)

// DDR registers 
struct ddr_reg {
    _rw u32 MR;
    _rw u32 RTR;
    _rw u32 CR;
    _rw u32 TPR0;
    _rw u32 TPR1;
    _rw u32 TPR2;
    __r u32 RESERVED0;
    _rw u32 LPR;
    _rw u32 MD;
    __r u32 RESERVED1;
    _rw u32 LPDDR2_LPDDR3_LPR;
    _rw u32 LPDDR2_LPDDR3_DDR3_MR4;
    _rw u32 LPDDR2_LPDDR3_DDR3_CAL;
    _rw u32 IO_CALIBR;
    _rw u32 OCMS;
    __w u32 OCMS_KEY1;
    __w u32 OCMS_KEY2;
    _rw u32 CONF_ARBITER;
    _rw u32 TIMEOUT;
    _rw u32 REQ_PORT_0123;
    _rw u32 REQ_PORT_4567;
    __r u32 BDW_PORT_0123;
    __r u32 BDW_PORT_4567;
    _rw u32 RD_DATA_PATH;
    _rw u32 MCFGR;
    _rw u32 MADDR[8];
    __r u32 MINFO[8];
    __r u32 RESERVED2[16];
    _rw u32 WPMR;
    __r u32 WPSR;
};

#define DDR ((struct ddr_reg *)0xF000C000)

struct matrix_pri {
    _rw u32 A;
    _rw u32 B;
};

// Matrix AHB-64 and AHB-32 register 
struct matrix_reg {
    _rw u32 MCFG[12];
    __r u32 RESERVED0[4];
    _rw u32 SCFG[15];
    __r u32 RESERVED1;
    struct matrix_pri PRI[15];
    __r u32 RESERVED2[21];
    __w u32 MEIER;
    __w u32 MEIDR;
    __r u32 MEIMR;
    __r u32 MESR;
    __r u32 MEAR[12];
    __r u32 RESERVED3[21];
    _rw u32 WPMR;
    __r u32 WPSR;
    __r u32 RESERVED4[5];
    _rw u32 SSR[15];
    __r u32 RESERVED5;
    _rw u32 SASSR[15];
    __r u32 RESERVED6[2];
    _rw u32 SRTSR[15];
    __r u32 RESERVED7;
    _rw u32 SPSELR1;
    _rw u32 SPSELR2;
    _rw u32 SPSELR3;
};

#define H32MX ((struct matrix_reg *)0xFC03C000)
#define H64MX ((struct matrix_reg *)0xF0018000)

// Special function register 
struct sfr_reg {
    __r u32 RESERVED0;
    _rw u32 DDRCFG;
    __r u32 RESERVED1[2];
    _rw u32 OHCIICR;
    __r u32 OHCIISR;
    __r u32 RESERVED2[4];
    _rw u32 SECURE;
    __r u32 RESERVED3;
    _rw u32 UTMICKTRIM;
    _rw u32 UTMIHSTRIM;
    _rw u32 UTMIFSTRIM;
    _rw u32 UTMISWAP;
    __r u32 RESERVED4[2];
    _rw u32 CAN;
    __r u32 SN0;
    __r u32 SN1;
    _rw u32 AICREDIR;
    _rw u32 L2CC_HRAMC;
    __r u32 RESERVED5[13];
    _rw u32 I2SCLKSEL;
    _rw u32 QSPICLK;
};

#define SFR ((struct sfr_reg *)0xF8030000)

// Reset controller registers 
struct reset_reg {
    __w u32 CR;
    __r u32 SR;
    _rw u32 MR;
};

#define RST ((struct reset_reg *)0xF8048000)

// L2 cache controller register 
struct l2cache_reg {
    __r u32 IDR;
    __r u32 TYPR;
    __r u32 RESERVED1[62];
    _rw u32 CR;
    _rw u32 ACR;
    _rw u32 TRCR;
    _rw u32 DRCR;
    __r u32 RESERVED2[60];
    _rw u32 ECR;
    _rw u32 ECFGR1;
    _rw u32 ECFGR0;
    _rw u32 EVR1;
    _rw u32 EVR0;
    __r u32 IMR;
    __r u32 MISR;
    __r u32 RISR;
    __r u32 ICR;
    __r u32 RESERVED3[323];
    _rw u32 CSR;
    __r u32 RESERVED4[15];
    _rw u32 IPALR;
    __r u32 RESERVED5[2];
    _rw u32 IWR;
    __r u32 RESERVED6[12];
    _rw u32 CPALR;
    __r u32 RESERVED7;
    _rw u32 CIR;
    _rw u32 CWR;
    __r u32 RESERVED8[12];
    _rw u32 CIPALR;
    __r u32 RESERVED;
    _rw u32 CIIR;
    _rw u32 CIWR;
    __r u32 RESERVED9[64];
    __r u32 DLKR;
    __r u32 ILKR;
    __r u32 RESERVED10[398];
    _rw u32 DCR;
    __r u32 RESERVED11[7];
    _rw u32 PCR;
    __r u32 RESERVED12[7];
    _rw u32 POWCR;
};

#define L2CAHCE ((struct l2cache_reg *)0x00A00000)

struct trng_reg {
    __w u32 CR;
    __r u32 RESERVED0[3];
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    __r u32 ISR;
    __r u32 RESERVED1[12];
    __r u32 ODATA;
};

#define TRNG ((struct trng_reg *)0xFC01C000)

struct timer_channel_reg {
    __w u32 CCR;
    _rw u32 CMR;
    _rw u32 SMMR;
    __r u32 RAB;
    __r u32 CV;
    _rw u32 RA;
    _rw u32 RB;
    _rw u32 RC;
    __r u32 SR;
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    _rw u32 EEMR;
    __r u32 RESERVED[3];
};

struct timer_reg {
    struct timer_channel_reg channel[3];

    __w u32 BCR;
    _rw u32 BMR;
    __w u32 QIER;
    __w u32 QIDR;
    __r u32 QIMR;
    __r u32 QISR;
    _rw u32 FMR;
    __r u32 RESERVED[2];
    _rw u32 WPMR;
};

#define TIMER0 ((struct timer_reg *)0xF800C000)
#define TIMER1 ((struct timer_reg *)0xF8010000)

#endif
