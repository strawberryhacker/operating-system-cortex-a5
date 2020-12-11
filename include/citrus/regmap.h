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

#include <citrus/types.h>

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

/// True random number generator
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

/// Timer channel registers
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

#define DMA_CHANNELS 16

/// DMA registers
struct dma_reg {
    __r u32 GTYPE;
    _rw u32 GCFG;
    _rw u32 GWAC;
    __w u32 GIE;
    __w u32 GID;
    __r u32 GIM;
    __r u32 GIS;
    __w u32 GE;
    __w u32 GD;
    __r u32 GS;
    _rw u32 GRS;
    _rw u32 GWS;
    __w u32 GRWS;
    __w u32 GRWR;
    __w u32 GSWR;
    __r u32 GSWS;
    __w u32 GSWF;
    __r u32 RESERVED0[3];

    struct {
        __w u32 CIE;
        __w u32 CID;
        __r u32 CIM;
        __r u32 CIS;
        _rw u32 CSA;
        _rw u32 CDA;
        _rw u32 CNDA;
        _rw u32 CNDC;
        _rw u32 CUBC;
        _rw u32 CBC;
        _rw u32 CC;
        _rw u32 CDS_MSP;
        _rw u32 CSUS;
        _rw u32 CDUS;
        __r u32 RESERVED1[2];
    } channel[DMA_CHANNELS];
};

#define DMA0 ((struct dma_reg *)0xF0010000)
#define DMA1 ((struct dma_reg *)0xF0004000)

/// SPI registers
struct spi_reg {
    __w u32 CR;
    _rw u32 MR;
    __r u32 RDR;
    __w u32 TDR;
    __r u32 SR;
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR; 
    __r u32 RESERVED0[4];

    // Chip select registers
    struct {
        _rw u32 CSR;
    } chip_select[4];

    _rw u32 FMR;
    __r u32 FLR;
    __r u32 CMPR;
    __r u32 RESERVED1[38];
    _rw u32 WPMR;
    __r u32 WPSR;
};

#define SPI0 ((struct spi_reg *)0xF8000000)
#define SPI1 ((struct spi_reg *)0xFC000000)

/// Flexcom registers only implemention UART mode
struct flexcom_reg {
    _rw u32 FLEX_MR;
    __r u32 RESERVED0[3];
    __r u32 FLEX_RHR;
    __r u32 RESERVED1[3];
    _rw u32 FLEX_THR;
    _rw u32 RESERVED2[119];
    __w u32 U_CR;
    _rw u32 U_MR;
    __w u32 U_IER;
    __w u32 U_IDR;
    __r u32 U_IMR;
    __r u32 U_SR;
    __r u32 U_RHR;
    __w u32 U_THR;
    _rw u32 U_BRGR;
    _rw u32 U_RTOR;
    _rw u32 U_TTGR;
};

#define FLEX0 ((struct flexcom_reg *)0xF8034000)
#define FLEX1 ((struct flexcom_reg *)0xF8038000)
#define FLEX2 ((struct flexcom_reg *)0xFC010000)
#define FLEX3 ((struct flexcom_reg *)0xFC014000)
#define FLEX4 ((struct flexcom_reg *)0xFC018000)

/// LCD control registers
struct lcd_ctrl {
    __w u32 CHER;
    __w u32 CHDR;
    __r u32 CHSR;
    __w u32 IER;
    __w u32 IDR;
    __r u32 IMR;
    __r u32 ISR;
};

struct lcd_dma {
    _rw u32 HEAD;
    _rw u32 ADDR;
    _rw u32 CTRL;
    _rw u32 NEXT;
};

/// LCD registers
struct lcd_reg {
    _rw u32 LCDCFG0;
    _rw u32 LCDCFG1;
    _rw u32 LCDCFG2;
    _rw u32 LCDCFG3;
    _rw u32 LCDCFG4;
    _rw u32 LCDCFG5;
    _rw u32 LCDCFG6;
    __r u32 RESERVED0;
    __w u32 LCDEN;
    __w u32 LCDDIS;
    __r u32 LCDSR;
    __w u32 LCDIER;
    __w u32 LCDIDR;
    __r u32 LCDIMR;
    __r u32 LCDISR;
    __w u32 ATTR;

    // Base layer
    struct lcd_ctrl base_ctrl;
    struct lcd_dma base_dma;
    _rw u32 BASECFG[7];
    __r u32 RESERVED1[46];

    // Overlay 1
    struct lcd_ctrl ov1_ctrl;
    struct lcd_dma ov1_dma;
    _rw u32 OV1CFG[10];
    __r u32 RESERVED2[43];

    // Overlay 2
    struct lcd_ctrl ov2_ctrl;
    struct lcd_dma ov2_dma;
    _rw u32 OV2CFG[10];
    __r u32 RESERVED3[43];

    // High end overlay
    struct lcd_ctrl heo_ctrl;
    struct lcd_dma heo_dma;
    struct lcd_dma heo_udma;
    struct lcd_dma heo_vdma;
    _rw u32 HEOCFG[42];
    __r u32 RESERVED4[67];

