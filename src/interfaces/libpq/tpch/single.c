#include <stdlib.h>
#include <unistd.h>
#include <libpq-fe.h>

int MYMAGICN = 1000;

void doSQL ( PGconn *conn, char *command );
void doSQL ( PGconn *conn, char *command ) {
    PGresult *result;
    int restatus;
//    printf ( "%s\n", command );
    result = PQexec ( conn, command );

    restatus = PQresultStatus(result);
    if (restatus != PGRES_COMMAND_OK && restatus != PGRES_TUPLES_OK)
        printf("status is %s\n", PQresStatus(restatus));
    //~ printf("#rows affected %s\n", PQcmdTuples(result));
    //~ printf("result message: %s\n", PQresultErrorMessage(result));
    switch ( PQresultStatus ( result ) ) {
    case PGRES_TUPLES_OK: {
//        int r, n;
//        int nrows = PQntuples ( result );
//        int nfields = PQnfields ( result );
//        printf ( "number of rows returned = %d\n", nrows );
//        printf ( "number of fields returned = %d\n", nfields );
//
//        for ( r = 0; r < nrows; r++ ) {
//            for ( n = 0; n < nfields; n++ )
//                printf ( " %s = %s(%d),",
//                         PQfname ( result, n ),
//                         PQgetvalue ( result, r, n ),
//                         PQgetlength ( result, r, n ) );
//
//            printf ( "\n" );
//        }
        break;
    }
    default: {
      break;
    }
    }

    PQclear ( result );
}

void insert(PGconn *conn);
void insert(PGconn *conn) {
    static char sqlStr[1000], line[1000];

    FILE *f = fopen("orders.tbl", "r");
    while (fgets(line, 900, f)) {
        sprintf(sqlStr, "INSERT INTO orders values(%s);", line);
        doSQL(conn, sqlStr);
    }
    fclose(f);
/*
    f = fopen("lineitem.tbl", "r");
    while (fgets(line, 900, f)) {
        sprintf(sqlStr, "INSERT INTO lineitem values(%s);", line);
        doSQL(conn, sqlStr);
    }
    fclose(f); */
}

void update(PGconn *conn);
void update(PGconn *conn) {
    static char sqlStr[1000];
    sprintf(sqlStr, "UPDATE orders SET o_comment=md5((random())::text) WHERE o_orderkey=%d", rand() % MYMAGICN);
    doSQL(conn, sqlStr);
}

