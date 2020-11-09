# Copyright (C) strawberryhacker

import os
import sys
from citrus import citrus_packet
from citrus import citrus_file
from loading import loading_simple
from loading import loading_bar

# Used for loading a custom application to the CitrusOS and execute it

def main():
    if len(sys.argv) != 3:
        print("Check parameters")
        sys.exit()

    com_port = sys.argv[1]
    file_path = sys.argv[2]

    packet = citrus_packet(com_port)
    loading_bar_simple = loading_simple()
    loading_bar_simple.set_message("Downloading")
    file = citrus_file(packet, loading_bar_simple)

    # We just send the file over
    file.send_file(file_path)

main()
