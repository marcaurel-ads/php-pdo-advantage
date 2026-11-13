/* ====================================================
 *
 *       (c) 2014 SAP AG or an SAP affiliate company. All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 *
 * ====================================================
 */

/* check for config, and import */
#ifdef HAVE_CONFIG_H
   #include "config.h"
#endif


/* INCLUDES */
#include "ctype.h"
#include "stdio.h"
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "pdo/php_pdo.h"
#include "pdo/php_pdo_driver.h"
#include "php_pdo_advantage.h"
#include "php_pdo_advantage_int.h"
#include "zend_exceptions.h"

ZEND_DECLARE_MODULE_GLOBALS(pdo_advantage);



/*  {{{ string_has_field
 *
 *  Returns 1 if we can find the field, 0 if we can't.
 */
static int string_has_field(const char* str, const int len, const char* field, int lenf TSRMLS_DC)
{
   int ret, i, stage;
   DEBUG_ENT(string_has_field);

   /*
    *   We can brute force through the string, if we assume
    *   the string is fairly short.
    */
   i = 0;
   stage = 0;
   ret = 0;
   /*
    *   There are lenf + 1 stages that must have been passed
    *   for the field to exist in the string.
    *
    *   Stage -1:
    *   We are currently searching through another field/value.
    *   Can't progress until we see ';'
    *
    *   Stage 0:
    *   We are starting a new field. Look for the first char
    *   of field. Ignore whitespace.
    *
    *   Stages 1..lenf - 1:
    *   Compare field[stage] with str[i]. If they are the same,
    *   progress. Otherwise go to -1;
    *
    *   Stage -2:
    *   If there's an = sign, we found the field. Ignore whitespace.
    */
   while ( i < len )
      {
      switch ( stage )
         {
         case -1:
            if ( str[i] == ';' ) stage++;
            break;
         case 0:
            if ( tolower(str[i]) == tolower(field[0]) )
               { /* found first char */
               stage++;
               }
            else if ( strchr(" \n\t", str[i]) )
               { /* whitespace. Ignore. */
               }
            else if ( str[i] == ';' )
               { /* reset to new field */
               stage = 0;
               }
            else
               { /* character not same as field pos */
               stage = -1;
               }
            break;
         case -2: /* used -2 instead of lenf because needed const */
            if ( str[i] == '=' )
               { /* field found */
               ret = 1;
               goto string_has_field_end;
               }
            else if ( strchr(" \n\t", str[i]) )
               { /* whitespace. Ignore. */
               }
            else if ( str[i] == ';' )
               { /*reset to new field */
               stage = 0;
               }
            else
               { /*character not same as field pos */
               stage = -1;
               }
            break;
         default: /* 1..(lenf - 1) */
            if ( tolower(str[i]) == tolower(field[stage]) )
               { /* found proper char */
               if ( stage == lenf - 1 )
                  {
                  stage = -2;
                  }
               else
                  {
                  stage++;
                  }
               }
            else if ( str[i] == ';' )
               { /* reset to new field */
               stage = 0;
               }
            else
               { /*character not same as field pos */
               stage = -1;
               }
            break;
         }
      i++;
      }

   string_has_field_end:
   DEBUG_EXIT(string_has_field);
   return ret;
}
/* }}} */



/*  {{{ pdo_advantage_error
 *
 * Generic error-handling function. Takes in file, line, and handlers, and,
 * knowing that an error has occurred, storing error info into handlers.
 */
