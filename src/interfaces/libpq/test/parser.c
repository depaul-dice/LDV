/*
 * parser.c
 *
 *  Created on: May 24, 2014
 *      Author: quanpt
 */

#include "postgres.h"
#include "nodes/pg_list.h"
#include "nodes/parsenodes.h"
#include "parser/parser.h"

const char *progname;

void print_list(List* l) {
	ListCell *it;
	foreach(it, l) {
		printf("list: %d\n", it->data.int_value);
	}
}

void print_query(Query* query) {
	printf("%d %d\n", query->commandType, CMD_INSERT);
	if (query->commandType == CMD_INSERT) {
		printf("INSERT ");
		print_list(query->rtable);
		print_list(query->targetList);
	}
}

int main(int argc, char** argv) {
	char *sql = "insert into weather values(0, \"cool\");";
	List *parsetree;
	Query *query;

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
	query = parse_analyze(parsetree, sql, NULL, 0);
	print_query(query);

	return 0;
}


