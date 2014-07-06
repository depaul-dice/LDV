/*-------------------------------------------------------------------------
 *
 * fe-exec.c
 *	  functions related to sending a query down to the backend
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/interfaces/libpq/fe-exec.c,v 1.194 2008/01/01 19:46:00 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

#include "libpq-fe.h"
#include "libpq-int.h"

#include "mb/pg_wchar.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#endif

/* keep this in same order as ExecStatusType in libpq-fe.h */
char	   *const pgresStatus[] = {
	"PGRES_EMPTY_QUERY",
	"PGRES_COMMAND_OK",
	"PGRES_TUPLES_OK",
	"PGRES_COPY_OUT",
	"PGRES_COPY_IN",
	"PGRES_BAD_RESPONSE",
	"PGRES_NONFATAL_ERROR",
	"PGRES_FATAL_ERROR"
};

/*
 * static state needed by PQescapeString and PQescapeBytea; initialize to
 * values that result in backward-compatible behavior
 */
static int	static_client_encoding = PG_SQL_ASCII;
static bool static_std_strings = false;


static bool PQsendQueryStart(PGconn *conn);
static int PQsendQueryGuts(PGconn *conn,
				const char *command,
				const char *stmtName,
				int nParams,
				const Oid *paramTypes,
				const char *const * paramValues,
				const int *paramLengths,
				const int *paramFormats,
				int resultFormat);
static void parseInput(PGconn *conn);
static bool PQexecStart(PGconn *conn);
static PGresult *PQexecFinish(PGconn *conn);
static int PQsendDescribe(PGconn *conn, char desc_type,
			   const char *desc_target);


/* ----------------
 * Space management for PGresult.
 *
 * Formerly, libpq did a separate malloc() for each field of each tuple
 * returned by a query.  This was remarkably expensive --- malloc/free
 * consumed a sizable part of the application's runtime.  And there is
 * no real need to keep track of the fields separately, since they will
 * all be freed together when the PGresult is released.  So now, we grab
 * large blocks of storage from malloc and allocate space for query data
 * within these blocks, using a trivially simple allocator.  This reduces
 * the number of malloc/free calls dramatically, and it also avoids
 * fragmentation of the malloc storage arena.
 * The PGresult structure itself is still malloc'd separately.  We could
 * combine it with the first allocation block, but that would waste space
 * for the common case that no extra storage is actually needed (that is,
 * the SQL command did not return tuples).
 *
 * We also malloc the top-level array of tuple pointers separately, because
 * we need to be able to enlarge it via realloc, and our trivial space
 * allocator doesn't handle that effectively.  (Too bad the FE/BE protocol
 * doesn't tell us up front how many tuples will be returned.)
 * All other subsidiary storage for a PGresult is kept in PGresult_data blocks
 * of size PGRESULT_DATA_BLOCKSIZE.  The overhead at the start of each block
 * is just a link to the next one, if any.	Free-space management info is
 * kept in the owning PGresult.
 * A query returning a small amount of data will thus require three malloc
 * calls: one for the PGresult, one for the tuples pointer array, and one
 * PGresult_data block.
 *
 * Only the most recently allocated PGresult_data block is a candidate to
 * have more stuff added to it --- any extra space left over in older blocks
 * is wasted.  We could be smarter and search the whole chain, but the point
 * here is to be simple and fast.  Typical applications do not keep a PGresult
 * around very long anyway, so some wasted space within one is not a problem.
 *
 * Tuning constants for the space allocator are:
 * PGRESULT_DATA_BLOCKSIZE: size of a standard allocation block, in bytes
 * PGRESULT_ALIGN_BOUNDARY: assumed alignment requirement for binary data
 * PGRESULT_SEP_ALLOC_THRESHOLD: objects bigger than this are given separate
 *	 blocks, instead of being crammed into a regular allocation block.
 * Requirements for correct function are:
 * PGRESULT_ALIGN_BOUNDARY must be a multiple of the alignment requirements
 *		of all machine data types.	(Currently this is set from configure
 *		tests, so it should be OK automatically.)
 * PGRESULT_SEP_ALLOC_THRESHOLD + PGRESULT_BLOCK_OVERHEAD <=
 *			PGRESULT_DATA_BLOCKSIZE
 *		pqResultAlloc assumes an object smaller than the threshold will fit
 *		in a new block.
 * The amount of space wasted at the end of a block could be as much as
 * PGRESULT_SEP_ALLOC_THRESHOLD, so it doesn't pay to make that too large.
 * ----------------
 */

#define PGRESULT_DATA_BLOCKSIZE		2048
#define PGRESULT_ALIGN_BOUNDARY		MAXIMUM_ALIGNOF		/* from configure */
#define PGRESULT_BLOCK_OVERHEAD		Max(sizeof(PGresult_data), PGRESULT_ALIGN_BOUNDARY)
#define PGRESULT_SEP_ALLOC_THRESHOLD	(PGRESULT_DATA_BLOCKSIZE / 2)


/*
 * PQmakeEmptyPGresult
 *	 returns a newly allocated, initialized PGresult with given status.
 *	 If conn is not NULL and status indicates an error, the conn's
 *	 errorMessage is copied.
 *
 * Note this is exported --- you wouldn't think an application would need
 * to build its own PGresults, but this has proven useful in both libpgtcl
 * and the Perl5 interface, so maybe it's not so unreasonable.
 */

PGresult *
PQmakeEmptyPGresult(PGconn *conn, ExecStatusType status)
{
	PGresult   *result;

	result = (PGresult *) malloc(sizeof(PGresult));
	if (!result)
		return NULL;

	result->ntups = 0;
	result->numAttributes = 0;
	result->attDescs = NULL;
	result->tuples = NULL;
	result->tupArrSize = 0;
	result->numParameters = 0;
	result->paramDescs = NULL;
	result->resultStatus = status;
	result->cmdStatus[0] = '\0';
	result->binary = 0;
	result->errMsg = NULL;
	result->errFields = NULL;
	result->null_field[0] = '\0';
	result->curBlock = NULL;
	result->curOffset = 0;
	result->spaceLeft = 0;

	if (conn)
	{
		/* copy connection data we might need for operations on PGresult */
		result->noticeHooks = conn->noticeHooks;
		result->client_encoding = conn->client_encoding;

		/* consider copying conn's errorMessage */
		switch (status)
		{
			case PGRES_EMPTY_QUERY:
			case PGRES_COMMAND_OK:
			case PGRES_TUPLES_OK:
			case PGRES_COPY_OUT:
			case PGRES_COPY_IN:
				/* non-error cases */
				break;
			default:
				pqSetResultError(result, conn->errorMessage.data);
				break;
		}
	}
	else
	{
		/* defaults... */
		result->noticeHooks.noticeRec = NULL;
		result->noticeHooks.noticeRecArg = NULL;
		result->noticeHooks.noticeProc = NULL;
		result->noticeHooks.noticeProcArg = NULL;
		result->client_encoding = PG_SQL_ASCII;
	}

	return result;
}

/*
 * pqResultAlloc -
 *		Allocate subsidiary storage for a PGresult.
 *
 * nBytes is the amount of space needed for the object.
 * If isBinary is true, we assume that we need to align the object on
 * a machine allocation boundary.
 * If isBinary is false, we assume the object is a char string and can
 * be allocated on any byte boundary.
 */
void *
pqResultAlloc(PGresult *res, size_t nBytes, bool isBinary)
{
	char	   *space;
	PGresult_data *block;

	if (!res)
		return NULL;

	if (nBytes <= 0)
		return res->null_field;

	/*
	 * If alignment is needed, round up the current position to an alignment
	 * boundary.
	 */
	if (isBinary)
	{
		int			offset = res->curOffset % PGRESULT_ALIGN_BOUNDARY;

		if (offset)
		{
			res->curOffset += PGRESULT_ALIGN_BOUNDARY - offset;
			res->spaceLeft -= PGRESULT_ALIGN_BOUNDARY - offset;
		}
	}

	/* If there's enough space in the current block, no problem. */
	if (nBytes <= (size_t) res->spaceLeft)
	{
		space = res->curBlock->space + res->curOffset;
		res->curOffset += nBytes;
		res->spaceLeft -= nBytes;
		return space;
	}

	/*
	 * If the requested object is very large, give it its own block; this
	 * avoids wasting what might be most of the current block to start a new
	 * block.  (We'd have to special-case requests bigger than the block size
	 * anyway.)  The object is always given binary alignment in this case.
	 */
	if (nBytes >= PGRESULT_SEP_ALLOC_THRESHOLD)
	{
		block = (PGresult_data *) malloc(nBytes + PGRESULT_BLOCK_OVERHEAD);
		if (!block)
			return NULL;
		space = block->space + PGRESULT_BLOCK_OVERHEAD;
		if (res->curBlock)
		{
			/*
			 * Tuck special block below the active block, so that we don't
			 * have to waste the free space in the active block.
			 */
			block->next = res->curBlock->next;
			res->curBlock->next = block;
		}
		else
		{
			/* Must set up the new block as the first active block. */
			block->next = NULL;
			res->curBlock = block;
			res->spaceLeft = 0; /* be sure it's marked full */
		}
		return space;
	}

	/* Otherwise, start a new block. */
	block = (PGresult_data *) malloc(PGRESULT_DATA_BLOCKSIZE);
	if (!block)
		return NULL;
	block->next = res->curBlock;
	res->curBlock = block;
	if (isBinary)
	{
		/* object needs full alignment */
		res->curOffset = PGRESULT_BLOCK_OVERHEAD;
		res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - PGRESULT_BLOCK_OVERHEAD;
	}
	else
	{
		/* we can cram it right after the overhead pointer */
		res->curOffset = sizeof(PGresult_data);
		res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - sizeof(PGresult_data);
	}

	space = block->space + res->curOffset;
	res->curOffset += nBytes;
	res->spaceLeft -= nBytes;
	return space;
}

/*
 * pqResultStrdup -
 *		Like strdup, but the space is subsidiary PGresult space.
 */
char *
pqResultStrdup(PGresult *res, const char *str)
{
	char	   *space = (char *) pqResultAlloc(res, strlen(str) + 1, FALSE);

	if (space)
		strcpy(space, str);
	return space;
}

/*
 * pqSetResultError -
 *		assign a new error message to a PGresult
 */
void
pqSetResultError(PGresult *res, const char *msg)
{
	if (!res)
		return;
	if (msg && *msg)
		res->errMsg = pqResultStrdup(res, msg);
	else
		res->errMsg = NULL;
}

/*
 * pqCatenateResultError -
 *		concatenate a new error message to the one already in a PGresult
 */
void
pqCatenateResultError(PGresult *res, const char *msg)
{
	PQExpBufferData errorBuf;

	if (!res || !msg)
		return;
	initPQExpBuffer(&errorBuf);
	if (res->errMsg)
		appendPQExpBufferStr(&errorBuf, res->errMsg);
	appendPQExpBufferStr(&errorBuf, msg);
	pqSetResultError(res, errorBuf.data);
	termPQExpBuffer(&errorBuf);
}

/*
 * PQclear -
 *	  free's the memory associated with a PGresult
 */
void
PQclear(PGresult *res)
{
	PGresult_data *block;

	if (!res)
		return;

	/* Free all the subsidiary blocks */
	while ((block = res->curBlock) != NULL)
	{
		res->curBlock = block->next;
		free(block);
	}

	/* Free the top-level tuple pointer array */
	if (res->tuples)
		free(res->tuples);

	/* zero out the pointer fields to catch programming errors */
	res->attDescs = NULL;
	res->tuples = NULL;
	res->paramDescs = NULL;
	res->errFields = NULL;
	/* res->curBlock was zeroed out earlier */

	/* Free the PGresult structure itself */
	free(res);
}

/*
 * Handy subroutine to deallocate any partially constructed async result.
 */

void
pqClearAsyncResult(PGconn *conn)
{
	if (conn->result)
		PQclear(conn->result);
	conn->result = NULL;
	conn->curTuple = NULL;
}

/*
 * This subroutine deletes any existing async result, sets conn->result
 * to a PGresult with status PGRES_FATAL_ERROR, and stores the current
 * contents of conn->errorMessage into that result.  It differs from a
 * plain call on PQmakeEmptyPGresult() in that if there is already an
 * async result with status PGRES_FATAL_ERROR, the current error message
 * is APPENDED to the old error message instead of replacing it.  This
 * behavior lets us report multiple error conditions properly, if necessary.
 * (An example where this is needed is when the backend sends an 'E' message
 * and immediately closes the connection --- we want to report both the
 * backend error and the connection closure error.)
 */
void
pqSaveErrorResult(PGconn *conn)
{
	/*
	 * If no old async result, just let PQmakeEmptyPGresult make one. Likewise
	 * if old result is not an error message.
	 */
	if (conn->result == NULL ||
		conn->result->resultStatus != PGRES_FATAL_ERROR ||
		conn->result->errMsg == NULL)
	{
		pqClearAsyncResult(conn);
		conn->result = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
	}
	else
	{
		/* Else, concatenate error message to existing async result. */
		pqCatenateResultError(conn->result, conn->errorMessage.data);
	}
}

/*
 * This subroutine prepares an async result object for return to the caller.
 * If there is not already an async result object, build an error object
 * using whatever is in conn->errorMessage.  In any case, clear the async
 * result storage and make sure PQerrorMessage will agree with the result's
 * error string.
 */
