CREATE INDEX fkey_1 ON customer (c_nationkey);

CREATE INDEX fkey_2 ON lineitem (l_orderkey);
CREATE INDEX fkey_3 ON lineitem (l_partkey, l_suppkey);
CREATE INDEX fkey_4 ON lineitem (l_partkey);
CREATE INDEX fkey_5 ON lineitem (l_suppkey);

CREATE INDEX fkey_6 ON nation (n_regionkey);

CREATE INDEX fkey_7 ON orders (o_custkey);

CREATE INDEX fkey_8 ON partsupp (ps_partkey);
CREATE INDEX fkey_9 ON partsupp (ps_suppkey);

CREATE INDEX fkey_10 ON supplier (s_nationkey);
