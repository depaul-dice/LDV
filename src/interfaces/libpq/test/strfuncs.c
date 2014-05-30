/*-------------------------------------------------------------------------
 *
 * strfuncs.c
 *	  Output functions for Postgres tree nodes.
 *
 * Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *    ported from
 *	  $PostgreSQL: pgsql/src/backend/nodes/outfuncs.c,v 1.322 2008/01/09 08:46:44 neilc Exp $
 *
 * NOTES
 *	  Every node type that can appear in stored rules' parsetrees *must*
 *	  have an output function defined here (as well as an input function
 *	  in readfuncs.c).	For use in debugging, we also provide output
 *	  functions for nodes that appear in raw parsetrees, path, and plan trees.
 *	  These nodes however need not have input functions.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "lib/stringinfo.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "utils/datum.h"
#include "provrewrite/prov_nodes.h"

/*
 * Macros to simplify output of different kinds of fields.	Use these
 * wherever possible to reduce the chance for silly typos.	Note that these
 * hard-wire conventions about the names of the local variables in an Out
 * routine.
 */

/* Write the label for the node type */
#define WRITE_NODE_TYPE(nodelabel) \
	appendStringInfoString(str, nodelabel)

/* Write an integer field (anything written as ":fldname %d") */
#define WRITE_INT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", node->fldname)

/* Write an unsigned integer field (anything written as ":fldname %u") */
#define WRITE_UINT_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write an OID field (don't hard-wire assumption that OID is same as uint) */
#define WRITE_OID_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %u", node->fldname)

/* Write a long-integer field */
#define WRITE_LONG_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %ld", node->fldname)

/* Write a char field (ie, one ascii character) */
#define WRITE_CHAR_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %c", node->fldname)

/* Write an enumerated-type field as an integer code */
#define WRITE_ENUM_FIELD(fldname, enumtype) \
	appendStringInfo(str, " :" CppAsString(fldname) " %d", \
					 (int) node->fldname)

/* Write a float field --- caller must give format to define precision */
#define WRITE_FLOAT_FIELD(fldname,format) \
	appendStringInfo(str, " :" CppAsString(fldname) " " format, node->fldname)

/* Write a boolean field */
#define WRITE_BOOL_FIELD(fldname) \
	appendStringInfo(str, " :" CppAsString(fldname) " %s", \
					 booltostr(node->fldname))

/* Write a character-string (possibly NULL) field */
#define WRITE_STRING_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _strToken(str, node->fldname))

/* Write a Node field */
#define WRITE_NODE_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _strNode(str, node->fldname))

/* Write a bitmapset field */
#define WRITE_BITMAPSET_FIELD(fldname) \
	(appendStringInfo(str, " :" CppAsString(fldname) " "), \
	 _strBitmapset(str, node->fldname))


#define booltostr(x)  ((x) ? "true" : "false")




/*
 * _strToken
 *	  Convert an ordinary string (eg, an identifier) into a form that
 *	  will be decoded back to a plain token by read.c's functions.
 *
 *	  If a null or empty string is given, it is encoded as "<>".
 */
static void
_strToken(StringInfo str, char *s)
{
	if (s == NULL || *s == '\0')
	{
		appendStringInfo(str, "<>");
		return;
	}

	/*
	 * Look for characters or patterns that are treated specially by read.c
	 * (either in pg_strtok() or in nodeRead()), and therefore need a
	 * protective backslash.
	 */
	/* These characters only need to be quoted at the start of the string */
	if (*s == '<' ||
		*s == '\"' ||
		isdigit((unsigned char) *s) ||
		((*s == '+' || *s == '-') &&
		 (isdigit((unsigned char) s[1]) || s[1] == '.')))
		appendStringInfoChar(str, '\\');
	while (*s)
	{
		/* These chars must be backslashed anywhere in the string */
		if (*s == ' ' || *s == '\n' || *s == '\t' ||
			*s == '(' || *s == ')' || *s == '{' || *s == '}' ||
			*s == '\\')
			appendStringInfoChar(str, '\\');
		appendStringInfoChar(str, *s++);
	}
}

static void
_strList(StringInfo str, List *node)
{
	ListCell   *lc;

	appendStringInfoChar(str, '(');

	if (IsA(node, IntList))
		appendStringInfoChar(str, 'i');
	else if (IsA(node, OidList))
		appendStringInfoChar(str, 'o');

	foreach(lc, node)
	{
		/*
		 * For the sake of backward compatibility, we emit a slightly
		 * different whitespace format for lists of nodes vs. other types of
		 * lists. XXX: is this necessary?
		 */
		if (IsA(node, List))
		{
			_strNode(str, lfirst(lc));
			if (lnext(lc))
				appendStringInfoChar(str, ' ');
		}
		else if (IsA(node, IntList))
			appendStringInfo(str, " %d", lfirst_int(lc));
		else if (IsA(node, OidList))
			appendStringInfo(str, " %u", lfirst_oid(lc));
		else
			elog(ERROR, "unrecognized list node type: %d",
				 (int) node->type);
	}

	appendStringInfoChar(str, ')');
}

/*
 * _strBitmapset -
 *	   converts a bitmap set of integers
 *
 * Note: the output format is "(b int int ...)", similar to an integer List.
 * Currently bitmapsets do not appear in any node type that is stored in
 * rules, so there is no support in readfuncs.c for reading this format.
 */
void
_strBitmapset(StringInfo str, Bitmapset *bms)
{
	Bitmapset  *tmpset;
	int			x;

	appendStringInfoChar(str, '(');
	appendStringInfoChar(str, 'b');
	tmpset = bms_copy(bms);
	while ((x = bms_first_member(tmpset)) >= 0)
		appendStringInfo(str, " %d", x);
	bms_free(tmpset);
	appendStringInfoChar(str, ')');
}

/*
 * Print the value of a Datum given its type.
 */
void
_strDatum(StringInfo str, Datum value, int typlen, bool typbyval)
{
	Size		length,
				i;
	char	   *s;

	length = datumGetSize(value, typbyval, typlen);

	if (typbyval)
	{
		s = (char *) (&value);
		appendStringInfo(str, "%u [ ", (unsigned int) length);
		for (i = 0; i < (Size) sizeof(Datum); i++)
			appendStringInfo(str, "%d ", (int) (s[i]));
		appendStringInfo(str, "]");
	}
	else
	{
		s = (char *) DatumGetPointer(value);
		if (!PointerIsValid(s))
			appendStringInfo(str, "0 [ ]");
		else
		{
			appendStringInfo(str, "%u [ ", (unsigned int) length);
			for (i = 0; i < length; i++)
				appendStringInfo(str, "%d ", (int) (s[i]));
			appendStringInfo(str, "]");
		}
	}
}


/*
 *	Stuff from plannodes.h
 */

static void
_strPlannedStmt(StringInfo str, PlannedStmt *node)
{
	WRITE_NODE_TYPE("PLANNEDSTMT");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_BOOL_FIELD(canSetTag);
	WRITE_NODE_FIELD(planTree);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(utilityStmt);
	WRITE_NODE_FIELD(intoClause);
	WRITE_NODE_FIELD(subplans);
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(returningLists);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(relationOids);
	WRITE_INT_FIELD(nParamExec);
}

/*
 * print the basic stuff of all nodes that inherit from Plan
 */
static void
_strPlanInfo(StringInfo str, Plan *node)
{
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
	WRITE_FLOAT_FIELD(plan_rows, "%.0f");
	WRITE_INT_FIELD(plan_width);
	WRITE_NODE_FIELD(targetlist);
	WRITE_NODE_FIELD(qual);
	WRITE_NODE_FIELD(lefttree);
	WRITE_NODE_FIELD(righttree);
	WRITE_NODE_FIELD(initPlan);
	WRITE_BITMAPSET_FIELD(extParam);
	WRITE_BITMAPSET_FIELD(allParam);
}

/*
 * print the basic stuff of all nodes that inherit from Scan
 */
static void
_strScanInfo(StringInfo str, Scan *node)
{
	_strPlanInfo(str, (Plan *) node);

	WRITE_UINT_FIELD(scanrelid);
}

/*
 * print the basic stuff of all nodes that inherit from Join
 */
static void
_strJoinPlanInfo(StringInfo str, Join *node)
{
	_strPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(joinqual);
}


static void
_strPlan(StringInfo str, Plan *node)
{
	WRITE_NODE_TYPE("PLAN");

	_strPlanInfo(str, (Plan *) node);
}

static void
_strResult(StringInfo str, Result *node)
{
	WRITE_NODE_TYPE("RESULT");

	_strPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(resconstantqual);
}

