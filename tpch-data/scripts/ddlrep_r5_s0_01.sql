DROP TABLE IF EXISTS part CASCADE;
DROP TABLE IF EXISTS supplier CASCADE;
DROP TABLE IF EXISTS partsupp CASCADE;
DROP TABLE IF EXISTS customer CASCADE;
DROP TABLE IF EXISTS orders CASCADE;
DROP TABLE IF EXISTS nation CASCADE;
DROP TABLE IF EXISTS region CASCADE;

DROP INDEX IF EXISTS fkey_1 CASCADE;
DROP INDEX IF EXISTS fkey_2 CASCADE;
DROP INDEX IF EXISTS fkey_3 CASCADE;
DROP INDEX IF EXISTS fkey_4 CASCADE;
DROP INDEX IF EXISTS fkey_5 CASCADE;
DROP INDEX IF EXISTS fkey_6 CASCADE;

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
) WITH OIDS;

CREATE TABLE supplier (
	s_suppkey	INT8,
	s_name		VARCHAR(25),
	s_address	VARCHAR(40),
	s_nationkey	INT8,
	s_phone		VARCHAR(15),
	s_acctbal	DECIMAL,
	s_comment	VARCHAR(101),
	PRIMARY KEY (s_suppkey)
) WITH OIDS;

CREATE TABLE partsupp (
	ps_partkey	INT8,
	ps_suppkey	INT8,
	ps_availqty	INT,
	ps_supplycost	DECIMAL,
	ps_comment	VARCHAR(199),
	PRIMARY KEY (ps_partkey, ps_suppkey)
) WITH OIDS;

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
) WITH OIDS;

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
) WITH OIDS;

CREATE TABLE nation (
	n_nationkey	INT8,
	n_name		VARCHAR(25),
	n_regionkey	INT8,
	n_comment	VARCHAR(152),
	PRIMARY KEY (n_nationkey)
) WITH OIDS;

CREATE TABLE region (
	r_regionkey	INT8,
	r_name		VARCHAR(25),
	r_comment	VARCHAR(152),
	PRIMARY KEY (r_regionkey)
) WITH OIDS;

COPY part FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/part.cpy' WITH CSV DELIMITER '|';       
COPY supplier FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/supplier.cpy' WITH CSV DELIMITER '|';       
COPY partsupp FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/partsupp.cpy' WITH CSV DELIMITER '|';      
COPY customer FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/customer.cpy' WITH CSV DELIMITER '|';       
COPY orders FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/orders.cpy' WITH CSV DELIMITER '|';       
COPY nation FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/nation.cpy' WITH CSV DELIMITER '|';
COPY region FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/region.cpy' WITH CSV DELIMITER '|';

ALTER TABLE supplier ADD FOREIGN KEY (s_nationkey) REFERENCES nation (n_nationkey);

ALTER TABLE partsupp ADD FOREIGN KEY (ps_partkey) REFERENCES part (p_partkey);
ALTER TABLE partsupp ADD FOREIGN KEY (ps_suppkey) REFERENCES supplier (s_suppkey);

ALTER TABLE customer ADD FOREIGN KEY (c_nationkey) REFERENCES nation (n_nationkey);

ALTER TABLE orders ADD FOREIGN KEY (o_custkey) REFERENCES customer (c_custkey);

ALTER TABLE nation ADD FOREIGN KEY (n_regionkey) REFERENCES region (r_regionkey);

CREATE INDEX fkey_1 ON customer (c_nationkey);

CREATE INDEX fkey_2 ON nation (n_regionkey);

CREATE INDEX fkey_3 ON orders (o_custkey);

CREATE INDEX fkey_4 ON partsupp (ps_partkey);
CREATE INDEX fkey_5 ON partsupp (ps_suppkey);

CREATE INDEX fkey_6 ON supplier (s_nationkey);

DROP TABLE IF EXISTS lineitem1 CASCADE;

