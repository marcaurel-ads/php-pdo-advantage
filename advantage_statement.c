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

#ifdef HAVE_CONFIG_H
   #include "config.h"
#endif

#ifdef WIN32
/* int sizes */
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
/* sprintf formatting */
   #define SPECIFY_INT64 "I64"
   #define SPECIFY_INT32 "I32"
#else
/* int sizes */
   #include "stdint.h"
/* sprintf formatting */
   #define SPECIFY_INT64 "ll"
   #define SPECIFY_INT32 ""
#endif

#include "stdlib.h"
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



/* {{{ convert_buf_to_long
 *
 *  Converts a buffer to a long, given its length and signedness.
 *
 *  FIXME: Could be much more elegant.
 */
static long convert_buf_to_long(void* buf, int len, short is_unsigned TSRMLS_DC)
{
   /*
    *  Chose to do this brute force in case encoding method changes
    */
   long ret;

   DEBUG_ENT(convert_buf_to_long);

   switch ( len )
      {
      case 1:
         ret = (is_unsigned) ? ((long) *((uint8_t*) buf)) : ((long) *((int8_t*) buf));
         break;
      case 2:
         ret = (is_unsigned) ? ((long) *((uint16_t*) buf)) : ((long) *((int16_t*) buf));
         break;
      case 4:
         ret = (is_unsigned) ? ((long) *((uint32_t*) buf)) : ((long) *((int32_t*) buf));
         break;
      case 8:
         ret = (is_unsigned) ? ((long) *((uint64_t*) buf)) : ((long) *((int64_t*) buf));
         break;
      default: /* no clue... hope for the best */
         ret = *((long*) buf);
         break;
      }

   DEBUG_EXIT(convert_buf_to_long);
   return ret;
}
/* }}} */



/* {{{ store_zval_from_ads_value
 *
 * Stores a_ads_value approprately into a zval, given
 * a pdo_param type. Returns 1 on success, 0 on failure.
 */