void doselect(PGconn *conn, int tpchquery) {
    char *sqlp;
    switch (tpchquery){
    case 1: // 97% of the lineitem is dump --> huge and long time
        sqlp = "select     l_returnflag,     l_linestatus,     sum(l_quantity) as sum_qty, "
            "    sum(l_extendedprice) as sum_base_price, "
            "    sum(l_extendedprice * (1 - l_discount)) as sum_disc_price, "
            "    sum(l_extendedprice * (1 - l_discount) * (1 + l_tax)) as sum_charge, "
            "    avg(l_quantity) as avg_qty,     avg(l_extendedprice) as avg_price, "
            "    avg(l_discount) as avg_disc,     count(*) as count_order "
            "from     lineitem where "
            "    l_shipdate <= date '1998-12-01' - interval '112 day' "
            "group by     l_returnflag,     l_linestatus "
            "order by     l_returnflag,     l_linestatus;";
        break;
    case 2: // db server segfault :(
        sqlp = "select"
                "    s_acctbal,"
                "    s_name,"
                "    n_name,"
                "    p_partkey,"
                "    p_mfgr,"
                "    s_address,"
                "    s_phone,"
                "    s_comment"
                " from"
                "    part,"
                "    supplier,"
                "    partsupp,"
                "    nation,"
                "    region"
                " where"
                "    p_partkey = ps_partkey"
                "    and s_suppkey = ps_suppkey"
                "    and p_size = 9"
                "    and p_type like '%TIN'"
                "    and s_nationkey = n_nationkey"
                "    and n_regionkey = r_regionkey"
                "    and r_name = 'MIDDLE EAST'"
                "    and ps_supplycost = ("
                "        select"
                "            min(ps_supplycost)"
                "        from"
                "            partsupp,"
                "            supplier,"
                "            nation,"
                "            region"
                "        where"
                "            p_partkey = ps_partkey"
                "            and s_suppkey = ps_suppkey"
                "            and s_nationkey = n_nationkey"
                "            and n_regionkey = r_regionkey"
                "            and r_name = 'MIDDLE EAST'"
                "    )"
                " order by"
                "    s_acctbal desc,"
                "    n_name,"
                "    s_name,"
                "    p_partkey"
                " limit 100;";
//        sqlp = "select"
//                "    s_acctbal,"
//                "    s_name,"
//                "    p_partkey,"
//                "    p_mfgr,"
//                "    s_address,"
//                "    s_phone,"
//                "    s_comment"
//                " from part, supplier, partsupp"
//                " where"
//                "    p_partkey = ps_partkey"
//                "    and s_suppkey = ps_suppkey";
        break;
    case 3:
        sqlp = "select"
                "    l_orderkey,"
                "    sum(l_extendedprice * (1 - l_discount)) as revenue,"
                "    o_orderdate,"
                "    o_shippriority"
                " from"
                "    customer,"
                "    orders,"
                "    lineitem"
                " where"
                "    c_mktsegment = 'HOUSEHOLD'"
                "    and c_custkey = o_custkey"
                "    and l_orderkey = o_orderkey"
                "    and o_orderdate < date '1995-03-29'"
                "    and l_shipdate > date '1995-03-29'"
                " group by"
                "    l_orderkey,"
                "    o_orderdate,"
                "    o_shippriority"
                " order by"
                "    revenue desc,"
                "    o_orderdate"
                " limit 10;";
        break;
    case 4:
        sqlp = NULL;
        break;
    case 5:
        sqlp = "select"
                "    n_name,"
                "    sum(l_extendedprice * (1 - l_discount)) as revenue"
                " from"
                "    customer,"
                "    orders,"
                "    lineitem,"
                "    supplier,"
                "    nation,"
                "    region"
                " where"
                "    c_custkey = o_custkey"
                "    and l_orderkey = o_orderkey"
                "    and l_suppkey = s_suppkey"
                "    and c_nationkey = s_nationkey"
                "    and s_nationkey = n_nationkey"
                "    and n_regionkey = r_regionkey"
                "    and r_name = 'AMERICA'"
                "    and o_orderdate >= date '1994-01-01'"
                "    and o_orderdate < date '1994-01-01' + interval '1 year'"
                " group by"
                "    n_name"
                " order by"
                "    revenue desc;";
        break;
    case 6:
        sqlp = "select"
                "    sum(l_extendedprice * l_discount) as revenue"
                " from"
                "    lineitem"
                " where"
                "    l_shipdate >= date '1994-01-01'"
                "    and l_shipdate < date '1994-01-01' + interval '1 year'"
                "    and l_discount between 0.03 - 0.01 and 0.03 + 0.01"
                "    and l_quantity < 24;";
        break;
    case 7:
    case 8:
    case 9:
        sqlp = NULL;
        break;
    case 10:
        sqlp = "select"
                "    c_custkey,"
                "    c_name,"
                "    sum(l_extendedprice * (1 - l_discount)) as revenue,"
                "    c_acctbal,"
                "    n_name,"
                "    c_address,"
                "    c_phone,"
                "    c_comment"
                " from"
                "    customer,"
                "    orders,"
                "    lineitem,"
                "    nation"
                " where"
                "    c_custkey = o_custkey"
                "    and l_orderkey = o_orderkey"
                "    and o_orderdate >= date '1993-03-01'"
                "    and o_orderdate < date '1993-03-01' + interval '3 month'"
                "    and l_returnflag = 'R'"
                "    and c_nationkey = n_nationkey"
                " group by"
                "    c_custkey,"
                "    c_name,"
                "    c_acctbal,"
                "    c_phone,"
                "    n_name,"
                "    c_address,"
                "    c_comment"
                " order by"
                "    revenue desc"
                " limit 20;";
        break;
    case 11:
        sqlp = "select"
                "    ps_partkey,"
                "    sum(ps_supplycost * ps_availqty) as value"
                " from"
                "    partsupp,"
                "    supplier,"
                "    nation"
                " where"
                "    ps_suppkey = s_suppkey"
                "    and s_nationkey = n_nationkey"
                "    and n_name = 'JAPAN'"
                " group by"
                "    ps_partkey having"
                "        sum(ps_supplycost * ps_availqty) > ("
                "            select"
                "                sum(ps_supplycost * ps_availqty) * 0.0001000000"
                "            from"
                "                partsupp,"
                "                supplier,"
                "                nation"
                "            where"
                "                ps_suppkey = s_suppkey"
                "                and s_nationkey = n_nationkey"
                "                and n_name = 'JAPAN'"
                "        )"
                " order by"
                "    value desc;";
        break;
    case 12:
        sqlp = "select"
                "    l_shipmode,"
                "    sum(case"
                "        when o_orderpriority = '1-URGENT'"
                "            or o_orderpriority = '2-HIGH'"
                "            then 1"
                "        else 0"
                "    end) as high_line_count,"
                "    sum(case"
                "        when o_orderpriority <> '1-URGENT'"
                "            and o_orderpriority <> '2-HIGH'"
                "            then 1"
                "        else 0"
                "    end) as low_line_count"
                " from"
                "    orders,"
                "    lineitem"
                " where"
                "    o_orderkey = l_orderkey"
                "    and l_shipmode in ('FOB', 'TRUCK')"
                "    and l_commitdate < l_receiptdate"
                "    and l_shipdate < l_commitdate"
                "    and l_receiptdate >= date '1996-01-01'"
                "    and l_receiptdate < date '1996-01-01' + interval '1 year'"
                " group by"
                "    l_shipmode"
                " order by"
                "    l_shipmode;";
        break;
    case 13:
        sqlp = NULL;
        break;
    case 14:
        sqlp = "select"
                "    100.00 * sum(case"
                "        when p_type like 'PROMO%'"
                "            then l_extendedprice * (1 - l_discount)"
                "        else 0"
                "    end) / sum(l_extendedprice * (1 - l_discount)) as promo_revenue"
                " from"
                "    lineitem,"
                "    part"
                " where"
                "    l_partkey = p_partkey"
                "    and l_shipdate >= date '1996-04-01'"
                "    and l_shipdate < date '1996-04-01' + interval '1 month';";
        break;
    case 15:
		sqlp = NULL;
		break;
	case 16:
		sqlp = "select"
				"	p_brand,"
				"	p_type,"
				"	p_size,"
				"	count(distinct ps_suppkey) as supplier_cnt"
				" from"
				"	partsupp,"
				"	part"
				" where"
				"	p_partkey = ps_partkey"
				"	and p_brand <> 'Brand#35'"
				"	and p_type not like 'ECONOMY BURNISHED%'"
				"	and p_size in (14, 7, 21, 24, 35, 33, 2, 20)"
				"	and ps_suppkey not in ("
				"		select"
				"			s_suppkey"
				"		from"
				"			supplier"
				"		where"
				"			s_comment like '%Customer%Complaints%'"
				"	)"
				" group by"
				"	p_brand,"
				"	p_type,"
				"	p_size"
				" order by"
				"	supplier_cnt desc,"
				"	p_brand,"
				"	p_type,"
				"	p_size;";
		break;
	case 17:
		sqlp = "select"
				"	sum(l_extendedprice) / 7.0 as avg_yearly"
				" from"
				"	lineitem,"
				"	part"
				" where"
				"	p_partkey = l_partkey"
				"	and p_brand = 'Brand#54'"
				"	and p_container = 'LG BAG'"
				"	and l_quantity < ("
				"		select"
				"			0.2 * avg(l_quantity)"
				"		from"
				"			lineitem"
				"		where"
				"			l_partkey = p_partkey"
				"	);";
		break;
	case 18:
		sqlp = "select"
				"	c_name,"
				"	c_custkey,"
				"	o_orderkey,"
				"	o_orderdate,"
				"	o_totalprice,"
				"	sum(l_quantity)"
				" from"
				"	customer,"
				"	orders,"
				"	lineitem"
				" where"
				"	o_orderkey in ("
				"		select"
				"			l_orderkey"
				"		from"
				"			lineitem"
				"		group by"
				"			l_orderkey having"
				"				sum(l_quantity) > 314"
				"	)"
				"	and c_custkey = o_custkey"
				"	and o_orderkey = l_orderkey"
				" group by"
				"	c_name,"
				"	c_custkey,"
				"	o_orderkey,"
				"	o_orderdate,"
				"	o_totalprice"
				" order by"
				"	o_totalprice desc,"
				"	o_orderdate;";
		break;
	case 19:
		sqlp = "select"
				"	sum(l_extendedprice* (1 - l_discount)) as revenue"
				" from"
				"	lineitem,"
				"	part"
				" where"
				"	("
				"		p_partkey = l_partkey"
				"		and p_brand = 'Brand#23'"
				"		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')"
				"		and l_quantity >= 5 and l_quantity <= 5 + 10"
				"		and p_size between 1 and 5"
				"		and l_shipmode in ('AIR', 'AIR REG')"
				"		and l_shipinstruct = 'DELIVER IN PERSON'"
				"	)"
				"	or"
				"	("
				"		p_partkey = l_partkey"
				"		and p_brand = 'Brand#15'"
				"		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')"
				"		and l_quantity >= 14 and l_quantity <= 14 + 10"
				"		and p_size between 1 and 10"
				"		and l_shipmode in ('AIR', 'AIR REG')"
				"		and l_shipinstruct = 'DELIVER IN PERSON'"
				"	)"
				"	or"
				"	("
				"		p_partkey = l_partkey"
				"		and p_brand = 'Brand#44'"
				"		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')"
				"		and l_quantity >= 28 and l_quantity <= 28 + 10"
				"		and p_size between 1 and 15"
				"		and l_shipmode in ('AIR', 'AIR REG')"
				"		and l_shipinstruct = 'DELIVER IN PERSON'"
				"	);";
		break;
	case 20:
	case 21:
	case 22:
		sqlp = NULL;
		break;
    case 30:
		sqlp = "SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate "
			"FROM lineitem "
			"WHERE l_suppkey BETWEEN 1 AND 10;";
		break;
    case 31:
        sqlp = "SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate "
            "FROM lineitem "
            "WHERE l_suppkey BETWEEN 1 AND 20;";
        break;
    case 32:
        sqlp = "SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate "
            "FROM lineitem "
            "WHERE l_suppkey BETWEEN 1 AND 50;";
        break;
    case 33:
        sqlp = "SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate "
            "FROM lineitem "
            "WHERE l_suppkey BETWEEN 1 AND 100;";
        break;
    case 34:
        sqlp = "SELECT l_quantity, l_partkey, l_extendedprice,  l_shipdate , l_receiptdate "
            "FROM lineitem "
            "WHERE l_suppkey BETWEEN 1 AND 250;";
        break;
    case 35:
		sqlp = "SELECT o_comment, l_comment "
			"FROM lineitem, orders, customer "
			"WHERE lineitem.l_orderkey = orders.o_orderkey "
			"      AND orders.o_custkey = customer.c_custkey "
			"  AND customer.c_name LIKE '%0000000%';";
		break;
    case 36:
        sqlp = "SELECT o_comment, l_comment "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%000000%';";
        break;
    case 37:
        sqlp = "SELECT o_comment, l_comment "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%00000%';";
        break;
    case 38:
        sqlp = "SELECT o_comment, l_comment "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%0000%';";
        break;
	case 39:
		sqlp = 	"SELECT count(*) "
			"FROM lineitem, orders, customer "
			"WHERE lineitem.l_orderkey = orders.o_orderkey "
			"      AND orders.o_custkey = customer.c_custkey "
			"  AND customer.c_name LIKE '%0000000%';";
		break;
    case 40:
        sqlp =  "SELECT count(*) "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%000000%';";
        break;
    case 41:
        sqlp =  "SELECT count(*) "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%00000%';";
        break;
    case 42:
        sqlp =  "SELECT count(*) "
            "FROM lineitem, orders, customer "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "      AND orders.o_custkey = customer.c_custkey "
            "  AND customer.c_name LIKE '%0000%';";
        break;
	case 43:
		sqlp = "SELECT o_orderkey, AVG(l_quantity) AS avgQ "
			"FROM lineitem, orders "
			"WHERE lineitem.l_orderkey = orders.o_orderkey "
			"	  AND l_suppkey BETWEEN 1 AND 10 "
			"GROUP BY o_orderkey";
		break;
    case 44:
        sqlp = "SELECT o_orderkey, AVG(l_quantity) AS avgQ "
            "FROM lineitem, orders "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "     AND l_suppkey BETWEEN 1 AND 20 "
            "GROUP BY o_orderkey";
        break;
    case 45:
        sqlp = "SELECT o_orderkey, AVG(l_quantity) AS avgQ "
            "FROM lineitem, orders "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "     AND l_suppkey BETWEEN 1 AND 50 "
            "GROUP BY o_orderkey";
        break;
    case 46:
        sqlp = "SELECT o_orderkey, AVG(l_quantity) AS avgQ "
            "FROM lineitem, orders "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "     AND l_suppkey BETWEEN 1 AND 100 "
            "GROUP BY o_orderkey";
        break;
    case 47:
        sqlp = "SELECT o_orderkey, AVG(l_quantity) AS avgQ "
            "FROM lineitem, orders "
            "WHERE lineitem.l_orderkey = orders.o_orderkey "
            "     AND l_suppkey BETWEEN 1 AND 250 "
            "GROUP BY o_orderkey";
        break;
    default:
		sqlp = NULL;
		break;
    }

    if (sqlp != NULL)
        doSQL(conn, sqlp);
    else
        printf("UNsupported sql query %d\n", tpchquery);
}