    // Post processing
    struct lcd_ctrl pp_ctrl;
    struct lcd_dma pp_dma;
    _rw u32 PPCFG[6];
    __r u32 RESERVED5[31];

    _rw u32 BASECLUT[256];
    _rw u32 OV1CLUT[256];
    _rw u32 OV2CLUT[256];
    _rw u32 HEOCLUT[256];
};

#define LCD ((struct lcd_reg *)0xF0000000)

struct gmac_reg {
    _rw u32 NCR;
    _rw u32 NCFGR;
    __r u32 NSR;
    _rw u32 UR;
    _rw u32 DCFGR;
    _rw u32 TSR;
    _rw u32 RBQB;
    _rw u32 TBQB;
    _rw u32 RSR;
    __r u32 ISR;
    __w u32 IER;
    __w u32 IDR;
    _rw u32 IMR;
    _rw u32 MAN;
    __r u32 RPQ;
    _rw u32 TPQ;
    _rw u32 TPSF;
    _rw u32 RPSF;
    _rw u32 RJFML;
    __r u32 RESERVED0[13];
    _rw u32 HRB;
    _rw u32 HRT;
    struct {
        _rw u32 BTM;
        _rw u32 TOP;
    } SA[4];
    _rw u32 TIDM[4];
    _rw u32 WOL;
    _rw u32 IPGS;
    _rw u32 SVLAN;
    _rw u32 TPFCP;
    _rw u32 SAMB1;
    _rw u32 SAMT1;
    __r u32 RESERVED1[3];
    _rw u32 NSC;
    _rw u32 SCL;
    _rw u32 SCH;
    __r u32 EFTSH;
    __r u32 EFRSH;
    __r u32 PEFTSH;
    __r u32 PEFRSH;
    __r u32 RESERVED2[2];
    __r u32 OTLO;
    __r u32 OTHI;
    __r u32 FT;
    __r u32 BCFT;
    __r u32 MFT;
    __r u32 PFT;
    __r u32 BFT64;
    __r u32 TBFT127;
    __r u32 TBFT255;
    __r u32 TBFT511;
    __r u32 TBFT1023;
    __r u32 TBFT1518;
    __r u32 GTBFT1518;
    __r u32 TUR;
    __r u32 SCF;
    __r u32 MCF;
    __r u32 EC;
    __r u32 LC;
    __r u32 DTF;
    __r u32 CSE;
    __r u32 ORLO;
    __r u32 ORHI;
    __r u32 FR;
    __r u32 BCFR;
    __r u32 MFR;
    __r u32 PFR;
    __r u32 BFR64;
    __r u32 TBFR127;
    __r u32 TBFR255;
    __r u32 TBFR511;
    __r u32 TBFR1023;
    __r u32 TBFR1518;
    __r u32 TMXBFR;
    __r u32 UFR;
    __r u32 OFR;
    __r u32 JR;
    __r u32 FCSE;
    __r u32 LFFE;
    __r u32 RSE;
    __r u32 AE;
    __r u32 RRE;
    __r u32 ROE;
    __r u32 IHCE;
    __r u32 TCE;
    __r u32 UCE;
    __r u32 RESERVED3[2];
    _rw u32 TISUBN;
    _rw u32 TSH;
    __r u32 RESERVED4[3];
    _rw u32 TSL;
    _rw u32 TN;
    __w u32 TA;
    _rw u32 TI;
    __r u32 EFTSL;
    __r u32 EFTN;
    __r u32 EFRSL;
    __r u32 EFRN;
    __r u32 PEFTSL;
    __r u32 PEFTN;
    __r u32 PEFRSL;
    __r u32 PEFRN;
    __r u32 RESERVED5[28];
    __r u32 RXLPI;
    __r u32 RXLPITIME;
    __r u32 TXLPI;
    __r u32 TXLPITIME;
    __r u32 RESERVED6[95];
    __r u32 ISRPQ[2];
    __r u32 RESERVED7[14];
    _rw u32 TBQBAPQ[2];
    __r u32 RESERVE8[14];
    _rw u32 RBQBAPQ[2];
    __r u32 RESERVED9[6];
    _rw u32 RBSRPQ[2];
    __r u32 RESERVED10[5];
    _rw u32 CBSCR;
    _rw u32 CBSISQA;
    _rw u32 CBSISQB;
    __r u32 RESERVED17[14];
    _rw u32 ST1RPQ[4];
    __r u32 RESERVED11[12];
    _rw u32 ST2RPQ[8];
    __r u32 RESERVED12[40];
    __w u32 IERPQ[2];
    __r u32 RESERVED13[6];
    __w u32 IDRPQ[2];
    __r u32 RESERVED14[6];
    _rw u32 IMRPQ[2];
    __r u32 RESERVED15[38];
    _rw u32 ST2ER[4];
    __r u32 RESERVED16[4];
    struct {
        _rw u32 CW0;
        _rw u32 CW1;
    } ST2[24];
};

#define GMAC ((struct gmac_reg *)0xF8008000)

#endif