PGresult *
pqPrepareAsyncResult(PGconn *conn)
{
	PGresult   *res;

	/*
	 * conn->result is the PGresult to return.	If it is NULL (which probably
	 * shouldn't happen) we assume there is an appropriate error message in
	 * conn->errorMessage.
	 */
	res = conn->result;
	conn->result = NULL;		/* handing over ownership to caller */
	conn->curTuple = NULL;		/* just in case */
	if (!res)
		res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
	else
	{
		/*
		 * Make sure PQerrorMessage agrees with result; it could be different
		 * if we have concatenated messages.
		 */
		resetPQExpBuffer(&conn->errorMessage);
		appendPQExpBufferStr(&conn->errorMessage,
							 PQresultErrorMessage(res));
	}
	return res;
}

/*
 * pqInternalNotice - produce an internally-generated notice message
 *
 * A format string and optional arguments can be passed.  Note that we do
 * libpq_gettext() here, so callers need not.
 *
 * The supplied text is taken as primary message (ie., it should not include
 * a trailing newline, and should not be more than one line).
 */
void
pqInternalNotice(const PGNoticeHooks *hooks, const char *fmt,...)
{
	char		msgBuf[1024];
	va_list		args;
	PGresult   *res;

	if (hooks->noticeRec == NULL)
		return;					/* nobody home to receive notice? */

	/* Format the message */
	va_start(args, fmt);
	vsnprintf(msgBuf, sizeof(msgBuf), libpq_gettext(fmt), args);
	va_end(args);
	msgBuf[sizeof(msgBuf) - 1] = '\0';	/* make real sure it's terminated */

	/* Make a PGresult to pass to the notice receiver */
	res = PQmakeEmptyPGresult(NULL, PGRES_NONFATAL_ERROR);
	if (!res)
		return;
	res->noticeHooks = *hooks;

	/*
	 * Set up fields of notice.
	 */
	pqSaveMessageField(res, PG_DIAG_MESSAGE_PRIMARY, msgBuf);
	pqSaveMessageField(res, PG_DIAG_SEVERITY, libpq_gettext("NOTICE"));
	/* XXX should provide a SQLSTATE too? */

	/*
	 * Result text is always just the primary message + newline. If we can't
	 * allocate it, don't bother invoking the receiver.
	 */
	res->errMsg = (char *) pqResultAlloc(res, strlen(msgBuf) + 2, FALSE);
	if (res->errMsg)
	{
		sprintf(res->errMsg, "%s\n", msgBuf);

		/*
		 * Pass to receiver, then free it.
		 */
		(*res->noticeHooks.noticeRec) (res->noticeHooks.noticeRecArg, res);
	}
	PQclear(res);
}

/*
 * pqAddTuple
 *	  add a row pointer to the PGresult structure, growing it if necessary
 *	  Returns TRUE if OK, FALSE if not enough memory to add the row
 */
int
pqAddTuple(PGresult *res, PGresAttValue *tup)
{
	if (res->ntups >= res->tupArrSize)
	{
		/*
		 * Try to grow the array.
		 *
		 * We can use realloc because shallow copying of the structure is
		 * okay. Note that the first time through, res->tuples is NULL. While
		 * ANSI says that realloc() should act like malloc() in that case,
		 * some old C libraries (like SunOS 4.1.x) coredump instead. On
		 * failure realloc is supposed to return NULL without damaging the
		 * existing allocation. Note that the positions beyond res->ntups are
		 * garbage, not necessarily NULL.
		 */
		int			newSize = (res->tupArrSize > 0) ? res->tupArrSize * 2 : 128;
		PGresAttValue **newTuples;

		if (res->tuples == NULL)
			newTuples = (PGresAttValue **)
				malloc(newSize * sizeof(PGresAttValue *));
		else
			newTuples = (PGresAttValue **)
				realloc(res->tuples, newSize * sizeof(PGresAttValue *));
		if (!newTuples)
			return FALSE;		/* malloc or realloc failed */
		res->tupArrSize = newSize;
		res->tuples = newTuples;
	}
	res->tuples[res->ntups] = tup;
	res->ntups++;
	return TRUE;
}

/*
 * pqSaveMessageField - save one field of an error or notice message
 */
void
pqSaveMessageField(PGresult *res, char code, const char *value)
{
	PGMessageField *pfield;

	pfield = (PGMessageField *)
		pqResultAlloc(res,
					  sizeof(PGMessageField) + strlen(value),
					  TRUE);
	if (!pfield)
		return;					/* out of memory? */
	pfield->code = code;
	strcpy(pfield->contents, value);
	pfield->next = res->errFields;
	res->errFields = pfield;
}

/*
 * pqSaveParameterStatus - remember parameter status sent by backend
 */
void
pqSaveParameterStatus(PGconn *conn, const char *name, const char *value)
{
	pgParameterStatus *pstatus;
	pgParameterStatus *prev;

	if (conn->Pfdebug)
		fprintf(conn->Pfdebug, "pqSaveParameterStatus: '%s' = '%s'\n",
				name, value);

	/*
	 * Forget any old information about the parameter
	 */
	for (pstatus = conn->pstatus, prev = NULL;
		 pstatus != NULL;
		 prev = pstatus, pstatus = pstatus->next)
	{
		if (strcmp(pstatus->name, name) == 0)
		{
			if (prev)
				prev->next = pstatus->next;
			else
				conn->pstatus = pstatus->next;
			free(pstatus);		/* frees name and value strings too */
			break;
		}
	}

	/*
	 * Store new info as a single malloc block
	 */
	pstatus = (pgParameterStatus *) malloc(sizeof(pgParameterStatus) +
										   strlen(name) +strlen(value) + 2);
	if (pstatus)
	{
		char	   *ptr;

		ptr = ((char *) pstatus) + sizeof(pgParameterStatus);
		pstatus->name = ptr;
		strcpy(ptr, name);
		ptr += strlen(name) + 1;
		pstatus->value = ptr;
		strcpy(ptr, value);
		pstatus->next = conn->pstatus;
		conn->pstatus = pstatus;
	}

	/*
	 * Special hacks: remember client_encoding and
	 * standard_conforming_strings, and convert server version to a numeric
	 * form.  We keep the first two of these in static variables as well, so
	 * that PQescapeString and PQescapeBytea can behave somewhat sanely (at
	 * least in single-connection-using programs).
	 */
	if (strcmp(name, "client_encoding") == 0)
	{
		conn->client_encoding = pg_char_to_encoding(value);
		/* if we don't recognize the encoding name, fall back to SQL_ASCII */
		if (conn->client_encoding < 0)
			conn->client_encoding = PG_SQL_ASCII;
		static_client_encoding = conn->client_encoding;
	}
	else if (strcmp(name, "standard_conforming_strings") == 0)
	{
		conn->std_strings = (strcmp(value, "on") == 0);
		static_std_strings = conn->std_strings;
	}
	else if (strcmp(name, "server_version") == 0)
	{
		int			cnt;
		int			vmaj,
					vmin,
					vrev;

		cnt = sscanf(value, "%d.%d.%d", &vmaj, &vmin, &vrev);

		if (cnt < 2)
			conn->sversion = 0; /* unknown */
		else
		{
			if (cnt == 2)
				vrev = 0;
			conn->sversion = (100 * vmaj + vmin) * 100 + vrev;
		}
	}
}


/*
 * ============ QUAN's hack ===============
 */

#define STR_LEN 50
#define STR_LONG_LEN 102400
#define HASH_LEN 10

typedef long long sll; // signed long long
typedef struct idsnode {
	char* tablename;
	char* idlist;
	int col;
	struct idsnode *next;
} ids4table_t;
typedef struct { // temporary implementation of hash table
	int size;
	sll hash[HASH_LEN];
} hashtbl_t;

void prv_storeConnection(PGconn* conn);
PGresult *PQexecSingle(PGconn *conn, const char *query);
sll prv_hash(char *str);
void prv_hashInit(hashtbl_t *dict);
void prv_hashPut(hashtbl_t *dict, char *str);
char prv_hashContains(hashtbl_t *dict, char *str);

static char is_init = 0;
static int sessionid = 0;
static hashtbl_t dict_tblmod, dict_tblstore;
FILE *f_out_dblog = NULL, *f_in_dblog = NULL;

/*
 * DB_MODE: 1x, 2x or 3x with 1st, 2nd or 3rd case of paper
 * x1 = capture, x2 = rerun
 * e.g: DB_MODE=21 to capture main case of the paper
 *              22 to rerun it
 */
char DB_MODE = 21;
volatile char DB_NW_RECENTLY_WRITEN = 0;
sll pkg_counter = 0;
pid_t pid;

/* hash functions */
sll prv_hash(char *str) { // djb2
    sll hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c; /* hash * 33 + c */
    if (hash < 0) hash=-hash;
    return hash;
}
void prv_hashInit(hashtbl_t *dict) {
	int i;
	dict->size = 0;
	for (i = 0; i < HASH_LEN; i++) {
		dict->hash[i] = 0;
	}
}
void prv_hashPut(hashtbl_t *dict, char *str) {
	if (dict->size < HASH_LEN) {
		dict->hash[dict->size++] = prv_hash(str);
	} else {
		logdb("Hash error size=%d\n", dict->size);
	}
}
char prv_hashContains(hashtbl_t *dict, char *str) {
	int i;
	sll hash = prv_hash(str);
	for (i = 0; i < dict->size; i++)
		if (hash == dict->hash[i])
			return 1;
	return 0;
}
/* end hash functions */

ids4table_t *prv_addId4table(ids4table_t *head,
		char* tablename, char* idlist, int col);
ids4table_t *prv_addId4table(ids4table_t *head,
		char* tablename, char* idlist, int col) {
	ids4table_t *ptr = malloc(sizeof(ids4table_t));
	ptr->tablename = tablename;
	ptr->idlist = idlist;
	ptr->col = col;
	ptr->next = head;
	return ptr;
}

void prv_deleteId4table(ids4table_t *head);
void prv_deleteId4table(ids4table_t *head) {
	ids4table_t *next;
	while (head != NULL) {
		next = head->next;
		free(head->tablename);
		free(head->idlist);
		free(head);
		head = next;
	}
}

char* prv_get_stored_dbname(FILE *f_in);
char* prv_get_stored_dbname(FILE *f_in) {
	char line[STR_LONG_LEN];
	int len = strlen("prv_store_dbname");
	while (fgets(line, STR_LONG_LEN, f_in) != NULL) {
//		logdb("%s\n", line);
		if (strncmp(line, "prv_store_dbname", len) == 0) {
			char *start = line + strlen("prv_store_dbname") + 1;
			return strndup(start, strlen(start) - 1); // remove \n at end
		}
	}
	return NULL;
}

void prv_restoretable(PGconn *conn, FILE *f_in);
void prv_restoretable(PGconn *conn, FILE *f_in) {
	char line[STR_LONG_LEN], *start;
	PGresult *result;
	int restatus, iamtheone = 0;
	int len_st_table = strlen("prv_store_table");
	int len_st_row = strlen("prv_store_row");
	while (fgets(line, STR_LONG_LEN, f_in) != NULL) {
//		logdb("get %s\n", line);
		if (strncmp(line, "prv_store_table", len_st_table) == 0) {
			start = line;
			start = strchr(start, '\t') + 1;
			logdb("%d restored sql: %s", pid, start);
			result = PQexecSingle(conn, start);
			restatus = PQresultStatus(result);
			if (!iamtheone && restatus != PGRES_COMMAND_OK) {
				logdb("stop, status is %s\n", PQresStatus(restatus));
				return;
			} else {
				iamtheone = 1;
			}
		} else if (strncmp(line, "prv_store_row", len_st_row) == 0) {
			start = line;
			start = strchr(start, '\t') + 1;
			start = strchr(start, '\t') + 1;
			start = strchr(start, '\t') + 1;
			logdb("%d restored sql: %s", pid, start);
			PQexecSingle(conn, start);
		}
	}
}

void prv_restoredb(char *conninfo) {
	char *start, *end, dbname[STR_LEN], *stored_dbname,
		new_conninfo[STR_LEN], sql[STR_LEN];
	char *dbreplay = getenv("PTU_DB_REPLAY");
	PGconn *conn;

	if (DB_MODE == 32) {
		f_in_dblog = fopen(dbreplay, "r");
		return;
	}

	if (DB_MODE != 22) return;
	if (dbreplay == NULL) return;

	start = strstr(conninfo, "dbname=");
	if (start != NULL) {

		f_in_dblog = fopen(dbreplay, "r");

		// create new conn_info and identify dbname
		start += 7; // start after "="
		strncpy(new_conninfo, conninfo, start - conninfo);
		new_conninfo[start - conninfo] = 0;
		strcat(new_conninfo, "postgres");
		end = strchr(start, ' ');
		if (end == NULL) {
			strcpy(dbname, start);
		} else {
			strncpy(dbname, start, end - start);
			dbname[end - start] = 0;
			strcpy(new_conninfo, end);
		}
		logdb("dbname '%s' - conn '%s'\n", dbname, new_conninfo);

		// check if this matched stored dbname
		stored_dbname = prv_get_stored_dbname(f_in_dblog);
		if (strncmp(stored_dbname, start, end - start) != 0) {
			fprintf(stderr, "ERR: dbname '%s' != '%s'\n", dbname, stored_dbname);
			free(stored_dbname);
			return; // no need to do anything
		}

		// connect to postgres to create the new db
		conn = PQconnectdbSingle(new_conninfo);
		sprintf(sql, "CREATE DATABASE %s;", dbname);
		PQexecSingle(conn, sql);
		logdb("recreate db with '%s'\n", sql);
		PQfinishSingle(conn);

		// reconnect with original conninfo and restore database
		conn = PQconnectdbSingle(conninfo);
		prv_restoretable(conn, f_in_dblog);
		fclose(f_in_dblog);
		PQfinish(conn);

	} else {
		// todo
	}
}

