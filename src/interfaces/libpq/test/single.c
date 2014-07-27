#include <stdlib.h>
#include <unistd.h>
#include <libpq-fe.h>

#define MYMAGICN 100

void doSQL ( PGconn *conn, char *command );
void doSQL ( PGconn *conn, char *command ) {
    PGresult *result;
    int restatus;
    printf ( "%s\n", command );
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
  char sqlStr[1000];
  usleep(rand() % 1000 * 1000);
  sprintf(sqlStr, "INSERT INTO tbl1 values(%d, %d)", rand() % MYMAGICN, rand() % MYMAGICN);
  // multiple query on a single issue is fine
  //~ sprintf(sqlStr, "INSERT INTO tbl1 values(%d, %d);INSERT INTO tbl1 values(%d, %d);", 
    //~ rand() % MYMAGICN, rand() % MYMAGICN, rand() % MYMAGICN, rand() % MYMAGICN); 
  doSQL(conn, sqlStr);
}

void fakeio(char* pattern, int seed);
void fakeio(char* pattern, int seed) {
    
    char path[100];
    FILE *fd;
    
    // fake input
    sprintf(path, pattern, "input", seed);
//    printf("read file %s\n", path);
    fd = fopen(path, "r");
    if (fd != NULL)
        fclose(fd);

    // fake output
    sprintf(path, pattern, "output", seed);
//    printf("write file %s\n", path);
    fd = fopen(path, "w");
    if (fd != NULL)
        fclose(fd);
}

int main(int argc, char** argv) {
    PGconn *conn;
    const char *conninfo;     // argv[1]
    int seed = MYMAGICN;      // argv[2]
    char insertmode = 1;       // true if seed param < 0
    
    if (argc > 1)
      conninfo = argv[1];
    else
      conninfo = "host=localhost dbname=single";
    conn = PQconnectdb ( conninfo );
      
    if (argc > 2)
      seed = atoi(argv[2]);
    if (seed <= 0) {
      insertmode = 0;
    } else 
      srand(seed);
      
    fakeio("io/%s.before.%d.txt", seed);

    if ( PQstatus ( conn ) == CONNECTION_OK ) {
        //printf ( "connection made\n" );
        if (insertmode) {
          if (seed == MYMAGICN) {
            doSQL ( conn, "DROP TABLE tbl1" );
            doSQL ( conn, "CREATE TABLE tbl1 (id INTEGER, value INTEGER)" );
          }
          sleep(1); // everyone wait for the table to be created
          // insert randome stuff
          insert(conn);
          insert(conn);
          doSQL ( conn, "UPDATE tbl1 SET value=60 WHERE value = 101;");
          doSQL ( conn, "DELETE FROM tbl1 WHERE value = 3;");
        } else {
          doSQL ( conn, "SELECT sum(value) FROM tbl1 WHERE value < 40" );
          doSQL ( conn, "select name, sum(price) from items i, persons p, sales s where p.id = s.personid and s.itemid = i.id group by name;");
          // doSQL ( conn, "SELECT id, value FROM tbl1 WHERE value < 50" );
          // doSQL ( conn, "SELECT * FROM tbl1" );
          // doSQL ( conn, "select sum(tbl1.value) from tbl2 join tbl1 on tbl1.id=tbl2.id where tbl2.value < 50;");
        }
    } else
        printf ( "connection failed %s\n", PQerrorMessage ( conn ) );
    
    fakeio("io/%s.after.%d.txt", seed);

    PQfinish ( conn );
    return EXIT_SUCCESS;
}
