#!/usr/bin/python

# read from stdin and dump info into a leveldb
# Usage with modified libpq (see run.sh)
#    cat *.dblog > dblog.txt
#    cat dblog.txt | python insertdb.py
# Input format:
#
#    prv_store_insert \t pid \t queryid \t version \t usec \t sql
#       prv_store_insert   20094   5403223075817924163 1   1424089366679806    INSERT INTO tbl1 values(49, 24)
#
#    prv_store_select \t pid \t queryid \t *(rowid) \t usec \t sql
#       prv_store_select   20115   4649265341407373570 16411.16429.16430   1424089371080113    SELECT sum(value) FROM tbl1 WHERE value > 4 AND value < 40
#
#    prv_store_tuple \t pid \t insertid \t rowid \t tuple
#       prv_store_tuple    20094   5403223075817924163 16429   ('49', '24')
#
#    update is not implemented yet (along with version)
#       prv_store_update   20094   4990762698207462051 1   1424089367432523    UPDATE tbl1 SET value=60 WHERE value = 101;
#
# Output format ((key, value) pairs):
#    prv.db.#pidkey.insertid.sql.#INSERTID.#VERSION.#USEC, #SQL  -->  can get #SQL from #INSERTID
#    prv.db.#pidkey.selectid.sql.#SELECTID.#USEC], #SQL  --> can get #SQL from #SELECTID
#    prv.db.#pidkey.selectid.rowid.#SELECTID.#USEC.#counter, #rowid (for each #rowid in *(rowid))  -->  can get #SELECTID--used-->#ROWID relationship
#    prv.db.#pidkey.insertid.rowid.#INSERTID.#ROWID, #TUPLE  -->  can get #ROWID--wasGeneratedBy-->#INSERTID relationship


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
        except KeyError:
            print "pidkey for " + tokens[PIDKEY] + " not found!"
    if tokens[ACTION] == 'prv_store_select':
        # 'prv_store_select 20115   4649265341407373570 16411.16429.16430   1424089371080113    SELECT sum(value) FROM tbl1 WHERE value > 4 AND value < 40'
        # 'action \t pid \t queryid \t *(rowid) \t usec \t sql'
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
        # 'action \t pid \t insertid \t rowid \t tuple'
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
