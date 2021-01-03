#ifndef TFTP_H
#define TFTP_H

#include <citrus/types.h>

struct tftp {
    u16 client_port;
    u16 server_port;
    u32 server_ip;


    u32 packet_number;
};

i32 tftp_create(struct tftp* t, const char* ip_addr);

i32 tftp_download(struct tftp* t, const char* filename, u8** file, u32* size);

#endif