void pdo_advantage_error (pdo_dbh_t *dbh, pdo_stmt_t *stmt,
                          const char *file, int line TSRMLS_DC)
{
   pdo_advantage_db_handle *H;
   pdo_advantage_stmt *S;
   pdo_advantage_errinfo *einfo;
   char* errmsg_to_print;

   DEBUG_ENT(pdo_advantage_error);

   /* extract driver and statement (if applicable) handles */
   H = (pdo_advantage_db_handle*) dbh->driver_data;
   S = (!stmt) ? (NULL)  : ((pdo_advantage_stmt*) stmt->driver_data);

   /* extract error info containers */
   einfo = (!stmt) ? (&H->einfo) : (&S->einfo);

   /*
    *   set contents of containers. If a sql error has occured, the api will have stored its info.
    */
   einfo->file = file;
   einfo->line = line;

   /*
    *   a sqlstate was likely created from a ads c transaction,
    *   but there's a chance that it was made on the pdo_advantage
    *   level. We check for this first before asking the api what it
    *   thinks
    */
   if ( !PDO_ADVANTAGE_G(api).ads_error( H->dbc, einfo->errmsg, sizeof(einfo->errmsg) ) )
      { /* "einfo->errcode" will be 0 if the api has no complaints.
         * This means the error was defined by the driver (me) */
      strcpy(einfo->sqlstate,
             (!stmt) ? (dbh->error_code) : (stmt->error_code));
      einfo->errcode = -1; // TODO: better errcode
      }
   else
      { /* take the rest from api */
      PDO_ADVANTAGE_G(api).ads_sqlstate ( H->dbc, einfo->sqlstate, sizeof(einfo->sqlstate) );
      if ( stmt )
         {
         strcpy(stmt->error_code, einfo->sqlstate);
         }
      else
         {
         strcpy(dbh->error_code, einfo->sqlstate);
         }
      }

   /*
    * Will occur when the dbh isn't fully initialized.
    */
   if ( !dbh->methods )
      {
      spprintf(&errmsg_to_print, 0, "SQLSTATE=%s: %s (%d)",
               einfo->sqlstate, einfo->errmsg, einfo->errcode);
      zend_throw_exception_ex(php_pdo_get_exception(), einfo->errcode TSRMLS_CC, errmsg_to_print);
      efree(errmsg_to_print);
      }

   /* clear the error we just dealt with from the api */
   PDO_ADVANTAGE_G(api).ads_clear_error( H->dbc );

   DEBUG_EXIT(pdo_advantage_error);
}
/* }}} */



/* {{{ pdo_advantage_fetch_error_func
 *
 * Provides additional information about the last error to the user if requested.
 */
static int pdo_advantage_fetch_error_func(pdo_dbh_t *dbh, pdo_stmt_t *stmt,
                                          zval *info TSRMLS_DC)
{
   pdo_advantage_db_handle *H;
   pdo_advantage_stmt *S;
   pdo_advantage_errinfo *einfo;
   char* errmsg_to_print;

   DEBUG_ENT(pdo_advantage_fetch_error_func);

   /* extract driver and statement (if applicable) handles */
   H = (!stmt) ? ((pdo_advantage_db_handle*) dbh->driver_data) : ((pdo_advantage_db_handle*) stmt->dbh->driver_data);
   S = (!stmt) ? (NULL) : ((pdo_advantage_stmt*) stmt->driver_data);

   /* extract error info containers */
   einfo = (!S) ? (&H->einfo) : (&S->einfo);

   /* create error message */
   errmsg_to_print = emalloc(200);
   spprintf(&errmsg_to_print, 0, "SQLSTATE=%s: %s (%d)",
            einfo->sqlstate, einfo->errmsg, einfo->errcode);

   /*
    *  add errno and message. Must be added in this order.
    */
   //add_index_long(info, 0, (long) einfo->errcode);
   //add_index_string(info, 1, errmsg_to_print,1);
   //add_index_string(info, 2, einfo->sqlstate,1);
   add_index_string(info,0,einfo->sqlstate,1);
   add_index_long(info, 1, (long) einfo->errcode);
   add_index_string(info, 2, errmsg_to_print,1);


   DEBUG_EXIT(pdo_advantage_fetch_error_func);
   return 1;
}
/* }}} */



/* {{{ advantage_handle_closer
 *
 * Closes the connection which the passed db hanlder points to.
 */
static int advantage_handle_closer (pdo_dbh_t *dbh TSRMLS_DC)
{
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_closer);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   // if we have a connection, close it
   if ( H->dbc )
      {
      PDO_ADVANTAGE_G(api).ads_disconnect( H->dbc );
      PDO_ADVANTAGE_G(api).ads_free_connection ( H->dbc );
      H->dbc = NULL;
      }

   // free driver data
   pefree(H, dbh->is_persistent);
   dbh->driver_data = NULL;

   DEBUG_EXIT(advantage_handle_closer);
   return 1;
}
/* }}} */



