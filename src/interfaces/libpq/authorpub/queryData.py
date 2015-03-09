#!/usr/bin/env python

import sys, json
import psycopg2
import sys

def main():
	#Define our connection string
	conn_string = "host='localhost' dbname='single' user='quanpt' password='secret'"
 
	# get a connection, if a connect cannot be made an exception will be raised here
	conn = psycopg2.connect(conn_string)
	conn.autocommit = True
 
	# conn.cursor will return a cursor object, you can use this cursor to perform queries
	cursor = conn.cursor()
	
	cursor.execute("SELECT Name, Keywords FROM Authors WHERE Keywords LIKE '%cloud%';")
	print("Query for authors with keyword 'cloud': ", cursor.fetchall())

	conn.commit()
	cursor.close()
	conn.close()

if __name__ == '__main__':
	main()