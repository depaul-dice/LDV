/*
 * parser.c
 *
 *  Created on: May 24, 2014
 *      Author: quanpt
 */
#include "postgres.h"
#include <ctype.h>
#include <lib/stringinfo.h>
#include <nodes/nodes.h>
#include <nodes/parsenodes.h>
#include <nodes/pg_list.h>
#include <parser/parser.h>
#include <port.h>
#include <stdio.h>
#include <storage/ipc.h>
#include <utils/memutils.h>
#include <utils/palloc.h>

/*
 * ==========================
 * convert sql string to tree
 */

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfoString(str, nodelabel)

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, "\n    :" CppAsString(fldname) " "), \
	 _outNode(str, node->fldname))

const char *progname;

/*
 * _outToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void _outToken(StringInfo str, char *s) {
	if (s == NULL || *s == '\0') {
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' || *s == '\"' || isdigit((unsigned char ) *s)
			|| ((*s == '+' || *s == '-')
					&& (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s) {
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' || *s == '(' || *s == ')'
				|| *s == '{' || *s == '}' || *s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

static void
_outInsertionInfo(StringInfo str, InsertStmt *node) {
	WRITE_NODE_TYPE("INSERTTIONINFO");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(cols);
	WRITE_NODE_FIELD(selectStmt);
	WRITE_NODE_FIELD(returningList);
}

void
ptuOutNode(StringInfo str, void *obj) {
	switch(nodeTag(obj)) {
	case T_InsertStmt:
		_outInsertionInfo(str, obj);
		break;
	default:
		_outNode(str, obj);
//		elog(ERROR, "unrecognized node type: %d", (int) nodeTag(obj));
	}
}

char *
_nodeToString(void *obj) {
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	ptuOutNode(&str, obj);
	return str.data;
}

void print_tree(List* raw_parsetree_list) {
	ListCell *list_item;foreach(list_item, raw_parsetree_list) {
		Node *stmt = (Node *) lfirst(list_item);
		char *msg = _nodeToString(stmt);
		char *sql = nodeToSql(stmt);
		printf("%s\n--> %s\n", msg, sql);
		pfree(msg);
		pfree(sql);
	}
}

void print_tree_sql(char* sql) {
	List *parsetree;

	printf("\n\n========\nSQL: '%s'\n", sql);

	parsetree = raw_parser(sql);
	if (parsetree == NIL) {
		printf("Error parsing '%s'\n", sql);
		return;
	}
	print_tree(parsetree);
	list_free_deep(parsetree);
}

int main(int argc, char** argv) {
	char *sql = "INSERT into weather values(0, \"cool\");";


	progname = get_progname(argv[0]);

	if (argc == 2)
		sql = argv[1];

	MemoryContextInit();

	print_tree_sql(sql);

//	print_tree_sql("CREATE TABLE tbl1 (id INTEGER, value INTEGER)");
//	print_tree_sql("INSERT INTO tbl1 values(79, 1)");
//	print_tree_sql("INSERT INTO tbl1 values(49, 24)");
//	print_tree_sql("SELECT sum(value) FROM tbl1 WHERE value < 50");
//	print_tree_sql("INSERT INTO distributors (did, dname) VALUES (DEFAULT, 'XYZ Widgets')");
//	print_tree_sql("insert into weather (select time, 'hot' as cond from weather where time = 42 limit 5);");

	proc_exit(0);
	return 0;
}