static short store_zval_from_ads_value( a_ads_data_value *val, zval* z, enum pdo_param_type t TSRMLS_DC)
{
   int newlen;
   double dval;
   long lval;
   char* strval;
   int64_t int64val;
   uint64_t uint64val;
   uint32_t uint32val;
   short res;

   DEBUG_ENT(store_zval_from_ads_value);

   if ( *(val->is_null) || (t == PDO_PARAM_NULL) ) goto szval_null;
   if ( t == PDO_PARAM_STR ) goto szval_str;
   if ( t == PDO_PARAM_INT ) goto szval_int;
   if ( t == PDO_PARAM_LOB ) goto szval_lob;
   res = 0;
   goto szval_end;

   szval_str:
   /* approach varies based on sql type */
   switch ( val->type )
      {
      case A_BINARY:
      case A_DATE:
      case A_TIME:
      case A_TIMESTAMP:
      case A_NCHAR:
      case A_STRING:
      case A_DECIMAL:
         convert_to_string(z);
         newlen = *(val->length);
         Z_STRVAL_P(z) = erealloc(Z_STRVAL_P(z), newlen + 1);
         memcpy(Z_STRVAL_P(z), val->buffer, newlen);
         Z_STRLEN_P(z) = newlen;
         Z_STRVAL_P(z)[newlen] = '\0';
         res = 1;
         break;
      case A_DOUBLE:
         dval = *((double*)val->buffer);
         Z_TYPE_P(z) = IS_DOUBLE;
         Z_DVAL_P(z) = dval;
         convert_to_string(z);
         res = 1;
         break;
      case A_VAL64:
         int64val = *((int64_t*) val->buffer);
         convert_to_string(z);
         /* 64-bit can have max 20 chars, +1 for EOF */
         Z_STRVAL_P(z) = erealloc(Z_STRVAL_P(z), 21);
         sprintf(Z_STRVAL_P(z), "%"SPECIFY_INT64"d", int64val);
         newlen = strlen(Z_STRVAL_P(z));
         Z_STRLEN_P(z) = newlen;
         Z_STRVAL_P(z)[newlen] = '\0';
         res = 1;
         break;
      case A_UVAL64:
         uint64val = *((uint64_t*) val->buffer);
         convert_to_string(z);
         Z_STRVAL_P(z) = erealloc(Z_STRVAL_P(z), 21);
         sprintf(Z_STRVAL_P(z), "%"SPECIFY_INT64"u", uint64val);
         newlen = strlen(Z_STRVAL_P(z));
         Z_STRLEN_P(z) = newlen;
         Z_STRVAL_P(z)[newlen] = '\0';
         res = 1;
         break;
      case A_UVAL32:
         uint32val = *((uint32_t*) val->buffer);
         convert_to_string(z);
         /* 32-bit can have max 10 chars, +1 for EOF */
         Z_STRVAL_P(z) = erealloc(Z_STRVAL_P(z), 11);
         sprintf(Z_STRVAL_P(z), "%"SPECIFY_INT32"u", uint32val);
         newlen = strlen(Z_STRVAL_P(z));
         Z_STRLEN_P(z) = newlen;
         Z_STRVAL_P(z)[newlen] = '\0';
         res = 1;
         break;
      case A_UVAL8:
      case A_UVAL16:
         lval = convert_buf_to_long(val->buffer, *val->length, 1 TSRMLS_CC);
         Z_TYPE_P(z) = IS_LONG;
         Z_LVAL_P(z) = lval;
         convert_to_string(z);
         res = 1;
         break;
      case A_VAL8:
      case A_VAL16:
      case A_VAL32:
         lval = convert_buf_to_long(val->buffer, *val->length, 0 TSRMLS_CC);
         Z_TYPE_P(z) = IS_LONG;
         Z_LVAL_P(z) = lval;
         convert_to_string(z);
         res = 1;
         break;
      default:
         res = 0;
         break;
      }
   Z_TYPE_P(z) = IS_STRING;
   goto szval_end;

   szval_int:
   /* approach varies based on sql type */
   switch ( val->type )
      {
      case A_BINARY:
         /* try reading it directly, I guess... */
         if ( *val->length != sizeof(long) )
            {
            res = 0;
            }
         else
            {
            lval = *((long*) val->buffer);
            Z_TYPE_P(z) = IS_LONG;
            Z_LVAL_P(z) = lval;
            res = 1;
            }
         break;
      case A_STRING:
         lval = strtol(val->buffer, &strval, 10);
         if ( val->buffer == strval )
            { /* no characters were read. String not a number */
            res = 0;
            }
         else
            { /* a character was read. We'll try that. */
            Z_TYPE_P(z) = IS_LONG;
            Z_LVAL_P(z) = lval;
            res = 1;
            }
         break;
      case A_DOUBLE:
         lval = (long) *((double*)val->buffer);
         Z_TYPE_P(z) = IS_LONG;
         Z_LVAL_P(z) = lval;
         res = 1;
         break;
      case A_UVAL8:
      case A_UVAL16:
      case A_UVAL32:
      case A_UVAL64:
         lval = convert_buf_to_long(val->buffer, *val->length, 1 TSRMLS_CC);
         Z_TYPE_P(z) = IS_LONG;
         Z_LVAL_P(z) = lval;
         res = 1;
         break;
      case A_VAL8:
      case A_VAL16:
      case A_VAL32:
      case A_VAL64:
         lval = convert_buf_to_long(val->buffer, *val->length, 0 TSRMLS_CC);
         Z_TYPE_P(z) = IS_LONG;
         Z_LVAL_P(z) = lval;
         res = 1;
         break;
      default:
         res = 0;
         break;
      }
   Z_TYPE_P(z) = IS_LONG;
   goto szval_end;

   szval_lob:
   /*
    *   note that, upon recognizing the zval
    *   is a string but the PDO_PARAM type is
    *   LOB, it will shove the value into a
    *   stream.
   */
   convert_to_string(z);
   newlen = *(val->length);
   Z_STRVAL_P(z) = erealloc(Z_STRVAL_P(z), newlen + 1);
   memcpy(Z_STRVAL_P(z), val->buffer, newlen);
   Z_STRLEN_P(z) = newlen;
   Z_STRVAL_P(z)[newlen] = '\0';
   res = 1;
   Z_TYPE_P(z) = IS_STRING;
   goto szval_end;

   szval_null:
   zval_dtor(z);
   ZVAL_NULL(z);
   res = 1;
   goto szval_end;

   szval_end:
   DEBUG_EXIT(store_zval_from_ads_value);
   return res;
}
/* }}} */



/* {{{ get_closest_pdo_type
 *
 * Determine closest PDO type from ads type
 */
static enum pdo_param_type get_closest_pdo_type(enum a_ads_data_type type TSRMLS_DC)
   {
   enum pdo_param_type ret;

   DEBUG_ENT(get_closest_pdo_type);

   switch ( type )
      {
      case A_BINARY:
         ret = PDO_PARAM_LOB;
         break;
      case A_UVAL64:    /* assume the worst */
      case A_STRING:
      case A_DOUBLE:    /* no double support */
      case A_DATE:
      case A_TIME:
      case A_TIMESTAMP:
      case A_NCHAR:
      case A_DECIMAL:
         ret = PDO_PARAM_STR;
         break;
      case A_UVAL32:
      case A_VAL64:
         if ( sizeof(long) <= 4 )
            { /* check if long can't handle it */
            ret = PDO_PARAM_STR;
            break;
            }
      case A_UVAL16:
      case A_UVAL8:
      case A_VAL32:
      case A_VAL16:
      case A_VAL8:
         ret = PDO_PARAM_INT;
         break;
      case A_INVALID_TYPE:
      default:
         zend_throw_exception_ex(php_pdo_get_exception(),
                                 -1 TSRMLS_CC, "Cannot convert ads type to a pdo type.");
         ret = PDO_PARAM_NULL;
         break;
      }

   DEBUG_EXIT(get_closest_pdo_type);
   return ret;
   }
/* }}} */



/* {{{ get_closest_ads_type
 *
 * Determine closest ads type from PDO type
 */