/* {{{ advantage_handle_preparer
 *
 * Called before a query or prepare, this creates the statement object.
 */
static int advantage_handle_preparer(pdo_dbh_t *dbh, const char *sql,
                                     long sql_len, pdo_stmt_t *stmt, zval *driver_options TSRMLS_DC)
{
   int res;
   char *nsql;
   int nsql_len;
   pdo_advantage_db_handle *H;
   pdo_advantage_stmt *S;

   DEBUG_ENT(advantage_handle_preparer);

   nsql = NULL;
   nsql_len = 0;
   H = (pdo_advantage_db_handle *) dbh->driver_data;
   S = ecalloc(1, sizeof(pdo_advantage_stmt));

   /* clear einfo */
   memcpy(S->einfo.sqlstate, "000000", 6);
   *S->einfo.errmsg = '\0';
   S->einfo.errcode = 0;
   S->einfo.line = -1;
   S->einfo.file = NULL;

   /* parse for positional prepare */
   stmt->supports_placeholders = PDO_PLACEHOLDER_POSITIONAL;
   res = pdo_parse_params(stmt, (char*)sql, sql_len, &nsql, &nsql_len TSRMLS_CC);

   if ( res > 0 )
      { /* query was re-written */
      sql = nsql;
      }
   else if ( res == 0 )
      { /* query was not re-written, but ok (I think)... */
      res = 1;
      }
   else if ( res < 0 )
      { /* failed */
      strcpy(dbh->error_code, stmt->error_code);
      DB_HANDLE_ERR(dbh);
      efree(S);
      res = 0;
      goto advantage_handle_preparer_end;
      }

   stmt->methods = &advantage_stmt_methods;

   //EJS - Passing false for ucIsUnicode
   S->stmt = PDO_ADVANTAGE_G(api).ads_prepare(H->dbc, (UNSIGNED8*)sql, '\0');

   if ( !S->stmt )
      { /* if the statement does not exist (could not create it)*/
      DB_HANDLE_ERR(dbh);
      res = 0;
      efree(S);
      goto advantage_handle_preparer_end;
      }

   S->cur_row = 0;

   /* all clear. Set driver data */
   stmt->driver_data = S;

   /* clear einfo */
   S->einfo.sqlstate[0] = 0;
   S->einfo.errmsg[0] = 0;
   S->einfo.file = NULL;
   S->einfo.line = 0;
   S->einfo.errcode = 0;

   advantage_handle_preparer_end:
   if ( nsql ) efree(nsql); /* free newly-made sql */
   DEBUG_EXIT(advantage_handle_preparer);
   return res;
}
/* }}} */



/* {{{ advantage_handle_doer
 *
 * Executes a raw SQL statement
 */
static long advantage_handle_doer(pdo_dbh_t *dbh, const char *sql, long sql_len TSRMLS_DC)
{
   UNSIGNED32 res;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_doer);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   res = PDO_ADVANTAGE_G(api).ads_execute_immediate( H->dbc, (UNSIGNED8*)sql );

   //if (res) COMMIT_AS_NEEDED;
   if ( !res )
      {
      DB_HANDLE_ERR(dbh);
      res = -1;
      }

   DEBUG_EXIT(advantage_handle_doer);
   return res;
}

/* }}} */



/* {{{ advantage_handle_quoter
 *
 * Properly quotes an unquoted sql statement for advantage
 */