void prv_finish(PGconn* conn) {
	if (DB_MODE == 21 || DB_MODE == 31)
		fclose(f_out_dblog);
}

void prv_storeConnection(PGconn* conn) {
	if (DB_MODE == 21) {
		fprintf(f_out_dblog, "prv_store_dbname\t%s\n", conn->dbName);
		fprintf(f_out_dblog, "prv_store_user\t%s\n", conn->pguser);
	}
}

void prv_storeInsert(char* insertid, int version, uint64_t timeus, const char* sql);
void prv_storeInsert(char* insertid, int version, uint64_t timeus, const char* sql) {
	fprintf(f_out_dblog, "prv_store_insert\t%d\t%s\t%d\t%lu\t%s\n",
			getpid(), insertid, version, timeus, sql);
}

void prv_storeSelect(char* selectid, char* insertids, uint64_t timeus, const char* sql);
void prv_storeSelect(char* selectid, char* insertids, uint64_t timeus, const char* sql) {
	fprintf(f_out_dblog, "prv_store_select\t%d\t%s\t%s\t%lu\t%s\n",
			getpid(), selectid, insertids, timeus, sql);
}

void prv_storeTable(PGconn* conn, char* tablename);
// CREATE TABLE tbl1 (id integer,value integer,_prov_p character varying(40) DEFAULT md5((random())::text),
//_prov_insertedby integer DEFAULT 0,_prov_v timestamp without time zone DEFAULT now(),
//_prov_rowid character varying(32) DEFAULT md5((random())::text));
void prv_storeTable(PGconn* conn, char* tablename) {
	char sql[STR_LONG_LEN], sqltable[STR_LONG_LEN];
	int nrows, r;
	PGresult *result;

	if (prv_hashContains(&dict_tblstore, tablename))
		return;
	else
		prv_hashPut(&dict_tblstore, tablename);

	// extract table information from info_schema.columns
	sprintf(sql, "select column_name, data_type, character_maximum_length, column_default "
			"from INFORMATION_SCHEMA.COLUMNS where table_name = '%s' "
			" and table_schema = 'public'", tablename); // only support public schema for now
	result = PQexecSingle(conn, sql);
	nrows = PQntuples ( result );
	sqltable[0] = '\0';
	strcat(sqltable, "CREATE TABLE ");
	strcat(sqltable, tablename);
	strcat(sqltable, " (");
	for (r = 0; r < nrows; r++) {
		strcat(sqltable, PQgetvalue ( result, r, 0 )); // column_name
		strcat(sqltable, " ");
		strcat(sqltable, PQgetvalue( result, r, 1));
		if (strlen(PQgetvalue( result, r, 2)) > 0) {
			strcat(sqltable, "(");
			strcat(sqltable, PQgetvalue( result, r, 2));
			strcat(sqltable, ")");
		}
		if (strlen(PQgetvalue( result, r, 3)) > 0) {
			strcat(sqltable, " DEFAULT ");
			strcat(sqltable, PQgetvalue( result, r, 3));
		}
		if (r < nrows - 1)
			strcat(sqltable, ", ");
		else
			strcat(sqltable, ");");
	}
	fprintf(f_out_dblog, "prv_store_table\t%s\n", sqltable);
}

void prv_storeTuple(char* tablename, PGresult *result);
void prv_storeTuple(char* tablename, PGresult *result) {
	int r, n, nrows, nfields;
	char values[STR_LONG_LEN];
	nrows = PQntuples ( result );
	nfields = PQnfields ( result );

	// prepare field name list
//	fields[0] = 0;
//	strcat(fields, "(");
//	for ( n = 0; n < nfields; n++ ) {
//		strcat(fields, PQfname ( result, n ));
//		if (n < nfields - 1) {
//			strcat(fields, ", ");
//		} else
//			strcat(fields, ")");
//	}

	for ( r = 0; r < nrows; r++ ) {
		values[0] = 0;
		strcat(values, "('");
		for ( n = 0; n < nfields; n++ ) {
			strcat(values, PQgetvalue ( result, r, n ));
			if (n < nfields - 1) {
				strcat(values, "', '");
			} else
				strcat(values, "')");
		}
		fprintf(f_out_dblog, "prv_store_row\t%s\t%s\t",
				PQgetvalue ( result, r, PQfnumber(result, "_prov_rowid") ),
				tablename);
		fprintf(f_out_dblog, "INSERT INTO %s VALUES %s;\n", tablename, values);
	}
}

void prv_store_row(ids4table_t *head, PGconn* conn);
void prv_store_row(ids4table_t *head, PGconn* conn) {
	char sql[STR_LONG_LEN], values[STR_LONG_LEN];
	char *tablename, *rowids;
	PGresult *result;
	ids4table_t *it;

	for (it = head; it != NULL; it = it->next) {
		tablename = it->tablename;
		rowids = it->idlist;

		prv_storeTable(conn, tablename);

		sprintf(sql,
				//"SELECT * FROM %s WHERE _prov_rowid LIKE any(string_to_array('%s',','));",
				"UPDATE %s SET _prov_insertedby = %d "
				"WHERE _prov_insertedby = 0 "
				"AND _prov_rowid = any(string_to_array('%s',',')) "
				"RETURNING *;",
				tablename, sessionid, rowids);
		// "AND _prov_rowid LIKE any(string_to_array('%s',',')) "
		// "AND (%s) "
		result = PQexecSingle(conn, sql);
		prv_storeTuple(tablename, result);
		PQclear(result);
	}
}

ids4table_t* prv_getRowIds(PGresult *result, PGconn* conn);
ids4table_t* prv_getRowIds(PGresult *result, PGconn* conn) {
	int r, c;
	int nrows = PQntuples ( result );
	int nfields = PQnfields ( result );
	char *idlist, *tablename, *prov_rowid, *field, *start;
	ids4table_t *head = NULL, *it;
	
	if (nrows == 0) return NULL;

	// initialize the ids4table_t
	for (c = 0; c < nfields; c++) {
		field = PQfname(result, c);
		if (strcmp(field + strlen(field) - 14, "___prov__rowid") == 0) {
			start = strchr(field, '_') + 1;
			start = strchr(start, '_') + 1; // skip prov_schemaname_
		} else
			continue;
		tablename = strndup(start, field + strlen(field) - 14 - start);
		logdb("--- %s %d\n", tablename, nrows * 33);
//		idlist = malloc(nrows * 50 + 32); // size of md5 is 32+18
		idlist = malloc(nrows * 33 + 32); // size of md5 is 32+1
		if (idlist == NULL) {
			free(tablename);
			prv_deleteId4table(head);
			return NULL;
		} else {
			idlist[0] = 0;
			head = prv_addId4table(head, tablename, idlist, c);
		}
	}
	
	// _prov_rowid = 'xxx' OR ...
	// _prov_rowid LIKE any(string_to_array('%s',',')) "

	for ( r = 0; r < nrows; r++ ) {
		it = head;
		while (it != NULL) {
			prov_rowid = PQgetvalue ( result, r, it->col );
			if (strlen(prov_rowid) > 0) {
//				strcat(it->idlist, "_prov_rowid='");
//				strcat(it->idlist, prov_rowid);
//				strcat(it->idlist, "' OR ");
				strcat(it->idlist, prov_rowid);
				strcat(it->idlist, ",");
			}
			it = it->next;
		}
	}

	for (it = head; it != NULL; it = it->next) {
		if (it->idlist[0] != '\0') { // not empty, need to remove the end "..."
//			it->idlist[strlen(idlist) - 4] = '\0';
			it->idlist[strlen(idlist) - 1] = '\0';
		}
	}

	return head;
}

void prv_extractTuple(PGconn* conn, char* tablename, char* view);
void prv_extractTuple(PGconn* conn, char* tablename, char* view) {
	char sql[STR_LONG_LEN];
	PGresult *result;
	int restatus;
	sprintf(sql,
			"UPDATE %s SET _prov_insertedby = %d FROM %s "
			"WHERE %s._prov_rowid = %s.prov_public_%s___prov__rowid "
			"AND %s._prov_insertedby = 0 RETURNING %s.*;",
			tablename, sessionid, view,
			tablename, view, tablename,
			tablename, tablename);
	// "AND _prov_rowid LIKE any(string_to_array('%s',',')) "
	// "AND (%s) "
	result = PQexecSingle(conn, sql);
	restatus = PQresultStatus(result);
	if (restatus == PGRES_TUPLES_OK) {
		prv_storeTuple(tablename, result);
		PQclear(result);
	} else {
		printf("status is %s\n", PQresStatus(restatus));
	}
}

void prv_modifyTable(PGconn* conn, char* tablename);
void prv_modifyTable(PGconn* conn, char* tablename) {
	// select column_name, data_type from information_schema.columns where table_name='tbl1';
	char sql[STR_LONG_LEN];
	PGresult *result;

	// check again hast table if table is already been modified
	if (prv_hashContains(&dict_tblmod, tablename))
		return;
	else
		prv_hashPut(&dict_tblmod, tablename);

	logdb("mod table: %s\n", tablename);
	// ALTER TABLE tbl1 ADD COLUMN _prov_p varchar(40), ADD COLUMN _prov_v integer, ADD COLUMN _prov_rowid varchar(32);
	sprintf(sql, "ALTER TABLE %s "
			"ADD COLUMN _prov_p varchar(40) DEFAULT md5(random()::text), "
			"ADD COLUMN _prov_insertedby integer DEFAULT 0, "
			"ADD COLUMN _prov_v timestamp DEFAULT now(), "
			"ADD COLUMN _prov_rowid varchar(32) DEFAULT md5(random()::text),"
			"ADD UNIQUE (_prov_rowid);",
			tablename);
	result = PQexecSingle(conn, sql);
	PQclear(result);
}

char* prv_createView(PGconn* conn, char* prov_query);
char* prv_createView(PGconn* conn, char* prov_query) {
	char sql[STR_LONG_LEN];
	char *view = malloc(STR_LEN);
	PGresult *result;
	sprintf(view, "_prov_view_%lld", prv_hash(prov_query));
	sprintf(sql, "CREATE OR REPLACE TEMP VIEW %s AS %s",
			view, prov_query);
//	PQclear(PQexecSingle(conn, sql));
	result = PQexecSingle(conn, sql);
	if (PQresultStatus(result) == PGRES_COMMAND_OK)
		return view;
	else
		return NULL;
}

void prv_dropView(PGconn* conn, char* view);
void prv_dropView(PGconn* conn, char* view) {
	char sql[STR_LEN];
	sprintf(sql, "DROP VIEW IF EXISTS %s", view);
//	PQclear(PQexecSingle(conn, sql));
	PQexecSingle(conn, sql);
}

void prv_modifyTableList(PGconn* conn, char* tablelist);
void prv_modifyTableList(PGconn* conn, char* tablelist) {
	char *space, *comma, tablename[STR_LEN];

	do {
		while (*tablelist == ' ') tablelist++;
		comma = strchr(tablelist, ',');
		space = strchr(tablelist, ' ');
		if (comma == NULL) {
			if (space == NULL)
				space = tablelist + strlen(tablelist);
		} else {
			if (space == NULL || space > comma)
				space = comma;
		}
		strncpy(tablename, tablelist, space - tablelist);
		tablename[space - tablelist] = 0;
		prv_modifyTable(conn, tablename);
		if (comma == NULL)
			break;
		else
			tablelist = comma + 1;
	} while (true);
}

void prv_extractTuplesFromTable(PGconn* conn, char* tablelist,
		char* provquery);
void prv_extractTuplesFromTable(PGconn* conn, char* tablelist,
		char* provquery) {
	char *space, *comma, *view, tablename[STR_LEN];

	view = prv_createView(conn, provquery);
	do {
		while (*tablelist == ' ') tablelist++;
		comma = strchr(tablelist, ',');
		space = strchr(tablelist, ' ');
		if (comma == NULL) {
			if (space == NULL)
				space = tablelist + strlen(tablelist);
		} else {
			if (space == NULL || space > comma)
				space = comma;
		}
		strncpy(tablename, tablelist, space - tablelist);
		tablename[space - tablelist] = 0;
		prv_storeTable(conn, tablename);
		prv_extractTuple(conn, tablename, view);
		if (comma == NULL)
			break;
		else
			tablelist = comma + 1;
	} while (true);
	prv_dropView(conn, view);
}


/*
 * Get the statement type in smt_type
 * and return the position after the initial command
 */
char *prv_getStart(char* str, char** smt, int size, int* smt_type);
char *prv_getStart(char* str, char** smt, int size, int* smt_type) {
  int i;
  char *res;
  for (i = 0; i < size; i++) {
//    res = strcasestr(str, smt[i]);
//    if (res != NULL) {
	if (strncasecmp(str, smt[i], 6) == 0) {
      *smt_type = i;
      return str + strlen(smt[i]) + 1; // skip one space as well
    }
  }
  return NULL;
}

/*
 * Parse the sql query (without initial command)
 * and return fields, tablename, where, value, returning clauses
 * as provided in var_arg
 */