static enum a_ads_data_type get_closest_ads_type(enum pdo_param_type type TSRMLS_DC)
   {
   enum a_ads_data_type ret;
   DEBUG_ENT(get_closest_ads_type);
   switch ( type )
      {
      case PDO_PARAM_NULL:
         ret = A_INVALID_TYPE;
         break;
      case PDO_PARAM_INT:
         /* size of their "long" can change based on architecture */
         if ( sizeof(long) == 4 )
            {
            ret = A_VAL32;
            }
         else
            {
            ret = A_VAL64;
            }
         break;
      case PDO_PARAM_STR:
         ret = A_STRING;
         break;
      case PDO_PARAM_LOB:
         ret = A_BINARY;
         break;
      case PDO_PARAM_BOOL:
         ret = A_UVAL8;
         break;
      default:
         /* we don't support anything else */
         zend_throw_exception_ex(php_pdo_get_exception(),
                                 -1 TSRMLS_CC, "Cannot convert PDO type to ads type.");
         ret = A_INVALID_TYPE;
         break;
      }

   DEBUG_EXIT(get_closes_ads_type);
   return ret;
   }
/* }}} */



ZEND_DECLARE_MODULE_GLOBALS(pdo_advantage);



/* {{{ advantage_stmt_dtor
 *
 * Closes a statement.
 */
static int advantage_stmt_dtor(pdo_stmt_t *stmt TSRMLS_DC)
{
   pdo_advantage_stmt *S;

   DEBUG_ENT(advantage_stmt_dtor);

   S = (pdo_advantage_stmt *) stmt-> driver_data;

   if ( S )
      { /* if the statement handler is actually there */
      PDO_ADVANTAGE_G(api).ads_free_stmt( S->stmt );
      efree(S);
      S = NULL;
      }

   DEBUG_EXIT(advantage_stmt_dtor);
   return 1;
}
/* }}} */



/* {{{ advantage_stmt_execute
 *
 * Executes the statement.
 */
static int advantage_stmt_execute(pdo_stmt_t *stmt TSRMLS_DC)
{
   int res;
   int intRowsAffected=0;
   pdo_advantage_stmt *S;
   pdo_dbh_t *dbh;
   pdo_advantage_db_handle *H;

   DEBUG_ENT(advantage_stmt_execute);

   S = (pdo_advantage_stmt *) stmt-> driver_data;

   /* Call reset on the statement to close any open cursors automatically */
   if (S->stmt)
      {
      res = PDO_ADVANTAGE_G(api).ads_reset(S->stmt);
      if ( !res )
         {
         DB_STMT_ERR(stmt);
         goto advantage_stmt_execute_end;
         }
      }

   /* am assuming that the statement object is there */
   res = PDO_ADVANTAGE_G(api).ads_execute(S->stmt);
   if ( !res )
      {
      DB_STMT_ERR(stmt);
      goto advantage_stmt_execute_end;
      }

   dbh = stmt->dbh;
   H = (pdo_advantage_db_handle*) dbh->driver_data;

   stmt->column_count = PDO_ADVANTAGE_G(api).ads_num_cols(S->stmt);
   stmt->row_count = PDO_ADVANTAGE_G(api).ads_num_rows(S->stmt);
   intRowsAffected = PDO_ADVANTAGE_G(api).ads_affected_rows(S->stmt);
   if ( intRowsAffected > 0 )
      { /* we also/or have some affected rows */
      stmt->row_count += intRowsAffected;
      }

   advantage_stmt_execute_end:
   DEBUG_EXIT(advantage_stmt_execute);
   return res;
}
/* }}} */



/* {{{ advantage_stmt_fetch
 *examples
 * Fetches a row from the executed sql_statement.
 */
