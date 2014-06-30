/**
TODO add copyright
*/

#include "hphp/runtime/ext/xdebug/xdebug_xml.h"

#ifndef __HAVE_XDEBUG_DBGP_H__
#define __HAVE_XDEBUG_DBGP_H__

#define STATE_NORMAL                   0
#define STATE_QUOTED                   1
#define STATE_OPT_FOLLOWS              2
#define STATE_SEP_FOLLOWS              3
#define STATE_VALUE_FOLLOWS_FIRST_CHAR 4
#define STATE_VALUE_FOLLOWS            5
#define STATE_SKIP_CHAR                6

#define XDEBUG_DBGP_NONE          0x00
#define XDEBUG_DBGP_POST_MORTEM   0x01

#define XDEBUG_ERROR_OK                              0
#define XDEBUG_ERROR_PARSE                           1
#define XDEBUG_ERROR_DUP_ARG                         2
#define XDEBUG_ERROR_INVALID_ARGS                    3
#define XDEBUG_ERROR_UNIMPLEMENTED                   4
#define XDEBUG_ERROR_COMMAND_UNAVAILABLE             5

#define CMD_OPTION(opt)    (opt == '-' ? args->value[26] : args->value[(opt) - 'a'])

/*
#define DBGP_FUNC_ENTRY(name,flags)       { #name, xdebug_dbgp_handle_##name, 0, flags },
#define DBGP_CONT_FUNC_ENTRY(name,flags)  { #name, xdebug_dbgp_handle_##name, 1, flags },
#define DBGP_STOP_FUNC_ENTRY(name,flags)  { #name, xdebug_dbgp_handle_##name, 2, flags },
*/
#define DBGP_FUNC_ENTRY(name,flags)       { #name, 0, flags },
#define DBGP_CONT_FUNC_ENTRY(name,flags)  { #name, 1, flags },
#define DBGP_STOP_FUNC_ENTRY(name,flags)  { #name, 2, flags },

typedef struct xdebug_dbgp_arg {
  char *value[27]; /* one extra for - */
} xdebug_dbgp_arg;

struct _xdebug_dbgp_cmd {
  char *name;
  //void (*handler)(DBGP_FUNC_PARAMETERS);
  int  cont;
  int  flags;
};
typedef struct _xdebug_dbgp_cmd xdebug_dbgp_cmd;
 

int xdebug_dbgp_parse_cmd(char *line, char **cmd, xdebug_dbgp_arg **ret_args);
xdebug_dbgp_cmd* lookup_cmd(char *cmd);
void xdebug_dbgp_arg_dtor(xdebug_dbgp_arg *arg);

#endif //__HAVE_XDEBUG_DBGP_H__
