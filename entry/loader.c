/// Copyright (C) strawberryhacker

#include <citrus/loader.h>
#include <citrus/types.h>
#include <citrus/timer.h>
#include <citrus/print.h>
#include <citrus/apic.h>
#include <citrus/crc.h>
#include <citrus/panic.h>
#include <citrus/interrupt.h>
#include <citrus/page_alloc.h>
#include <citrus/align.h>
#include <citrus/elf.h>
#include <regmap.h>
#include <stddef.h>

/// This loader is temorarily and communicates with the host computer over
/// serial. It is capable of receiving frames with a cmd field and up to 4k of
/// data. The fram format is
/// 
///   [ start ]  [ cmd ]  [ size ]  [ data ]  [ crc ]  [ end ]
///
/// The crc field is a 8-bit CRC8 which is calculated over the folling fields;
/// cmd, size and data

#define START_TAG 0xAA
#define END_TAG   0x55
#define POLY      0xB2

/// Private variables for the state machine
static volatile struct packet packet;
volatile enum loader_state loader_state;

/// Sends a reponse back to the host computer. This should be 0 for failure and
/// 1 for success
static void loader_send_resp(u8 resp)
{
    // Send the byte
    while (!(UART4->SR & (1 << 1)));
    UART4->THR = resp;
}

/// Processes an ELF program
void run_elf(const u8* elf_buffer, u32 elf_size)
{
    print("Gotten elf size => %d\n", elf_size);
    elf_init(elf_buffer, elf_size);
}

static volatile struct page* elf_page;
static volatile u8* elf_ptr;
static volatile u32 elf_order;
static volatile u32 elf_size = 0;

/// Called when a new packet has been received. This function must process the
/// packet and send a response back when it is done. This response will trigger
/// the transfer of the next packet
static void loader_new_frame(volatile struct packet* p)
{
    if (p->cmd == 2) {
        elf_size = p->data[0] | (p->data[1] << 8) | 
                  (p->data[2] << 16) | (p->data[3] << 24);
        print("Allocating data => %d\n", elf_size);

        elf_order = pages_to_order((u32)align_up((void *)elf_size, 4096) / 4096);
        elf_page = alloc_pages(elf_order);

        elf_ptr = page_to_va((struct page *)elf_page);
    }
    if (p->cmd == 1) {
        
        for (u32 i = 0; i < p->size; i++) {
            *elf_ptr++ = p->data[i];
        }

        if (p->size != 512) {
            print("Done!\n");
            loader_send_resp(1);
            run_elf(page_to_va((struct page *)elf_page), elf_size);
        }
    }
    loader_send_resp(1);
}

/// This is called when the serial line has been inactive for more than 1 second
/// while in the middle of a frame. In this case the frame is discarded and the
/// state maching is reset to the default state
static void timeout_interrupt(void)
{
    irq_disable();
    timer_clear_flag();

    print("Loader timeout\n");
    loader_state = LOADER_IDLE;

}

/// Used for parsing the frame size and controlling packet data overflow
static volatile u32 index;

/// Main serial interrupt handler. This will handle all the commands from the
/// host including frame commands and reboot commands
static void loader_interrupt(void)
{
    // Receive data
    u8 data = UART4->RHR;
    
    if (loader_state == LOADER_IDLE) {
        // Exteral reboot
        if (data == 0x06) {
            RST->CR = 0xA5000000 | 1;
            while (1);
        }

        // Start of frame
        if (data == START_TAG) {
            loader_state = LOADER_CMD;
            timer_reset();
        }

        // Ready ACK
        if (data == 0x56) {
            loader_send_resp(0x56);
        }
        return;
    }

    // Reset the timeout counter
    timer_reset();

    // In the middle of a frame
    if (loader_state == LOADER_CMD) {

        packet.cmd = data;
        index = 0;
        packet.size = 0;
        loader_state = LOADER_SIZE;

    } else if (loader_state == LOADER_SIZE) {

        // The size field is received LSB first
        packet.size |= data << index;
        if (index == 8) {
            index = 0;
            if (packet.size == 0) {
                loader_state = LOADER_CRC;
            } else {
                loader_state = LOADER_DATA;
            }
        } else {
            index += 8;
        }

    } else if (loader_state == LOADER_DATA) {

        // Catch buffer overflow
        if (index >= MAX_DATA_SIZE) {
            panic("ELF loader fail");
        }

        packet.data[index++] = data;
        if (index >= packet.size) {
            loader_state = LOADER_CRC;
        }

    } else if (loader_state == LOADER_CRC) {

        packet.crc = data;
        loader_state = LOADER_END;

    } else if (loader_state == LOADER_END) {
        loader_state = LOADER_IDLE;
        if (data == END_TAG) {
            // Check if the frame we received is correct
            u8 crc = crc_calculate((u8 *)&packet.cmd, packet.size + 3, POLY);

            if (crc == packet.crc) {
                // We're done
                timer_stop();
                loader_new_frame(&packet);
            } else {
                // CRC does not match
                print("CRC error\n");
                loader_send_resp(0);
            }
        } else {
            // Frame format does not match
            print("END tag error\n");
            loader_send_resp(0);
        }
    }
}

/// Inializes the loader including timout timers and data structures
void loader_init(void)
{
    loader_state = LOADER_IDLE;

    // Initialize the timeout timer to generate a overflow after 1000 ms
    timer_init(648000, timeout_interrupt);

    // Add the loader interrupt handler
    apic_add_handler(28, loader_interrupt);
    apic_set_priority(28, 2);
    apic_enable(28);
}