void poke(PGconn *conn) {
    doSQL ( conn, "select * from customer where 0 = 1;" );
    doSQL ( conn, "select * from lineitem where 0 = 1;" );
    doSQL ( conn, "select * from nation where 0 = 1;" );
    doSQL ( conn, "select * from orders where 0 = 1;" );
    doSQL ( conn, "select * from part where 0 = 1;" );
    doSQL ( conn, "select * from partsupp where 0 = 1;" );
    doSQL ( conn, "select * from region where 0 = 1;" );
    doSQL ( conn, "select * from supplier where 0 = 1;" );
}

int main(int argc, char** argv) {
    PGconn *conn;
    const char *conninfo;     // argv[1]
    int seed = MYMAGICN;      // argv[2]
    char mode = 1;      // true if seed param % 10 == 0
    int counter = 2;          // number of repeatition
    char sql[256];
    int tpchquery = 1;
    
    if (argc > 1)
        conninfo = argv[1];
    else
        conninfo = "host=localhost";
    conn = PQconnectdb ( conninfo );
      
    if (argc > 2)
        seed = atoi(argv[2]);
    if (seed > 0)
        srand(seed);
    mode = seed % 10;

    if (argc > 3)
        MYMAGICN = atoi(argv[3]);
    counter = MYMAGICN;

    if (argc > 4)
        tpchquery = atoi(argv[4]);

    if ( PQstatus ( conn ) == CONNECTION_OK ) {
//        printf ( "connection made\n" );
        if (mode == 1) {
            printf("insert mode\n");
            insert(conn);
        } else if (mode == 2) {
            printf("select mode\n");
            for (; counter > 0; counter--)
                doselect(conn, tpchquery);
        } else if (mode == 3) {
            printf("update mode\n");
            for (; counter > 0; counter--)
                update(conn);
        } else if (mode == 4) {
            printf("bypass mode\n");
            for (; counter > 0; counter--) {
                sprintf(sql, "BYPASS SELECT PROVENANCE sum(value) FROM tbl1 WHERE value < %d", counter);
                doSQL ( conn, sql );
            }
        } else if (mode == 5) {
            printf("pokedb mode\n");
            // do nothing, this is to test PQconnectdb (restore db)
            poke(conn);
        }

    } else
        printf ( "connection failed %s\n", PQerrorMessage ( conn ) );

    PQfinish ( conn );
    return EXIT_SUCCESS;
}
