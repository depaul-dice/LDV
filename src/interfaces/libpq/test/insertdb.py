#!/usr/bin/python

import leveldb, sys, string, fileinput
printset = set(string.printable)
path = sys.argv[1] if len(sys.argv) > 1 else 'cde-package/provenance.cde-root.1.log_db'
totalrange = 20

db = leveldb.LevelDB(path, create_if_missing = False)

# 'prv_store_insert\t12784\t8011914157105071939\t1\t1400440844729033\n'
# 'action \t pid \t insertid \t version \t usec'
(ACTION, PIDKEY, INSERTID, VERSION, USEC, SQL) = range(0, 6)
for line in fileinput.input():
    tokens = line[:-1].split('\t')
    if tokens[ACTION] == 'prv_store_insert':
        try:
            pidkey = db.Get('pid.'+tokens[PIDKEY])
            db.Put('prv.db.'+pidkey+'.usec.'+tokens[USEC], tokens[INSERTID]+'.'+tokens[VERSION])
            db.Put('prv.db.'+pidkey+'.insertid.'+ tokens[INSERTID]+'.'+tokens[VERSION]+'.'+tokens[USEC], tokens[SQL])
#            db.Put('prv.db.'+pidkey+'.insertid.usec'+ tokens[INSERTID]+'.'+tokens[VERSION], tokens[USEC])
        except KeyError:
            print "pidkey for " + tokens[PIDKEY] + " not found!"
        
    
    
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
