# Copyright (C) strawberryhacker

import os
import sys
import serial
import time

from citrus import citrus_packet
from citrus import citrus_file
from loading import loading_simple
from loading import loading_bar

# Used for loading a custom application to the CitrusOS and execute it

def main():
    start = int(round(time.time() * 1000))
    if len(sys.argv) != 3:
        print("Check parameters")
        sys.exit()

    com_port = sys.argv[1]
    file_path = sys.argv[2]

    # Just open a new COM port
    try:
        s = serial.Serial(port=com_port, \
            baudrate=921600, timeout=1)

    except serial.SerialException as e:
        print("Cannot open COM port - ", e)
        sys.exit()

    packet = citrus_packet(s)
    loading_bar_simple = loading_simple()
    loading_bar_simple.set_message("Downloading")
    file = citrus_file(packet, loading_bar_simple)
    
    print("Entering citrus-boot...")
    packet.send_packet(b'', packet.CMD_RESET)

    # We just send the file over
    file.send_file(file_path)
    stop = int(round(time.time() * 1000))
    print("Kernel download complete in", stop - start, "ms")
    print("Please wait...", end="\r")
    s.close()
    print(' ' * 50, end="\r")

main()