static int advantage_stmt_fetch(pdo_stmt_t *stmt, enum pdo_fetch_orientation ori,
                                long offset TSRMLS_DC)
{
   int res=1;
   int triedToExecute;
   char *sqlstate;
   pdo_advantage_stmt *S;

   DEBUG_ENT(advantage_stmt_fetch);

   S = (pdo_advantage_stmt *) stmt-> driver_data;

   /*
    *   Current behaviour is as follows:
    *   NEXT is handled by the driver
    *   ABS is handled by the driver, setting cur_row always pos
    *   PRIOR goes as far as 0
    *   RELATIVE, forward handled by the driver
    *   RELATIVE, backward can go as far as 0
    */
   triedToExecute = 0;
   while ( !triedToExecute )
      switch ( ori )
         {
         case PDO_FETCH_ORI_NEXT:
            res = PDO_ADVANTAGE_G(api).ads_fetch_next( S->stmt );
            if ( res )
               {
               S->cur_row++;
               }
            else
               {
               /*
               *   We do not want an error if we run out
               *   of rows. Instead, we just return 0.
               */
               sqlstate = emalloc(6);
               PDO_ADVANTAGE_G(api).ads_sqlstate (
                                                 ((pdo_advantage_db_handle*) stmt->dbh->driver_data)->dbc, sqlstate, 6 );
               if ( strcmp(sqlstate, "02000") )
                  { /* state is not the "no data" state */
                  DB_STMT_ERR(stmt);
                  }
               else
                  { /* state is the "no data" state */
                  PDO_ADVANTAGE_G(api).ads_clear_error(
                                                      ((pdo_advantage_db_handle*) stmt->dbh->driver_data)->dbc);
                  }

               efree(sqlstate);
               }
            triedToExecute = 1;
            break;
         case PDO_FETCH_ORI_ABS:
            if ( ( offset >= 0 && offset == S->cur_row)
                 || (offset < 0 && (stmt->row_count + offset == S->cur_row)) )
               {
               res = 1;
               triedToExecute = 1;
               break;
               }
            else
               {
               res = PDO_ADVANTAGE_G(api).ads_fetch_absolute( S->stmt, offset );
               if ( res )
                  {
                  S->cur_row = (offset >= 0) ? offset : stmt->row_count + offset;
                  }
               else
                  {
                  /*
                  *  We do not want an error if we run out
                  *  of rows. Instead, we just return 0.
                  */
                  sqlstate = emalloc(6);
                  PDO_ADVANTAGE_G(api).ads_sqlstate (
                                                    ((pdo_advantage_db_handle*) stmt->dbh->driver_data)->dbc, sqlstate, 6 );
                  if ( strcmp(sqlstate, "02000") )
                     { /* state is not the "no data" state */
                     DB_STMT_ERR(stmt);
                     }
                  else
                     { /* state is the "no data" state */
                     PDO_ADVANTAGE_G(api).ads_clear_error(
                                                         ((pdo_advantage_db_handle*) stmt->dbh->driver_data)->dbc);
                     }
                  efree(sqlstate);
                  }
               }
            triedToExecute = 1;
            break;
         case PDO_FETCH_ORI_PRIOR:
            ori = PDO_FETCH_ORI_ABS;
            offset = (offset) ? (S->cur_row - 1) : (offset);
            break;
         case PDO_FETCH_ORI_FIRST:
            ori = PDO_FETCH_ORI_ABS;
            offset = 1;
            break;
         case PDO_FETCH_ORI_LAST:
            ori = PDO_FETCH_ORI_ABS;
            offset = -1;
            break;
         case PDO_FETCH_ORI_REL:
            ori = PDO_FETCH_ORI_ABS;
            offset = S->cur_row + offset;
            offset = (offset < 0) ? 0 : offset;
            break;
         default:
            res = 0;
            strcpy(stmt->error_code, "HY106");
            DB_STMT_ERR(stmt);
            triedToExecute = 1;
            break;
         }
   // end while

   DEBUG_EXIT(advantage_stmt_fetch);
   return res;
}
/* }}} */



/* {{{ advantage_stmt_param_hook
 *
 * Handles bound parameters and columns.
 */
static int advantage_stmt_param_hook(pdo_stmt_t *stmt,
                                     struct pdo_bound_param_data *param, enum pdo_param_event event_type TSRMLS_DC)
{
   int res, buf_size;
   short is_stream_data, is_large;
   char* blob_ptr;
   char* blob_end;
   pdo_advantage_stmt *S;
   pdo_advantage_param *P;
   pdo_advantage_col *C;
   php_stream *stm;
   a_ads_data_value val;

   DEBUG_ENT(advantage_stmt_param_hook);

   res = 0;
   S = (pdo_advantage_stmt*) stmt->driver_data;
   is_stream_data = 0;
   is_large = 0;

