CREATE TABLE part (
	p_partkey 	BIGINT NOT NULL,
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
	s_suppkey	BIGINT NOT NULL,
	s_name		VARCHAR(25),
	s_address	VARCHAR(40),
	s_nationkey	BIGINT,
	s_phone		VARCHAR(15),
	s_acctbal	DECIMAL,
	s_comment	VARCHAR(101),
	PRIMARY KEY (s_suppkey)
);

CREATE TABLE partsupp (
	ps_partkey	BIGINT NOT NULL,
	ps_suppkey	BIGINT  NOT NULL,
	ps_availqty	INT,
	ps_supplycost	DECIMAL,
	ps_comment	VARCHAR(199),
	PRIMARY KEY (ps_partkey, ps_suppkey)
);

CREATE TABLE customer (
	c_custkey	BIGINT NOT NULL,
	c_name		VARCHAR(25),
	c_address	VARCHAR(40),
	c_nationkey	BIGINT,
	c_phone		VARCHAR(15),
	c_acctbal	DECIMAL,
	c_mktsegment	VARCHAR(10),
	c_comment	VARCHAR(117),
	PRIMARY KEY (c_custkey)
);

CREATE TABLE orders (
	o_orderkey	BIGINT NOT NULL,
	o_custkey	BIGINT,
	o_orderstatus	VARCHAR(1),
	o_totalprice	DECIMAL(10,2),
	o_orderdate	DATE,
	o_orderpriority	VARCHAR(15),
	o_clerk		VARCHAR(15),
	o_shippriority	INT,
	o_comment	VARCHAR(79),
	PRIMARY KEY (o_orderkey)
);

CREATE TABLE lineitem (
	l_orderkey	BIGINT NOT NULL,
	l_partkey	BIGINT,
	l_suppkey	BIGINT,
	l_linenumber	BIGINT NOT NULL,
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
	n_nationkey	BIGINT NOT NULL,
	n_name		VARCHAR(25),
	n_regionkey	BIGINT,
	n_comment	VARCHAR(152),
	PRIMARY KEY (n_nationkey)
);

CREATE TABLE region (
	r_regionkey	BIGINT NOT NULL,
	r_name		VARCHAR(25),
	r_comment	VARCHAR(152),
	PRIMARY KEY (r_regionkey)
);

IMPORT FROM '/home/boris/tpchdata/tpch_1/part.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT  INTO part;       
IMPORT FROM '/home/boris/tpchdata/tpch_1/supplier.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO supplier;      
IMPORT FROM '/home/boris/tpchdata/tpch_1/partsupp.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO partsupp;       
IMPORT FROM '/home/boris/tpchdata/tpch_1/customer.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO customer;      
IMPORT FROM '/home/boris/tpchdata/tpch_1/orders.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO orders;        
IMPORT FROM '/home/boris/tpchdata/tpch_1/lineitem.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO lineitem;        
IMPORT FROM '/home/boris/tpchdata/tpch_1/nation.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO nation; 
IMPORT FROM '/home/boris/tpchdata/tpch_1/region.cpy' OF DEL modified by coldel| COMMITCOUNT 1000 INSERT INTO region; 

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

