/*
 * parser.c
 *
 *  Created on: May 24, 2014
 *      Author: quanpt
 */

#include "postgres.h"
#include "nodes/pg_list.h"
#include "parser/parser.h"

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _outNode(str, node->fldname))

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfoString(str, nodelabel)

const char *progname;

typedef struct InsertionInfo
{
	NodeTag		type;
	Node       *relation;		/* relation to insert into */
	List	   *cols;			/* optional: names of the target columns */
	Node	   *selectStmt;		/* the source SELECT/VALUES, or NULL */
	List	   *returningList;	/* list of expressions to return */
} InsertionInfo; // get this from _copyInsertStmt

static void
_outInsertionInfo(StringInfo str, InsertionInfo *node) {
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

void print_tree(List* raw_parsetree_list, int depth) {
	ListCell *list_item;
	printf("%*s" "%d %d\n", depth * 4, " ", raw_parsetree_list->type, raw_parsetree_list->length);
	foreach(list_item, raw_parsetree_list) {
		Node	   *stmt = (Node *) lfirst(list_item);
		char *msg = _nodeToString(stmt);
		printf("%s\n", msg);
		pfree(msg);
	}
}

int main(int argc, char** argv) {
	char *sql = "INSERT into weather values(0, \"cool\");";
	List *parsetree;

	progname = get_progname(argv[0]);

	argv = save_ps_display_args(argc, argv);
	set_pglocale_pgservice(argv[0], "postgres");

	if (argc == 2)
		sql = argv[1];
	printf("SQL: '%s'\n", sql);

	MemoryContextInit();
	parsetree = raw_parser(sql);
	if (parsetree == NIL) {
		printf("Error\n");
		return -1;
	}
	print_tree(parsetree, 1);

	list_free_deep(parsetree);
	proc_exit();
	return 0;
}


