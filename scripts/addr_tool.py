# Copyright (C) strawberryhacker

# Used for calculating registers offsets in IO space

while True:
	start_v = int(input("Start addr: "), 16)
	stop_i  = int(input("Stop addr: "), 16)
	diff = ((stop_i + 4) - start_v) / 4
	if (diff.is_integer() == False):
		print("Input error")
	elif (diff < 0):
		print("Negative number")
	else:
		print("Padding words: ", int(diff))