static void
_strAppend(StringInfo str, Append *node)
{
	WRITE_NODE_TYPE("APPEND");

	_strPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(appendplans);
	WRITE_BOOL_FIELD(isTarget);
}

static void
_strBitmapAnd(StringInfo str, BitmapAnd *node)
{
	WRITE_NODE_TYPE("BITMAPAND");

	_strPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
}

static void
_strBitmapOr(StringInfo str, BitmapOr *node)
{
	WRITE_NODE_TYPE("BITMAPOR");

	_strPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(bitmapplans);
}

static void
_strScan(StringInfo str, Scan *node)
{
	WRITE_NODE_TYPE("SCAN");

	_strScanInfo(str, (Scan *) node);
}

static void
_strSeqScan(StringInfo str, SeqScan *node)
{
	WRITE_NODE_TYPE("SEQSCAN");

	_strScanInfo(str, (Scan *) node);
}

static void
_strIndexScan(StringInfo str, IndexScan *node)
{
	WRITE_NODE_TYPE("INDEXSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexqualorig);
	WRITE_NODE_FIELD(indexstrategy);
	WRITE_NODE_FIELD(indexsubtype);
	WRITE_ENUM_FIELD(indexorderdir, ScanDirection);
}

static void
_strBitmapIndexScan(StringInfo str, BitmapIndexScan *node)
{
	WRITE_NODE_TYPE("BITMAPINDEXSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_OID_FIELD(indexid);
	WRITE_NODE_FIELD(indexqual);
	WRITE_NODE_FIELD(indexqualorig);
	WRITE_NODE_FIELD(indexstrategy);
	WRITE_NODE_FIELD(indexsubtype);
}

static void
_strBitmapHeapScan(StringInfo str, BitmapHeapScan *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(bitmapqualorig);
}

static void
_strTidScan(StringInfo str, TidScan *node)
{
	WRITE_NODE_TYPE("TIDSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(tidquals);
}

static void
_strSubqueryScan(StringInfo str, SubqueryScan *node)
{
	WRITE_NODE_TYPE("SUBQUERYSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(subrtable);
}

static void
_strFunctionScan(StringInfo str, FunctionScan *node)
{
	WRITE_NODE_TYPE("FUNCTIONSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(funcexpr);
	WRITE_NODE_FIELD(funccolnames);
	WRITE_NODE_FIELD(funccoltypes);
	WRITE_NODE_FIELD(funccoltypmods);
}

static void
_strValuesScan(StringInfo str, ValuesScan *node)
{
	WRITE_NODE_TYPE("VALUESSCAN");

	_strScanInfo(str, (Scan *) node);

	WRITE_NODE_FIELD(values_lists);
}

static void
_strJoin(StringInfo str, Join *node)
{
	WRITE_NODE_TYPE("JOIN");

	_strJoinPlanInfo(str, (Join *) node);
}

static void
_strNestLoop(StringInfo str, NestLoop *node)
{
	WRITE_NODE_TYPE("NESTLOOP");

	_strJoinPlanInfo(str, (Join *) node);
}

static void
_strMergeJoin(StringInfo str, MergeJoin *node)
{
	int			numCols;
	int			i;

	WRITE_NODE_TYPE("MERGEJOIN");

	_strJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(mergeclauses);

	numCols = list_length(node->mergeclauses);

	appendStringInfo(str, " :mergeFamilies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %u", node->mergeFamilies[i]);

	appendStringInfo(str, " :mergeStrategies");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", node->mergeStrategies[i]);

	appendStringInfo(str, " :mergeNullsFirst");
	for (i = 0; i < numCols; i++)
		appendStringInfo(str, " %d", (int) node->mergeNullsFirst[i]);
}

static void
_strHashJoin(StringInfo str, HashJoin *node)
{
	WRITE_NODE_TYPE("HASHJOIN");

	_strJoinPlanInfo(str, (Join *) node);

	WRITE_NODE_FIELD(hashclauses);
}

static void
_strAgg(StringInfo str, Agg *node)
{
	int i;

	WRITE_NODE_TYPE("AGG");

	_strPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(aggstrategy, AggStrategy);
	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfo(str, " :grpOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->grpOperators[i]);

	WRITE_LONG_FIELD(numGroups);
}

static void
_strGroup(StringInfo str, Group *node)
{
	int			i;

	WRITE_NODE_TYPE("GROUP");

	_strPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :grpColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->grpColIdx[i]);

	appendStringInfo(str, " :grpOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->grpOperators[i]);
}

static void
_strMaterial(StringInfo str, Material *node)
{
	WRITE_NODE_TYPE("MATERIAL");

	_strPlanInfo(str, (Plan *) node);
}

static void
_strSort(StringInfo str, Sort *node)
{
	int			i;

	WRITE_NODE_TYPE("SORT");

	_strPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :sortColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->sortColIdx[i]);

	appendStringInfo(str, " :sortOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->sortOperators[i]);

	appendStringInfo(str, " :nullsFirst");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %s", booltostr(node->nullsFirst[i]));
}

static void
_strUnique(StringInfo str, Unique *node)
{
	int			i;

	WRITE_NODE_TYPE("UNIQUE");

	_strPlanInfo(str, (Plan *) node);

	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :uniqColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->uniqColIdx[i]);

	appendStringInfo(str, " :uniqOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->uniqOperators[i]);
}

static void
_strSetOp(StringInfo str, SetOp *node)
{
	int			i;

	WRITE_NODE_TYPE("SETOP");

	_strPlanInfo(str, (Plan *) node);

	WRITE_ENUM_FIELD(cmd, SetOpCmd);
	WRITE_INT_FIELD(numCols);

	appendStringInfo(str, " :dupColIdx");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %d", node->dupColIdx[i]);

	appendStringInfo(str, " :dupOperators");
	for (i = 0; i < node->numCols; i++)
		appendStringInfo(str, " %u", node->dupOperators[i]);

	WRITE_INT_FIELD(flagColIdx);
}

static void
_strLimit(StringInfo str, Limit *node)
{
	WRITE_NODE_TYPE("LIMIT");

	_strPlanInfo(str, (Plan *) node);

	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
}

static void
_strHash(StringInfo str, Hash *node)
{
	WRITE_NODE_TYPE("HASH");

	_strPlanInfo(str, (Plan *) node);
}

/*****************************************************************************
 *
 *	Stuff from primnodes.h.
 *
 *****************************************************************************/

static void
_strAlias(StringInfo str, Alias *node)
{
	WRITE_NODE_TYPE("ALIAS");

	WRITE_STRING_FIELD(aliasname);
	WRITE_NODE_FIELD(colnames);
}

static void
_strRangeVar(StringInfo str, RangeVar *node)
{
	WRITE_NODE_TYPE("RANGEVAR");

	/*
	 * we deliberately ignore catalogname here, since it is presently not
	 * semantically meaningful
	 */
	WRITE_STRING_FIELD(schemaname);
	WRITE_STRING_FIELD(relname);
	WRITE_ENUM_FIELD(inhOpt, InhOption);
	WRITE_BOOL_FIELD(istemp);
	WRITE_NODE_FIELD(alias);
	WRITE_BOOL_FIELD(isProvBase);
	WRITE_NODE_FIELD(provAttrs);
	WRITE_NODE_FIELD(annotations);
}

static void
_strIntoClause(StringInfo str, IntoClause *node)
{
	WRITE_NODE_TYPE("INTOCLAUSE");

	WRITE_NODE_FIELD(rel);
	WRITE_NODE_FIELD(colNames);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(onCommit, OnCommitAction);
	WRITE_STRING_FIELD(tableSpaceName);
}

static void
_strVar(StringInfo str, Var *node)
{
	WRITE_NODE_TYPE("VAR");

	WRITE_UINT_FIELD(varno);
	WRITE_INT_FIELD(varattno);
	WRITE_OID_FIELD(vartype);
	WRITE_INT_FIELD(vartypmod);
	WRITE_UINT_FIELD(varlevelsup);
	WRITE_UINT_FIELD(varnoold);
	WRITE_INT_FIELD(varoattno);
}

static void
_strConst(StringInfo str, Const *node)
{
	WRITE_NODE_TYPE("CONST");

	WRITE_OID_FIELD(consttype);
	WRITE_INT_FIELD(consttypmod);
	WRITE_INT_FIELD(constlen);
	WRITE_BOOL_FIELD(constbyval);
	WRITE_BOOL_FIELD(constisnull);

	appendStringInfo(str, " :constvalue ");
	if (node->constisnull)
		appendStringInfo(str, "<>");
	else
		_strDatum(str, node->constvalue, node->constlen, node->constbyval);
}

static void
_strParam(StringInfo str, Param *node)
{
	WRITE_NODE_TYPE("PARAM");

	WRITE_ENUM_FIELD(paramkind, ParamKind);
	WRITE_INT_FIELD(paramid);
	WRITE_OID_FIELD(paramtype);
	WRITE_INT_FIELD(paramtypmod);
}

static void
_strAggref(StringInfo str, Aggref *node)
{
	WRITE_NODE_TYPE("AGGREF");

	WRITE_OID_FIELD(aggfnoid);
	WRITE_OID_FIELD(aggtype);
	WRITE_NODE_FIELD(args);
	WRITE_UINT_FIELD(agglevelsup);
	WRITE_BOOL_FIELD(aggstar);
	WRITE_BOOL_FIELD(aggdistinct);
}

static void
_strArrayRef(StringInfo str, ArrayRef *node)
{
	WRITE_NODE_TYPE("ARRAYREF");

	WRITE_OID_FIELD(refarraytype);
	WRITE_OID_FIELD(refelemtype);
	WRITE_INT_FIELD(reftypmod);
	WRITE_NODE_FIELD(refupperindexpr);
	WRITE_NODE_FIELD(reflowerindexpr);
	WRITE_NODE_FIELD(refexpr);
	WRITE_NODE_FIELD(refassgnexpr);
}

static void
_strFuncExpr(StringInfo str, FuncExpr *node)
{
	WRITE_NODE_TYPE("FUNCEXPR");

	WRITE_OID_FIELD(funcid);
	WRITE_OID_FIELD(funcresulttype);
	WRITE_BOOL_FIELD(funcretset);
	WRITE_ENUM_FIELD(funcformat, CoercionForm);
	WRITE_NODE_FIELD(args);
}

static void
_strOpExpr(StringInfo str, OpExpr *node)
{
	WRITE_NODE_TYPE("OPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_strDistinctExpr(StringInfo str, DistinctExpr *node)
{
	WRITE_NODE_TYPE("DISTINCTEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_strScalarArrayOpExpr(StringInfo str, ScalarArrayOpExpr *node)
{
	WRITE_NODE_TYPE("SCALARARRAYOPEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_BOOL_FIELD(useOr);
	WRITE_NODE_FIELD(args);
}

static void
_strBoolExpr(StringInfo str, BoolExpr *node)
{
	char	   *opstr = NULL;

	WRITE_NODE_TYPE("BOOLEXPR");

	/* do-it-yourself enum representation */
	switch (node->boolop)
	{
		case AND_EXPR:
			opstr = "and";
			break;
		case OR_EXPR:
			opstr = "or";
			break;
		case NOT_EXPR:
			opstr = "not";
			break;
	}
	appendStringInfo(str, " :boolop ");
	_strToken(str, opstr);

	WRITE_NODE_FIELD(args);
}

static void
_strSubLink(StringInfo str, SubLink *node)
{
	WRITE_NODE_TYPE("SUBLINK");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(operName);
	WRITE_NODE_FIELD(subselect);
}

static void
_strSubPlan(StringInfo str, SubPlan *node)
{
	WRITE_NODE_TYPE("SUBPLAN");

	WRITE_ENUM_FIELD(subLinkType, SubLinkType);
	WRITE_NODE_FIELD(testexpr);
	WRITE_NODE_FIELD(paramIds);
	WRITE_INT_FIELD(plan_id);
	WRITE_OID_FIELD(firstColType);
	WRITE_BOOL_FIELD(useHashTable);
	WRITE_BOOL_FIELD(unknownEqFalse);
	WRITE_NODE_FIELD(setParam);
	WRITE_NODE_FIELD(parParam);
	WRITE_NODE_FIELD(args);
}

static void
_strFieldSelect(StringInfo str, FieldSelect *node)
{
	WRITE_NODE_TYPE("FIELDSELECT");

	WRITE_NODE_FIELD(arg);
	WRITE_INT_FIELD(fieldnum);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
}

static void
_strFieldStore(StringInfo str, FieldStore *node)
{
	WRITE_NODE_TYPE("FIELDSTORE");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(newvals);
	WRITE_NODE_FIELD(fieldnums);
	WRITE_OID_FIELD(resulttype);
}

static void
_strRelabelType(StringInfo str, RelabelType *node)
{
	WRITE_NODE_TYPE("RELABELTYPE");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_ENUM_FIELD(relabelformat, CoercionForm);
}

static void
_strCoerceViaIO(StringInfo str, CoerceViaIO *node)
{
	WRITE_NODE_TYPE("COERCEVIAIO");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
}

static void
_strArrayCoerceExpr(StringInfo str, ArrayCoerceExpr *node)
{
	WRITE_NODE_TYPE("ARRAYCOERCEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(elemfuncid);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_BOOL_FIELD(isExplicit);
	WRITE_ENUM_FIELD(coerceformat, CoercionForm);
}

static void
_strConvertRowtypeExpr(StringInfo str, ConvertRowtypeExpr *node)
{
	WRITE_NODE_TYPE("CONVERTROWTYPEEXPR");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_ENUM_FIELD(convertformat, CoercionForm);
}

static void
_strCaseExpr(StringInfo str, CaseExpr *node)
{
	WRITE_NODE_TYPE("CASE");

	WRITE_OID_FIELD(casetype);
	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(args);
	WRITE_NODE_FIELD(defresult);
}

static void
_strCaseWhen(StringInfo str, CaseWhen *node)
{
	WRITE_NODE_TYPE("WHEN");

	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(result);
}

static void
_strCaseTestExpr(StringInfo str, CaseTestExpr *node)
{
	WRITE_NODE_TYPE("CASETESTEXPR");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_strArrayExpr(StringInfo str, ArrayExpr *node)
{
	WRITE_NODE_TYPE("ARRAY");

	WRITE_OID_FIELD(array_typeid);
	WRITE_OID_FIELD(element_typeid);
	WRITE_NODE_FIELD(elements);
	WRITE_BOOL_FIELD(multidims);
}

static void
_strRowExpr(StringInfo str, RowExpr *node)
{
	WRITE_NODE_TYPE("ROW");

	WRITE_NODE_FIELD(args);
	WRITE_OID_FIELD(row_typeid);
	WRITE_ENUM_FIELD(row_format, CoercionForm);
}

static void
_strRowCompareExpr(StringInfo str, RowCompareExpr *node)
{
	WRITE_NODE_TYPE("ROWCOMPARE");

	WRITE_ENUM_FIELD(rctype, RowCompareType);
	WRITE_NODE_FIELD(opnos);
	WRITE_NODE_FIELD(opfamilies);
	WRITE_NODE_FIELD(largs);
	WRITE_NODE_FIELD(rargs);
}

static void
_strCoalesceExpr(StringInfo str, CoalesceExpr *node)
{
	WRITE_NODE_TYPE("COALESCE");

	WRITE_OID_FIELD(coalescetype);
	WRITE_NODE_FIELD(args);
}

static void
_strMinMaxExpr(StringInfo str, MinMaxExpr *node)
{
	WRITE_NODE_TYPE("MINMAX");

	WRITE_OID_FIELD(minmaxtype);
	WRITE_ENUM_FIELD(op, MinMaxOp);
	WRITE_NODE_FIELD(args);
}

static void
_strXmlExpr(StringInfo str, XmlExpr *node)
{
	WRITE_NODE_TYPE("XMLEXPR");

	WRITE_ENUM_FIELD(op, XmlExprOp);
	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(named_args);
	WRITE_NODE_FIELD(arg_names);
	WRITE_NODE_FIELD(args);
	WRITE_ENUM_FIELD(xmloption, XmlOptionType);
	WRITE_OID_FIELD(type);
	WRITE_INT_FIELD(typmod);
}

static void
_strNullIfExpr(StringInfo str, NullIfExpr *node)
{
	WRITE_NODE_TYPE("NULLIFEXPR");

	WRITE_OID_FIELD(opno);
	WRITE_OID_FIELD(opfuncid);
	WRITE_OID_FIELD(opresulttype);
	WRITE_BOOL_FIELD(opretset);
	WRITE_NODE_FIELD(args);
}

static void
_strNullTest(StringInfo str, NullTest *node)
{
	WRITE_NODE_TYPE("NULLTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(nulltesttype, NullTestType);
}

static void
_strBooleanTest(StringInfo str, BooleanTest *node)
{
	WRITE_NODE_TYPE("BOOLEANTEST");

	WRITE_NODE_FIELD(arg);
	WRITE_ENUM_FIELD(booltesttype, BoolTestType);
}

static void
_strCoerceToDomain(StringInfo str, CoerceToDomain *node)
{
	WRITE_NODE_TYPE("COERCETODOMAIN");

	WRITE_NODE_FIELD(arg);
	WRITE_OID_FIELD(resulttype);
	WRITE_INT_FIELD(resulttypmod);
	WRITE_ENUM_FIELD(coercionformat, CoercionForm);
}

static void
_strCoerceToDomainValue(StringInfo str, CoerceToDomainValue *node)
{
	WRITE_NODE_TYPE("COERCETODOMAINVALUE");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_strSetToDefault(StringInfo str, SetToDefault *node)
{
	WRITE_NODE_TYPE("SETTODEFAULT");

	WRITE_OID_FIELD(typeId);
	WRITE_INT_FIELD(typeMod);
}

static void
_strCurrentOfExpr(StringInfo str, CurrentOfExpr *node)
{
	WRITE_NODE_TYPE("CURRENTOFEXPR");

	WRITE_UINT_FIELD(cvarno);
	WRITE_STRING_FIELD(cursor_name);
	WRITE_INT_FIELD(cursor_param);
}

static void
_strTargetEntry(StringInfo str, TargetEntry *node)
{
	WRITE_NODE_TYPE("TARGETENTRY");

	WRITE_NODE_FIELD(expr);
	WRITE_INT_FIELD(resno);
	WRITE_STRING_FIELD(resname);
	WRITE_UINT_FIELD(ressortgroupref);
	WRITE_OID_FIELD(resorigtbl);
	WRITE_INT_FIELD(resorigcol);
	WRITE_BOOL_FIELD(resjunk);
}

static void
_strRangeTblRef(StringInfo str, RangeTblRef *node)
{
	WRITE_NODE_TYPE("RANGETBLREF");

	WRITE_INT_FIELD(rtindex);
}

static void
_strJoinExpr(StringInfo str, JoinExpr *node)
{
	WRITE_NODE_TYPE("JOINEXPR");

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_BOOL_FIELD(isNatural);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(using);
	WRITE_NODE_FIELD(quals);
	WRITE_NODE_FIELD(alias);
	WRITE_INT_FIELD(rtindex);
}

static void
_strFromExpr(StringInfo str, FromExpr *node)
{
	WRITE_NODE_TYPE("FROMEXPR");

	WRITE_NODE_FIELD(fromlist);
	WRITE_NODE_FIELD(quals);
}

/*****************************************************************************
 *
 *	Stuff from relation.h.
 *
 *****************************************************************************/

/*
 * print the basic stuff of all nodes that inherit from Path
 *
 * Note we do NOT print the parent, else we'd be in infinite recursion
 */
static void
_strPathInfo(StringInfo str, Path *node)
{
	WRITE_ENUM_FIELD(pathtype, NodeTag);
	WRITE_FLOAT_FIELD(startup_cost, "%.2f");
	WRITE_FLOAT_FIELD(total_cost, "%.2f");
	WRITE_NODE_FIELD(pathkeys);
}

/*
 * print the basic stuff of all nodes that inherit from JoinPath
 */
static void
_strJoinPathInfo(StringInfo str, JoinPath *node)
{
	_strPathInfo(str, (Path *) node);

	WRITE_ENUM_FIELD(jointype, JoinType);
	WRITE_NODE_FIELD(outerjoinpath);
	WRITE_NODE_FIELD(innerjoinpath);
	WRITE_NODE_FIELD(joinrestrictinfo);
}

static void
_strPath(StringInfo str, Path *node)
{
	WRITE_NODE_TYPE("PATH");

	_strPathInfo(str, (Path *) node);
}

static void
_strIndexPath(StringInfo str, IndexPath *node)
{
	WRITE_NODE_TYPE("INDEXPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(indexinfo);
	WRITE_NODE_FIELD(indexclauses);
	WRITE_NODE_FIELD(indexquals);
	WRITE_BOOL_FIELD(isjoininner);
	WRITE_ENUM_FIELD(indexscandir, ScanDirection);
	WRITE_FLOAT_FIELD(indextotalcost, "%.2f");
	WRITE_FLOAT_FIELD(indexselectivity, "%.4f");
	WRITE_FLOAT_FIELD(rows, "%.0f");
}

static void
_strBitmapHeapPath(StringInfo str, BitmapHeapPath *node)
{
	WRITE_NODE_TYPE("BITMAPHEAPPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapqual);
	WRITE_BOOL_FIELD(isjoininner);
	WRITE_FLOAT_FIELD(rows, "%.0f");
}

static void
_strBitmapAndPath(StringInfo str, BitmapAndPath *node)
{
	WRITE_NODE_TYPE("BITMAPANDPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_strBitmapOrPath(StringInfo str, BitmapOrPath *node)
{
	WRITE_NODE_TYPE("BITMAPORPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(bitmapquals);
	WRITE_FLOAT_FIELD(bitmapselectivity, "%.4f");
}

static void
_strTidPath(StringInfo str, TidPath *node)
{
	WRITE_NODE_TYPE("TIDPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(tidquals);
}

static void
_strAppendPath(StringInfo str, AppendPath *node)
{
	WRITE_NODE_TYPE("APPENDPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpaths);
}

static void
_strResultPath(StringInfo str, ResultPath *node)
{
	WRITE_NODE_TYPE("RESULTPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(quals);
}

static void
_strMaterialPath(StringInfo str, MaterialPath *node)
{
	WRITE_NODE_TYPE("MATERIALPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpath);
}

static void
_strUniquePath(StringInfo str, UniquePath *node)
{
	WRITE_NODE_TYPE("UNIQUEPATH");

	_strPathInfo(str, (Path *) node);

	WRITE_NODE_FIELD(subpath);
	WRITE_ENUM_FIELD(umethod, UniquePathMethod);
	WRITE_FLOAT_FIELD(rows, "%.0f");
}

static void
_strNestPath(StringInfo str, NestPath *node)
{
	WRITE_NODE_TYPE("NESTPATH");

	_strJoinPathInfo(str, (JoinPath *) node);
}

static void
_strMergePath(StringInfo str, MergePath *node)
{
	WRITE_NODE_TYPE("MERGEPATH");

	_strJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_mergeclauses);
	WRITE_NODE_FIELD(outersortkeys);
	WRITE_NODE_FIELD(innersortkeys);
}

static void
_strHashPath(StringInfo str, HashPath *node)
{
	WRITE_NODE_TYPE("HASHPATH");

	_strJoinPathInfo(str, (JoinPath *) node);

	WRITE_NODE_FIELD(path_hashclauses);
}

static void
_strPlannerGlobal(StringInfo str, PlannerGlobal *node)
{
	WRITE_NODE_TYPE("PLANNERGLOBAL");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(paramlist);
	WRITE_NODE_FIELD(subplans);
	WRITE_NODE_FIELD(subrtables);
	WRITE_BITMAPSET_FIELD(rewindPlanIDs);
	WRITE_NODE_FIELD(finalrtable);
	WRITE_NODE_FIELD(relationOids);
}

static void
_strPlannerInfo(StringInfo str, PlannerInfo *node)
{
	WRITE_NODE_TYPE("PLANNERINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(parse);
	WRITE_NODE_FIELD(glob);
	WRITE_UINT_FIELD(query_level);
	WRITE_NODE_FIELD(join_rel_list);
	WRITE_NODE_FIELD(resultRelations);
	WRITE_NODE_FIELD(returningLists);
	WRITE_NODE_FIELD(init_plans);
	WRITE_NODE_FIELD(eq_classes);
	WRITE_NODE_FIELD(canon_pathkeys);
	WRITE_NODE_FIELD(left_join_clauses);
	WRITE_NODE_FIELD(right_join_clauses);
	WRITE_NODE_FIELD(full_join_clauses);
	WRITE_NODE_FIELD(oj_info_list);
	WRITE_NODE_FIELD(in_info_list);
	WRITE_NODE_FIELD(append_rel_list);
	WRITE_NODE_FIELD(query_pathkeys);
	WRITE_NODE_FIELD(group_pathkeys);
	WRITE_NODE_FIELD(sort_pathkeys);
	WRITE_FLOAT_FIELD(total_table_pages, "%.0f");
	WRITE_FLOAT_FIELD(tuple_fraction, "%.4f");
	WRITE_BOOL_FIELD(hasJoinRTEs);
	WRITE_BOOL_FIELD(hasOuterJoins);
	WRITE_BOOL_FIELD(hasHavingQual);
	WRITE_BOOL_FIELD(hasPseudoConstantQuals);
}

static void
_strRelOptInfo(StringInfo str, RelOptInfo *node)
{
	WRITE_NODE_TYPE("RELOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_ENUM_FIELD(reloptkind, RelOptKind);
	WRITE_BITMAPSET_FIELD(relids);
	WRITE_FLOAT_FIELD(rows, "%.0f");
	WRITE_INT_FIELD(width);
	WRITE_NODE_FIELD(reltargetlist);
	WRITE_NODE_FIELD(pathlist);
	WRITE_NODE_FIELD(cheapest_startup_path);
	WRITE_NODE_FIELD(cheapest_total_path);
	WRITE_NODE_FIELD(cheapest_unique_path);
	WRITE_UINT_FIELD(relid);
	WRITE_ENUM_FIELD(rtekind, RTEKind);
	WRITE_INT_FIELD(min_attr);
	WRITE_INT_FIELD(max_attr);
	WRITE_NODE_FIELD(indexlist);
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_NODE_FIELD(subplan);
	WRITE_NODE_FIELD(subrtable);
	WRITE_NODE_FIELD(baserestrictinfo);
	WRITE_NODE_FIELD(joininfo);
	WRITE_BOOL_FIELD(has_eclass_joins);
	WRITE_BITMAPSET_FIELD(index_outer_relids);
	WRITE_NODE_FIELD(index_inner_paths);
}

static void
_strIndexOptInfo(StringInfo str, IndexOptInfo *node)
{
	WRITE_NODE_TYPE("INDEXOPTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_OID_FIELD(indexoid);
	/* Do NOT print rel field, else infinite recursion */
	WRITE_UINT_FIELD(pages);
	WRITE_FLOAT_FIELD(tuples, "%.0f");
	WRITE_INT_FIELD(ncolumns);
	WRITE_NODE_FIELD(indexprs);
	WRITE_NODE_FIELD(indpred);
	WRITE_BOOL_FIELD(predOK);
	WRITE_BOOL_FIELD(unique);
}

static void
_strEquivalenceClass(StringInfo str, EquivalenceClass *node)
{
	/*
	 * To simplify reading, we just chase up to the topmost merged EC and
	 * print that, without bothering to show the merge-ees separately.
	 */
	while (node->ec_merged)
		node = node->ec_merged;

	WRITE_NODE_TYPE("EQUIVALENCECLASS");

	WRITE_NODE_FIELD(ec_opfamilies);
	WRITE_NODE_FIELD(ec_members);
	WRITE_NODE_FIELD(ec_sources);
	WRITE_NODE_FIELD(ec_derives);
	WRITE_BITMAPSET_FIELD(ec_relids);
	WRITE_BOOL_FIELD(ec_has_const);
	WRITE_BOOL_FIELD(ec_has_volatile);
	WRITE_BOOL_FIELD(ec_below_outer_join);
	WRITE_BOOL_FIELD(ec_broken);
	WRITE_UINT_FIELD(ec_sortref);
}

static void
_strEquivalenceMember(StringInfo str, EquivalenceMember *node)
{
	WRITE_NODE_TYPE("EQUIVALENCEMEMBER");

	WRITE_NODE_FIELD(em_expr);
	WRITE_BITMAPSET_FIELD(em_relids);
	WRITE_BOOL_FIELD(em_is_const);
	WRITE_BOOL_FIELD(em_is_child);
	WRITE_OID_FIELD(em_datatype);
}

static void
_strPathKey(StringInfo str, PathKey *node)
{
	WRITE_NODE_TYPE("PATHKEY");

	WRITE_NODE_FIELD(pk_eclass);
	WRITE_OID_FIELD(pk_opfamily);
	WRITE_INT_FIELD(pk_strategy);
	WRITE_BOOL_FIELD(pk_nulls_first);
}

static void
_strRestrictInfo(StringInfo str, RestrictInfo *node)
{
	WRITE_NODE_TYPE("RESTRICTINFO");

	/* NB: this isn't a complete set of fields */
	WRITE_NODE_FIELD(clause);
	WRITE_BOOL_FIELD(is_pushed_down);
	WRITE_BOOL_FIELD(outerjoin_delayed);
	WRITE_BOOL_FIELD(can_join);
	WRITE_BOOL_FIELD(pseudoconstant);
	WRITE_BITMAPSET_FIELD(clause_relids);
	WRITE_BITMAPSET_FIELD(required_relids);
	WRITE_BITMAPSET_FIELD(left_relids);
	WRITE_BITMAPSET_FIELD(right_relids);
	WRITE_NODE_FIELD(orclause);
	/* don't write parent_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_FIELD(mergeopfamilies);
	/* don't write left_ec, leads to infinite recursion in plan tree dump */
	/* don't write right_ec, leads to infinite recursion in plan tree dump */
	WRITE_NODE_FIELD(left_em);
	WRITE_NODE_FIELD(right_em);
	WRITE_BOOL_FIELD(outer_is_left);
	WRITE_OID_FIELD(hashjoinoperator);
}

static void
_strInnerIndexscanInfo(StringInfo str, InnerIndexscanInfo *node)
{
	WRITE_NODE_TYPE("INNERINDEXSCANINFO");
	WRITE_BITMAPSET_FIELD(other_relids);
	WRITE_BOOL_FIELD(isouterjoin);
	WRITE_NODE_FIELD(cheapest_startup_innerpath);
	WRITE_NODE_FIELD(cheapest_total_innerpath);
}

static void
_strOuterJoinInfo(StringInfo str, OuterJoinInfo *node)
{
	WRITE_NODE_TYPE("OUTERJOININFO");

	WRITE_BITMAPSET_FIELD(min_lefthand);
	WRITE_BITMAPSET_FIELD(min_righthand);
	WRITE_BITMAPSET_FIELD(syn_lefthand);
	WRITE_BITMAPSET_FIELD(syn_righthand);
	WRITE_BOOL_FIELD(is_full_join);
	WRITE_BOOL_FIELD(lhs_strict);
	WRITE_BOOL_FIELD(delay_upper_joins);
}

static void
_strInClauseInfo(StringInfo str, InClauseInfo *node)
{
	WRITE_NODE_TYPE("INCLAUSEINFO");

	WRITE_BITMAPSET_FIELD(lefthand);
	WRITE_BITMAPSET_FIELD(righthand);
	WRITE_NODE_FIELD(sub_targetlist);
	WRITE_NODE_FIELD(in_operators);
}

static void
_strAppendRelInfo(StringInfo str, AppendRelInfo *node)
{
	WRITE_NODE_TYPE("APPENDRELINFO");

	WRITE_UINT_FIELD(parent_relid);
	WRITE_UINT_FIELD(child_relid);
	WRITE_OID_FIELD(parent_reltype);
	WRITE_OID_FIELD(child_reltype);
	WRITE_NODE_FIELD(col_mappings);
	WRITE_NODE_FIELD(translated_vars);
	WRITE_OID_FIELD(parent_reloid);
}

static void
_strPlannerParamItem(StringInfo str, PlannerParamItem *node)
{
	WRITE_NODE_TYPE("PLANNERPARAMITEM");

	WRITE_NODE_FIELD(item);
	WRITE_UINT_FIELD(abslevel);
}

/*****************************************************************************
 *
 *	Stuff from parsenodes.h.
 *
 *****************************************************************************/

static void
_strCreateStmt(StringInfo str, CreateStmt *node)
{
	WRITE_NODE_TYPE("CREATESTMT");

	WRITE_NODE_FIELD(relation);
	WRITE_NODE_FIELD(tableElts);
	WRITE_NODE_FIELD(inhRelations);
	WRITE_NODE_FIELD(constraints);
	WRITE_NODE_FIELD(options);
	WRITE_ENUM_FIELD(oncommit, OnCommitAction);
	WRITE_STRING_FIELD(tablespacename);
}

static void
_strIndexStmt(StringInfo str, IndexStmt *node)
{
	WRITE_NODE_TYPE("INDEXSTMT");

	WRITE_STRING_FIELD(idxname);
	WRITE_NODE_FIELD(relation);
	WRITE_STRING_FIELD(accessMethod);
	WRITE_STRING_FIELD(tableSpace);
	WRITE_NODE_FIELD(indexParams);
	WRITE_NODE_FIELD(options);
	WRITE_NODE_FIELD(whereClause);
	WRITE_BOOL_FIELD(unique);
	WRITE_BOOL_FIELD(primary);
	WRITE_BOOL_FIELD(isconstraint);
	WRITE_BOOL_FIELD(concurrent);
}

static void
_strNotifyStmt(StringInfo str, NotifyStmt *node)
{
	WRITE_NODE_TYPE("NOTIFY");

	WRITE_NODE_FIELD(relation);
}

static void
_strDeclareCursorStmt(StringInfo str, DeclareCursorStmt *node)
{
	WRITE_NODE_TYPE("DECLARECURSOR");

	WRITE_STRING_FIELD(portalname);
	WRITE_INT_FIELD(options);
	WRITE_NODE_FIELD(query);
}

static void
_strSelectStmt(StringInfo str, SelectStmt *node)
{
	WRITE_NODE_TYPE("SELECT");

	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(intoClause);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(fromClause);
	WRITE_NODE_FIELD(whereClause);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingClause);
	WRITE_NODE_FIELD(valuesLists);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(lockingClause);
	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(provenanceClause);
}

static void
_strFuncCall(StringInfo str, FuncCall *node)
{
	WRITE_NODE_TYPE("FUNCCALL");

	WRITE_NODE_FIELD(funcname);
	WRITE_NODE_FIELD(args);
	WRITE_BOOL_FIELD(agg_star);
	WRITE_BOOL_FIELD(agg_distinct);
	WRITE_INT_FIELD(location);
}

static void
_strDefElem(StringInfo str, DefElem *node)
{
	WRITE_NODE_TYPE("DEFELEM");

	WRITE_STRING_FIELD(defname);
	WRITE_NODE_FIELD(arg);
}

static void
_strLockingClause(StringInfo str, LockingClause *node)
{
	WRITE_NODE_TYPE("LOCKINGCLAUSE");

	WRITE_NODE_FIELD(lockedRels);
	WRITE_BOOL_FIELD(forUpdate);
	WRITE_BOOL_FIELD(noWait);
}

static void
_strXmlSerialize(StringInfo str, XmlSerialize *node)
{
	WRITE_NODE_TYPE("XMLSERIALIZE");

	WRITE_ENUM_FIELD(xmloption, XmlOptionType);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(typename);
}

static void
_strColumnDef(StringInfo str, ColumnDef *node)
{
	WRITE_NODE_TYPE("COLUMNDEF");

	WRITE_STRING_FIELD(colname);
	WRITE_NODE_FIELD(typename);
	WRITE_INT_FIELD(inhcount);
	WRITE_BOOL_FIELD(is_local);
	WRITE_BOOL_FIELD(is_not_null);
	WRITE_NODE_FIELD(raw_default);
	WRITE_STRING_FIELD(cooked_default);
	WRITE_NODE_FIELD(constraints);
}

static void
_strTypeName(StringInfo str, TypeName *node)
{
	WRITE_NODE_TYPE("TYPENAME");

	WRITE_NODE_FIELD(names);
	WRITE_OID_FIELD(typeid);
	WRITE_BOOL_FIELD(timezone);
	WRITE_BOOL_FIELD(setof);
	WRITE_BOOL_FIELD(pct_type);
	WRITE_NODE_FIELD(typmods);
	WRITE_INT_FIELD(typemod);
	WRITE_NODE_FIELD(arrayBounds);
	WRITE_INT_FIELD(location);
}

static void
_strTypeCast(StringInfo str, TypeCast *node)
{
	WRITE_NODE_TYPE("TYPECAST");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(typename);
}

static void
_strIndexElem(StringInfo str, IndexElem *node)
{
	WRITE_NODE_TYPE("INDEXELEM");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(expr);
	WRITE_NODE_FIELD(opclass);
	WRITE_ENUM_FIELD(ordering, SortByDir);
	WRITE_ENUM_FIELD(nulls_ordering, SortByNulls);
}

static void
_strQuery(StringInfo str, Query *node)
{
	WRITE_NODE_TYPE("QUERY");

	WRITE_ENUM_FIELD(commandType, CmdType);
	WRITE_ENUM_FIELD(querySource, QuerySource);
	WRITE_BOOL_FIELD(canSetTag);

	/*
	 * Hack to work around missing outfuncs routines for a lot of the
	 * utility-statement node types.  (The only one we actually *need* for
	 * rules support is NotifyStmt.)  Someday we ought to support 'em all, but
	 * for the meantime do this to avoid getting lots of warnings when running
	 * with debug_print_parse on.
	 */
	if (node->utilityStmt)
	{
		switch (nodeTag(node->utilityStmt))
		{
			case T_CreateStmt:
			case T_IndexStmt:
			case T_NotifyStmt:
			case T_DeclareCursorStmt:
				WRITE_NODE_FIELD(utilityStmt);
				break;
			default:
				appendStringInfo(str, " :utilityStmt ?");
				break;
		}
	}
	else
		appendStringInfo(str, " :utilityStmt <>");

	WRITE_INT_FIELD(resultRelation);
	WRITE_NODE_FIELD(intoClause);
	WRITE_BOOL_FIELD(hasAggs);
	WRITE_BOOL_FIELD(hasSubLinks);
	WRITE_NODE_FIELD(rtable);
	WRITE_NODE_FIELD(jointree);
	WRITE_NODE_FIELD(targetList);
	WRITE_NODE_FIELD(returningList);
	WRITE_NODE_FIELD(groupClause);
	WRITE_NODE_FIELD(havingQual);
	WRITE_NODE_FIELD(distinctClause);
	WRITE_NODE_FIELD(sortClause);
	WRITE_NODE_FIELD(limitOffset);
	WRITE_NODE_FIELD(limitCount);
	WRITE_NODE_FIELD(rowMarks);
	WRITE_NODE_FIELD(setOperations);
	WRITE_NODE_FIELD(provInfo);
}

static void
_strSortClause(StringInfo str, SortClause *node)
{
	WRITE_NODE_TYPE("SORTCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
	WRITE_BOOL_FIELD(nulls_first);
}

static void
_strGroupClause(StringInfo str, GroupClause *node)
{
	WRITE_NODE_TYPE("GROUPCLAUSE");

	WRITE_UINT_FIELD(tleSortGroupRef);
	WRITE_OID_FIELD(sortop);
	WRITE_BOOL_FIELD(nulls_first);
}

static void
_strRowMarkClause(StringInfo str, RowMarkClause *node)
{
	WRITE_NODE_TYPE("ROWMARKCLAUSE");

	WRITE_UINT_FIELD(rti);
	WRITE_BOOL_FIELD(forUpdate);
	WRITE_BOOL_FIELD(noWait);
}

static void
_strSetOperationStmt(StringInfo str, SetOperationStmt *node)
{
	WRITE_NODE_TYPE("SETOPERATIONSTMT");

	WRITE_ENUM_FIELD(op, SetOperation);
	WRITE_BOOL_FIELD(all);
	WRITE_NODE_FIELD(larg);
	WRITE_NODE_FIELD(rarg);
	WRITE_NODE_FIELD(colTypes);
	WRITE_NODE_FIELD(colTypmods);
}

static void
_strRangeTblEntry(StringInfo str, RangeTblEntry *node)
{
	WRITE_NODE_TYPE("RTE");

	/* put alias + eref first to make dump more legible */
	WRITE_NODE_FIELD(alias);
	WRITE_NODE_FIELD(eref);
	WRITE_ENUM_FIELD(rtekind, RTEKind);

	switch (node->rtekind)
	{
		case RTE_RELATION:
		case RTE_SPECIAL:
			WRITE_OID_FIELD(relid);
			break;
		case RTE_SUBQUERY:
			WRITE_NODE_FIELD(subquery);
			break;
		case RTE_FUNCTION:
			WRITE_NODE_FIELD(funcexpr);
			WRITE_NODE_FIELD(funccoltypes);
			WRITE_NODE_FIELD(funccoltypmods);
			break;
		case RTE_VALUES:
			WRITE_NODE_FIELD(values_lists);
			break;
		case RTE_JOIN:
			WRITE_ENUM_FIELD(jointype, JoinType);
			WRITE_NODE_FIELD(joinaliasvars);
			break;
		default:
			elog(ERROR, "unrecognized RTE kind: %d", (int) node->rtekind);
			break;
	}

	WRITE_BOOL_FIELD(inh);
	WRITE_BOOL_FIELD(inFromCl);
	WRITE_UINT_FIELD(requiredPerms);
	WRITE_OID_FIELD(checkAsUser);
	WRITE_BOOL_FIELD(isProvBase);
	WRITE_NODE_FIELD(provAttrs);
	WRITE_NODE_FIELD(annotations);
}

static void
_strAExpr(StringInfo str, A_Expr *node)
{
	WRITE_NODE_TYPE("AEXPR");

	switch (node->kind)
	{
		case AEXPR_OP:
			appendStringInfo(str, " ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_AND:
			appendStringInfo(str, " AND");
			break;
		case AEXPR_OR:
			appendStringInfo(str, " OR");
			break;
		case AEXPR_NOT:
			appendStringInfo(str, " NOT");
			break;
		case AEXPR_OP_ANY:
			appendStringInfo(str, " ");
			WRITE_NODE_FIELD(name);
			appendStringInfo(str, " ANY ");
			break;
		case AEXPR_OP_ALL:
			appendStringInfo(str, " ");
			WRITE_NODE_FIELD(name);
			appendStringInfo(str, " ALL ");
			break;
		case AEXPR_DISTINCT:
			appendStringInfo(str, " DISTINCT ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_NULLIF:
			appendStringInfo(str, " NULLIF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_OF:
			appendStringInfo(str, " OF ");
			WRITE_NODE_FIELD(name);
			break;
		case AEXPR_IN:
			appendStringInfo(str, " IN ");
			WRITE_NODE_FIELD(name);
			break;
		default:
			appendStringInfo(str, " ??");
			break;
	}

	WRITE_NODE_FIELD(lexpr);
	WRITE_NODE_FIELD(rexpr);
	WRITE_INT_FIELD(location);
}

static void
_strValue(StringInfo str, Value *value)
{
	switch (value->type)
	{
		case T_Integer:
			appendStringInfo(str, "%ld", value->val.ival);
			break;
		case T_Float:

			/*
			 * We assume the value is a valid numeric literal and so does not
			 * need quoting.
			 */
			appendStringInfoString(str, value->val.str);
			break;
		case T_String:
			appendStringInfoChar(str, '"');
			_strToken(str, value->val.str);
			appendStringInfoChar(str, '"');
			break;
		case T_BitString:
			/* internal representation already has leading 'b' */
			appendStringInfoString(str, value->val.str);
			break;
		case T_Null:
			/* this is seen only within A_Const, not in transformed trees */
			appendStringInfoString(str, "NULL");
			break;
		default:
			elog(ERROR, "unrecognized node type: %d", (int) value->type);
			break;
	}
}

static void
_strColumnRef(StringInfo str, ColumnRef *node)
{
	WRITE_NODE_TYPE("COLUMNREF");

	WRITE_NODE_FIELD(fields);
	WRITE_INT_FIELD(location);
}

static void
_strParamRef(StringInfo str, ParamRef *node)
{
	WRITE_NODE_TYPE("PARAMREF");

	WRITE_INT_FIELD(number);
}

static void
_strAConst(StringInfo str, A_Const *node)
{
	WRITE_NODE_TYPE("A_CONST");

	appendStringInfo(str, " :val ");
	_strValue(str, &(node->val));
	WRITE_NODE_FIELD(typename);
}

static void
_strA_Indices(StringInfo str, A_Indices *node)
{
	WRITE_NODE_TYPE("A_INDICES");

	WRITE_NODE_FIELD(lidx);
	WRITE_NODE_FIELD(uidx);
}

static void
_strA_Indirection(StringInfo str, A_Indirection *node)
{
	WRITE_NODE_TYPE("A_INDIRECTION");

	WRITE_NODE_FIELD(arg);
	WRITE_NODE_FIELD(indirection);
}

static void
_strResTarget(StringInfo str, ResTarget *node)
{
	WRITE_NODE_TYPE("RESTARGET");

	WRITE_STRING_FIELD(name);
	WRITE_NODE_FIELD(indirection);
	WRITE_NODE_FIELD(val);
	WRITE_INT_FIELD(location);
}

static void
_strConstraint(StringInfo str, Constraint *node)
{
	WRITE_NODE_TYPE("CONSTRAINT");

	WRITE_STRING_FIELD(name);

	appendStringInfo(str, " :contype ");
	switch (node->contype)
	{
		case CONSTR_PRIMARY:
			appendStringInfo(str, "PRIMARY_KEY");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexspace);
			break;

		case CONSTR_UNIQUE:
			appendStringInfo(str, "UNIQUE");
			WRITE_NODE_FIELD(keys);
			WRITE_NODE_FIELD(options);
			WRITE_STRING_FIELD(indexspace);
			break;

		case CONSTR_CHECK:
			appendStringInfo(str, "CHECK");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_DEFAULT:
			appendStringInfo(str, "DEFAULT");
			WRITE_NODE_FIELD(raw_expr);
			WRITE_STRING_FIELD(cooked_expr);
			break;

		case CONSTR_NOTNULL:
			appendStringInfo(str, "NOT_NULL");
			break;

		default:
			appendStringInfo(str, "<unrecognized_constraint>");
			break;
	}
}

static void
_strFkConstraint(StringInfo str, FkConstraint *node)
{
	WRITE_NODE_TYPE("FKCONSTRAINT");

	WRITE_STRING_FIELD(constr_name);
	WRITE_NODE_FIELD(pktable);
	WRITE_NODE_FIELD(fk_attrs);
	WRITE_NODE_FIELD(pk_attrs);
	WRITE_CHAR_FIELD(fk_matchtype);
	WRITE_CHAR_FIELD(fk_upd_action);
	WRITE_CHAR_FIELD(fk_del_action);
	WRITE_BOOL_FIELD(deferrable);
	WRITE_BOOL_FIELD(initdeferred);
	WRITE_BOOL_FIELD(skip_validation);
}


// ================================
// quanpt

/* Write the label for the node type */
#define STR_KEYWORD(keyword) \
		appendStringInfoString(str, keyword " ")

#define STR_NODE(fldname) \
		_strNode(str, node->fldname)

#define STR_MULTINODE(fldname) \
		_strNode(str, node->fldname)

static void
_strInsertStmt(StringInfo str, InsertStmt *node) {
	STR_KEYWORD("INSERT INTO");
	STR_NODE(relation);
	STR_MULTINODE(cols);
	_strSelectStmt(str, node->selectStmt);
	if (node->returningList != NULL && node->returningList->length > 0) {
		STR_KEYWORD("RETURNING");
		STR_MULTINODE(returningList);
	}
}

// ================================

/*
 * _strNode -
 *	  converts a Node into ascii string and append it to 'str'
 */
void
_strNode(StringInfo str, void *obj)
{
	if (obj == NULL)
		appendStringInfo(str, "<>");
	else if (IsA(obj, List) ||IsA(obj, IntList) || IsA(obj, OidList))
		_strList(str, obj);
	else if (IsA(obj, Integer) ||
			 IsA(obj, Float) ||
			 IsA(obj, String) ||
			 IsA(obj, BitString))
	{
		/* nodeRead does not want to see { } around these! */
		_strValue(str, obj);
	}
	else
	{
		appendStringInfoChar(str, '{');
		switch (nodeTag(obj))
		{
			case T_PlannedStmt:
				_strPlannedStmt(str, obj);
				break;
			case T_Plan:
				_strPlan(str, obj);
				break;
			case T_Result:
				_strResult(str, obj);
				break;
			case T_Append:
				_strAppend(str, obj);
				break;
			case T_BitmapAnd:
				_strBitmapAnd(str, obj);
				break;
			case T_BitmapOr:
				_strBitmapOr(str, obj);
				break;
			case T_Scan:
				_strScan(str, obj);
				break;
			case T_SeqScan:
				_strSeqScan(str, obj);
				break;
			case T_IndexScan:
				_strIndexScan(str, obj);
				break;
			case T_BitmapIndexScan:
				_strBitmapIndexScan(str, obj);
				break;
			case T_BitmapHeapScan:
				_strBitmapHeapScan(str, obj);
				break;
			case T_TidScan:
				_strTidScan(str, obj);
				break;
			case T_SubqueryScan:
				_strSubqueryScan(str, obj);
				break;
			case T_FunctionScan:
				_strFunctionScan(str, obj);
				break;
			case T_ValuesScan:
				_strValuesScan(str, obj);
				break;
			case T_Join:
				_strJoin(str, obj);
				break;
			case T_NestLoop:
				_strNestLoop(str, obj);
				break;
			case T_MergeJoin:
				_strMergeJoin(str, obj);
				break;
			case T_HashJoin:
				_strHashJoin(str, obj);
				break;
			case T_Agg:
				_strAgg(str, obj);
				break;
			case T_Group:
				_strGroup(str, obj);
				break;
			case T_Material:
				_strMaterial(str, obj);
				break;
			case T_Sort:
				_strSort(str, obj);
				break;
			case T_Unique:
				_strUnique(str, obj);
				break;
			case T_SetOp:
				_strSetOp(str, obj);
				break;
			case T_Limit:
				_strLimit(str, obj);
				break;
			case T_Hash:
				_strHash(str, obj);
				break;
			case T_Alias:
				_strAlias(str, obj);
				break;
			case T_RangeVar:
				_strRangeVar(str, obj);
				break;
			case T_IntoClause:
				_strIntoClause(str, obj);
				break;
			case T_Var:
				_strVar(str, obj);
				break;
			case T_Const:
				_strConst(str, obj);
				break;
			case T_Param:
				_strParam(str, obj);
				break;
			case T_Aggref:
				_strAggref(str, obj);
				break;
			case T_ArrayRef:
				_strArrayRef(str, obj);
				break;
			case T_FuncExpr:
				_strFuncExpr(str, obj);
				break;
			case T_OpExpr:
				_strOpExpr(str, obj);
				break;
			case T_DistinctExpr:
				_strDistinctExpr(str, obj);
				break;
			case T_ScalarArrayOpExpr:
				_strScalarArrayOpExpr(str, obj);
				break;
			case T_BoolExpr:
				_strBoolExpr(str, obj);
				break;
			case T_SubLink:
				_strSubLink(str, obj);
				break;
			case T_SubPlan:
				_strSubPlan(str, obj);
				break;
			case T_FieldSelect:
				_strFieldSelect(str, obj);
				break;
			case T_FieldStore:
				_strFieldStore(str, obj);
				break;
			case T_RelabelType:
				_strRelabelType(str, obj);
				break;
			case T_CoerceViaIO:
				_strCoerceViaIO(str, obj);
				break;
			case T_ArrayCoerceExpr:
				_strArrayCoerceExpr(str, obj);
				break;
			case T_ConvertRowtypeExpr:
				_strConvertRowtypeExpr(str, obj);
				break;
			case T_CaseExpr:
				_strCaseExpr(str, obj);
				break;
			case T_CaseWhen:
				_strCaseWhen(str, obj);
				break;
			case T_CaseTestExpr:
				_strCaseTestExpr(str, obj);
				break;
			case T_ArrayExpr:
				_strArrayExpr(str, obj);
				break;
			case T_RowExpr:
				_strRowExpr(str, obj);
				break;
			case T_RowCompareExpr:
				_strRowCompareExpr(str, obj);
				break;
			case T_CoalesceExpr:
				_strCoalesceExpr(str, obj);
				break;
			case T_MinMaxExpr:
				_strMinMaxExpr(str, obj);
				break;
			case T_XmlExpr:
				_strXmlExpr(str, obj);
				break;
			case T_NullIfExpr:
				_strNullIfExpr(str, obj);
				break;
			case T_NullTest:
				_strNullTest(str, obj);
				break;
			case T_BooleanTest:
				_strBooleanTest(str, obj);
				break;
			case T_CoerceToDomain:
				_strCoerceToDomain(str, obj);
				break;
			case T_CoerceToDomainValue:
				_strCoerceToDomainValue(str, obj);
				break;
			case T_SetToDefault:
				_strSetToDefault(str, obj);
				break;
			case T_CurrentOfExpr:
				_strCurrentOfExpr(str, obj);
				break;
			case T_TargetEntry:
				_strTargetEntry(str, obj);
				break;
			case T_RangeTblRef:
				_strRangeTblRef(str, obj);
				break;
			case T_JoinExpr:
				_strJoinExpr(str, obj);
				break;
			case T_FromExpr:
				_strFromExpr(str, obj);
				break;

			case T_Path:
				_strPath(str, obj);
				break;
			case T_IndexPath:
				_strIndexPath(str, obj);
				break;
			case T_BitmapHeapPath:
				_strBitmapHeapPath(str, obj);
				break;
			case T_BitmapAndPath:
				_strBitmapAndPath(str, obj);
				break;
			case T_BitmapOrPath:
				_strBitmapOrPath(str, obj);
				break;
			case T_TidPath:
				_strTidPath(str, obj);
				break;
			case T_AppendPath:
				_strAppendPath(str, obj);
				break;
			case T_ResultPath:
				_strResultPath(str, obj);
				break;
			case T_MaterialPath:
				_strMaterialPath(str, obj);
				break;
			case T_UniquePath:
				_strUniquePath(str, obj);
				break;
			case T_NestPath:
				_strNestPath(str, obj);
				break;
			case T_MergePath:
				_strMergePath(str, obj);
				break;
			case T_HashPath:
				_strHashPath(str, obj);
				break;
			case T_PlannerGlobal:
				_strPlannerGlobal(str, obj);
				break;
			case T_PlannerInfo:
				_strPlannerInfo(str, obj);
				break;
			case T_RelOptInfo:
				_strRelOptInfo(str, obj);
				break;
			case T_IndexOptInfo:
				_strIndexOptInfo(str, obj);
				break;
			case T_EquivalenceClass:
				_strEquivalenceClass(str, obj);
				break;
			case T_EquivalenceMember:
				_strEquivalenceMember(str, obj);
				break;
			case T_PathKey:
				_strPathKey(str, obj);
				break;
			case T_RestrictInfo:
				_strRestrictInfo(str, obj);
				break;
			case T_InnerIndexscanInfo:
				_strInnerIndexscanInfo(str, obj);
				break;
			case T_OuterJoinInfo:
				_strOuterJoinInfo(str, obj);
				break;
			case T_InClauseInfo:
				_strInClauseInfo(str, obj);
				break;
			case T_AppendRelInfo:
				_strAppendRelInfo(str, obj);
				break;
			case T_PlannerParamItem:
				_strPlannerParamItem(str, obj);
				break;

			case T_CreateStmt:
				_strCreateStmt(str, obj);
				break;
			case T_IndexStmt:
				_strIndexStmt(str, obj);
				break;
			case T_NotifyStmt:
				_strNotifyStmt(str, obj);
				break;
			case T_DeclareCursorStmt:
				_strDeclareCursorStmt(str, obj);
				break;
			case T_SelectStmt:
				_strSelectStmt(str, obj);
				break;
			case T_ColumnDef:
				_strColumnDef(str, obj);
				break;
			case T_TypeName:
				_strTypeName(str, obj);
				break;
			case T_TypeCast:
				_strTypeCast(str, obj);
				break;
			case T_IndexElem:
				_strIndexElem(str, obj);
				break;
			case T_Query:
				_strQuery(str, obj);
				break;
			case T_SortClause:
				_strSortClause(str, obj);
				break;
			case T_GroupClause:
				_strGroupClause(str, obj);
				break;
			case T_RowMarkClause:
				_strRowMarkClause(str, obj);
				break;
			case T_SetOperationStmt:
				_strSetOperationStmt(str, obj);
				break;
			case T_RangeTblEntry:
				_strRangeTblEntry(str, obj);
				break;
			case T_A_Expr:
				_strAExpr(str, obj);
				break;
			case T_ColumnRef:
				_strColumnRef(str, obj);
				break;
			case T_ParamRef:
				_strParamRef(str, obj);
				break;
			case T_A_Const:
				_strAConst(str, obj);
				break;
			case T_A_Indices:
				_strA_Indices(str, obj);
				break;
			case T_A_Indirection:
				_strA_Indirection(str, obj);
				break;
			case T_ResTarget:
				_strResTarget(str, obj);
				break;
			case T_Constraint:
				_strConstraint(str, obj);
				break;
			case T_FkConstraint:
				_strFkConstraint(str, obj);
				break;
			case T_FuncCall:
				_strFuncCall(str, obj);
				break;
			case T_DefElem:
				_strDefElem(str, obj);
				break;
			case T_LockingClause:
				_strLockingClause(str, obj);
				break;
			case T_XmlSerialize:
				_strXmlSerialize(str, obj);
				break;
			// quanpt
			case T_InsertStmt:
				_strInsertStmt(str, obj);
				break;
			default:
				outProvNode(str, obj);
				break;
		}
		appendStringInfoChar(str, '}');
	}
}

/*
 * nodeToString -
 *	   returns the ascii representation of the Node as a palloc'd string
 */
char *
nodeToSql(void *obj)
{
	StringInfoData str;

	/* see stringinfo.h for an explanation of this maneuver */
	initStringInfo(&str);
	_strNode(&str, obj);
	return str.data;
}