static int advantage_handle_quoter(pdo_dbh_t *dbh, const char *unquoted, int unquotedlen,
                                   char **quoted, int *quotedlen, enum pdo_param_type param_type  TSRMLS_DC)
{
   /* copied, with some modifications, from native driver */
   int i, dest_len;
   char*dest;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_quoter);

   dest_len = 0;
   H = (pdo_advantage_db_handle*) dbh->driver_data;

   for ( i = 0; i < unquotedlen; i++ )
      {
      switch ( unquoted[i] )
         {
         case '\0':
         case '\n':
         case '\r':
         case 26 /* ^Z */:
         case ';':
         case '"':
            dest_len += 4;
            break;
         case '\'':
         case '\\':
            dest_len += 2;
            break;
         default:
            dest_len++;
         }
      }

   dest = (char *) emalloc(dest_len + 3);
   dest_len = 0;
   dest[dest_len++] = '\'';
   for ( i = 0; i < unquotedlen; i++ )
      {
      switch ( unquoted[i] )
         {
         case '\0':
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '0';
            dest[dest_len++] = '0';
            break;
         case '\n':
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '0';
            dest[dest_len++] = 'a';
            break;
         case '\r':
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '0';
            dest[dest_len++] = 'd';
            break;
         case 26 :
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '1';
            dest[dest_len++] = 'a';
            break;
         case ';':
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '3';
            dest[dest_len++] = 'b';
            break;
         case '"':
            dest[dest_len++] = '\\';
            dest[dest_len++] = 'x';
            dest[dest_len++] = '2';
            dest[dest_len++] = '2';
            break;
         case '\'':
            dest[dest_len++] = '\'';
            dest[dest_len++] = '\'';
            break;
         case '\\':
            dest[dest_len++] = '\\';
            dest[dest_len++] = '\\';
            break;
         default:
            dest[dest_len++] = unquoted[i];
            break;
         }

      }

   dest[dest_len++] = '\'';
   dest[dest_len] = '\0';
   *quoted = dest;
   *quotedlen = dest_len;

   DEBUG_EXIT(advantage_handle_quoter);
   return 1;
}
/* }}} */



/* {{{ advantage_handle_begin
 *
 * Begin a "transaction". A transaction is
 * a set of instructions which are not committed until the transaction ends with a commit.
 */
static int advantage_handle_begin(pdo_dbh_t *dbh TSRMLS_DC)
{
   int res;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_begin);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   res = PDO_ADVANTAGE_G(api).AdsBeginTransaction( H->dbc->hConnect );

   if ( res )
      DB_HANDLE_ERR(dbh);
   else
      dbh->in_txn = 1;

   DEBUG_EXIT(advantage_handle_begin);
   return res;
}
/* }}} */



/* {{{ advantage_handle_commit
 *
 * Commits. Also ends a transaction.
 */
static int advantage_handle_commit(pdo_dbh_t *dbh TSRMLS_DC)
{
   int res;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_commit);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   res = PDO_ADVANTAGE_G(api).ads_commit( H->dbc );

   if ( !res )
      DB_HANDLE_ERR(dbh);
   else
      dbh->in_txn = 0;

   DEBUG_EXIT(advantage_handle_commit);
   return res;
}
/* }}} */



/* {{{ advantage_handle_rollback
 *
 * Rollback to last commit.
 */
static int advantage_handle_rollback( pdo_dbh_t *dbh TSRMLS_DC)
{
   int res;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_handle_rollback);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   res = PDO_ADVANTAGE_G(api).ads_rollback( H->dbc );

   if ( !res )
      DB_HANDLE_ERR(dbh);
   else
      dbh->in_txn = 0;

   DEBUG_EXIT(advantage_handle_rollback);
   return res;
}
/* }}} */



/* {{{ advantage_handle_get_attribute
 *
 * Retrieves a database attribute
 */