   switch ( event_type )
      {
      case PDO_PARAM_EVT_FREE:
         if ( param->is_param )
            { /* bound is parameter */
            P = (pdo_advantage_param*) param->driver_data;
            /* 'desired' deallocated by dbcapi */
            if ( P->input.value.is_null ) efree(P->input.value.is_null);
            if ( P->output.value.is_null ) efree(P->output.value.is_null);
            if ( P->input.value.length ) efree(P->input.value.length);
            if ( P->output.value.length ) efree(P->output.value.length);
            efree(P);
            }
         else
            { /* bound is column */
            C = (pdo_advantage_col*) param->driver_data;
            efree(C);
            }
         param->driver_data = NULL;
         res = 1;
         break;


      case PDO_PARAM_EVT_ALLOC:
         if ( param->is_param )
            { /* bound is parameter */
            /*
             * Allocate a parameter handle and
             * initialize some of its values.
             */
            P = (pdo_advantage_param*) ecalloc(1, sizeof(pdo_advantage_param));
            /*
             * Handle contains three param structs:
             *
             * desired: contains information about what advantage expects
             *        to put out.
             * output:   A buffer which will capture output from results
             * input:    A buffer which will capture input from zval
             */
            res = PDO_ADVANTAGE_G(api).ads_describe_bind_param( S->stmt, param->paramno, &P->desired);
            if ( !res )
               {
               DB_STMT_ERR(stmt);
               break;
               }

            /* initialize output param */
            P->output.direction = DD_OUTPUT;
            P->output.value.is_null = ecalloc(1,1); // need only one byte
            *(P->output.value.is_null) = 1;
            P->output.value.length = ecalloc(1,sizeof(int));
            *(P->output.value.length) = 0;

            /* initialize input param */
            P->input.direction = DD_INPUT;
            P->input.value.is_null = ecalloc(1,1);
            *(P->input.value.is_null) = 1;
            P->input.value.length = ecalloc(1,sizeof(int));
            *(P->input.value.length) = 0;

            param->driver_data = P;
            }
         else
            { /* bound is column */
            /*
             *  Allocate a column handle and
             *  initialize some of its values.
             */
            C = (pdo_advantage_col*) ecalloc(1, sizeof(pdo_advantage_col));
            C->has_info = 0;

            param->driver_data = C;

            res = 1;
            }
         break;


      case PDO_PARAM_EVT_EXEC_PRE:
         if ( param->is_param )
            { /* bound is parameter */
            /*
             * We need to initialize buffers and actually bind the params
             */
            P = (pdo_advantage_param*) param->driver_data;
            /*
             * If there is some form of output, we
             * must allocate our own buffer. Note
             * that we add 1 to buffer size to add
             * the EOF char
             */
            if ( P->desired.direction & DD_OUTPUT )
               { /* we have some output qualities to bind */
               P->output.value.type = P->desired.value.type;
               switch ( P->output.value.type )
                  {
                  case A_STRING:
                  case A_BINARY:
                     /* buffer size has been set for us */
                     P->output.value.buffer_size = P->desired.value.buffer_size;
                     P->output.value.buffer =
                     ecalloc(1, P->output.value.buffer_size);
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_DOUBLE:
                     buf_size = sizeof(double);
                     P->output.value.buffer = ecalloc(1, buf_size);
                     P->output.value.buffer_size = buf_size;
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_VAL64:
                  case A_UVAL64:
                     buf_size = 8;
                     P->output.value.buffer = ecalloc(1, buf_size);
                     P->output.value.buffer_size = buf_size;
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_VAL32:
                  case A_UVAL32:
                     buf_size = 4;
                     P->output.value.buffer = ecalloc(1, buf_size);
                     P->output.value.buffer_size = buf_size;
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_VAL16:
                  case A_UVAL16:
                     buf_size = 2;
                     P->output.value.buffer = ecalloc(1, buf_size);
                     P->output.value.buffer_size = buf_size;
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_VAL8:
                  case A_UVAL8:
                     buf_size = 1;
                     P->output.value.buffer = ecalloc(1, buf_size);
                     P->output.value.buffer_size = buf_size;
                     *(P->output.value.is_null) = 0;
                     break;
                  case A_INVALID_TYPE:
                  default:
                     /* treat as NULL */
                     P->output.value.buffer = NULL;
                     P->output.value.buffer_size = 0;
                     break;
                  } // switch

               /* bind the output */
               res = PDO_ADVANTAGE_G(api).ads_bind_param( S->stmt, param->paramno, &P->output);
               if ( !res )
                  {
                  DB_STMT_ERR(stmt);
                  break;
                  }
               } // OUTPUT

            if ( P->desired.direction & DD_INPUT )
               { /* param has some input qualities */
               if ( Z_TYPE_P(param->parameter) == IS_NULL )
                  { /* the given input is null */
                  P->input.value.buffer = NULL;
                  *(P->input.value.length) = 0;
                  }
               else
                  { /* the given input is not null */
                  /* set input.value.type */
                  P->input.value.type = get_closest_ads_type(param->param_type TSRMLS_CC);

                  switch ( P->input.value.type )
                     {
                     case A_BINARY:
                     case A_STRING:
                        if ( Z_TYPE_P(param->parameter) == IS_RESOURCE )
                           { /* we are sending stream data */
                           is_stream_data = 1;
                           buf_size = 4096;
                           P->input.value.buffer = emalloc(buf_size);
                           php_stream_from_zval_no_verify(stm, &param->parameter);

                           /*
                            *   FIXME: Needs a better algorithm for growing byte
                            *   fetching. Currently doubles each iteration.
                            */
                           blob_ptr = P->input.value.buffer;
                           *(P->input.value.length) = 0;
                           while ( !php_stream_eof(stm) )
                              {
                              buf_size = php_stream_read(stm, blob_ptr, buf_size);
                              blob_ptr += buf_size;
                              *(P->input.value.length) += buf_size;
                              buf_size += buf_size;
                              P->input.value.buffer = erealloc(P->input.value.buffer, 2*buf_size);
                              }
                           }
                        else
                           { /* we are sending regular data */
                           convert_to_string(param->parameter);
                           *(P->input.value.length) = ((int) Z_STRLEN_P(param->parameter));
                           P->input.value.buffer = Z_STRVAL_P(param->parameter);
                           }
                        /* if the buffer is really long, we will send it in chunks */
                        if ( *(P->input.value.length) > 4096 ) is_large = 1;
                        break;
                     case A_VAL64:
                     case A_UVAL64:
                        *(P->input.value.length) = 8;
                        P->input.value.buffer = (char*) &Z_LVAL_P(param->parameter);
                        break;
                     case A_VAL32:
                     case A_UVAL32:
                        *(P->input.value.length) = 4;
                        P->input.value.buffer = (char*) &Z_LVAL_P(param->parameter);
                        break;
                     case A_VAL16:
                     case A_UVAL16:
                        *(P->input.value.length) = 2;
                        P->input.value.buffer = (char*)&Z_LVAL_P(param->parameter);
                        break;
                     case A_VAL8:
                     case A_UVAL8:
                        *(P->input.value.length) = 1;
                        P->input.value.buffer = (char*) &Z_LVAL_P(param->parameter);
                        break;
                     case A_INVALID_TYPE:
                     default:
                        *(P->input.value.length) = 0;
                        P->input.value.buffer = NULL;
                        break;
                     }

                  if ( P->input.value.type != A_INVALID_TYPE ) *(P->input.value.is_null) = 0;
                  }
               /* bind the input */
               res = PDO_ADVANTAGE_G(api).ads_bind_param( S->stmt, param->paramno, &P->input);
               if ( !res )
                  {
                  DB_STMT_ERR(stmt);
                  break;
                  }
               /* if large (or stream: we need to free it), send data in chunks */
               if ( is_large | is_stream_data )
                  {
                  blob_ptr = P->input.value.buffer;
                  blob_end = P->input.value.buffer + *(P->input.value.length);
                  buf_size = 4096;
                  while ( blob_ptr < blob_end )
                     {
                     if ( buf_size >= (blob_end - blob_ptr) )
                        buf_size = blob_end - blob_ptr;

                     res = PDO_ADVANTAGE_G(api).ads_send_param_data( S->stmt, param->paramno, blob_ptr, buf_size);
                     if ( !res )
                        {
                        DB_STMT_ERR(stmt);
                        break;
                        }
                     blob_ptr += buf_size;
                     }
                  }
               /* free buffer if stream (we made it) */
               if ( is_stream_data )
                  {
                  efree(P->input.value.buffer);
                  P->input.value.buffer = NULL;
                  }
               } // INPUT
            } // param->is_param
         else
            { /* bound is container */
            /* nothing needs to be done */
            res = 1;
            }
         break;


      case PDO_PARAM_EVT_EXEC_POST:
         if ( param->is_param )
            { /* bound is parameter */
            P = (pdo_advantage_param*)param->driver_data;
            /*
             * if there was output, we want to copy that
             * output back to the zval.
             */
            if ( P->desired.direction & DD_OUTPUT )
               {
               res = store_zval_from_ads_value(&P->output.value, param->parameter, param->param_type TSRMLS_CC);
               }
            else
               {
               res = 1;
               }
            }
         else
            { /* bound is column */
            /*
             * we cannot use this time even to get column info:
             * the user may bind any time after execution. The
             * only time we know the col is bound is after a fetch.
             */
            res = 1;
            }
         break;

      case PDO_PARAM_EVT_FETCH_POST:
         if ( param->is_param )
            { /* bound is param */
            /* no need to do anything */
            res = 1;
            }
         else
            { /* bound is column */
            C = (pdo_advantage_col*) param->driver_data;
            /*
             *  We need to store the row's column's value in the zval provided,
             *  but first we need to determine the type if we haven't already
             */
            if ( !C->has_info )
               {
               res = PDO_ADVANTAGE_G(api).ads_get_column_info(S->stmt, param->paramno, &C->info);
               if ( !res )
                  {
                  DB_STMT_ERR(stmt);
                  break;
                  }

               /* we can set the param type at this point */
               param->param_type = get_closest_pdo_type(C->info.type TSRMLS_CC);

               C->has_info = 1;
               }
            /*
             *  Gather the value
             */
            res = PDO_ADVANTAGE_G(api).ads_get_column( S->stmt, param->paramno, &val);
            if ( !res )
               {
               DB_STMT_ERR(stmt);
               break;
               }
            /* store value in zval */
            res = store_zval_from_ads_value( &val, param->parameter, param->param_type TSRMLS_CC);
            }
         break;

      default:
         /* we don't care about any other state */
         res = 1;
         break;
      }

