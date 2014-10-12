DROP TABLE IF EXISTS lineitemREPLICA CASCADE;

DROP INDEX IF EXISTS fkey_line1_REPLICA CASCADE;
DROP INDEX IF EXISTS fkey_line2_REPLICA CASCADE;
DROP INDEX IF EXISTS fkey_line3_REPLICA CASCADE;
DROP INDEX IF EXISTS fkey_line4_REPLICA CASCADE;

CREATE TABLE lineitemREPLICA (
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

COPY lineitemREPLICA FROM 'DATAPATH/tpch_DBSIZE/lineitem.cpy' WITH CSV DELIMITER '|';       

ALTER TABLE lineitemREPLICA ADD FOREIGN KEY (l_partkey, l_suppkey) REFERENCES partsupp (ps_partkey, ps_suppkey);
ALTER TABLE lineitemREPLICA ADD FOREIGN KEY (l_partkey) REFERENCES part (p_partkey);
ALTER TABLE lineitemREPLICA ADD FOREIGN KEY (l_suppkey) REFERENCES supplier (s_suppkey);
ALTER TABLE lineitemREPLICA ADD FOREIGN KEY (l_orderkey) REFERENCES orders (o_orderkey);

CREATE INDEX fkey_line1_REPLICA ON lineitemREPLICA (l_orderkey);
CREATE INDEX fkey_line2_REPLICA ON lineitemREPLICA (l_partkey, l_suppkey);
CREATE INDEX fkey_line3_REPLICA ON lineitemREPLICA (l_partkey);
CREATE INDEX fkey_line4_REPLICA ON lineitemREPLICA (l_suppkey);