static int advantage_handle_get_attribute(pdo_dbh_t *dbh, long attr, zval *return_value TSRMLS_DC)
{
   ADSHANDLE res;
   int length;
   char *buf;
   pdo_advantage_db_handle *H;
   ADSHANDLE temp = 0;
   a_ads_data_value val;

   DEBUG_ENT(advantage_handle_get_attribute);

   //May need to support other Attributes? http://www.php.net/manual/en/pdo.getattribute.php
   switch ( attr )
      {
      case PDO_ATTR_SERVER_VERSION:
         {
            /*
             * Have to query the database for this info.
             */
            H = (pdo_advantage_db_handle*) dbh->driver_data;
            temp = PDO_ADVANTAGE_G(api).ads_execute_direct( H->dbc, "SELECT version FROM (EXECUTE PROCEDURE sp_mgGetInstallInfo()) t" );
            if ( !temp )
               {
               DB_HANDLE_ERR(dbh);
               res = 0;
               break;
               }
            res = PDO_ADVANTAGE_G(api).ads_fetch_next(temp);
            if ( !res )
               {
               /*
                *   should be a statement error, but the statement
                *   it messed up on is not the one in 'stmt'
                */
               DB_HANDLE_ERR(dbh);
               PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
               break;
               }
            res = PDO_ADVANTAGE_G(api).ads_get_column( temp,0,&val);
            if ( !res )
               {
               DB_HANDLE_ERR(dbh);
               PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
               break;
               }
            length = *(val.length);
            buf = emalloc(length + 1);
            memcpy(buf, val.buffer, length);
            buf[length] = '\0';
            PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
            ZVAL_STRINGL(return_value, buf, length,1);
            break;
         }
      case PDO_ATTR_CLIENT_VERSION:
         {
            length = 32;
            buf = emalloc(length);
            res = PDO_ADVANTAGE_G(api).ads_client_version( buf, length );
            ZVAL_STRINGL(return_value, buf, strlen(buf),1);
         }
         if ( !res ) DB_HANDLE_ERR(dbh);
         break;
      default:
         strcpy(dbh->error_code, "IM001");
         DB_HANDLE_ERR(dbh);
         res = 0;
         break;
      }

   DEBUG_EXIT(advantage_handle_get_attr);
   return res;
}
/* }}} */



/* {{{ advantage_handle_set_attribute
 *
 * Sets a database attribute
 */
static int advantage_handle_set_attribute(pdo_dbh_t *dbh, long attr, zval *val TSRMLS_DC)
{
   DEBUG_ENT(advantage_handle_set_attribute);
   /*   As far as I can tell, sacapi does not support any of the optional
    *   PDO params. I don't think the user should be able to mod client
    *   info either
    */
   DEBUG_EXIT(advantage_handle_set_attribute);
   return 0;
}
/* }}} */



/* {{{ advantage_handle_last_id
 *
 * Retrieves the ID of the last inserted row
 */
static char * advantage_handle_last_id(pdo_dbh_t *dbh, const char *name, unsigned int* len TSRMLS_DC)
{
   char *res;
   pdo_advantage_db_handle *H;
   a_ads_data_value val;
   ADSHANDLE temp;

   DEBUG_ENT(advantage_handle_last_id);

   H = (pdo_advantage_db_handle*) dbh->driver_data;

   temp = PDO_ADVANTAGE_G(api).ads_execute_direct( H->dbc, "SELECT CAST(LASTAUTOINC(CONNECTION) AS SQL_VARCHAR(50)) FROM system.iota" );
   if ( !temp )
      {
      DB_HANDLE_ERR(dbh);
      res = NULL;
      goto advantage_handle_last_id_end;
      }
   if ( !(PDO_ADVANTAGE_G(api).ads_fetch_next( temp )) || !(PDO_ADVANTAGE_G(api).ads_get_column( temp,0,&val )) )
      {
      DB_HANDLE_ERR(dbh);
      res = NULL;
      goto advantage_handle_last_id_end;
      }
   *len = *(val.length);
   res = emalloc(*len + 1);
   memcpy(res, val.buffer, *len);
   res[*len] = '\0';

   advantage_handle_last_id_end:
   if ( temp ) PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
   DEBUG_EXIT(advantage_handle_last_id);
   return res ;
}
/* }}} */



/* {{{ advantage_check_liveness
 *
 * Checks if a persistent connection is live.
 */