   DEBUG_EXIT(advantage_stmt_param_hook);
   return res;
}
/* }}} */



/* {{{ advantage_stmt_describe_col
 *
 *  Gets information from a particular column.
 */
static int advantage_stmt_describe_col(pdo_stmt_t *stmt, int colno TSRMLS_DC)
{
   int res;
   pdo_advantage_stmt *S;
   struct pdo_column_data *col;
   a_ads_column_info buf;

   DEBUG_ENT(advantage_stmt_describe_col);

   S = (pdo_advantage_stmt*) stmt->driver_data;
   col = &stmt->columns[colno];
   res = PDO_ADVANTAGE_G(api).ads_get_column_info( S->stmt, colno, &buf);

   if ( !res )
      { /* could not acquire column info */
      DB_STMT_ERR(stmt);
      goto advantage_stmt_describe_col_end;
      }

   /* set column data */
   col->name = estrdup( buf.name ) ;
   col->namelen = strlen( buf.name );
   col->maxlen = (unsigned long) buf.max_size;
   col->precision = (unsigned long) buf.precision;
   col->param_type = get_closest_pdo_type( buf.type TSRMLS_CC);

   advantage_stmt_describe_col_end:
   DEBUG_EXIT(advantage_stmt_describe_col);
   return res;
}
/* }}} */



/* {{{ advantage_stmt_get_col_data
 *
 * Get data from a particular column.
 */
