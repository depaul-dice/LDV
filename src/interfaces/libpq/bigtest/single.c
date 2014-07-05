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
        int r, n;
        int nrows = PQntuples ( result );
        int nfields = PQnfields ( result );
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
  static char sqlStr[1000];
  sprintf(sqlStr, "INSERT INTO tbl1 values(%d, %d)", rand() % MYMAGICN, rand() % MYMAGICN);
  doSQL(conn, sqlStr);
}

void update(PGconn *conn);
void update(PGconn *conn) {
  static char sqlStr[1000];
  sprintf(sqlStr, "UPDATE tbl1 SET value=%d WHERE id=%d", rand() % MYMAGICN, rand() % MYMAGICN);
  doSQL(conn, sqlStr);
}

int main(int argc, char** argv) {
    PGconn *conn;
    const char *conninfo;     // argv[1]
    int seed = MYMAGICN;      // argv[2]
    char mode = 1;      // true if seed param % 10 == 0
    int counter = 2;          // number of repeatition
    char sql[256];
    
    if (argc > 1)
    	conninfo = argv[1];
    else
    	conninfo = "host=localhost dbname=single";
    conn = PQconnectdb ( conninfo );
      
    if (argc > 2)
    	seed = atoi(argv[2]);
    if (seed > 0)
    	srand(seed);
    mode = seed % 10;

    if (argc > 3)
    	MYMAGICN = atoi(argv[3]);
    counter = MYMAGICN;

    if ( PQstatus ( conn ) == CONNECTION_OK ) {
//        printf ( "connection made\n" );
        if (mode == 1) {
			for (; counter > 0; counter--)
				insert(conn);
        } else if (mode == 2) {
        	for (; counter > 0; counter--) {
				sprintf(sql, "SELECT sum(value) FROM tbl1 WHERE value < %d", counter);
				doSQL ( conn, sql );
	//          doSQL ( conn, "select name, sum(price) from items i, persons p, sales s where p.id = s.personid and s.itemid = i.id group by name;");
        	}
        }  else if (mode == 3) {
        	for (; counter > 0; counter--)
        		update(conn);
        }
    } else
        printf ( "connection failed %s\n", PQerrorMessage ( conn ) );

    PQfinish ( conn );
    return EXIT_SUCCESS;
}
