#!/usr/bin/env python

import json, subprocess, time

def main():
	dates = [2001, 2002]
	subprocess.call("rm -f author_*.txt", shell=True)
	for date in dates:
		subprocess.call(["python", "./processYear.py", str(date)])
	for c in range(ord('A'),ord('C')):
		c = chr(c)
		subprocess.call(["python", "./processAuthor.py", c])
		#time.sleep(1)

if __name__ == '__main__':
	main()