static int advantage_stmt_get_col_data(pdo_stmt_t *stmt, int colno, char **ptr,
                                       unsigned long *len, int *caller_frees TSRMLS_DC)
{
   int res;
   pdo_advantage_stmt *S;
   struct pdo_column_data *C;
   a_ads_data_value val;
   zval *z;

   DEBUG_ENT(advantage_stmt_get_col_data);

   S = (pdo_advantage_stmt*)stmt->driver_data;
   C = &stmt->columns[colno];
   res = PDO_ADVANTAGE_G(api).ads_get_column(S->stmt, colno, &val);

   if ( !res )
      { /* couldn't get column data */
      DB_STMT_ERR(stmt);
      goto sasgcd_end;
      }

   /* delete if caller frees */
   if ( *caller_frees )
      {
      efree(*ptr);
      }

   if ( *(val.is_null) )
      {
      *ptr = NULL;
      *len = 0;
      goto sasgcd_end;
      }

   ALLOC_INIT_ZVAL(z);
   store_zval_from_ads_value( &val, z, C->param_type TSRMLS_CC);

   /* behave according to column data type */
   switch ( C->param_type )
      {
      case PDO_PARAM_LOB:
         /* handled same way as string */
      case PDO_PARAM_STR:
         *len = Z_STRLEN_P(z);
         *ptr = emalloc(*len + 1);
         memcpy(*ptr, Z_STRVAL_P(z), *len);
         (*ptr)[*len] = '\0';
         *caller_frees = 1;
         break;
      case PDO_PARAM_INT:
         *len = sizeof(long);
         *ptr = emalloc(*len);
         memcpy(*ptr,&Z_LVAL_P(z), *len);
         *caller_frees = 1;
         break;
      case PDO_PARAM_NULL:
      default:
         *ptr = NULL;
         *len = 0;
         break;
      }

   zval_dtor(z);

   sasgcd_end:
   DEBUG_EXIT(advantage_stmt_get_col_data);
   return res;
}
/* }}} */





/* {{{ advantage_stmt_set_attr
 *
 * Set statement attributes
 */
static int advantage_stmt_set_attr(pdo_stmt_t *stmt, long attr, zval *val TSRMLS_DC)
{
   DEBUG_ENT(advantage_stmt_set_param);
   /*   As far as I can tell, sacapi does not support any of the optional
    *   PDO params. I don't think the user should be able to mod client
    *   info either
    */
   DEBUG_EXIT(advantage_stmt_set_param);
   return 0;
}
/* }}} */



/* {{{ advantage_stmt_get_attr
 *
 * Get statement attributes.
 */
static int advantage_stmt_get_attr(pdo_stmt_t *stmt, long attr, zval *return_value TSRMLS_DC)
{
   int res, len;
   char *buf;
   pdo_dbh_t *dbh;
   pdo_advantage_db_handle *H;
   ADSHANDLE temp;
   a_ads_data_value val;

   DEBUG_ENT(advantage_stmt_get_attr);

   switch ( attr )
      {
      case PDO_ATTR_SERVER_VERSION:
         {
            /*
             * Have to query the database for this info.
             */
            dbh = stmt->dbh;
            H = (pdo_advantage_db_handle*) dbh->driver_data;
            temp = PDO_ADVANTAGE_G(api).ads_execute_direct( H->dbc, "SELECT PROPERTY ( 'ProductVersion' )" );
            if ( !temp )
               {
               DB_HANDLE_ERR(dbh);
               res = 0;
               break;
               }
            if ( !(PDO_ADVANTAGE_G(api).ads_fetch_next(temp))
                 || !(PDO_ADVANTAGE_G(api).ads_get_column( temp,0,&val )) )
               {
               /*
                *   should be a statement error, but the statement
                *   it messed up on is not the one in 'stmt'
                */
               DB_HANDLE_ERR(dbh);
               PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
               break;
               }
            else
               {
               res = 1;
               }
            PDO_ADVANTAGE_G(api).ads_free_stmt( temp );
            len = *(val.length);
            buf = emalloc(len);
            memcpy(buf, val.buffer, len);
            ZVAL_STRINGL(return_value, buf, len,1);
            break;
         }
      case PDO_ATTR_CLIENT_VERSION:
         {
            buf = emalloc(20);
            res = PDO_ADVANTAGE_G(api).ads_client_version(
                                                         buf, sizeof(buf));
            ZVAL_STRINGL(return_value, buf, strlen(buf),1);
         }
         if ( !res ) DB_STMT_ERR(stmt);
         break;
      default:
         strcpy(stmt->error_code, "IM001");
         DB_STMT_ERR(stmt);
         res = 0;
         break;
      }

   DEBUG_EXIT(advantage_stmt_get_attr);
   return res;
}
/* }}} */



/* {{{ advantage_stmt_get_col_meta
 *
 * Get metadata of a particular column.
 */
