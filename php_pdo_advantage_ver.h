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

#ifndef PHP_PDO_ADVANTAGE_VER
#define PHP_PDO_ADVANTAGE_VER

#define PHP_PDO_SA_MAJOR   0
#define PHP_PDO_SA_MINOR   1
#define PHP_PDO_SA_PATCH   0

#define _php_str( x ) # x
#define _eval( x ) _php_str( x )

#define PHP_PDO_ADS_MAJOR _eval( PHP_PDO_SA_MAJOR )
#define PHP_PDO_ADS_MINOR _eval( PHP_PDO_SA_MINOR )
#define PHP_PDO_ADS_PATCH _eval( PHP_PDO_SA_PATCH )

#define PHP_PDO_ADVANTAGE_VERSION  PHP_PDO_ADS_MAJOR "." PHP_PDO_ADS_MINOR "." PHP_PDO_ADS_PATCH

#endif
