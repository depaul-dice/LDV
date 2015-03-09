#!/usr/bin/env python

import random, json
from publication import Publication

def randList(aList, length = None):
	n = len(aList) - 1
	if length is None:
		length = n
	return [aList[i] for i in list(set([random.randint(0, n) for j in range(random.randint(1,length))]))]

def randAuthor(authors):
	return randList(authors)

def randKeywork(keywords):
	return randList(keywords, 3)

def main():
	random.seed(3)
	authors = ["Alice", "Bob", "Anna", "Billy"]
	keywords = ["file", "memory", "disk", "cpu", "os", "network", "cloud"]
	dates = [2001, 2001, 2001, 2002, 2002]
	pubs = [Publication(str(date), randAuthor(authors), "info " + str(random.randint(1,100)), randKeywork(keywords)) for date in dates]
	print(authors)
	for p in pubs:
		print p

	for date in set(dates):
		f = open("info_" + str(date) + ".txt", "w")
		for p in pubs:
			if p.date == str(date):
				f.write(json.dumps(p.jsonInfo()) + "\n")
		f.close()

		f = open("keyword_" + str(date) + ".txt", "w")
		for p in pubs:
			if p.date == str(date):
				f.write(json.dumps(p.jsonKeyword()) + "\n")
		f.close()

if __name__ == '__main__':
	main()