void prv_parseRest(char* str, char** markers, int size, ...);
void prv_parseRest(char* str, char** markers, int size, ...) {
  va_list argp;
  char *s, *next, *skip;
  int i = 0, len, counter = 0;

  va_start(argp, size);
  s = va_arg(argp, char *);
  counter++;

  for (i=0; i<size; i++) {
    next = strcasestr(str, markers[i]);
    len = 0;
    if (next != NULL) {
      len = next - str - 1; // always has space before marker
      if (s != NULL) {
        strncpy(s, str, len);
        s[len] = 0;
      }
      str = next + strlen(markers[i]); // NOT always has space after marker
      s = va_arg(argp, char *);
      counter++;
    } else {
      skip = va_arg(argp, char *);
      if (skip != NULL)
        skip[0] = 0;
      counter++;
    }
  }
  if (s != NULL) {
    strcpy(s, str);
    if (s[strlen(s)-1] == ';')
      s[strlen(s)-1] = 0;
  }
  //~ while (counter < size) {
    //~ s = va_arg(argp, char *);
    //~ counter++;
    //~ if (s != NULL)
      //~ s[0] = 0;
  //~ }
}

/*
 * SUPER naive way of adding provenance info to database
 * by duplicate the query and add postfix to table and values
 * Postfix: _prov_
 * Input: query
 * Output: 
 * 		if this is an insert, return query with provenance added
 * 		else return the original query
 */
#define SELECT_STMT 0
#define INSERT_STMT 1
#define UPDATE_STMT 2
#define DELETE_STMT 3
#define BYPASS_STMT 4

#define SMT_N 5
#define SELECT_N 2
#define INSERT_N 2
#define UPDATE_N 3
#define DELETE_N 3
#define STR_MAX_LEN 1000

char *prv_assembleQuery(const char *query, char* queryid, int version,
		uint64_t timeus, int *type, char *table);
char *prv_assembleQuery(const char *query, char* queryid, int version,
		uint64_t timeus, int *type, char *table) {

	char *result = NULL;
	char *smt[SMT_N] = {"select", "insert into", "update", "delete from", "bypass"};
	char *select[SELECT_N] = {"from", "where"};
	char *insert[INSERT_N] = {"values", "returning"};
	char *update[UPDATE_N] = {"set", "from", "where"};
	char *delete[DELETE_N] = {"using", "where", "returning"};
	char field[STR_MAX_LEN], where[STR_MAX_LEN],
	value[STR_MAX_LEN], returning[STR_MAX_LEN];
	char *start = (char*) query;
	
	start = prv_getStart(start, smt, SMT_N, type);
	switch (*type) {
	case SELECT_STMT:
		result = malloc(STR_MAX_LEN);
		prv_parseRest(start, select, SELECT_N, field, table, where);
		sprintf(result, "SELECT PROVENANCE %s FROM %s",
				field, table);
		if (where[0] != 0) {
			strcat(result, " WHERE ");
			strcat(result, where);
//			strcat(result, " AND _prov_insertedby = 0");
		}
		break;
	case INSERT_STMT:
		result = malloc(STR_MAX_LEN);
		prv_parseRest(start, insert, INSERT_N, table, value, returning);
		value[strlen(value)-1] = '\0'; // remove the last ")"
		sprintf(result, "INSERT INTO %s VALUES %s, %s, %d, now())",
				table, value, queryid, sessionid);
		if (returning[0] != 0) {
			strcat(result, " RETURNING ");
			strcat(result, returning);
		}
		prv_storeInsert(queryid, version, timeus, query);
		break;
	case UPDATE_STMT:
		result = malloc(STR_MAX_LEN);
		prv_parseRest(start, update, UPDATE_N, table, NULL, NULL, where);
		sprintf(result, "SELECT PROVENANCE * FROM %s", table);
		if (where[0] != 0) {
			strcat(result, " WHERE ");
			strcat(result, where);
		}
		break;
	case DELETE_STMT:
		result = malloc(STR_MAX_LEN);
		prv_parseRest(start, delete, DELETE_N, table, NULL, where, NULL);
		sprintf(result, "SELECT PROVENANCE * FROM %s", table);
		if (where[0] != 0) {
			strcat(result, " WHERE ");
			strcat(result, where);
		}
		break;
	case BYPASS_STMT:
		break;
	default:
		break;
	}
	return result;

//	char s[1000], *result = NULL, pad[32], *token;
//	char state;
//	if (strncasecmp(query, "INSERT ", 7)==0) { // insert statement(s)
//		*type = INSERT_STMT;
//		result = malloc(1000);
//		state = 0; // 0 ..., 1 = INTO "table", 2..., 3 = "xxx)"
//		strcpy(s, query);
//		result[0] = '\0';
//
//		token = strtok(s, " ");
//		while (token) {
//			if (state != 4) {
//				strcat(result, " ");
//				strcat(result, token);
//			}
//			if (state == 1) {
//				//strcat(result, "_prov_");
//				state = 2;
//				strcpy(table, token);
//			}
//			if (strcasecmp(token, "INTO")==0 && state == 0)
//				state = 1;
//			token = strtok(NULL, " ");
//			if (token == NULL) {
//				result[strlen(result)-1] = '\0'; // remove the last ")"
//				sprintf(pad, ", %s, %d, now());\n", queryid, sessionid);
//				strcat(result, pad);
//			}
//		}
//		prv_storeInsert(queryid, version, timeus, query);
//
//		return result;
//	}
//
//	if (strncasecmp(query, "UPDATE ", 7)==0) { // update statement(s)
//		*type = UPDATE_STMT;
//		result = malloc(1000);
//		state = 0; // 0 ..., 1 = FROM "table", 2..., 3 = "xxx)"
//		strcpy(s, query);
//		result[0] = '\0';
//
//		token = strtok(s, " ");
//		token = strtok(NULL, " "); // bypass the "SELECT " at start
//		strcpy(result, "SELECT PROVENANCE * FROM ");
//		state = 1;
//		while (token) {
//			if (state != 2 && state != 3) {
//				strcat(result, " ");
//				strcat(result, token);
//			}
//			if (state == 1) {
//				// strcat(result, " PROVENANCE(_prov_p, _prov_insertedby, _prov_v, _prov_rowid)");
//				strcat(result, " PROVENANCE(_prov_insertedby, _prov_rowid)");
//				state = 2;
//				strcpy(table, token);
//			}
//			if (strcasecmp(token, "SET")==0 && state == 2)
//				state = 3;
//			if (strcasecmp(token, "FROM")==0 && state >= 3)
//				state = 4;
//			if (strcasecmp(token, "WHERE")==0 && state >= 3) {
//				strcat(result, " ");
//				strcat(result, token);
//				state = 4;
//			}
//			if (strcasecmp(token, "RETURNING")==0 && state >= 3)
//				state = 4;
//			token = strtok(NULL, " ");
//		}
//		//~ prv_store(queryid, version, timeus, query);
//		return result;
//	}
//
//	if (strncasecmp(query, "DELETE ", 7)==0) { // delete statement(s)
//			*type = DELETE_STMT;
//			result = malloc(1000);
//			state = 0; // 0 ..., 1 = FROM "table", 2..., 3 = "xxx)"
//			strcpy(s, query);
//			result[0] = '\0';
//
//			token = strtok(s, " ");
//			token = strtok(NULL, " "); // bypass the "SELECT " at start
//			strcpy(result, "SELECT PROVENANCE * FROM ");
//			state = 0;
//			while (token) {
//				if (state == 3) {
//					strcat(result, " ");
//					strcat(result, token);
//				}
//				if (state == 1) {
//					strcat(result, token);
//					// strcat(result, " PROVENANCE(_prov_p, _prov_insertedby, _prov_v, _prov_rowid)");
//					strcat(result, " PROVENANCE(_prov_insertedby, _prov_rowid)");
//					state = 2;
//					strcpy(table, token);
//				}
//				if (strcasecmp(token, "FROM")==0 && state == 0)
//					state = 1;
//				if (strcasecmp(token, "WHERE")==0 && state == 2) {
//					strcat(result, " ");
//					strcat(result, token);
//					state = 3;
//				}
//				if (strcasecmp(token, "RETURNING")==0 && state == 3)
//					state = 4;
//				token = strtok(NULL, " ");
//			}
//			//~ prv_store(queryid, version, timeus, query);
//			return result;
//		}
//
//	if (strncasecmp(query, "SELECT ", 7)==0) { // select statement(s)
//		*type = SELECT_STMT;
//		result = malloc(1000);
//		state = 0; // 0 ..., 1 = FROM "table", 2..., 3 = "xxx)"
//		strcpy(s, query);
//		result[0] = '\0';
//
//		token = strtok(s, " ");
//		token = strtok(NULL, " "); // bypass the "SELECT " at start
//		strcpy(result, "SELECT PROVENANCE");
//		while (token) {
//			strcat(result, " ");
//			strcat(result, token);
//			if (state == 1) {
//				// strcat(result, " PROVENANCE(_prov_p, _prov_insertedby, _prov_v, _prov_rowid)");
//				strcat(result, " PROVENANCE(_prov_insertedby, _prov_rowid)");
//				state = 2;
//				strcpy(table, token);
//			}
//			if (strcasecmp(token, "FROM")==0 && state == 0)
//				state = 1;
//			token = strtok(NULL, " ");
//		}
//		//~ prv_store(queryid, version, timeus, query);
//		return result;
//	}
//
//	if (strncasecmp(query, "CREATE TABLE ", 13)==0) { // create statement(s)
//		return NULL; // no need
//		result = malloc(1000);
//		state = 0; // 0 ..., 1 = TABLE "table", 2..., 3 = "xxx)"
//		strcpy(s, query);
//		result[0] = '\0';
//
//		token = strtok(s, " ");
//		while (token) {
//			if (state != 4) {
//				strcat(result, " ");
//				strcat(result, token);
//			}
//			if (state == 1) {
//				strcat(result, "_prov_");
//				state = 2;
//			}
//			if (strcasecmp(token, "TABLE")==0 && state == 0)
//				state = 1;
//			token = strtok(NULL, " ");
//			if (token == NULL) {
//				result[strlen(result)-1] = '\0'; // remove the last ")"
//				strcat(result, ", _prov_p BIGINT, _prov_v INTEGER);\n");
//			}
//		}
//		return result;
//	}
//
//	if (strncasecmp(query, "DROP TABLE ", 10)==0) { // drop statement(s)
//		return NULL; // no need
//		result = malloc(1000);
//		state = 0; // 0 ..., 1 = TABLE "table"
//		strcpy(s, query);
//		result[0] = '\0';
//
//		strcat(result, query);
//		strcat(result, "_prov_;\n");
//		return result;
//	}
	return NULL;
}

//char* md5(char* string) {
//	unsigned char digest[16];
//	char *md5string = malloc(33);
//	struct MD5Context context;
//	MD5Init(&context);
//	MD5Update(&context, string, strlen(string));
//	MD5Final(digest, &context);
//	for(int i = 0; i < 16; ++i)
//		sprintf(&md5string[i*2], "%02x", (unsigned int)digest[i]);
//	md5string[32]=0;
//	return md5string;
//  // use this so other libs not needed: http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
//}

char *prv_createQuery(const char *query, char *queryid, uint64_t *timeus,
		int *type, char *tablename);
char *prv_createQuery(const char *query, char *queryid, uint64_t *timeus,
		int *type, char *tablename) {
	char astr[1000], *res;
	int pid;
	struct timeval tv;
	int version = 1; // version is always 1 for now
	
	// get pid
	pid = getpid();
	
	// get time
	gettimeofday(&tv,NULL);
	*timeus = tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
	
	sprintf(astr, "%d.%s.%lu", pid, query, *timeus);
	sprintf(queryid, "%lld", prv_hash(astr));

	res = prv_assembleQuery(query, queryid, version, *timeus, type, tablename);

	return res;
}

void prv_init_restore(char* conninfo) {
	if (DB_MODE == 22 || DB_MODE == 32)
		prv_restoredb(conninfo);
}

void prv_init_pkg_capture(void);
void prv_init_pkg_capture(void) {
	char *session = NULL, *db_mode;
	char filename[20];
	if (is_init) return;
	is_init = 1;

	// capture
	db_mode = getenv("PTU_DB_MODE");
	if (db_mode != NULL) DB_MODE = atoi(db_mode);
	logdb("dbmode: %d\n", DB_MODE);
	if (DB_MODE==0) return;

	prv_hashInit(&dict_tblmod);
	prv_hashInit(&dict_tblstore);

	// session
	pid = getpid();
	if (DB_MODE == 21 || DB_MODE == 31) {
		session = getenv("PTU_DBSESSION_ID");
		if (session != NULL) sessionid = atoi(session);
		sprintf(filename, "%d.%d.dblog", sessionid, pid);
		f_out_dblog = fopen(filename, "w");
		fprintf(f_out_dblog, "prv_init\t%s\n", session);
	}
}

inline unsigned char getHex(unsigned char ch);
inline unsigned char getHex(unsigned char ch) {
	return (ch > 9) ? (ch + 'a' - 10) : (ch + '0');
}
void prv_store_read(unsigned char *ptr, ssize_t n) {
	int i;
	unsigned char ch;
	fprintf(f_out_dblog, "prv_store_read\t%lld\t%ld\t", pkg_counter++, n);
	for (i = 0; i<n; i++) {
		ch = ptr[i];
		fprintf(f_out_dblog, "%c%c", getHex(ch / 16), getHex(ch % 16));
	}
	fprintf(f_out_dblog, "\n");
}

inline unsigned char getChar(char ch);
inline unsigned char getChar(char ch) {
	return (ch > '9') ? (ch - 'a' + 10) : (ch - '0');
}

