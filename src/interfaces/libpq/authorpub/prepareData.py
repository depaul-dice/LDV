#!/usr/bin/env python

import random, json

class Publication:
	counter = 1
	def __init__(self, date, authors, info, keyword):
		self.id = "id_" + str(Publication.counter)
		Publication.counter += 1
		self.date = date
		self.authors = authors
		self.info = info
		self.keyword = keyword

	def __repr__(self):
		return str([self.id, self.date, self.authors, self.info])

	def jsonInfo(self):
		return [self.id, self.date, self.authors, self.info]

	def jsonKeyword(self):
		return [self.id, self.date, self.keyword]

def randList(aList):
	n = len(aList) - 1
	return [aList[i] for i in list(set([random.randint(0, n) for j in range(random.randint(1,n))]))]

def randAuthor(authors):
	return randList(authors)

def randKeywork(keywords):
	return randList(keywords)

def main():
	random.seed(1)
	authors = ["Alice", "Bob", "Charlie", "Dave"]
	keywords = ["file", "memory", "disk", "cpu", "os", "network", "cloud"]
	dates = [2001, 2001, 2001, 2002, 2002, 2003, 2003, 2003]
	pubs = [Publication(str(date), randAuthor(authors), "info " + str(random.randint(1,100)), randKeywork(keywords)) for date in dates]
	# print(authors)
	# print(pubs)

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