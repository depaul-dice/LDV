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

COPY part FROM 'DATAPATH/tpch_DBSIZE/part.cpy' WITH CSV DELIMITER '|';       
COPY supplier FROM 'DATAPATH/tpch_DBSIZE/supplier.cpy' WITH CSV DELIMITER '|';       
COPY partsupp FROM 'DATAPATH/tpch_DBSIZE/partsupp.cpy' WITH CSV DELIMITER '|';      
COPY customer FROM 'DATAPATH/tpch_DBSIZE/customer.cpy' WITH CSV DELIMITER '|';       
COPY orders FROM 'DATAPATH/tpch_DBSIZE/orders.cpy' WITH CSV DELIMITER '|';       
COPY nation FROM 'DATAPATH/tpch_DBSIZE/nation.cpy' WITH CSV DELIMITER '|';
COPY region FROM 'DATAPATH/tpch_DBSIZE/region.cpy' WITH CSV DELIMITER '|';

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

