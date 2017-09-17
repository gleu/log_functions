/*-------------------------------------------------------------------------
 *
 *  log_functions.c
 *  Log steps and/or statements inside a PL/pgsql function.
 *
 *
 *  Copyright (c) 2011-2017, Guillaume Lelarge (Dalibo),
 *  guillaume.lelarge@dalibo.com
 *
 *-------------------------------------------------------------------------
 */


/**********************************************************************
 * Headers
 **********************************************************************/

#include <stdio.h>
#include "postgres.h"
#if PG_VERSION_NUM >= 90300
#include "access/htup_details.h"
#endif
#include "utils/syscache.h"
#include "catalog/pg_proc.h"
#include "plpgsql.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

/**********************************************************************
 * GUC variables
 **********************************************************************/

static bool	log_functions_log_declare = true;
static bool	log_functions_log_function_begin = true;
static bool	log_functions_log_function_end = true;
static bool	log_functions_log_statement_begin = false;
static bool	log_functions_log_statement_end = false;

/**********************************************************************
 * Function prototypes
 **********************************************************************/

void load_plugin( PLpgSQL_plugin * hooks );
static void profiler_init(PLpgSQL_execstate * estate, PLpgSQL_function * func);
static void profiler_func_beg(PLpgSQL_execstate * estate, PLpgSQL_function * func);
static void profiler_func_end(PLpgSQL_execstate * estate, PLpgSQL_function * func);
static void profiler_stmt_beg(PLpgSQL_execstate * estate, PLpgSQL_stmt * stmt);
static void profiler_stmt_end(PLpgSQL_execstate * estate, PLpgSQL_stmt * stmt);
char *decode_stmt_type(int typ);

/**********************************************************************
 * Other variables
 **********************************************************************/

static PLpgSQL_plugin plugin_funcs = { profiler_init, profiler_func_beg, profiler_func_end, profiler_stmt_beg, profiler_stmt_end };

/**********************************************************************
 * Function definitions
 **********************************************************************/

/* -------------------------------------------------------------------
 * _PG_init()
 * ------------------------------------------------------------------*/

void _PG_init( void )
{
	PLpgSQL_plugin ** var_ptr = (PLpgSQL_plugin **) find_rendezvous_variable("PLpgSQL_plugin");

	*var_ptr = &plugin_funcs;

	/* Define custom GUC variables. */
	DefineCustomBoolVariable("log_functions.log_declare",
							 "Logs the start of the DECLARE block.",
							 NULL,
							 &log_functions_log_declare,
							 true,
							 PGC_SUSET,
							 0,
#if PG_VERSION_NUM >= 90100
				NULL,
#endif
							 NULL,
							 NULL);
	DefineCustomBoolVariable("log_functions.log_function_begin",
							 "Logs the start of the BEGIN/END block.",
							 NULL,
							 &log_functions_log_function_begin,
							 true,
							 PGC_SUSET,
							 0,
#if PG_VERSION_NUM >= 90100
				NULL,
#endif
							 NULL,
							 NULL);
	DefineCustomBoolVariable("log_functions.log_function_end",
							 "Logs the end of the BEGIN/END block.",
							 NULL,
							 &log_functions_log_function_end,
							 true,
							 PGC_SUSET,
							 0,
#if PG_VERSION_NUM >= 90100
				NULL,
#endif
							 NULL,
							 NULL);
	DefineCustomBoolVariable("log_functions.log_statement_begin",
							 "Logs the start of a statement.",
							 NULL,
							 &log_functions_log_statement_begin,
							 false,
							 PGC_SUSET,
							 0,
#if PG_VERSION_NUM >= 90100
				NULL,
#endif
							 NULL,
							 NULL);
	DefineCustomBoolVariable("log_functions.log_statement_end",
							 "Logs the end of a statement.",
							 NULL,
							 &log_functions_log_statement_end,
							 false,
							 PGC_SUSET,
							 0,
#if PG_VERSION_NUM >= 90100
				NULL,
#endif
							 NULL,
							 NULL);
}

/* -------------------------------------------------------------------
 * load_plugin()
 * ------------------------------------------------------------------*/

void load_plugin( PLpgSQL_plugin * hooks )
{
	hooks->func_setup = profiler_init;
	hooks->func_beg   = profiler_func_beg;
	hooks->func_end   = profiler_func_end;
	hooks->stmt_beg   = profiler_stmt_beg;
	hooks->stmt_end   = profiler_stmt_end;
}

/* -------------------------------------------------------------------
 * findProcName()
 * ------------------------------------------------------------------*/

