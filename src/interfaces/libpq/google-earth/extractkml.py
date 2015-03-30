#!/usr/bin/env python

import sys, json
import psycopg2
import psycopg2.extras
import sys

KML_START = """<?xml version="1.0" encoding="UTF-8"?><kml xmlns="http://www.opengis.net/kml/2.2" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="http://www.opengis.net/kml/2.2 http://schemas.opengis.net/kml/2.2.0/ogckml22.xsd">
   <Document xmlns:atom="http://purl.org/atom/ns#">
      <name>geo_bddq-yxar:geo_bddq-yxar-1</name>
      <LookAt>
         <longitude>-87.67361275067813</longitude>
         <latitude>41.8427972613491</latitude>
         <altitude>56756.72380582584</altitude>
         <range>45862.40982007242</range>
         <tilt>0.0</tilt>
         <heading>0.0</heading>
         <altitudeMode>clampToGround</altitudeMode>
      </LookAt>
"""

KML_END = """   </Document>
</kml>
"""

KML_PLACEMARK = """      <Style id="placemark_{shortname}">
            <IconStyle>
            <Icon>
               <href>{sym}</href>
            </Icon>
            </IconStyle>
      </Style>
      <Placemark id="{shortname}">
         <name><![CDATA[{name}]]></name>
         <description><![CDATA[{description}]]></description>
         <styleUrl>#placemark_{shortname}</styleUrl>
         <Point>
            <coordinates>{longitude},{latitude}</coordinates>
         </Point>
      </Placemark>
"""


def main():
    #Define our connection string
    conn_string = "host=localhost dbname=gitdb user=quanpt password=secret port=54320"
 
    # get a connection, if a connect cannot be made an exception will be raised here
    conn = psycopg2.connect(conn_string)
    #conn.autocommit = True
 
    # conn.cursor will return a cursor object, you can use this cursor to perform queries
    cursor = conn.cursor()
    
    fields = "shortname, name, description, color, scale, sym, latitude, longitude".split(", ")
    cursor.execute("SELECT " + str(fields)[1:-2].replace("'","") + " FROM description d, landmarks l WHERE d.desc_id = l.land_id AND (name LIKE '%Theater%' or name LIKE '%Theatre%');")

    f = open("output.kml", 'w')
    f.write(KML_START)
    for row in cursor:
        rowobj = dict()
        for i in range(len(fields)):
            rowobj[fields[i]] = row[i]
        f.write(KML_PLACEMARK.format(**rowobj))
    f.write(KML_END)
    f.close()

    conn.commit()
    conn.close()

if __name__ == '__main__':
    main()