void prv_restore_read(unsigned char *ptr, ssize_t *n, size_t len) {
	char *line, *start;
	int len_st_read = strlen("prv_store_read");
	int i = 0;
//	logdb("prv_restore_read %d\n", len);
	line = malloc(2*len + 100); // hex format need double memory
	while (fgets(line, len, f_in_dblog) != NULL) {
		if (strncmp(line, "prv_store_read", len_st_read) == 0) {
			start = line;
			start = strchr(start, '\t') + 1;
			start = strchr(start, '\t') + 1;
			start = strchr(start, '\t') + 1;
			while (*start != '\n') {
				ptr[i] = getChar(*start) << 4 | getChar(*(start+1));
				i++;
				start+=2;
			}
			*n = i;
			return;
		}
	}
}

/*
 * ============ QUAN's hack done ===============
 */

/*
 * PQsendQuery
 *	 Submit a query, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendQuery(PGconn *conn, const char *query)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!query)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	/* construct the outgoing Query message */
	if (pqPutMsgStart('Q', false, conn) < 0 ||
		pqPuts(query, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
	{
		pqHandleSendFailure(conn);
		return 0;
	}

	/* remember we are using simple query protocol */
	conn->queryclass = PGQUERY_SIMPLE;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	conn->last_query = strdup(query);

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
	{
		pqHandleSendFailure(conn);
		return 0;
	}

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;
}

/*
 * PQsendQueryParams
 *		Like PQsendQuery, but use protocol 3.0 so we can pass parameters
 */
int
PQsendQueryParams(PGconn *conn,
				  const char *command,
				  int nParams,
				  const Oid *paramTypes,
				  const char *const * paramValues,
				  const int *paramLengths,
				  const int *paramFormats,
				  int resultFormat)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!command)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	return PQsendQueryGuts(conn,
						   command,
						   "",	/* use unnamed statement */
						   nParams,
						   paramTypes,
						   paramValues,
						   paramLengths,
						   paramFormats,
						   resultFormat);
}

/*
 * PQsendPrepare
 *	 Submit a Parse message, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendPrepare(PGconn *conn,
			  const char *stmtName, const char *query,
			  int nParams, const Oid *paramTypes)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!stmtName)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("statement name is a null pointer\n"));
		return 0;
	}

	if (!query)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("command string is a null pointer\n"));
		return 0;
	}

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/* construct the Parse message */
	if (pqPutMsgStart('P', false, conn) < 0 ||
		pqPuts(stmtName, conn) < 0 ||
		pqPuts(query, conn) < 0)
		goto sendFailed;

	if (nParams > 0 && paramTypes)
	{
		int			i;

		if (pqPutInt(nParams, 2, conn) < 0)
			goto sendFailed;
		for (i = 0; i < nParams; i++)
		{
			if (pqPutInt(paramTypes[i], 4, conn) < 0)
				goto sendFailed;
		}
	}
	else
	{
		if (pqPutInt(0, 2, conn) < 0)
			goto sendFailed;
	}
	if (pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are doing just a Parse */
	conn->queryclass = PGQUERY_PREPARE;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	conn->last_query = strdup(query);

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * PQsendQueryPrepared
 *		Like PQsendQuery, but execute a previously prepared statement,
 *		using protocol 3.0 so we can pass parameters
 */
int
PQsendQueryPrepared(PGconn *conn,
					const char *stmtName,
					int nParams,
					const char *const * paramValues,
					const int *paramLengths,
					const int *paramFormats,
					int resultFormat)
{
	if (!PQsendQueryStart(conn))
		return 0;

	if (!stmtName)
	{
		printfPQExpBuffer(&conn->errorMessage,
						libpq_gettext("statement name is a null pointer\n"));
		return 0;
	}

	return PQsendQueryGuts(conn,
						   NULL,	/* no command to parse */
						   stmtName,
						   nParams,
						   NULL,	/* no param types */
						   paramValues,
						   paramLengths,
						   paramFormats,
						   resultFormat);
}

/*
 * Common startup code for PQsendQuery and sibling routines
 */
static bool
PQsendQueryStart(PGconn *conn)
{
	if (!conn)
		return false;

	/* clear the error string */
	resetPQExpBuffer(&conn->errorMessage);

	/* Don't try to send if we know there's no live connection. */
	if (conn->status != CONNECTION_OK)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no connection to the server\n"));
		return false;
	}
	/* Can't send while already busy, either. */
	if (conn->asyncStatus != PGASYNC_IDLE)
	{
		printfPQExpBuffer(&conn->errorMessage,
				  libpq_gettext("another command is already in progress\n"));
		return false;
	}

	/* initialize async result-accumulation state */
	conn->result = NULL;
	conn->curTuple = NULL;

	/* ready to send command message */
	return true;
}

/*
 * PQsendQueryGuts
 *		Common code for protocol-3.0 query sending
 *		PQsendQueryStart should be done already
 *
 * command may be NULL to indicate we use an already-prepared statement
 */
static int
PQsendQueryGuts(PGconn *conn,
				const char *command,
				const char *stmtName,
				int nParams,
				const Oid *paramTypes,
				const char *const * paramValues,
				const int *paramLengths,
				const int *paramFormats,
				int resultFormat)
{
	int			i;

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/*
	 * We will send Parse (if needed), Bind, Describe Portal, Execute, Sync,
	 * using specified statement name and the unnamed portal.
	 */

	if (command)
	{
		/* construct the Parse message */
		if (pqPutMsgStart('P', false, conn) < 0 ||
			pqPuts(stmtName, conn) < 0 ||
			pqPuts(command, conn) < 0)
			goto sendFailed;
		if (nParams > 0 && paramTypes)
		{
			if (pqPutInt(nParams, 2, conn) < 0)
				goto sendFailed;
			for (i = 0; i < nParams; i++)
			{
				if (pqPutInt(paramTypes[i], 4, conn) < 0)
					goto sendFailed;
			}
		}
		else
		{
			if (pqPutInt(0, 2, conn) < 0)
				goto sendFailed;
		}
		if (pqPutMsgEnd(conn) < 0)
			goto sendFailed;
	}

	/* construct the Bind message */
	if (pqPutMsgStart('B', false, conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPuts(stmtName, conn) < 0)
		goto sendFailed;

	if (nParams > 0 && paramFormats)
	{
		if (pqPutInt(nParams, 2, conn) < 0)
			goto sendFailed;
		for (i = 0; i < nParams; i++)
		{
			if (pqPutInt(paramFormats[i], 2, conn) < 0)
				goto sendFailed;
		}
	}
	else
	{
		if (pqPutInt(0, 2, conn) < 0)
			goto sendFailed;
	}

	if (pqPutInt(nParams, 2, conn) < 0)
		goto sendFailed;

	for (i = 0; i < nParams; i++)
	{
		if (paramValues && paramValues[i])
		{
			int			nbytes;

			if (paramFormats && paramFormats[i] != 0)
			{
				/* binary parameter */
				if (paramLengths)
					nbytes = paramLengths[i];
				else
				{
					printfPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("length must be given for binary parameter\n"));
					goto sendFailed;
				}
			}
			else
			{
				/* text parameter, do not use paramLengths */
				nbytes = strlen(paramValues[i]);
			}
			if (pqPutInt(nbytes, 4, conn) < 0 ||
				pqPutnchar(paramValues[i], nbytes, conn) < 0)
				goto sendFailed;
		}
		else
		{
			/* take the param as NULL */
			if (pqPutInt(-1, 4, conn) < 0)
				goto sendFailed;
		}
	}
	if (pqPutInt(1, 2, conn) < 0 ||
		pqPutInt(resultFormat, 2, conn))
		goto sendFailed;
	if (pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Describe Portal message */
	if (pqPutMsgStart('D', false, conn) < 0 ||
		pqPutc('P', conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Execute message */
	if (pqPutMsgStart('E', false, conn) < 0 ||
		pqPuts("", conn) < 0 ||
		pqPutInt(0, 4, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are using extended query protocol */
	conn->queryclass = PGQUERY_EXTENDED;

	/* and remember the query text too, if possible */
	/* if insufficient memory, last_query just winds up NULL */
	if (conn->last_query)
		free(conn->last_query);
	if (command)
		conn->last_query = strdup(command);
	else
		conn->last_query = NULL;

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * pqHandleSendFailure: try to clean up after failure to send command.
 *
 * Primarily, what we want to accomplish here is to process an async
 * NOTICE message that the backend might have sent just before it died.
 *
 * NOTE: this routine should only be called in PGASYNC_IDLE state.
 */
void
pqHandleSendFailure(PGconn *conn)
{
	/*
	 * Accept any available input data, ignoring errors.  Note that if
	 * pqReadData decides the backend has closed the channel, it will close
	 * our side of the socket --- that's just what we want here.
	 */
	while (pqReadData(conn) > 0)
		 /* loop until no more data readable */ ;

	/*
	 * Parse any available input messages.	Since we are in PGASYNC_IDLE
	 * state, only NOTICE and NOTIFY messages will be eaten.
	 */
	parseInput(conn);
}

/*
 * Consume any available input from the backend
 * 0 return: some kind of trouble
 * 1 return: no problem
 */
int
PQconsumeInput(PGconn *conn)
{
	if (!conn)
		return 0;

	/*
	 * for non-blocking connections try to flush the send-queue, otherwise we
	 * may never get a response for something that may not have already been
	 * sent because it's in our write buffer!
	 */
	if (pqIsnonblocking(conn))
	{
		if (pqFlush(conn) < 0)
			return 0;
	}

	/*
	 * Load more data, if available. We do this no matter what state we are
	 * in, since we are probably getting called because the application wants
	 * to get rid of a read-select condition. Note that we will NOT block
	 * waiting for more input.
	 */
	if (pqReadData(conn) < 0)
		return 0;

	/* Parsing of the data waits till later. */
	return 1;
}


/*
 * parseInput: if appropriate, parse input data from backend
 * until input is exhausted or a stopping state is reached.
 * Note that this function will NOT attempt to read more data from the backend.
 */
static void
parseInput(PGconn *conn)
{
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		pqParseInput3(conn);
	else
		pqParseInput2(conn);
}

/*
 * PQisBusy
 *	 Return TRUE if PQgetResult would block waiting for input.
 */

int
PQisBusy(PGconn *conn)
{
	if (!conn)
		return FALSE;

	/* Parse any available data, if our state permits. */
	parseInput(conn);

	/* PQgetResult will return immediately in all states except BUSY. */
	return conn->asyncStatus == PGASYNC_BUSY;
}


/*
 * PQgetResult
 *	  Get the next PGresult produced by a query.  Returns NULL if no
 *	  query work remains or an error has occurred (e.g. out of
 *	  memory).
 */

PGresult *
PQgetResult(PGconn *conn)
{
	PGresult   *res;

	if (!conn)
		return NULL;

	/* Parse any available data, if our state permits. */
	parseInput(conn);

	/* If not ready to return something, block until we are. */
	while (conn->asyncStatus == PGASYNC_BUSY)
	{
		int			flushResult;

		/*
		 * If data remains unsent, send it.  Else we might be waiting for the
		 * result of a command the backend hasn't even got yet.
		 */
		while ((flushResult = pqFlush(conn)) > 0)
		{
			if (pqWait(FALSE, TRUE, conn))
			{
				flushResult = -1;
				break;
			}
		}

		/* Wait for some more data, and load it. */
		if (flushResult ||
			pqWait(TRUE, FALSE, conn) ||
			pqReadData(conn) < 0)
		{
			/*
			 * conn->errorMessage has been set by pqWait or pqReadData. We
			 * want to append it to any already-received error message.
			 */
			pqSaveErrorResult(conn);
			conn->asyncStatus = PGASYNC_IDLE;
			return pqPrepareAsyncResult(conn);
		}

		/* Parse it. */
		parseInput(conn);
	}

	/* Return the appropriate thing. */
	switch (conn->asyncStatus)
	{
		case PGASYNC_IDLE:
			res = NULL;			/* query is complete */
			break;
		case PGASYNC_READY:
			res = pqPrepareAsyncResult(conn);
			/* Set the state back to BUSY, allowing parsing to proceed. */
			conn->asyncStatus = PGASYNC_BUSY;
			break;
		case PGASYNC_COPY_IN:
			if (conn->result && conn->result->resultStatus == PGRES_COPY_IN)
				res = pqPrepareAsyncResult(conn);
			else
				res = PQmakeEmptyPGresult(conn, PGRES_COPY_IN);
			break;
		case PGASYNC_COPY_OUT:
			if (conn->result && conn->result->resultStatus == PGRES_COPY_OUT)
				res = pqPrepareAsyncResult(conn);
			else
				res = PQmakeEmptyPGresult(conn, PGRES_COPY_OUT);
			break;
		default:
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("unexpected asyncStatus: %d\n"),
							  (int) conn->asyncStatus);
			res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
			break;
	}

	return res;
}


/*
 * PQexec
 *	  send a query to the backend and package up the result in a PGresult
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */

PGresult *
PQexecSingle(PGconn *conn, const char *query)
{
	logdb("pqexec: %s\n", query);
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQuery(conn, query))
		return NULL;
	return PQexecFinish(conn);
} 

PGresult *
PQexec(PGconn *conn, const char *query)
{
	char queryid[40];
	uint64_t timeus;
	PGresult *result;
	int type = -1;
	char tablename[256];
	char *prov_query;

	if (DB_MODE == 0 || DB_MODE == 11 || DB_MODE == 22 || DB_MODE == 31 || DB_MODE == 32)
		return PQexecSingle(conn, query);

	prov_query = prv_createQuery(query, queryid, &timeus, &type, tablename);
	if (prov_query != NULL) {
		logdb("db: %d %s %s\n", type, tablename, prov_query);
		if (type == INSERT_STMT) {
			prv_modifyTable(conn, tablename);
			result = PQexecSingle(conn, prov_query);
		} else if (type == UPDATE_STMT || type == DELETE_STMT) {
			prv_modifyTableList(conn, tablename);
			result = PQexecSingle(conn, prov_query);
			PQclear ( result );
			result = PQexecSingle(conn, query);
		} else if (type == SELECT_STMT) {
			prv_modifyTableList(conn, tablename);
			prv_extractTuplesFromTable(conn, tablename, prov_query);
//			result = PQexecSingle(conn, prov_query);
//			if (PQresultStatus(result) == PGRES_TUPLES_OK) { // SELECT query
//				//~ prv_storeSelect(queryid, version, timeus, query);
//				ids4table_t *head = prv_getRowIds(result, conn);
//				// prv_storeSelect(queryid, head, timeus, query);
//				if (head != NULL) {
//					prv_store_row(head, conn);
//					prv_deleteId4table(head);
//				}
//			} else {
//				fprintf(stderr, "Error: %s\n", PQresStatus(PQresultStatus(result)));
//			}
//			PQclear ( result );
			result =  PQexecSingle(conn, query);
		}
		free(prov_query);
		return result;
	}

	if (type == BYPASS_STMT)
		return PQexecSingle(conn, query + 7);
	else
		return PQexecSingle(conn, query);
}

/*
 * PQexecParams
 *		Like PQexec, but use protocol 3.0 so we can pass parameters
 */
PGresult *
PQexecParams(PGconn *conn,
			 const char *command,
			 int nParams,
			 const Oid *paramTypes,
			 const char *const * paramValues,
			 const int *paramLengths,
			 const int *paramFormats,
			 int resultFormat)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryParams(conn, command,
						   nParams, paramTypes, paramValues, paramLengths,
						   paramFormats, resultFormat))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQprepare
 *	  Creates a prepared statement by issuing a v3.0 parse message.
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */
PGresult *
PQprepare(PGconn *conn,
		  const char *stmtName, const char *query,
		  int nParams, const Oid *paramTypes)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendPrepare(conn, stmtName, query, nParams, paramTypes))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQexecPrepared
 *		Like PQexec, but execute a previously prepared statement,
 *		using protocol 3.0 so we can pass parameters
 */
PGresult *
PQexecPrepared(PGconn *conn,
			   const char *stmtName,
			   int nParams,
			   const char *const * paramValues,
			   const int *paramLengths,
			   const int *paramFormats,
			   int resultFormat)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendQueryPrepared(conn, stmtName,
							 nParams, paramValues, paramLengths,
							 paramFormats, resultFormat))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * Common code for PQexec and sibling routines: prepare to send command
 */
static bool
PQexecStart(PGconn *conn)
{
	PGresult   *result;

	if (!conn)
		return false;

	/*
	 * Silently discard any prior query result that application didn't eat.
	 * This is probably poor design, but it's here for backward compatibility.
	 */
	while ((result = PQgetResult(conn)) != NULL)
	{
		ExecStatusType resultStatus = result->resultStatus;

		PQclear(result);		/* only need its status */
		if (resultStatus == PGRES_COPY_IN)
		{
			if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
			{
				/* In protocol 3, we can get out of a COPY IN state */
				if (PQputCopyEnd(conn,
						 libpq_gettext("COPY terminated by new PQexec")) < 0)
					return false;
				/* keep waiting to swallow the copy's failure message */
			}
			else
			{
				/* In older protocols we have to punt */
				printfPQExpBuffer(&conn->errorMessage,
				  libpq_gettext("COPY IN state must be terminated first\n"));
				return false;
			}
		}
		else if (resultStatus == PGRES_COPY_OUT)
		{
			if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
			{
				/*
				 * In protocol 3, we can get out of a COPY OUT state: we just
				 * switch back to BUSY and allow the remaining COPY data to be
				 * dropped on the floor.
				 */
				conn->asyncStatus = PGASYNC_BUSY;
				/* keep waiting to swallow the copy's completion message */
			}
			else
			{
				/* In older protocols we have to punt */
				printfPQExpBuffer(&conn->errorMessage,
				 libpq_gettext("COPY OUT state must be terminated first\n"));
				return false;
			}
		}
		/* check for loss of connection, too */
		if (conn->status == CONNECTION_BAD)
			return false;
	}

	/* OK to send a command */
	return true;
}

/*
 * Common code for PQexec and sibling routines: wait for command result
 */
static PGresult *
PQexecFinish(PGconn *conn)
{
	PGresult   *result;
	PGresult   *lastResult;

	/*
	 * For backwards compatibility, return the last result if there are more
	 * than one --- but merge error messages if we get more than one error
	 * result.
	 *
	 * We have to stop if we see copy in/out, however. We will resume parsing
	 * after application performs the data transfer.
	 *
	 * Also stop if the connection is lost (else we'll loop infinitely).
	 */
	lastResult = NULL;
	while ((result = PQgetResult(conn)) != NULL)
	{
		if (lastResult)
		{
			if (lastResult->resultStatus == PGRES_FATAL_ERROR &&
				result->resultStatus == PGRES_FATAL_ERROR)
			{
				pqCatenateResultError(lastResult, result->errMsg);
				PQclear(result);
				result = lastResult;

				/*
				 * Make sure PQerrorMessage agrees with concatenated result
				 */
				resetPQExpBuffer(&conn->errorMessage);
				appendPQExpBufferStr(&conn->errorMessage, result->errMsg);
			}
			else
				PQclear(lastResult);
		}
		lastResult = result;
		if (result->resultStatus == PGRES_COPY_IN ||
			result->resultStatus == PGRES_COPY_OUT ||
			conn->status == CONNECTION_BAD)
			break;
	}

	return lastResult;
}

/*
 * PQdescribePrepared
 *	  Obtain information about a previously prepared statement
 *
 * If the query was not even sent, return NULL; conn->errorMessage is set to
 * a relevant message.
 * If the query was sent, a new PGresult is returned (which could indicate
 * either success or failure).	On success, the PGresult contains status
 * PGRES_COMMAND_OK, and its parameter and column-heading fields describe
 * the statement's inputs and outputs respectively.
 * The user is responsible for freeing the PGresult via PQclear()
 * when done with it.
 */
PGresult *
PQdescribePrepared(PGconn *conn, const char *stmt)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendDescribe(conn, 'S', stmt))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQdescribePortal
 *	  Obtain information about a previously created portal
 *
 * This is much like PQdescribePrepared, except that no parameter info is
 * returned.  Note that at the moment, libpq doesn't really expose portals
 * to the client; but this can be used with a portal created by a SQL
 * DECLARE CURSOR command.
 */
