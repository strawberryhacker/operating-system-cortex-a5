# Copyright (C) strawberryhacker

# Simple loading bar
class loading_simple:

    def __init__(self):
        pass
    
    def set_total(self, total):
        self.total = total
        self.curr = 0

    def set_message(self, message):
        self.message = message

    def increment(self, inc):
        self.curr += inc
        if self.curr != self.total:
            percent = ("{:3d}").format(int(100 * (self.curr / float(self.total))))
            print(self.message + f": {percent}%", end = "\r")
        else:
            print(self.message + ": Complete")

# Bar style loading bar
class loading_bar:
    def __init__(self):
        pass
    
    def set_total(self, total):
        self.total = total
        self.curr = 0

    def increment(self, inc):
        print("bar")

