#!/usr/bin/env python

import json, subprocess, sys
from publication import Publication

def getString(anArray):
	res = ""
	for a in anArray:
		res += str(a) + " "
	return res[:-1]

def main():
	date = sys.argv[1]
	
	# get publication authors and info
	f = open("info_" + date + ".txt", "r")
	pubs = []
	for l in f.readlines():
		pub = json.loads(l)
		pubs.append(Publication(pub[Publication.DATE], pub[Publication.AUTHORS], pub[Publication.INFO], ["kw"], id=pub[0]))
	f.close()

	# get publication keywords
	f = open("keyword_" + date + ".txt", "r")
	for l in f.readlines():
		kw = json.loads(l)
		pub = next((x for x in pubs if x.id == kw[0] and x.date == kw[1]), None)
		if pub is None:
			print("ERROR: " + str(kw))
		else:
			pub.keywords = kw[2]
	f.close()

	# for p in pubs:
	# 	print p

	# create author map
	author2kw = {}
	for p in pubs:
		for a in p.authors:
			try:
				author2kw[a].update(p.keywords)
			except KeyError:
				author2kw[a] = set(p.keywords)

	for c in range(ord('A'),ord('Z')):
		c = chr(c)
		authors_c = [a for a in author2kw if a[0] == c]
		if len(authors_c) > 0:
			f = open("author_" + c + "_" + date + ".txt", "w")
			for a in authors_c:
				res = [a]
				res.append(list(author2kw[a]))
				print res
				f.write(json.dumps(res) + '\n')
				#f.write('"' + a + '", "' + getString(author2kw[a]) + '"\n')
			f.close()

if __name__ == '__main__':
	main()