# Copyright (C) strawberryhacker

import os
import sys
import serial
import math

# Interface for sending a packet to the operating system and delivaring a 
# response back

class citrus_packet:
    
    # Packet interface
    HEADER_ID   = 0xCA
    HEADER_SIZE = 8
    CRC_POLY    = 0x45
    CHUNK_SIZE  = 4096

    # Commands
    CMD_DATA  = 0x00
    CMD_SIZE  = 0x01
    CMD_RESET = 0x02
    CMD_KILL  = 0x03
    CMD_MOUSE = 0x11

    # Error response indicating transmission retry
    RESP_ERROR = b'\x00'

    def __init__(self, com_port, retry = 10):
        self.serial = com_port

        # Save the retry count
        self.retry = retry

    def get_crc(self, data):
        crc = 0
        for i in range(len(data)):
            crc = crc ^ data[i]
            for j in range(8):
                if crc & 0x01:
                    crc = crc ^ self.CRC_POLY
                crc = crc >> 1
        return crc

    def send_packet(self, data, cmd):
        crc = self.get_crc(bytearray(data))

        # We want the size to be LE
        data_size = len(data).to_bytes(4, byteorder = "little")
        header_size = self.HEADER_SIZE.to_bytes(1, byteorder = "little")
        header = bytearray([self.HEADER_ID, crc, cmd]) + \
                 bytearray(header_size) +     \
                 bytearray(data_size)

        payload = bytearray(data)

        timeout = self.retry
        while timeout != 0:
            self.serial.write(header + payload)

            # Wait for the response
            resp = self.serial.read(size = 1)
            if len(resp) == 0:
                return None
            if resp != self.RESP_ERROR:
                return resp
            timeout -= 1
        
        return None

# Class for sending files to the citrus operating system
class citrus_file:
    def __init__(self, packet_object, loading_object):
        self.packet = packet_object
        self.loading = loading_object

    def send_file(self, path):
        size = os.path.getsize(path)
        size_bytes = size.to_bytes(4, byteorder = "little")

        # Set the loading bar size
        self.loading.set_total(size)

        # Issue an allocate memory command
        if self.packet.send_packet(size_bytes, self.packet.CMD_SIZE) == None:
            sys.exit()

        f = open(path, 'rb')
        while True:
            chunk = f.read(self.packet.CHUNK_SIZE)

            # Send the data
            if self.packet.send_packet(chunk, self.packet.CMD_DATA) == None:
                f.close()
                sys.exit()
            
            self.loading.increment(len(chunk))
            if len(chunk) < self.packet.CHUNK_SIZE:
                break
        
        f.close()