DROP INDEX IF EXISTS fkey_line1_1 CASCADE;
DROP INDEX IF EXISTS fkey_line2_1 CASCADE;
DROP INDEX IF EXISTS fkey_line3_1 CASCADE;
DROP INDEX IF EXISTS fkey_line4_1 CASCADE;

CREATE TABLE lineitem1 (
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
) WITH OIDS;

COPY lineitem1 FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitem1 ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem1 ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem1 ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem1 ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_1 ON lineitem1 (l_orderkey);
CREATE INDEX fkey_line2_1 ON lineitem1 (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_1 ON lineitem1 (l_partkey);
CREATE INDEX fkey_line4_1 ON lineitem1 (l_suppkey);
DROP TABLE IF EXISTS lineitem2 CASCADE;

DROP INDEX IF EXISTS fkey_line1_2 CASCADE;
DROP INDEX IF EXISTS fkey_line2_2 CASCADE;
DROP INDEX IF EXISTS fkey_line3_2 CASCADE;
DROP INDEX IF EXISTS fkey_line4_2 CASCADE;

CREATE TABLE lineitem2 (
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
) WITH OIDS;

COPY lineitem2 FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitem2 ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem2 ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem2 ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem2 ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_2 ON lineitem2 (l_orderkey);
CREATE INDEX fkey_line2_2 ON lineitem2 (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_2 ON lineitem2 (l_partkey);
CREATE INDEX fkey_line4_2 ON lineitem2 (l_suppkey);
DROP TABLE IF EXISTS lineitem3 CASCADE;

DROP INDEX IF EXISTS fkey_line1_3 CASCADE;
DROP INDEX IF EXISTS fkey_line2_3 CASCADE;
DROP INDEX IF EXISTS fkey_line3_3 CASCADE;
DROP INDEX IF EXISTS fkey_line4_3 CASCADE;

CREATE TABLE lineitem3 (
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
) WITH OIDS;

COPY lineitem3 FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitem3 ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem3 ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem3 ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem3 ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_3 ON lineitem3 (l_orderkey);
CREATE INDEX fkey_line2_3 ON lineitem3 (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_3 ON lineitem3 (l_partkey);
CREATE INDEX fkey_line4_3 ON lineitem3 (l_suppkey);
DROP TABLE IF EXISTS lineitem4 CASCADE;

DROP INDEX IF EXISTS fkey_line1_4 CASCADE;
DROP INDEX IF EXISTS fkey_line2_4 CASCADE;
DROP INDEX IF EXISTS fkey_line3_4 CASCADE;
DROP INDEX IF EXISTS fkey_line4_4 CASCADE;

CREATE TABLE lineitem4 (
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
) WITH OIDS;

COPY lineitem4 FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitem4 ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem4 ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem4 ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem4 ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_4 ON lineitem4 (l_orderkey);
CREATE INDEX fkey_line2_4 ON lineitem4 (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_4 ON lineitem4 (l_partkey);
CREATE INDEX fkey_line4_4 ON lineitem4 (l_suppkey);
DROP TABLE IF EXISTS lineitem5 CASCADE;

DROP INDEX IF EXISTS fkey_line1_5 CASCADE;
DROP INDEX IF EXISTS fkey_line2_5 CASCADE;
DROP INDEX IF EXISTS fkey_line3_5 CASCADE;
DROP INDEX IF EXISTS fkey_line4_5 CASCADE;

CREATE TABLE lineitem5 (
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
) WITH OIDS;

COPY lineitem5 FROM '/home/quanpt/assi/perm/tpch/scriptsAndData/data/tpch_0_01/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitem5 ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitem5 ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitem5 ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitem5 ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_5 ON lineitem5 (l_orderkey);
CREATE INDEX fkey_line2_5 ON lineitem5 (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_5 ON lineitem5 (l_partkey);
CREATE INDEX fkey_line4_5 ON lineitem5 (l_suppkey);
