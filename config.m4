
PHP_ARG_WITH(pdo_advantage, for pdo_advantage support,
[  --with-pdo_advantage             Include pdo_advantage support])

if test "$PHP_PDO_ADVANTAGE" != "no"; then

  if test "$PHP_PDO" = "no" && test "$ext_shared" = "no"; then
    AC_MSG_ERROR([PDO is not enabled! Add --enable-pdo to your configure line.])
  fi

  ifdef([PHP_CHECK_PDO_INCLUDES],
  [
    PHP_CHECK_PDO_INCLUDES
  ],[
    AC_MSG_CHECKING([for PDO includes])
    if test -f $abs_srcdir/include/php/ext/pdo/php_pdo_driver.h; then
      pdo_inc_path=$abs_srcdir/ext
    elif test -f $abs_srcdir/ext/pdo/php_pdo_driver.h; then
      pdo_inc_path=$abs_srcdir/ext
    elif test -f $prefix/include/php/ext/pdo/php_pdo_driver.h; then
      pdo_inc_path=$prefix/include/php/ext
    else
      AC_MSG_ERROR([Cannot find php_pdo_driver.h.])
    fi
    AC_MSG_RESULT($pdo_inc_path)
  ])
  php_pdo_advantage_sources_core="pdo_advantage.c advantage_driver.c advantage_statement.c adscapidll.c"

  PHP_NEW_EXTENSION(pdo_advantage, $php_pdo_advantage_sources_core, $ext_shared,,-I$pdo_inc_path)
  AC_DEFINE(HAVE_PDO_ADVANTAGE, 1, [ Whether you have PDO Advantage ])
  ifdef([PHP_ADD_EXTENSION_DEP],
  [
    PHP_ADD_EXTENSION_DEP(pdo_advantage, pdo)
  ])
fi