static int advantage_stmt_get_col_meta(pdo_stmt_t *stmt, long colno, zval *return_value TSRMLS_DC)
{
   int res=0;
   long len;
   char *buf;
   // zval* flags; /* stores additional fields provided by ads */
   pdo_advantage_stmt *S;
   a_ads_column_info info;

   DEBUG_ENT(advantage_stmt_get_col_meta);

   S = (pdo_advantage_stmt*)stmt->driver_data;

   /* query for column info (metadata) */
   res = PDO_ADVANTAGE_G(api).ads_get_column_info(S->stmt, colno, &info);
   if ( !res )
      {
      DB_STMT_ERR(stmt);
      goto advantage_stmt_get_col_meta_end;
      }

   /* initialize arrays */
   //MAKE_STD_ZVAL(flags);
   array_init(return_value);
   //array_init(flags);

   /* column name */
   len = strlen(info.name);
   buf = emalloc(len);
   memcpy(buf, info.name, len);
   add_assoc_string(return_value, "name", buf, 1);

   /* column length */
   add_assoc_long(return_value, "len", ((long) info.max_size));

   /* column precision */
   add_assoc_long(return_value, "precision", ((long) info.precision));

   /* column pdo type */
   add_assoc_long(return_value, "pdo_type", ((long)
                                             get_closest_pdo_type( info.type TSRMLS_CC)));

   /* column scale */
   add_assoc_long(return_value, "scale", ((long) info.scale));

   /* table name */
   // TODO

   /* php type */
   // TODO (?)

   /* native type */
#define NT_CASE(typename) \
    case typename: \
         len = strlen(#typename); \
    buf = emalloc(len); \
    memcpy(buf, #typename, len); \
    add_assoc_string(return_value, "native_type", buf, 1); \
    break
   switch ( info.native_type )
      {
      NT_CASE(DT_NOTYPE);
      NT_CASE(DT_DATE);
      NT_CASE(DT_TIME);
      NT_CASE(DT_TIMESTAMP);
      NT_CASE(DT_VARCHAR);
      NT_CASE(DT_FIXCHAR);
      NT_CASE(DT_LONGVARCHAR);
      NT_CASE(DT_STRING);
      NT_CASE(DT_DOUBLE);
      NT_CASE(DT_FLOAT);
      NT_CASE(DT_DECIMAL);
      NT_CASE(DT_INT);
      NT_CASE(DT_SMALLINT);
      NT_CASE(DT_BINARY);
      NT_CASE(DT_LONGBINARY);
      NT_CASE(DT_TINYINT);
      NT_CASE(DT_BIGINT);
      NT_CASE(DT_UNSINT);
      NT_CASE(DT_UNSSMALLINT);
      NT_CASE(DT_UNSBIGINT);
      NT_CASE(DT_BIT);
      NT_CASE(DT_LONGNVARCHAR);
      }

   /* flags */
   //TODO

   advantage_stmt_get_col_meta_end:
   DEBUG_EXIT(advantage_stmt_get_col_meta);
   return res;
}
/* }}} */


/* {{{ advantage_stmt_next_rowset
* Move statement to next rowset.
*
*/
static int advantage_stmt_next_rowset(pdo_stmt_t *stmt TSRMLS_DC) /* {{{ */
{
   int res=0;
   int intRowsAffected = 0;
   pdo_advantage_stmt *S;

   DEBUG_ENT(advantage_stmt_next_rowset);

   S = (pdo_advantage_stmt*)stmt->driver_data;

   res = PDO_ADVANTAGE_G(api).ads_get_next_result( S->stmt);
   if ( !res )
      {
      DB_STMT_ERR(stmt);
      goto advantage_stmt_next_rowset_end;
      }

   stmt->column_count = PDO_ADVANTAGE_G(api).ads_num_cols(S->stmt);
   stmt->row_count = PDO_ADVANTAGE_G(api).ads_num_rows(S->stmt);
   intRowsAffected = PDO_ADVANTAGE_G(api).ads_affected_rows(S->stmt);
   if ( intRowsAffected > 0 )
      { /* we also/or have some affected rows */
      stmt->row_count += intRowsAffected;
      }


   advantage_stmt_next_rowset_end:
   DEBUG_EXIT(advantage_stmt_next_rowset);
   return res;
}
/* }}} */

/* {{{ advantage_stmt_cursor_closer
* Close any open cursors on this statement
*
*/
static int advantage_stmt_cursor_closer(pdo_stmt_t *stmt TSRMLS_DC) /* {{{ */
{
   int res=0;
   pdo_advantage_stmt *S;

   S = (pdo_advantage_stmt *) stmt->driver_data;

   DEBUG_ENT(advantage_stmt_cursor_closer);

   if (S->stmt)
      {
      res = PDO_ADVANTAGE_G(api).ads_reset(S->stmt);
      if ( !res )
         {
         DB_STMT_ERR(stmt);
         goto advantage_stmt_cursor_closer;
         }
      }

   advantage_stmt_cursor_closer:
   DEBUG_EXIT(advantage_stmt_cursor_closer);
   return res;
}
/* }}} */


/* {{{ advantage_stmt_methods
 *
 * Provides function pointers of above to PDO.
 */
struct pdo_stmt_methods advantage_stmt_methods = {
   advantage_stmt_dtor,
   advantage_stmt_execute,
   advantage_stmt_fetch,
   advantage_stmt_describe_col,
   advantage_stmt_get_col_data,
   advantage_stmt_param_hook,
   NULL, /* advantage_stmt_set_attr */
   advantage_stmt_get_attr,
   advantage_stmt_get_col_meta,
   advantage_stmt_next_rowset,
   advantage_stmt_cursor_closer
};
/* }}} */
