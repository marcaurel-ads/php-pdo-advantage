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

#ifndef PHP_PDO_ADVANTAGE_INT_H
#define PHP_PDO_ADVANTAGE_INT_H

#include "adscapidll.h"

/* Represents error info */
typedef struct
   {
   char sqlstate[6];
   char errmsg[ADS_MAX_ERROR_LEN];
   int errcode, line;
   const char* file;
   } pdo_advantage_errinfo;

/* Holds representation of connection and error info */
typedef struct
   {
   a_ads_connection *dbc;
   pdo_advantage_errinfo einfo;
   } pdo_advantage_db_handle;

/* Holds information for a single parameter */
typedef struct
   {
   a_ads_bind_param desired; // ads desired param vals
   a_ads_bind_param input;   // param to be bound as input
   a_ads_bind_param output;  // param to be bound as output
   } pdo_advantage_param;

/* Holds information for a single column */
typedef struct
   {
   short* is_unsigned;
   unsigned int has_info;
   a_ads_column_info info;
   } pdo_advantage_col;

/* Holds representation of a statement and error info*/
typedef struct
   {
   ADSHANDLE stmt;
   unsigned long cur_row, num_params;
   pdo_advantage_errinfo einfo;
   } pdo_advantage_stmt;

extern pdo_driver_t pdo_advantage_driver;
extern struct pdo_stmt_methods advantage_stmt_methods;

/* void because of build order. Avoids compiler complaining about contents */
void pdo_advantage_error (pdo_dbh_t *dbh, pdo_stmt_t *stmt,
                          const char *file, int line TSRMLS_DC);
#define DB_HANDLE_ERR(dbh) pdo_advantage_error(dbh, NULL, __FILE__, __LINE__ TSRMLS_CC)
#define DB_STMT_ERR(stmt) pdo_advantage_error(stmt->dbh, stmt, __FILE__, __LINE__ TSRMLS_CC)

#endif