static char * findProcName(Oid oid)
{
	HeapTuple	procTuple;
	char       *procName;

	procTuple = SearchSysCache(PROCOID, ObjectIdGetDatum(oid), 0, 0, 0);

	if(!HeapTupleIsValid(procTuple))
		elog(ERROR, "log_functions: cache lookup for proc %u failed", oid);

	procName = NameStr(((Form_pg_proc) GETSTRUCT(procTuple))->proname);
	ReleaseSysCache(procTuple);

	return procName;
}

/* -------------------------------------------------------------------
 * profiler_init()
 * ------------------------------------------------------------------*/

static void profiler_init( PLpgSQL_execstate * estate, PLpgSQL_function * func )
{
	if (log_functions_log_declare)
		elog(LOG, "log_functions, DECLARE, %s", findProcName(func->fn_oid));
}

/* -------------------------------------------------------------------
 * profiler_func_beg()
 * ------------------------------------------------------------------*/

static void profiler_func_beg( PLpgSQL_execstate * estate, PLpgSQL_function * func )
{
	if (log_functions_log_function_begin)
		elog(LOG, "log_functions, BEGIN, %s", findProcName(func->fn_oid));
}

/* -------------------------------------------------------------------
 * profiler_func_end()
 * ------------------------------------------------------------------*/

static void profiler_func_end( PLpgSQL_execstate * estate, PLpgSQL_function * func )
{
	if (log_functions_log_function_end)
		elog(LOG, "log_functions, END, %s", findProcName(func->fn_oid));
}

/* -------------------------------------------------------------------
 * profiler_stmt_beg()
 * ------------------------------------------------------------------*/

static void profiler_stmt_beg( PLpgSQL_execstate * estate, PLpgSQL_stmt * stmt )
{
	if (log_functions_log_statement_begin)
		elog(LOG, "log_functions, STMT START, line %d, type %s", stmt->lineno, decode_stmt_type(stmt->cmd_type));
}

/* -------------------------------------------------------------------
 * profiler_stmt_end()
 * ------------------------------------------------------------------*/

static void profiler_stmt_end( PLpgSQL_execstate * estate, PLpgSQL_stmt * stmt )
{
	if (log_functions_log_statement_end)
		elog(LOG, "log_functions, STMT STOP, line %d, type %s", stmt->lineno, decode_stmt_type(stmt->cmd_type));
}

/* -------------------------------------------------------------------
 * decode_stmt_type()
 * ------------------------------------------------------------------*/

char *decode_stmt_type(int typ)
{
	char *decoded_type = "unknown";

    switch (typ)
    {    
        case PLPGSQL_STMT_BLOCK:
            decoded_type = "BLOCK";
            break;

        case PLPGSQL_STMT_ASSIGN:
            decoded_type = "ASSIGN";
            break;

        case PLPGSQL_STMT_PERFORM:
            decoded_type = "PERFORM";
            break;

        case PLPGSQL_STMT_GETDIAG:
            decoded_type = "GETDIAG";
            break;

        case PLPGSQL_STMT_IF:
            decoded_type = "IF";
            break;

        case PLPGSQL_STMT_CASE:
            decoded_type = "CASE";
            break;

        case PLPGSQL_STMT_LOOP:
            decoded_type = "LOOP";
            break;

        case PLPGSQL_STMT_WHILE:
            decoded_type = "WHILE";
            break;

        case PLPGSQL_STMT_FORI:
            decoded_type = "FORI";
            break;

        case PLPGSQL_STMT_FORS:
            decoded_type = "FORS";
            break;

        case PLPGSQL_STMT_FORC:
            decoded_type = "FORC";
            break;

        case PLPGSQL_STMT_EXIT:
            decoded_type = "EXIT";
            break;

        case PLPGSQL_STMT_RETURN:
            decoded_type = "RETURN";
            break;

        case PLPGSQL_STMT_RETURN_NEXT:
            decoded_type = "RETURN NEXT";
            break;

        case PLPGSQL_STMT_RETURN_QUERY:
            decoded_type = "RETURN QUERY";
            break;

        case PLPGSQL_STMT_RAISE:
            decoded_type = "RAISE";
            break;

        case PLPGSQL_STMT_EXECSQL:
            decoded_type = "EXEC SQL";
            break;

        case PLPGSQL_STMT_DYNEXECUTE:
            decoded_type = "DYNEXECUTE";
            break;

        case PLPGSQL_STMT_DYNFORS:
            decoded_type = "DYNFORS";
            break;

        case PLPGSQL_STMT_OPEN:
            decoded_type = "OPEN";
            break;

        case PLPGSQL_STMT_FETCH:
            decoded_type = "FETCH";
            break;

        case PLPGSQL_STMT_CLOSE:
            decoded_type = "CLOSE";
            break;
    }

    return decoded_type;
}
