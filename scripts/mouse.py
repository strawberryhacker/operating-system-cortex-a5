import pyautogui
import time
import os
import sys
import serial
from citrus import citrus_packet
from citrus import citrus_file
from loading import loading_simple
from loading import loading_bar

# Used for loading a custom application to the CitrusOS and execute it


#while True:
#    var = pyautogui.position()
#    print(var[0], end=" ")
#    print(var[1])
#    time.sleep(0.01)

def main():
    
    # Just open a new COM port
    try:
        s = serial.Serial("COM4", \
            baudrate=921600, timeout=1)

    except serial.SerialException as e:
        print("Cannot open COM port - ", e)
        sys.exit()

    packet = citrus_packet(s)
    while True:
        var = pyautogui.position()
        packet.send_packet(bytearray([var[0] & 0xFF, (var[0] >> 8) & 0xFF, var[1] & 0xFF, (var[1] >> 8) & 0xFF]), 11)
        time.sleep(0.01)

main()