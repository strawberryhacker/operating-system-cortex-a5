## SDMMC intro

This document will give a brief introduction to SD card physical layer and the SDMMC interface.

## SD card physical layer

Lets summarize the differen speed modes, capacity modes and voltage modes

### Capacities

 - Standard capacity - up to 2 GB
 - High capacity - between 2 GB and 32 GB
 - Extended capacity - between 32 GB and 2 TB
 - Ultra capacity - between 2 TB and 128 TB
 
### Voltages

 - High voltage - from 2.7V to 3.6V
 - UHS-II SD - VDD1 from 2.7V to 3.6V and VDD2 from 1.7V to 1.95V
 - SD express - same as above but with a VDD3 mode
 
### Speed modes

 - Default speed 3.3V - up to 25 MHz
 - High speed 3.3V - up to 50 MHz
 - SDR12 UHS-I 1.8V - up to 25 MHz
 - SDR25 UHS-I 1.8V - up to 50 MHz
 - SDR50 UHS-I 1.8V - up to 100 MHz
 - SDR104 UHS-I 1.8V - up to 208 MHz
 - DDR50 UHS-I 1.8V - up to 50 MHz (double data rate)
 
On UHS-I cards only default speed and high speed are available at 3.3V signaling, while the other faster modes are available at 1.8V.

### Protocol

The SD protocol is a host centric protocol. It defines the following; a command is a token that starts an operation. This might be a direct command to access a register or a command that initiates a transfer. The command is transfered serially on the CMD line. A response is a message transfered on the MCD line from the card to the host, which is the response to the prevoius command. A data message is transfering data serially on 1 or 4 data lines. This is allways initiated by a command. Either acommand can start a single data transfer, or it can start a multi-block data transfer. In the last option the card will continously send data untill a stop data command has been received. 

### SD card registers
 
The SD card has a couple of important registers
 
 - CID - 128 bit - 
 - RCA - 16 bit - relative card address suggested by card and acked by host
 - DSR - 16 bit -
 - CSD - 128 bit - 
 - SCR - 64 bit - 
 - OCR - 32 bit - 
 - SSR - 512 bit - 
 - CSR - 32 bit -
 
## SDMMC
 
Let take a look at the SDMMC host controller spesification from the SD Association. 
  
The SDMMC defines 18 register in the register map
 
 - SD command generation
 - Response
 - Buffer data port
 - Host control 1 and others
 - Interrupt controls
 - Capabilites
 - Host control 2
 - Force event
 - ADMA2
 - Preset value (all prevoius fields are mandatory on Version 3.00)
 - ADMA3
 - UHS-II
 - Pointers
 - Common area (mandatory)
 - Some UHS-II shit follwing...
 
### SD command generation
 
The register consist of 

 - SDMA system address / ARG2 register
 - Block size register
 - Block count register
 - Argument 1 register
 - Transfer mode register
 - Command register

### Response

The response register are listed below

 - Response register 0
 - Response register 1
 - Response register 2
 - Response register 3
 
### Buffer data port

The buffer data port only consist of one register

 - Buffer data port register
 
### Host control 1 and others

This consist of

 - Present state register
 - Host control 1 register
 - Power control register
 - Block gap control register
 - Wakeup control register
 - Clock control register
 - Timeout control register
 - Software reset register
 
### Interrupt control

 - Normal interrupt status register
 - Error interrupt status register
 - Normal interrupt status enable register
 - Error interrupt status enable register
 - Normal interrupt signal enable register
 - Error interupt signal enable register
 - Auto CMD error status register
 
### Host control 2 register (Version 3.00)

 - Host control 2 register
 
### Capabilities

 - Capabilities 0 register
 - Capabilities 1 register
 
### Force event

 - Force event for ato CMD error status
 - Force event for error interrpt status
 
### ADMA2

 - ADMA error status register
 - ADMA system address register 0

### Preset value

 - Preset value (initialization)
 - Preset value (default speed)
 - Preset value (high-speed)
 - Preset value (SDR12)
 - Preset value (SDR25)
 - Preset value (SDR50)
 - Preset value (SDR104)
 - Preset value (DDR50)
 
### Common area

The comon area follows and are common to all the card slots. 
 
