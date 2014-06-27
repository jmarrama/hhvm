message(STATUS "Compiling everything in xdebug cause of awesome")
HHVM_EXTENSION(xdebug ext_xdebug.cpp xdebug_str.cpp xdebug_xml.cpp)
HHVM_SYSTEMLIB(xdebug ext_xdebug.php)
