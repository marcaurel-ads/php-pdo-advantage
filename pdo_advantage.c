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

#include "stdio.h"
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "pdo/php_pdo.h"
#include "pdo/php_pdo_driver.h"
#include "php_pdo_advantage.h"
#include "php_pdo_advantage_int.h"
#include "php_pdo_advantage_ver.h"
#include "adscapidll.h"


#if defined( WIN32 )

   #define DBCAPI_PREFIX   ""
   #define DBCAPI_SUFFIX   ".dll"

   #if defined( ZTS )
      #define DBCAPI_ZTS     ""
   #else
      #define DBCAPI_ZTS      ""
   #endif

   #define DLL_PATH    "PATH"

#else

   #define DBCAPI_PREFIX   "lib"
   #define DBCAPI_SUFFIX   ".so"

   #if defined( ZTS )
      #define DBCAPI_ZTS     "_r"
   #else
      #define DBCAPI_ZTS      ""
   #endif

   #define DLL_PATH    "LD_LIBRARY_PATH"

#endif

#define DBCAPI_NAME "ACE"
/* api initialization error message */
#define DBCAPI_NOT_FOUND_ERROR "The Advantage libraries \
    could not be loaded. Please ensure that " DBCAPI_NAME " can be found in your " DLL_PATH " environment variable."

ZEND_DECLARE_MODULE_GLOBALS(pdo_advantage);

/* True global resources - no need for thread safety here */
static int le_pdo_advantage;



/* {{{ pdo_advantage_functions[]
 *
 * Every user visible function must have an entry in pdo_advantage_functions[].
 */
const zend_function_entry pdo_advantage_functions[] = {
   {NULL, NULL, NULL}
};
/* }}} */



/* {{{ pdo_advantage_module_entry
 *
 * TODO:Might need a ZEND_MODULE_API_NO check. STANDARD_MODULE_HEADER_EX
 * and the next two lines may be new additions.
 * TODO:Create a proper version number header file.
 */
/* establishes dependency relations with PDO */
static zend_module_dep pdo_advantage_deps[] = {
   ZEND_MOD_REQUIRED("pdo")
   {NULL,NULL,NULL}
};
/* adds pdo_advantage as a module */
zend_module_entry pdo_advantage_module_entry = {
   STANDARD_MODULE_HEADER_EX,
   NULL,
   pdo_advantage_deps,
   "PDO_ADVANTAGE",
   pdo_advantage_functions,
   PHP_MINIT(pdo_advantage),
   PHP_MSHUTDOWN(pdo_advantage),
   PHP_RINIT(pdo_advantage),
   PHP_RSHUTDOWN(pdo_advantage),
   PHP_MINFO(pdo_advantage),
   PHP_PDO_ADVANTAGE_VERSION,
   STANDARD_MODULE_PROPERTIES
};
/* }}} */



#ifdef COMPILE_DL_PDO_ADVANTAGE
ZEND_GET_MODULE(pdo_advantage)
#endif



/* {{{ php_pdo_advantage_init_globals
 */
static void php_pdo_advantage_init_globals(zend_pdo_advantage_globals *pdo_advantage_globals)
{
#if 0
   pdo_advantage_globals->global_value = 0;
   pdo_advantage_globals->global_string = NULL;
#endif
   memset( pdo_advantage_globals, 0, sizeof(*pdo_advantage_globals) );

}
/* }}} */



/* {{{ PHP_MINIT_FUNCTION
 *
 * Initializes globals and loads dbcapi
 */
PHP_MINIT_FUNCTION(pdo_advantage)
{

   // initialize globals (besides api)
   ZEND_INIT_MODULE_GLOBALS(pdo_advantage, php_pdo_advantage_init_globals, NULL);

   // load dbcapi
   if ( !ads_initialize_interface( &(PDO_ADVANTAGE_G(api)), NULL) )
      {
      // documents failed load
      php_error_docref(NULL TSRMLS_CC, E_WARNING, DBCAPI_NOT_FOUND_ERROR );
      }

   if ( FAILURE == php_pdo_register_driver(&pdo_advantage_driver) )
      {
      return FAILURE;
      }

   return SUCCESS;

}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pdo_advantage)
{

   // finalize api
   if ( !PDO_ADVANTAGE_G(api).initialized )
      {
      return SUCCESS;
      }

   if ( PDO_ADVANTAGE_G(ads_init_called) == 1 )
      {

      PDO_ADVANTAGE_G(api).ads_fini();

      PDO_ADVANTAGE_G(ads_init_called) = 0;
      }
   ads_finalize_interface( &(PDO_ADVANTAGE_G(api)) );

   php_pdo_unregister_driver(&pdo_advantage_driver);
   return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 *
 * Initializes dbcapi
 */
PHP_RINIT_FUNCTION(pdo_advantage)
{
   UNSIGNED32 usVersionAvailable;
   if ( PDO_ADVANTAGE_G(api).initialized && PDO_ADVANTAGE_G(ads_init_called) == 0 )
      {

      if ( PDO_ADVANTAGE_G(ads_init_called) == 0 && !PDO_ADVANTAGE_G(api).ads_init( "PHP", 1, &usVersionAvailable ) )
         {
         return FAILURE;
         }

      PDO_ADVANTAGE_G(ads_init_called) = 1;
      }
   else  //when PHP is loaded by Apache as an ISAPI dll, we need to initialize on each request
   {
      // load dbcapi
      if ( !ads_initialize_interface( &(PDO_ADVANTAGE_G(api)), NULL) )
         {
         // documents failed load
         php_error_docref(NULL TSRMLS_CC, E_WARNING, DBCAPI_NOT_FOUND_ERROR );
         }
   }

   return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(pdo_advantage)
{
   return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 *
 */
PHP_MINFO_FUNCTION(pdo_advantage)
{
   UNSIGNED8 client_version[32];
   UNSIGNED32 rc=0;

   php_info_print_table_start();
   php_info_print_table_header(2, "pdo_advantage support", "enabled");
   // php_info_print_table_row(...);
   if ( PDO_ADVANTAGE_G(api).initialized )
      {
      rc = PDO_ADVANTAGE_G(api).ads_client_version( client_version, sizeof(client_version) );
      if ( !rc )
         {
         php_sprintf( client_version, "Unknown" );
         }
      php_info_print_table_row(2, "ADVANTAGE client version", client_version);
      }
   else
      {
      php_info_print_table_row(2, "ADVANTAGE client version", DBCAPI_NOT_FOUND_ERROR );
      }
   php_info_print_table_end();

}
/* }}} */

