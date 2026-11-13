# php-pdo-advantage

`PDO_Advantage`: a PHP PDO driver for Advantage Database Server, adapted by
iAnywhere and SAP from the SQL Anywhere PDO driver. It loads the Advantage
Client Engine (`ace32`/`ace64`/`libace`) at run time and calls its DBCAPI
(`ads_*` functions, declared in `dbcapi.h`) plus `AdsBeginTransaction`.

Licensed under the Apache License 2.0 (`LICENSE`, and the header of each
source file).

## Source

ADS 12.0, Windows installer `php_win`, folder `PDO_Advantage/`, taken unchanged.

Not included: `ace.h`. It is the Advantage Client Engine SDK header and is
licensed to Advantage licensees only, not under Apache 2.0. Building needs it
from the ACE SDK. Also left out: the prebuilt DLLs.
