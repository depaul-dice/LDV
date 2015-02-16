#!/usr/bin/python

import leveldb, sys, string, fileinput
printset = set(string.printable)
path = sys.argv[1] if len(sys.argv) > 1 else 'cde-package/provenance.cde-root.1.log_db'
totalrange = 20

db = leveldb.LevelDB(path, create_if_missing = False)

ACTION = 0
for line in fileinput.input():
    tokens = line[:-1].split('\t')
    if tokens[ACTION] == 'prv_store_insert':
        # 'prv_store_insert	26516	543777651662415126	1	1400549462612869	INSERT INTO tbl1 values(79, 1)'
        # 'action \t pid \t queryid \t version \t usec \t sql'
        (_, PIDKEY, QUERYID, VERSION, USEC, SQL) = range(0, 6)
        try:
            pidkey = db.Get('pid.'+tokens[PIDKEY])
            db.Put('prv.db.'+pidkey+'.insertid.sql.'+ tokens[QUERYID]+'.'+tokens[VERSION]+'.'+tokens[USEC], tokens[SQL])
            #~ db.Put('prv.db.'+pidkey+'.insertid.id.'+tokens[USEC], tokens[QUERYID]+'.'+tokens[VERSION])
        except KeyError:
            print "pidkey for " + tokens[PIDKEY] + " not found!"
    if tokens[ACTION] == 'prv_store_select':
        # 'prv_store_select	26538	4897368946983237418	543777651662415126.1.489876779991029268.1.7874536620689878546.1.5489998234078931821.1.3557775467278126851.1	1400549467338983	SELECT sum(value) FROM tbl1 WHERE value < 50\n'
        # 'action \t pid \t queryid \t *(insertid.ver) \t usec \t sql'
        (_, PIDKEY, QUERYID, INSERTIDS, USEC, SQL) = range(0, 6)
        try:
            pidkey = db.Get('pid.'+tokens[PIDKEY])
            db.Put('prv.db.'+pidkey+'.selectid.sql.'+ tokens[QUERYID]+'.'+tokens[USEC], tokens[SQL])
            ids = tokens[INSERTIDS].split('.');
            for i in range(len(ids)):
                db.Put('prv.db.'+pidkey+'.selectid.rowid.'+ tokens[QUERYID]+'.'+tokens[USEC]+'.'+str(i), ids[i])
            # if len(ids) > 0:
            #     for i in range(len(ids)/2):
            #         (insertid, version) = ids[2*i:2*i+2]
            #         db.Put('prv.db.'+pidkey+'.selectid.insertid.'+ tokens[QUERYID]+'.'+tokens[USEC]+'.'+str(i), \
            #             insertid + '.' + version)
        except KeyError:
            print "pidkey for " + tokens[PIDKEY] + " not found!"
    if tokens[ACTION] == 'prv_store_tuple':
        # 'prv_store_tuple  14983   2221643613338354282 16428   ('49', '24', '2221643613338354282', '1001', '2015-02-16 20:22:35.6869', '16428')'
        (_, PIDKEY, QUERYID, ROWID, TUPLE) = range(0, 5)
        try:
            pidkey = db.Get('pid.'+tokens[PIDKEY])
            db.Put('prv.db.'+pidkey+'.insertid.rowid.'+tokens[QUERYID]+'.'+tokens[ROWID], tokens[TUPLE])
        except KeyError:
            print "pidkey for " + tokens[PIDKEY] + " not found!"
        pass
    
    
#~ for (k,v) in db.RangeIter():
  #~ if set(v).issubset(printset):
    #~ print "[%s] '%s' -> [%s] '%s'" % (len(k), k, len(v), v)
  #~ else:
    #~ hexv = v.encode('hex')
    #~ strv = ''.join([x if (ord(x) > 31 and ord(x)<128) else ('\\'+str(ord(x))) for x in v])
    #~ if len(hexv) <= totalrange:
      #~ print "[%s] '%s' -> [%s] '0x%s' %s" % (len(k), k, len(v), hexv, strv)
    #~ else:
      #~ hexv = hexv[:totalrange/2] + '...' + hexv[-totalrange/2:]
      #~ print "[%s] '%s' -> [%s] '0x%s' %s" % (len(k), k, len(v), hexv, strv)
