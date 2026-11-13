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

#ifndef PHP_PDO_ADVANTAGE_H
#define PHP_PDO_ADVANTAGE_H

#include "adscapidll.h"

extern zend_module_entry pdo_advantage_module_entry;
#define phpext_pdo_advantage_ptr &pdo_advantage_module_entry

#ifdef ZTS
   #include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(pdo_advantage);
PHP_MSHUTDOWN_FUNCTION(pdo_advantage);
PHP_RINIT_FUNCTION(pdo_advantage);
PHP_RSHUTDOWN_FUNCTION(pdo_advantage);
PHP_MINFO_FUNCTION(pdo_advantage);

ZEND_BEGIN_MODULE_GLOBALS(pdo_advantage)
#if 0
long  global_value;
char *global_string;
#endif
void * context;
AdvantageInterface api;
long ads_init_called;
ZEND_END_MODULE_GLOBALS(pdo_advantage)


#ifdef ZTS
   #define PDO_ADVANTAGE_G(v) TSRMG(pdo_advantage_globals_id, zend_pdo_advantage_globals *, v)
#else
   #define PDO_ADVANTAGE_G(v) (pdo_advantage_globals.v)
#endif

#undef DEBUG

#ifndef DEBUG
   #define DEBUG_ENT(func) NULL
   #define DEBUG_EXIT(func) NULL
   #define DEBUG_LINE(line) NULL
   #define DEBUG_STRING(str) NULL
   #define DEBUG_DEC(dec) NULL
#else
   #define DEBUG_ENT(func) printf("DEBUG: entering %s\n", #func)
   #define DEBUG_EXIT(func) printf("DEBUG: leaving %s\n", #func)
   #define DEBUG_LINE(line) printf("DEBUG:%s\n", #line)
   #define DEBUG_STRING(str) printf("DEBUG: %s = %s\n", #str, str)
   #define DEBUG_DEC(dec) printf("DEBUG: %s = %d\n", #dec, dec)
#endif

#define COMMIT_AS_NEEDED \
   if (dbh->auto_commit & !dbh->in_txn) \
      res = PDO_ADVANTAGE_G(api).ads_commit(H->dbc)

//ZEN_EXTERN_MODULE_GLOBALS(pdo_advantage)

#endif   /* PHP_PDO_ADVANTAGE_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
*/
