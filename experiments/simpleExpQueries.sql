------------------------------------------------------------
-- QUERIES
------------------------------------------------------------

-- SIMPLE SELECTION QUERY
-- l_suppkey is has SF * 10,000 values = 10000 Values for 1GB instance
-- lineitems are more or less evenly distributed among suppliers
-- Thus we can predict the selectivity by changing the range
-- use 1% = 1 AND 10, 2% = 1 AND 20, 5%, 10%, 25%
SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate 
FROM lineitem 
WHERE l_suppkey BETWEEN 1 AND 100;

-- SIMPLE SJ query
-- add a join into the mix
-- slightly more complex query
-- we want order comments and lineitem comments for orders from certain customers to be able to apply some mining techniques to that
-- we can effect the selectivity by changing the number of leading 0's
SELECT o_comment, l_comment
FROM lineitem l, orders o, customer c
WHERE l.l_orderkey = o.o_orderkey 
      AND o.o_custkey = c.c_custkey
  AND c.c_name LIKE '%000000%';

-- to check selectivity
SELECT count(*) 
FROM lineitem l, orders o, customer c
WHERE l.l_orderkey = o.o_orderkey 
      AND o.o_custkey = c.c_custkey
  AND c.c_name LIKE '%000000%';


-- Aggregation over selection
-- find avg quantity and total cost per order for orders which use a certain supplier
-- Thus we can predict the selectivity by changing the range
-- use 1% = 1 AND 10, 2% = 1 AND 20, 5%, 10%, 25%
SELECT o_orderkey, AVG(l_quantity) AS avgQ
FROM lineitem l, orders o
WHERE l.l_orderkey = o.o_orderkey
	  AND s_suppkey BETWEEN 1 AND 100
GROUP BY o_orderkey

------------------------------------------------------------
-- INSTRUCTIONS AND HELP BELOW
------------------------------------------------------------

-- for the provenance queries it would make sense to only get a tuple id (which I think you do anyways Quan, right)
-- make sure that you do this like that:
SELECT PROVENANCE o_orderkey, AVG(l_quantity) AS avgQ
FROM lineitem PROVENANCE (l_linenumber) l, orders PROVENANCE(o_orderkey) o
WHERE l.l_orderkey = o.o_orderkey
  AND l_suppkey BETWEEN 1 AND 10
GROUP BY o_orderkey;

-- do not project out later, because that may sometimes lead to inferior query plans. E.g., don't do this:
SELECT o_orderkey, avgQ, prov_public_lineitem_l__linenumber, prov_public_orders_o__orderkey
FROM   (SELECT PROVENANCE o_orderkey, AVG(l_quantity) AS avgQ
	   FROM lineitem l, orders o
	   WHERE l.l_orderkey = o.o_orderkey
	     AND l_suppkey BETWEEN 1 AND 10
	   GROUP BY o_orderkey) p
;


-- the server has already a 1GB TPCH instance installed (also 100MB, 10MB, and 1MB)
-- the corresponding database names are tpch_1, tpch_0_1, tpch_0_01, tpch_0_001
-- we have 2 Perm versions installed on this server, the directory layout for both installations is the same:
--  * /local/perm/code - source code
--  * /local/perm/install - installation including binaries and so on
--  * /local/perm/cluster - database clusters
--  * /local/perm/scripts - scripts for starting, stopping, checking whether server is running, switching between debug and releast configurations
-- you should probably using the version which has a special provenance aggregation operator implementation. The scripts for this one start with prefix 'a', e.g., apermOn.sh to start the server 

   
