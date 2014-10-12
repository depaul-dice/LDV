CREATE TABLE part (
	p_partkey 	INT8,
	p_name		VARCHAR(55),
	p_mfgr		VARCHAR(25),
	p_brand		VARCHAR(10),
	p_type		VARCHAR(25),
	p_size		INT,
	p_container	VARCHAR(10),
	p_retailprice	DECIMAL,
	p_comment	VARCHAR(23),
	PRIMARY KEY (p_partkey)
);

CREATE TABLE supplier (
	s_suppkey	INT8,
	s_name		VARCHAR(25),
	s_address	VARCHAR(40),
	s_nationkey	INT8,
	s_phone		VARCHAR(15),
	s_acctbal	DECIMAL,
	s_comment	VARCHAR(101),
	PRIMARY KEY (s_suppkey)
);

CREATE TABLE partsupp (
	ps_partkey	INT8,
	ps_suppkey	INT8,
	ps_availqty	INT,
	ps_supplycost	DECIMAL,
	ps_comment	VARCHAR(199),
	PRIMARY KEY (ps_partkey, ps_suppkey)
);

CREATE TABLE customer (
	c_custkey	INT8,
	c_name		VARCHAR(25),
	c_address	VARCHAR(40),
	c_nationkey	INT8,
	c_phone		VARCHAR(15),
	c_acctbal	DECIMAL,
	c_mktsegment	VARCHAR(10),
	c_comment	VARCHAR(117),
	PRIMARY KEY (c_custkey)
);

CREATE TABLE orders (
	o_orderkey	INT8,
	o_custkey	INT8,
	o_orderstatus	VARCHAR(1),
	o_totalprice	DECIMAL,
	o_orderdate	DATE,
	o_orderpriority	VARCHAR(15),
	o_clerk		VARCHAR(15),
	o_shippriority	INT,
	o_comment	VARCHAR(79),
	PRIMARY KEY (o_orderkey)
);

CREATE TABLE lineitem (
	l_orderkey	INT8,
	l_partkey	INT8,
	l_suppkey	INT8,
	l_linenumber	INT8,
	l_quantity	DECIMAL,
	l_extendedprice	DECIMAL,
	l_discount	DECIMAL,
	l_tax		DECIMAL,
	l_returnflag	VARCHAR(1),
	l_linestatus	VARCHAR(1),
	l_shipdate	DATE,
	l_commitdate	DATE,
	l_receiptdate	DATE,
	l_shipinstruct	VARCHAR(25),
	l_shipmode	VARCHAR(10),
	l_comment	VARCHAR(44),
	PRIMARY KEY (l_orderkey, l_linenumber)
);

CREATE TABLE nation (
	n_nationkey	INT8,
	n_name		VARCHAR(25),
	n_regionkey	INT8,
	n_comment	VARCHAR(152),
	PRIMARY KEY (n_nationkey)
);

CREATE TABLE region (
	r_regionkey	INT8,
	r_name		VARCHAR(25),
	r_comment	VARCHAR(152),
	PRIMARY KEY (r_regionkey)
);

COPY part FROM '/Users/Shared/tpch_1/part.cpy' WITH CSV DELIMITER '|';       
COPY supplier FROM '/Users/Shared/tpch_1/supplier.cpy' WITH CSV DELIMITER '|';       
COPY partsupp FROM '/Users/Shared/tpch_1/partsupp.cpy' WITH CSV DELIMITER '|';      
COPY customer FROM '/Users/Shared/tpch_1/customer.cpy' WITH CSV DELIMITER '|';       
COPY orders FROM '/Users/Shared/tpch_1/orders.cpy' WITH CSV DELIMITER '|';       
COPY lineitem FROM '/Users/Shared/tpch_1/lineitem.cpy' WITH CSV DELIMITER '|';       
COPY nation FROM '/Users/Shared/tpch_1/nation.cpy' WITH CSV DELIMITER '|';
COPY region FROM '/Users/Shared/tpch_1/region.cpy' WITH CSV DELIMITER '|';

ALTER TABLE supplier ADD FOREIGN KEY (s_nationkey) REFERENCES nation (n_nationkey);

ALTER TABLE partsupp ADD FOREIGN KEY (ps_partkey) REFERENCES part (p_partkey);
ALTER TABLE partsupp ADD FOREIGN KEY (ps_suppkey) REFERENCES supplier (s_suppkey);

ALTER TABLE customer ADD FOREIGN KEY (c_nationkey) REFERENCES nation (n_nationkey);

ALTER TABLE orders ADD FOREIGN KEY (o_custkey) REFERENCES customer (c_custkey);

ALTER TABLE lineitem ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

ALTER TABLE nation ADD FOREIGN KEY (n_regionkey) REFERENCES region (r_regionkey);

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