static int advantage_check_liveness(pdo_dbh_t *dbh TSRMLS_DC)
{
   UNSIGNED32 res=0;
   int shouldBeOne;
   pdo_advantage_db_handle *H;
   ADSHANDLE temp=0;
   a_ads_data_value val;

   DEBUG_ENT(advantage_check_liveness);
   /*
    *   Can only do this brutte force: query the
    *   server for a known value and check that
    *   the return value is the same as expected.
    */
   H = (pdo_advantage_db_handle*) dbh->driver_data;
   /* execute stmt */
   temp = PDO_ADVANTAGE_G(api).ads_execute_direct( H->dbc, "SELECT 1 FROM system.iota" );
   if ( !temp )
      {
      DB_HANDLE_ERR(dbh);
      res = 0;
      goto advantage_check_liveness_end;
      }
   /* fetch first value */
   res = PDO_ADVANTAGE_G(api).ads_fetch_next(temp);
   if ( !res )
      {
      DB_HANDLE_ERR(dbh);
      goto advantage_check_liveness_end;
      }
   res = PDO_ADVANTAGE_G(api).ads_get_column( temp,0,&val);
   if ( !res )
      {
      DB_HANDLE_ERR(dbh);
      goto advantage_check_liveness_end;
      }
   /* buffer should contain int value 1 */
   shouldBeOne = *((int *) val.buffer);
   if ( shouldBeOne != 1 )
      {
      strcpy(dbh->error_code, "HY00"); // FIXME Needs more descriptive SQL state
      DB_HANDLE_ERR(dbh);
      res = 0;
      goto advantage_check_liveness_end;
      }

   advantage_check_liveness_end:
   if ( temp ) PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
   DEBUG_EXIT(advantage_check_liveness);
   return res;
}
/* }}} */



/* {{{ advantage_methods
 *
 * Lists the function pointers of the advantage supported methods
 */
static struct pdo_dbh_methods advantage_methods = {
   // comment out if not supported
   advantage_handle_closer,
   advantage_handle_preparer,
   advantage_handle_doer,
   advantage_handle_quoter,
   advantage_handle_begin,
   advantage_handle_commit,
   advantage_handle_rollback,
   NULL, /* advantage_handle_set_attribute, */
   advantage_handle_last_id,
   pdo_advantage_fetch_error_func,
   advantage_handle_get_attribute,
   advantage_check_liveness,
   NULL /*advantage_get_driver_methods */
};
/* }}} */



/* {{{ pdo_advantage_handle_factory
 *
 * Creates database handles (connections).
 * TODO: username and password dbh field support
 */
static int pdo_advantage_handle_factory(pdo_dbh_t *dbh, zval *driver_options TSRMLS_DC)
{
   UNSIGNED32 res=0;
   char *dsn;
   pdo_advantage_db_handle *H;
   DEBUG_ENT(pdo_advantage_handle_factory);

   /* driver_data of dbh. The only part we can muck around with */
   H = pecalloc(1, sizeof(*H), dbh->is_persistent);
   dbh->driver_data = H;

   /* clear error info */
   memcpy(H->einfo.sqlstate, "000000", 6);
   *H->einfo.errmsg = '\0';
   H->einfo.errcode = 0;
   H->einfo.line = -1;
   H->einfo.file = NULL;

   /* api defined as a global var */

   /* establish connection */
   H->dbc = PDO_ADVANTAGE_G(api).ads_new_connection();

   if ( dbh->username && *dbh->username
        && !string_has_field(dbh->data_source,strlen(dbh->data_source), "user id", 3 TSRMLS_CC) )
      { /* uid field defined outside of con string */
      spprintf(&dsn, 0, "%s;user id=%s", dbh->data_source, dbh->username);
      pefree((char*)dbh->data_source, dbh->is_persistent);
      dbh->data_source = dsn;
      }

   if ( dbh->password && *dbh->password
        && !string_has_field(dbh->data_source,strlen(dbh->data_source), "password", 3 TSRMLS_CC) )
      { /* pwd field defined outside of con string */
      spprintf(&dsn, 0, "%s;password=%s", dbh->data_source, dbh->password);
      pefree((char*)dbh->data_source, dbh->is_persistent);
      dbh->data_source = dsn;
      }

   res = PDO_ADVANTAGE_G(api).ads_connect(H->dbc,(UNSIGNED8*)dbh->data_source);
   if ( !res )
      {
      DB_HANDLE_ERR(dbh);
      }

   /* set database handle methods to advantage methods */
   dbh->methods = &advantage_methods;

   DEBUG_EXIT(pdo_advantage_handle_factory);
   return res;
}
/* }}} */



/* {{{ pdo_advantage_driver */
pdo_driver_t pdo_advantage_driver = {
   PDO_DRIVER_HEADER(advantage),
   pdo_advantage_handle_factory
};
/* }}} */
