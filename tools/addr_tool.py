# Copyright (C) strawberryhacker

while True:
	start_v = int(input("Start addr: "), 16)
	stop_v  = int(input("Stop addr: "), 16)
	diff = ((stop_i + 4) - start_i) / 4
	if (res.is_integer() == False):
		print("Input error")
	elif (res < 0):
		print("Negative number")
	else:
		print("Padding words: ", int(res))
