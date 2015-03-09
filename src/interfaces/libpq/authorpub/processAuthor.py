#!/usr/bin/env python

import sys, json
import psycopg2
import sys
 
def write2db(author2kw):
	#Define our connection string
	conn_string = "host='localhost' dbname='single' user='quanpt' password='secret'"
 
	# get a connection, if a connect cannot be made an exception will be raised here
	conn = psycopg2.connect(conn_string)
	conn.autocommit = True
 
	# conn.cursor will return a cursor object, you can use this cursor to perform queries
	cursor = conn.cursor()
	
	for author in author2kw:
		sql = "INSERT INTO authors VALUES (%s, %s);"
		#'" + author + "', '" + str(list(author2kw[author]))[1:-1].replace("'","\"") + "');"
		cursor.execute(sql, (author, str(list(author2kw[author]))[1:-1]))

	conn.commit()
	cursor.close()
	conn.close()

def readAuthors(c):
	author2kw = {}
	for date in range(2001, 2003):
		try:
			f = open("author_" + c + "_" + str(date) + ".txt", "r")
			for line in f.readlines():
				data = json.loads(line)
				try:
					author2kw[data[0]].update(data[1])
				except KeyError:
					author2kw[data[0]] = set(data[1])
			f.close()
		except IOError:
			pass
	return author2kw

def main():
	c = sys.argv[1]
	
	author2kw = readAuthors(c)

	try:
		write2db(author2kw)
	except psycopg2.OperationalError:
		print("Error working with PostgreSQL")

	print("Done with author names start with " + c)

if __name__ == '__main__':
	main()