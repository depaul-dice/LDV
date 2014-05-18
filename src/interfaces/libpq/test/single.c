#include <stdlib.h>
#include <unistd.h>
#include <libpq-fe.h>

#define MYMAGICN 100

void doSQL ( PGconn *conn, char *command );
void doSQL ( PGconn *conn, char *command ) {
    PGresult *result;
    printf ( "%s\n", command );
    result = PQexec ( conn, command );

    if (PQresultStatus(result) != PGRES_COMMAND_OK)
        printf("status is %s\n", PQresStatus(PQresultStatus(result)));
    //~ printf("#rows affected %s\n", PQcmdTuples(result));
    //~ printf("result message: %s\n", PQresultErrorMessage(result));
    switch ( PQresultStatus ( result ) ) {
    case PGRES_TUPLES_OK: {
        int r, n;
        int nrows = PQntuples ( result );
        int nfields = PQnfields ( result );
        //~ printf ( "number of rows returned = %d\n", nrows );
        //~ printf ( "number of fields returned = %d\n", nfields );

        for ( r = 0; r < nrows; r++ ) {
            for ( n = 0; n < nfields; n++ )
                printf ( " %s = %s(%d),",
                         PQfname ( result, n ),
                         PQgetvalue ( result, r, n ),
                         PQgetlength ( result, r, n ) );

            printf ( "\n" );
        }
    }
    default: {
      break;
    }
    }

    PQclear ( result );
}

void insert(PGconn *conn);
void insert(PGconn *conn) {
  char sqlStr[1000];
  usleep(rand() % 1000 * 1000);
  sprintf(sqlStr, "INSERT INTO tbl1 values(%d, %d)", rand() % MYMAGICN, rand() % MYMAGICN);
  // multiple query on a single issue is fine
  //~ sprintf(sqlStr, "INSERT INTO tbl1 values(%d, %d);INSERT INTO tbl1 values(%d, %d);", 
    //~ rand() % MYMAGICN, rand() % MYMAGICN, rand() % MYMAGICN, rand() % MYMAGICN); 
  doSQL(conn, sqlStr);
}

int main(int argc, char** argv) {
    PGconn *conn;
    const char *conninfo;     // argv[1]
    int seed = MYMAGICN;      // argv[2]
    char insertmode = 1;       // true if seed param < 0
    
    if (argc > 1)
      conninfo = argv[1];
    else
      conninfo = "dbname=single";
    conn = PQconnectdb ( conninfo );
      
    if (argc > 2)
      seed = atoi(argv[2]);
    if (seed < 0) {
      insertmode = 0;
    } else 
      srand(seed);

    if ( PQstatus ( conn ) == CONNECTION_OK ) {
        printf ( "connection made\n" );
        if (insertmode) {
          if (seed == MYMAGICN) {
            doSQL ( conn, "DROP TABLE tbl1" );
            doSQL ( conn, "CREATE TABLE tbl1 (id INTEGER, value INTEGER)" );
          }
          sleep(1); // everyone wait for the table to be created
          insert(conn);
          insert(conn);
          insert(conn);
          insert(conn);
          insert(conn);
          insert(conn);
        } else {
          //~ doSQL ( conn, "SELECT * FROM tbl1_prov_" );
          doSQL ( conn, "SELECT sum(value) FROM tbl1 WHERE value < 50" );
          doSQL ( conn, "SELECT PROVENANCE sum(value) FROM tbl1 PROVENANCE(value) WHERE value < 50" );
          doSQL ( conn, "SELECT PROVENANCE sum(value) FROM tbl1_prov_ PROVENANCE(_prov_p, _prov_v) WHERE value < 50" );
        }
    }

    else
        printf ( "connection failed %s\n", PQerrorMessage ( conn ) );

    PQfinish ( conn );
    return EXIT_SUCCESS;
}
