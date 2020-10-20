# Copyright (C) strawberryhacker

import serial
import argparse
import math
import sys
import time

# Disclaimer: this is not an example of good python code. This is not the
# intention either. I dont really want to touch it 

class flasher:
    START_BYTE = 0xAA
    END_BYTE = 0x55
    POLYNOMIAL = 0xB2
    CMD_WRITE_PAGE = 0x04
    CMD_RESET = 0x06
    BAR_LENGTH = 40
    total = 1
    progress = 0

    def __init__(self):
        pass
    
    def parser(self):
        parser = argparse.ArgumentParser(description="Chip flasher")
        parser.add_argument("-c", "--com_port",
                            help="Specify the com port COMx or /dev/ttySx")
        parser.add_argument("-f", "--file",
                            required=True,
                            help="Specifies the binary")
        args = parser.parse_args()
        self.file = args.file
        self.com_port = args.com_port

    def update_progress(self):
        percent = ("{:3d}").format(int(100 * (self.progress / float(self.total))))
        progress = int((self.progress * self.BAR_LENGTH) // self.total)
        bar = '#' * progress + '.' * (self.BAR_LENGTH - progress)
        p = f'\033[42m\033[30m\rProgress: [{percent}%]\033[0m [{bar}]'
        print(p, end ="\r")
        if (self.total == self.progress):
            e = ' '.ljust(len(p) - 15)
            print(e, end='\r')

    def serial_open(self):
        try:
            self.com = serial.Serial(port=self.com_port, baudrate=230400, timeout=1)
        except serial.SerialException as e:
            print(e)
            exit()

    def file_read(self):
        f = open(self.file, "rb")
        return f.read()

    def calculate_fcs(self, data):
        crc = 0
        for i in range(len(data)):
            crc = crc ^ data[i]
            for j in range(8):
                if crc & 0x01:
                    crc = crc ^ self.POLYNOMIAL
                crc = crc >> 1
        return crc

    def resp_ok(self):
        resp = self.com.read(size = 1)
        if (len(resp) == 0):
            print("Timeout")
            sys.exit()
        if resp == b'\x06':
            return 1
        return 0

    def send_frame(self, cmd, payload):
        payload_size = len(payload)
        start_byte = bytearray([self.START_BYTE])
        cmd_byte = bytearray([cmd])
        size = bytearray([payload_size & 0xFF, (payload_size >> 8) & 0xFF])
        data = bytearray(payload)
        fcs = bytearray([self.calculate_fcs(cmd_byte + size + payload)])
        end_byte = bytearray([self.END_BYTE])
        timeout = 0
        while True:
            self.com.write(start_byte + cmd_byte + size + data + fcs + end_byte)
            if self.resp_ok():
                return
            if timeout > 10:
                print("Error")
                sys.exit()
            timeout += 1 

    def reset_bootloader(self):
        self.send_frame(self.CMD_RESET, bytearray([]))

    def go_to_bootloader(self):
        self.com.write(b'\x06')
        self.resp_ok()

    def load_kernel(self):
        self.serial_open()
        kernel_binary = self.file_read()
        print("Connecting to c-boot...")
        self.com.flush()
        self.go_to_bootloader()
        self.reset_bootloader()
        length = len(kernel_binary)
        number_of_block = math.ceil(length / 512)
        self.total = number_of_block
        if (length % 512) == 0:
            self.total += 1
        self.progress = 0
        self.update_progress()
        for i in range(number_of_block):
            binary_fragment = kernel_binary[i * 512 : (i + 1) * 512]
            self.send_frame(self.CMD_WRITE_PAGE, binary_fragment)
            self.progress += 1
            self.update_progress()
            if i == (number_of_block - 1):
                if len(binary_fragment) == 512:
                    status = self.send_frame(self.CMD_WRITE_PAGE, bytearray([]))
                    self.check_resp(status)
                    self.progress += 1
                    self.update_progress()
        print("Kernel download complete!")

test = flasher()
test.parser()
test.load_kernel()