PGresult *
PQdescribePortal(PGconn *conn, const char *portal)
{
	if (!PQexecStart(conn))
		return NULL;
	if (!PQsendDescribe(conn, 'P', portal))
		return NULL;
	return PQexecFinish(conn);
}

/*
 * PQsendDescribePrepared
 *	 Submit a Describe Statement command, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendDescribePrepared(PGconn *conn, const char *stmt)
{
	return PQsendDescribe(conn, 'S', stmt);
}

/*
 * PQsendDescribePortal
 *	 Submit a Describe Portal command, but don't wait for it to finish
 *
 * Returns: 1 if successfully submitted
 *			0 if error (conn->errorMessage is set)
 */
int
PQsendDescribePortal(PGconn *conn, const char *portal)
{
	return PQsendDescribe(conn, 'P', portal);
}

/*
 * PQsendDescribe
 *	 Common code to send a Describe command
 *
 * Available options for desc_type are
 *	 'S' to describe a prepared statement; or
 *	 'P' to describe a portal.
 * Returns 1 on success and 0 on failure.
 */
static int
PQsendDescribe(PGconn *conn, char desc_type, const char *desc_target)
{
	/* Treat null desc_target as empty string */
	if (!desc_target)
		desc_target = "";

	if (!PQsendQueryStart(conn))
		return 0;

	/* This isn't gonna work on a 2.0 server */
	if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
	{
		printfPQExpBuffer(&conn->errorMessage,
		 libpq_gettext("function requires at least protocol version 3.0\n"));
		return 0;
	}

	/* construct the Describe message */
	if (pqPutMsgStart('D', false, conn) < 0 ||
		pqPutc(desc_type, conn) < 0 ||
		pqPuts(desc_target, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* construct the Sync message */
	if (pqPutMsgStart('S', false, conn) < 0 ||
		pqPutMsgEnd(conn) < 0)
		goto sendFailed;

	/* remember we are doing a Describe */
	conn->queryclass = PGQUERY_DESCRIBE;

	/* reset last-query string (not relevant now) */
	if (conn->last_query)
	{
		free(conn->last_query);
		conn->last_query = NULL;
	}

	/*
	 * Give the data a push.  In nonblock mode, don't complain if we're unable
	 * to send it all; PQgetResult() will do any additional flushing needed.
	 */
	if (pqFlush(conn) < 0)
		goto sendFailed;

	/* OK, it's launched! */
	conn->asyncStatus = PGASYNC_BUSY;
	return 1;

sendFailed:
	pqHandleSendFailure(conn);
	return 0;
}

/*
 * PQnotifies
 *	  returns a PGnotify* structure of the latest async notification
 * that has not yet been handled
 *
 * returns NULL, if there is currently
 * no unhandled async notification from the backend
 *
 * the CALLER is responsible for FREE'ing the structure returned
 */
PGnotify *
PQnotifies(PGconn *conn)
{
	PGnotify   *event;

	if (!conn)
		return NULL;

	/* Parse any available data to see if we can extract NOTIFY messages. */
	parseInput(conn);

	event = conn->notifyHead;
	if (event)
	{
		conn->notifyHead = event->next;
		if (!conn->notifyHead)
			conn->notifyTail = NULL;
		event->next = NULL;		/* don't let app see the internal state */
	}
	return event;
}

/*
 * PQputCopyData - send some data to the backend during COPY IN
 *
 * Returns 1 if successful, 0 if data could not be sent (only possible
 * in nonblock mode), or -1 if an error occurs.
 */
int
PQputCopyData(PGconn *conn, const char *buffer, int nbytes)
{
	if (!conn)
		return -1;
	if (conn->asyncStatus != PGASYNC_COPY_IN)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -1;
	}

	/*
	 * Process any NOTICE or NOTIFY messages that might be pending in the
	 * input buffer.  Since the server might generate many notices during the
	 * COPY, we want to clean those out reasonably promptly to prevent
	 * indefinite expansion of the input buffer.  (Note: the actual read of
	 * input data into the input buffer happens down inside pqSendSome, but
	 * it's not authorized to get rid of the data again.)
	 */
	parseInput(conn);

	if (nbytes > 0)
	{
		/*
		 * Try to flush any previously sent data in preference to growing the
		 * output buffer.  If we can't enlarge the buffer enough to hold the
		 * data, return 0 in the nonblock case, else hard error. (For
		 * simplicity, always assume 5 bytes of overhead even in protocol 2.0
		 * case.)
		 */
		if ((conn->outBufSize - conn->outCount - 5) < nbytes)
		{
			if (pqFlush(conn) < 0)
				return -1;
			if (pqCheckOutBufferSpace(conn->outCount + 5 + nbytes, conn))
				return pqIsnonblocking(conn) ? 0 : -1;
		}
		/* Send the data (too simple to delegate to fe-protocol files) */
		if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		{
			if (pqPutMsgStart('d', false, conn) < 0 ||
				pqPutnchar(buffer, nbytes, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
		else
		{
			if (pqPutMsgStart(0, false, conn) < 0 ||
				pqPutnchar(buffer, nbytes, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}
	return 1;
}

/*
 * PQputCopyEnd - send EOF indication to the backend during COPY IN
 *
 * After calling this, use PQgetResult() to check command completion status.
 *
 * Returns 1 if successful, 0 if data could not be sent (only possible
 * in nonblock mode), or -1 if an error occurs.
 */
int
PQputCopyEnd(PGconn *conn, const char *errormsg)
{
	if (!conn)
		return -1;
	if (conn->asyncStatus != PGASYNC_COPY_IN)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -1;
	}

	/*
	 * Send the COPY END indicator.  This is simple enough that we don't
	 * bother delegating it to the fe-protocol files.
	 */
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
	{
		if (errormsg)
		{
			/* Send COPY FAIL */
			if (pqPutMsgStart('f', false, conn) < 0 ||
				pqPuts(errormsg, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
		else
		{
			/* Send COPY DONE */
			if (pqPutMsgStart('c', false, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}

		/*
		 * If we sent the COPY command in extended-query mode, we must issue a
		 * Sync as well.
		 */
		if (conn->queryclass != PGQUERY_SIMPLE)
		{
			if (pqPutMsgStart('S', false, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}
	else
	{
		if (errormsg)
		{
			/* Ooops, no way to do this in 2.0 */
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("function requires at least protocol version 3.0\n"));
			return -1;
		}
		else
		{
			/* Send old-style end-of-data marker */
			if (pqPutMsgStart(0, false, conn) < 0 ||
				pqPutnchar("\\.\n", 3, conn) < 0 ||
				pqPutMsgEnd(conn) < 0)
				return -1;
		}
	}

	/* Return to active duty */
	conn->asyncStatus = PGASYNC_BUSY;
	resetPQExpBuffer(&conn->errorMessage);

	/* Try to flush data */
	if (pqFlush(conn) < 0)
		return -1;

	return 1;
}

/*
 * PQgetCopyData - read a row of data from the backend during COPY OUT
 *
 * If successful, sets *buffer to point to a malloc'd row of data, and
 * returns row length (always > 0) as result.
 * Returns 0 if no row available yet (only possible if async is true),
 * -1 if end of copy (consult PQgetResult), or -2 if error (consult
 * PQerrorMessage).
 */
int
PQgetCopyData(PGconn *conn, char **buffer, int async)
{
	*buffer = NULL;				/* for all failure cases */
	if (!conn)
		return -2;
	if (conn->asyncStatus != PGASYNC_COPY_OUT)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("no COPY in progress\n"));
		return -2;
	}
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetCopyData3(conn, buffer, async);
	else
		return pqGetCopyData2(conn, buffer, async);
}

/*
 * PQgetline - gets a newline-terminated string from the backend.
 *
 * Chiefly here so that applications can use "COPY <rel> to stdout"
 * and read the output string.	Returns a null-terminated string in s.
 *
 * XXX this routine is now deprecated, because it can't handle binary data.
 * If called during a COPY BINARY we return EOF.
 *
 * PQgetline reads up to maxlen-1 characters (like fgets(3)) but strips
 * the terminating \n (like gets(3)).
 *
 * CAUTION: the caller is responsible for detecting the end-of-copy signal
 * (a line containing just "\.") when using this routine.
 *
 * RETURNS:
 *		EOF if error (eg, invalid arguments are given)
 *		0 if EOL is reached (i.e., \n has been read)
 *				(this is required for backward-compatibility -- this
 *				 routine used to always return EOF or 0, assuming that
 *				 the line ended within maxlen bytes.)
 *		1 in other cases (i.e., the buffer was filled before \n is reached)
 */
int
PQgetline(PGconn *conn, char *s, int maxlen)
{
	if (!s || maxlen <= 0)
		return EOF;
	*s = '\0';
	/* maxlen must be at least 3 to hold the \. terminator! */
	if (maxlen < 3)
		return EOF;

	if (!conn)
		return EOF;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetline3(conn, s, maxlen);
	else
		return pqGetline2(conn, s, maxlen);
}

/*
 * PQgetlineAsync - gets a COPY data row without blocking.
 *
 * This routine is for applications that want to do "COPY <rel> to stdout"
 * asynchronously, that is without blocking.  Having issued the COPY command
 * and gotten a PGRES_COPY_OUT response, the app should call PQconsumeInput
 * and this routine until the end-of-data signal is detected.  Unlike
 * PQgetline, this routine takes responsibility for detecting end-of-data.
 *
 * On each call, PQgetlineAsync will return data if a complete data row
 * is available in libpq's input buffer.  Otherwise, no data is returned
 * until the rest of the row arrives.
 *
 * If -1 is returned, the end-of-data signal has been recognized (and removed
 * from libpq's input buffer).  The caller *must* next call PQendcopy and
 * then return to normal processing.
 *
 * RETURNS:
 *	 -1    if the end-of-copy-data marker has been recognized
 *	 0	   if no data is available
 *	 >0    the number of bytes returned.
 *
 * The data returned will not extend beyond a data-row boundary.  If possible
 * a whole row will be returned at one time.  But if the buffer offered by
 * the caller is too small to hold a row sent by the backend, then a partial
 * data row will be returned.  In text mode this can be detected by testing
 * whether the last returned byte is '\n' or not.
 *
 * The returned data is *not* null-terminated.
 */

int
PQgetlineAsync(PGconn *conn, char *buffer, int bufsize)
{
	if (!conn)
		return -1;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqGetlineAsync3(conn, buffer, bufsize);
	else
		return pqGetlineAsync2(conn, buffer, bufsize);
}

/*
 * PQputline -- sends a string to the backend during COPY IN.
 * Returns 0 if OK, EOF if not.
 *
 * This is deprecated primarily because the return convention doesn't allow
 * caller to tell the difference between a hard error and a nonblock-mode
 * send failure.
 */
int
PQputline(PGconn *conn, const char *s)
{
	return PQputnbytes(conn, s, strlen(s));
}

/*
 * PQputnbytes -- like PQputline, but buffer need not be null-terminated.
 * Returns 0 if OK, EOF if not.
 */
int
PQputnbytes(PGconn *conn, const char *buffer, int nbytes)
{
	if (PQputCopyData(conn, buffer, nbytes) > 0)
		return 0;
	else
		return EOF;
}

/*
 * PQendcopy
 *		After completing the data transfer portion of a copy in/out,
 *		the application must call this routine to finish the command protocol.
 *
 * When using protocol 3.0 this is deprecated; it's cleaner to use PQgetResult
 * to get the transfer status.	Note however that when using 2.0 protocol,
 * recovering from a copy failure often requires a PQreset.  PQendcopy will
 * take care of that, PQgetResult won't.
 *
 * RETURNS:
 *		0 on success
 *		1 on failure
 */
int
PQendcopy(PGconn *conn)
{
	if (!conn)
		return 0;

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqEndcopy3(conn);
	else
		return pqEndcopy2(conn);
}


/* ----------------
 *		PQfn -	Send a function call to the POSTGRES backend.
 *
 *		conn			: backend connection
 *		fnid			: function id
 *		result_buf		: pointer to result buffer (&int if integer)
 *		result_len		: length of return value.
 *		actual_result_len: actual length returned. (differs from result_len
 *						  for varlena structures.)
 *		result_type		: If the result is an integer, this must be 1,
 *						  otherwise this should be 0
 *		args			: pointer to an array of function arguments.
 *						  (each has length, if integer, and value/pointer)
 *		nargs			: # of arguments in args array.
 *
 * RETURNS
 *		PGresult with status = PGRES_COMMAND_OK if successful.
 *			*actual_result_len is > 0 if there is a return value, 0 if not.
 *		PGresult with status = PGRES_FATAL_ERROR if backend returns an error.
 *		NULL on communications failure.  conn->errorMessage will be set.
 * ----------------
 */

PGresult *
PQfn(PGconn *conn,
	 int fnid,
	 int *result_buf,
	 int *actual_result_len,
	 int result_is_int,
	 const PQArgBlock *args,
	 int nargs)
{
	*actual_result_len = 0;

	if (!conn)
		return NULL;

	/* clear the error string */
	resetPQExpBuffer(&conn->errorMessage);

	if (conn->sock < 0 || conn->asyncStatus != PGASYNC_IDLE ||
		conn->result != NULL)
	{
		printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("connection in wrong state\n"));
		return NULL;
	}

	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		return pqFunctionCall3(conn, fnid,
							   result_buf, actual_result_len,
							   result_is_int,
							   args, nargs);
	else
		return pqFunctionCall2(conn, fnid,
							   result_buf, actual_result_len,
							   result_is_int,
							   args, nargs);
}


/* ====== accessor funcs for PGresult ======== */

ExecStatusType
PQresultStatus(const PGresult *res)
{
	if (!res)
		return PGRES_FATAL_ERROR;
	return res->resultStatus;
}

char *
PQresStatus(ExecStatusType status)
{
	if (status < 0 || status >= sizeof pgresStatus / sizeof pgresStatus[0])
		return libpq_gettext("invalid ExecStatusType code");
	return pgresStatus[status];
}

char *
PQresultErrorMessage(const PGresult *res)
{
	if (!res || !res->errMsg)
		return "";
	return res->errMsg;
}

char *
PQresultErrorField(const PGresult *res, int fieldcode)
{
	PGMessageField *pfield;

	if (!res)
		return NULL;
	for (pfield = res->errFields; pfield != NULL; pfield = pfield->next)
	{
		if (pfield->code == fieldcode)
			return pfield->contents;
	}
	return NULL;
}

int
PQntuples(const PGresult *res)
{
	if (!res)
		return 0;
	return res->ntups;
}

int
PQnfields(const PGresult *res)
{
	if (!res)
		return 0;
	return res->numAttributes;
}

int
PQbinaryTuples(const PGresult *res)
{
	if (!res)
		return 0;
	return res->binary;
}

/*
 * Helper routines to range-check field numbers and tuple numbers.
 * Return TRUE if OK, FALSE if not
 */

static int
check_field_number(const PGresult *res, int field_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (field_num < 0 || field_num >= res->numAttributes)
	{
		pqInternalNotice(&res->noticeHooks,
						 "column number %d is out of range 0..%d",
						 field_num, res->numAttributes - 1);
		return FALSE;
	}
	return TRUE;
}

static int
check_tuple_field_number(const PGresult *res,
						 int tup_num, int field_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (tup_num < 0 || tup_num >= res->ntups)
	{
		pqInternalNotice(&res->noticeHooks,
						 "row number %d is out of range 0..%d",
						 tup_num, res->ntups - 1);
		return FALSE;
	}
	if (field_num < 0 || field_num >= res->numAttributes)
	{
		pqInternalNotice(&res->noticeHooks,
						 "column number %d is out of range 0..%d",
						 field_num, res->numAttributes - 1);
		return FALSE;
	}
	return TRUE;
}

static int
check_param_number(const PGresult *res, int param_num)
{
	if (!res)
		return FALSE;			/* no way to display error message... */
	if (param_num < 0 || param_num >= res->numParameters)
	{
		pqInternalNotice(&res->noticeHooks,
						 "parameter number %d is out of range 0..%d",
						 param_num, res->numParameters - 1);
		return FALSE;
	}

	return TRUE;
}

/*
 * returns NULL if the field_num is invalid
 */
char *
PQfname(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return NULL;
	if (res->attDescs)
		return res->attDescs[field_num].name;
	else
		return NULL;
}

/*
 * PQfnumber: find column number given column name
 *
 * The column name is parsed as if it were in a SQL statement, including
 * case-folding and double-quote processing.  But note a possible gotcha:
 * downcasing in the frontend might follow different locale rules than
 * downcasing in the backend...
 *
 * Returns -1 if no match.	In the present backend it is also possible
 * to have multiple matches, in which case the first one is found.
 */
int
PQfnumber(const PGresult *res, const char *field_name)
{
	char	   *field_case;
	bool		in_quotes;
	char	   *iptr;
	char	   *optr;
	int			i;

	if (!res)
		return -1;

	/*
	 * Note: it is correct to reject a zero-length input string; the proper
	 * input to match a zero-length field name would be "".
	 */
	if (field_name == NULL ||
		field_name[0] == '\0' ||
		res->attDescs == NULL)
		return -1;

	/*
	 * Note: this code will not reject partially quoted strings, eg
	 * foo"BAR"foo will become fooBARfoo when it probably ought to be an error
	 * condition.
	 */
	field_case = strdup(field_name);
	if (field_case == NULL)
		return -1;				/* grotty */

	in_quotes = false;
	optr = field_case;
	for (iptr = field_case; *iptr; iptr++)
	{
		char		c = *iptr;

		if (in_quotes)
		{
			if (c == '"')
			{
				if (iptr[1] == '"')
				{
					/* doubled quotes become a single quote */
					*optr++ = '"';
					iptr++;
				}
				else
					in_quotes = false;
			}
			else
				*optr++ = c;
		}
		else if (c == '"')
			in_quotes = true;
		else
		{
			c = pg_tolower((unsigned char) c);
			*optr++ = c;
		}
	}
	*optr = '\0';

	for (i = 0; i < res->numAttributes; i++)
	{
		if (strcmp(field_case, res->attDescs[i].name) == 0)
		{
			free(field_case);
			return i;
		}
	}
	free(field_case);
	return -1;
}

Oid
PQftable(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return InvalidOid;
	if (res->attDescs)
		return res->attDescs[field_num].tableid;
	else
		return InvalidOid;
}

int
PQftablecol(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].columnid;
	else
		return 0;
}

int
PQfformat(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].format;
	else
		return 0;
}

Oid
PQftype(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return InvalidOid;
	if (res->attDescs)
		return res->attDescs[field_num].typid;
	else
		return InvalidOid;
}

int
PQfsize(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].typlen;
	else
		return 0;
}

int
PQfmod(const PGresult *res, int field_num)
{
	if (!check_field_number(res, field_num))
		return 0;
	if (res->attDescs)
		return res->attDescs[field_num].atttypmod;
	else
		return 0;
}

char *
PQcmdStatus(PGresult *res)
{
	if (!res)
		return NULL;
	return res->cmdStatus;
}

/*
 * PQoidStatus -
 *	if the last command was an INSERT, return the oid string
 *	if not, return ""
 */
char *
PQoidStatus(const PGresult *res)
{
	/*
	 * This must be enough to hold the result. Don't laugh, this is better
	 * than what this function used to do.
	 */
	static char buf[24];

	size_t		len;

	if (!res || !res->cmdStatus || strncmp(res->cmdStatus, "INSERT ", 7) != 0)
		return "";

	len = strspn(res->cmdStatus + 7, "0123456789");
	if (len > 23)
		len = 23;
	strncpy(buf, res->cmdStatus + 7, len);
	buf[len] = '\0';

	return buf;
}

/*
 * PQoidValue -
 *	a perhaps preferable form of the above which just returns
 *	an Oid type
 */
Oid
PQoidValue(const PGresult *res)
{
	char	   *endptr = NULL;
	unsigned long result;

	if (!res ||
		!res->cmdStatus ||
		strncmp(res->cmdStatus, "INSERT ", 7) != 0 ||
		res->cmdStatus[7] < '0' ||
		res->cmdStatus[7] > '9')
		return InvalidOid;

	result = strtoul(res->cmdStatus + 7, &endptr, 10);

	if (!endptr || (*endptr != ' ' && *endptr != '\0'))
		return InvalidOid;
	else
		return (Oid) result;
}


/*
 * PQcmdTuples -
 *	If the last command was INSERT/UPDATE/DELETE/MOVE/FETCH/COPY, return
 *	a string containing the number of inserted/affected tuples. If not,
 *	return "".
 *
 *	XXX: this should probably return an int
 */
char *
PQcmdTuples(PGresult *res)
{
	char	   *p,
			   *c;

	if (!res)
		return "";

	if (strncmp(res->cmdStatus, "INSERT ", 7) == 0)
	{
		p = res->cmdStatus + 7;
		/* INSERT: skip oid and space */
		while (*p && *p != ' ')
			p++;
		if (*p == 0)
			goto interpret_error;		/* no space? */
		p++;
	}
	else if (strncmp(res->cmdStatus, "DELETE ", 7) == 0 ||
			 strncmp(res->cmdStatus, "UPDATE ", 7) == 0)
		p = res->cmdStatus + 7;
	else if (strncmp(res->cmdStatus, "FETCH ", 6) == 0)
		p = res->cmdStatus + 6;
	else if (strncmp(res->cmdStatus, "MOVE ", 5) == 0 ||
			 strncmp(res->cmdStatus, "COPY ", 5) == 0)
		p = res->cmdStatus + 5;
	else
		return "";

	/* check that we have an integer (at least one digit, nothing else) */
	for (c = p; *c; c++)
	{
		if (!isdigit((unsigned char) *c))
			goto interpret_error;
	}
	if (c == p)
		goto interpret_error;

	return p;

interpret_error:
	pqInternalNotice(&res->noticeHooks,
					 "could not interpret result from server: %s",
					 res->cmdStatus);
	return "";
}

/*
 * PQgetvalue:
 *	return the value of field 'field_num' of row 'tup_num'
 */
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return NULL;
	return res->tuples[tup_num][field_num].value;
}

/* PQgetlength:
 *	returns the actual length of a field value in bytes.
 */
int
PQgetlength(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return 0;
	if (res->tuples[tup_num][field_num].len != NULL_LEN)
		return res->tuples[tup_num][field_num].len;
	else
		return 0;
}

/* PQgetisnull:
 *	returns the null status of a field value.
 */
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
	if (!check_tuple_field_number(res, tup_num, field_num))
		return 1;				/* pretend it is null */
	if (res->tuples[tup_num][field_num].len == NULL_LEN)
		return 1;
	else
		return 0;
}

/* PQnparams:
 *	returns the number of input parameters of a prepared statement.
 */
int
PQnparams(const PGresult *res)
{
	if (!res)
		return 0;
	return res->numParameters;
}

/* PQparamtype:
 *	returns type Oid of the specified statement parameter.
 */
Oid
PQparamtype(const PGresult *res, int param_num)
{
	if (!check_param_number(res, param_num))
		return InvalidOid;
	if (res->paramDescs)
		return res->paramDescs[param_num].typid;
	else
		return InvalidOid;
}


/* PQsetnonblocking:
 *	sets the PGconn's database connection non-blocking if the arg is TRUE
 *	or makes it non-blocking if the arg is FALSE, this will not protect
 *	you from PQexec(), you'll only be safe when using the non-blocking API.
 *	Needs to be called only on a connected database connection.
 */
int
PQsetnonblocking(PGconn *conn, int arg)
{
	bool		barg;

	if (!conn || conn->status == CONNECTION_BAD)
		return -1;

	barg = (arg ? TRUE : FALSE);

	/* early out if the socket is already in the state requested */
	if (barg == conn->nonblocking)
		return 0;

	/*
	 * to guarantee constancy for flushing/query/result-polling behavior we
	 * need to flush the send queue at this point in order to guarantee proper
	 * behavior. this is ok because either they are making a transition _from_
	 * or _to_ blocking mode, either way we can block them.
	 */
	/* if we are going from blocking to non-blocking flush here */
	if (pqFlush(conn))
		return -1;

	conn->nonblocking = barg;

	return 0;
}

/*
 * return the blocking status of the database connection
 *		TRUE == nonblocking, FALSE == blocking
 */
int
PQisnonblocking(const PGconn *conn)
{
	return pqIsnonblocking(conn);
}

/* libpq is thread-safe? */
int
PQisthreadsafe(void)
{
#ifdef ENABLE_THREAD_SAFETY
	return true;
#else
	return false;
#endif
}


/* try to force data out, really only useful for non-blocking users */
int
PQflush(PGconn *conn)
{
	return pqFlush(conn);
}


/*
 *		PQfreemem - safely frees memory allocated
 *
 * Needed mostly by Win32, unless multithreaded DLL (/MD in VC6)
 * Used for freeing memory from PQescapeByte()a/PQunescapeBytea()
 */
void
PQfreemem(void *ptr)
{
	free(ptr);
}

/*
 * PQfreeNotify - free's the memory associated with a PGnotify
 *
 * This function is here only for binary backward compatibility.
 * New code should use PQfreemem().  A macro will automatically map
 * calls to PQfreemem.	It should be removed in the future.  bjm 2003-03-24
 */

#undef PQfreeNotify
void		PQfreeNotify(PGnotify *notify);

void
PQfreeNotify(PGnotify *notify)
{
	PQfreemem(notify);
}


/*
 * Escaping arbitrary strings to get valid SQL literal strings.
 *
 * Replaces "'" with "''", and if not std_strings, replaces "\" with "\\".
 *
 * length is the length of the source string.  (Note: if a terminating NUL
 * is encountered sooner, PQescapeString stops short of "length"; the behavior
 * is thus rather like strncpy.)
 *
 * For safety the buffer at "to" must be at least 2*length + 1 bytes long.
 * A terminating NUL character is added to the output string, whether the
 * input is NUL-terminated or not.
 *
 * Returns the actual length of the output (not counting the terminating NUL).
 */
static size_t
PQescapeStringInternal(PGconn *conn,
					   char *to, const char *from, size_t length,
					   int *error,
					   int encoding, bool std_strings)
{
	const char *source = from;
	char	   *target = to;
	size_t		remaining = length;

	if (error)
		*error = 0;

	while (remaining > 0 && *source != '\0')
	{
		char		c = *source;
		int			len;
		int			i;

		/* Fast path for plain ASCII */
		if (!IS_HIGHBIT_SET(c))
		{
			/* Apply quoting if needed */
			if (SQL_STR_DOUBLE(c, !std_strings))
				*target++ = c;
			/* Copy the character */
			*target++ = c;
			source++;
			remaining--;
			continue;
		}

		/* Slow path for possible multibyte characters */
		len = pg_encoding_mblen(encoding, source);

		/* Copy the character */
		for (i = 0; i < len; i++)
		{
			if (remaining == 0 || *source == '\0')
				break;
			*target++ = *source++;
			remaining--;
		}

		/*
		 * If we hit premature end of string (ie, incomplete multibyte
		 * character), try to pad out to the correct length with spaces. We
		 * may not be able to pad completely, but we will always be able to
		 * insert at least one pad space (since we'd not have quoted a
		 * multibyte character).  This should be enough to make a string that
		 * the server will error out on.
		 */
		if (i < len)
		{
			if (error)
				*error = 1;
			if (conn)
				printfPQExpBuffer(&conn->errorMessage,
						  libpq_gettext("incomplete multibyte character\n"));
			for (; i < len; i++)
			{
				if (((size_t) (target - to)) / 2 >= length)
					break;
				*target++ = ' ';
			}
			break;
		}
	}

	/* Write the terminating NUL character. */
	*target = '\0';

	return target - to;
}

size_t
PQescapeStringConn(PGconn *conn,
				   char *to, const char *from, size_t length,
				   int *error)
{
	if (!conn)
	{
		/* force empty-string result */
		*to = '\0';
		if (error)
			*error = 1;
		return 0;
	}
	return PQescapeStringInternal(conn, to, from, length, error,
								  conn->client_encoding,
								  conn->std_strings);
}

size_t
PQescapeString(char *to, const char *from, size_t length)
{
	return PQescapeStringInternal(NULL, to, from, length, NULL,
								  static_client_encoding,
								  static_std_strings);
}

/*
 *		PQescapeBytea	- converts from binary string to the
 *		minimal encoding necessary to include the string in an SQL
 *		INSERT statement with a bytea type column as the target.
 *
 *		The following transformations are applied
 *		'\0' == ASCII  0 == \000
 *		'\'' == ASCII 39 == ''
 *		'\\' == ASCII 92 == \\
 *		anything < 0x20, or > 0x7e ---> \ooo
 *										(where ooo is an octal expression)
 *		If not std_strings, all backslashes sent to the output are doubled.
 */
static unsigned char *
PQescapeByteaInternal(PGconn *conn,
					  const unsigned char *from, size_t from_length,
					  size_t *to_length, bool std_strings)
{
	const unsigned char *vp;
	unsigned char *rp;
	unsigned char *result;
	size_t		i;
	size_t		len;
	size_t		bslash_len = (std_strings ? 1 : 2);

	/*
	 * empty string has 1 char ('\0')
	 */
	len = 1;

	vp = from;
	for (i = from_length; i > 0; i--, vp++)
	{
		if (*vp < 0x20 || *vp > 0x7e)
			len += bslash_len + 3;
		else if (*vp == '\'')
			len += 2;
		else if (*vp == '\\')
			len += bslash_len + bslash_len;
		else
			len++;
	}

	*to_length = len;
	rp = result = (unsigned char *) malloc(len);
	if (rp == NULL)
	{
		if (conn)
			printfPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("out of memory\n"));
		return NULL;
	}

	vp = from;
	for (i = from_length; i > 0; i--, vp++)
	{
		if (*vp < 0x20 || *vp > 0x7e)
		{
			if (!std_strings)
				*rp++ = '\\';
			(void) sprintf((char *) rp, "\\%03o", *vp);
			rp += 4;
		}
		else if (*vp == '\'')
		{
			*rp++ = '\'';
			*rp++ = '\'';
		}
		else if (*vp == '\\')
		{
			if (!std_strings)
			{
				*rp++ = '\\';
				*rp++ = '\\';
			}
			*rp++ = '\\';
			*rp++ = '\\';
		}
		else
			*rp++ = *vp;
	}
	*rp = '\0';

	return result;
}

unsigned char *
PQescapeByteaConn(PGconn *conn,
				  const unsigned char *from, size_t from_length,
				  size_t *to_length)
{
	if (!conn)
		return NULL;
	return PQescapeByteaInternal(conn, from, from_length, to_length,
								 conn->std_strings);
}

unsigned char *
PQescapeBytea(const unsigned char *from, size_t from_length, size_t *to_length)
{
	return PQescapeByteaInternal(NULL, from, from_length, to_length,
								 static_std_strings);
}


#define ISFIRSTOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '3')
#define ISOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '7')
#define OCTVAL(CH) ((CH) - '0')

/*
 *		PQunescapeBytea - converts the null terminated string representation
 *		of a bytea, strtext, into binary, filling a buffer. It returns a
 *		pointer to the buffer (or NULL on error), and the size of the
 *		buffer in retbuflen. The pointer may subsequently be used as an
 *		argument to the function PQfreemem.
 *
 *		The following transformations are made:
 *		\\	 == ASCII 92 == \
 *		\ooo == a byte whose value = ooo (ooo is an octal number)
 *		\x	 == x (x is any character not matched by the above transformations)
 */
unsigned char *
PQunescapeBytea(const unsigned char *strtext, size_t *retbuflen)
{
	size_t		strtextlen,
				buflen;
	unsigned char *buffer,
			   *tmpbuf;
	size_t		i,
				j;

	if (strtext == NULL)
		return NULL;

	strtextlen = strlen((const char *) strtext);

	/*
	 * Length of input is max length of output, but add one to avoid
	 * unportable malloc(0) if input is zero-length.
	 */
	buffer = (unsigned char *) malloc(strtextlen + 1);
	if (buffer == NULL)
		return NULL;

	for (i = j = 0; i < strtextlen;)
	{
		switch (strtext[i])
		{
			case '\\':
				i++;
				if (strtext[i] == '\\')
					buffer[j++] = strtext[i++];
				else
				{
					if ((ISFIRSTOCTDIGIT(strtext[i])) &&
						(ISOCTDIGIT(strtext[i + 1])) &&
						(ISOCTDIGIT(strtext[i + 2])))
					{
						int			byte;

						byte = OCTVAL(strtext[i++]);
						byte = (byte << 3) + OCTVAL(strtext[i++]);
						byte = (byte << 3) + OCTVAL(strtext[i++]);
						buffer[j++] = byte;
					}
				}

				/*
				 * Note: if we see '\' followed by something that isn't a
				 * recognized escape sequence, we loop around having done
				 * nothing except advance i.  Therefore the something will be
				 * emitted as ordinary data on the next cycle. Corner case:
				 * '\' at end of string will just be discarded.
				 */
				break;

			default:
				buffer[j++] = strtext[i++];
				break;
		}
	}
	buflen = j;					/* buflen is the length of the dequoted data */

	/* Shrink the buffer to be no larger than necessary */
	/* +1 avoids unportable behavior when buflen==0 */
	tmpbuf = realloc(buffer, buflen + 1);

	/* It would only be a very brain-dead realloc that could fail, but... */
	if (!tmpbuf)
	{
		free(buffer);
		return NULL;
	}

	*retbuflen = buflen;
	return tmpbuf;
}
