/**
copyright
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "hphp/runtime/ext/xdebug/xdebug_dbgp.h"

/*****************************************************************************
** Dispatcher tables for supported debug commands
*/

static xdebug_dbgp_cmd dbgp_commands[] = {
  /* DBGP_FUNC_ENTRY(break) */
  DBGP_FUNC_ENTRY(breakpoint_get,    XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(breakpoint_list,   XDEBUG_DBGP_POST_MORTEM)
  DBGP_FUNC_ENTRY(breakpoint_remove, XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(breakpoint_set,    XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(breakpoint_update, XDEBUG_DBGP_NONE)

  DBGP_FUNC_ENTRY(context_get,       XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(context_names,     XDEBUG_DBGP_POST_MORTEM)

  DBGP_FUNC_ENTRY(eval,              XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(feature_get,       XDEBUG_DBGP_POST_MORTEM)
  DBGP_FUNC_ENTRY(feature_set,       XDEBUG_DBGP_NONE)

  DBGP_FUNC_ENTRY(typemap_get,       XDEBUG_DBGP_POST_MORTEM)
  DBGP_FUNC_ENTRY(property_get,      XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(property_set,      XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(property_value,    XDEBUG_DBGP_NONE)

  DBGP_FUNC_ENTRY(source,            XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(stack_depth,       XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(stack_get,         XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(status,            XDEBUG_DBGP_POST_MORTEM)

  DBGP_FUNC_ENTRY(stderr,            XDEBUG_DBGP_NONE)
  DBGP_FUNC_ENTRY(stdout,            XDEBUG_DBGP_NONE)

  DBGP_CONT_FUNC_ENTRY(run,          XDEBUG_DBGP_NONE)
  DBGP_CONT_FUNC_ENTRY(step_into,    XDEBUG_DBGP_NONE)
  DBGP_CONT_FUNC_ENTRY(step_out,     XDEBUG_DBGP_NONE)
  DBGP_CONT_FUNC_ENTRY(step_over,    XDEBUG_DBGP_NONE)

  DBGP_STOP_FUNC_ENTRY(stop,         XDEBUG_DBGP_POST_MORTEM)
  DBGP_STOP_FUNC_ENTRY(detach,       XDEBUG_DBGP_NONE)

  /* Non standard functions */
  DBGP_FUNC_ENTRY(xcmd_profiler_name_get,    XDEBUG_DBGP_POST_MORTEM)
  DBGP_FUNC_ENTRY(xcmd_get_executable_lines, XDEBUG_DBGP_NONE)
  { 0, 0 }
};

/*****************************************************************************
** Utility functions
*/

xdebug_dbgp_cmd* lookup_cmd(char *cmd)
{
  xdebug_dbgp_cmd *ptr = dbgp_commands;

  while (ptr->name) {
    if (strcmp(ptr->name, cmd) == 0) {
      return ptr;
    }
    ptr++;
  }
  return nullptr;
}

void php_stripcslashes(char *str, int *len)
{
  char *source, *target, *end;
  int  nlen = *len, i;
  char numtmp[4];

  for (source=str, end=str+nlen, target=str; source < end; source++) {
    if (*source == '\\' && source+1 < end) {
      source++;
      switch (*source) {
        case 'n':  *target++='\n'; nlen--; break;
        case 'r':  *target++='\r'; nlen--; break;
        case 'a':  *target++='\a'; nlen--; break;
        case 't':  *target++='\t'; nlen--; break;
        case 'v':  *target++='\v'; nlen--; break;
        case 'b':  *target++='\b'; nlen--; break;
        case 'f':  *target++='\f'; nlen--; break;
        case '\\': *target++='\\'; nlen--; break;
        case 'x':
          if (source+1 < end && isxdigit((int)(*(source+1)))) {
            numtmp[0] = *++source;
            if (source+1 < end && isxdigit((int)(*(source+1)))) {
              numtmp[1] = *++source;
              numtmp[2] = '\0';
              nlen-=3;
            } else {
              numtmp[1] = '\0';
              nlen-=2;
            }
            *target++=(char)strtol(numtmp, nullptr, 16);
            break;
          }
          /* break is left intentionally */
        default:
          i=0;
          while (source < end && *source >= '0' && *source <= '7' && i<3) {
            numtmp[i++] = *source++;
          }
          if (i) {
            numtmp[i]='\0';
            *target++=(char)strtol(numtmp, nullptr, 8);
            nlen-=i;
            source--;
          } else {
            *target++=*source;
            nlen--;
          }
      }
    } else {
      *target++=*source;
    }
  }

  if (nlen != 0) {
    *target='\0';
  }

  *len = nlen;
}

int xdebug_dbgp_parse_cmd(char *line, char **cmd, xdebug_dbgp_arg **ret_args)
{
  xdebug_dbgp_arg *args = nullptr;
  char *ptr;
  int   state;
  int   charescaped = 0;
  char  opt = ' ', *value_begin = nullptr;

  args = (xdebug_dbgp_arg*) xdmalloc(sizeof (xdebug_dbgp_arg));
  memset(args->value, 0, sizeof(args->value));
  *cmd = nullptr;

  /* Find the end of the command, this is always on the first space */
  ptr = strchr(line, ' ');
  if (!ptr) {
    /* No space found. If the line is not empty, return the line
     * and assume it only consists of the command name. If the line
     * is 0 chars long, we return a failure. */
    if (strlen(line)) {
      *cmd = strdup(line);
      *ret_args = args;
      return XDEBUG_ERROR_OK;
    } else {
      goto parse_error;
    }
  } else {
    /* A space was found, so we copy everything before it
     * into the cmd parameter. */
    *cmd = (char*) xdcalloc(1, ptr - line + 1);
    memcpy(*cmd, line, ptr - line);
  }
  /* Now we loop until we find the end of the string, which is the \0
   * character */
  state = STATE_NORMAL;
  do {
    ptr++;
    switch (state) {
      case STATE_NORMAL:
        if (*ptr != '-') {
          goto parse_error;
        } else {
          state = STATE_OPT_FOLLOWS;
        }
        break;
      case STATE_OPT_FOLLOWS:
        opt = *ptr;
        state = STATE_SEP_FOLLOWS;
        break;
      case STATE_SEP_FOLLOWS:
        if (*ptr != ' ') {
          goto parse_error;
        } else {
          state = STATE_VALUE_FOLLOWS_FIRST_CHAR;
          value_begin = ptr + 1;
        }
        break;
      case STATE_VALUE_FOLLOWS_FIRST_CHAR:
        if (*ptr == '"' && opt != '-') {
          value_begin = ptr + 1;
          state = STATE_QUOTED;
        } else {
          state = STATE_VALUE_FOLLOWS;
        }
        break;
      case STATE_VALUE_FOLLOWS:
        if ((*ptr == ' ' && opt != '-') || *ptr == '\0') {
          int index = opt - 'a';

          if (opt == '-') {
            index = 26;
          }

          if (!args->value[index]) {
            args->value[index] = (char*) xdcalloc(1, ptr - value_begin + 1);
            memcpy(args->value[index], value_begin, ptr - value_begin);
            state = STATE_NORMAL;
          } else {
            goto duplicate_opts;
          }
        }
        break;
      case STATE_QUOTED:
        /* if the quote is escaped, remain in STATE_QUOTED.  This
           will also handle other escaped chars, or an instance of
           an escaped slash followed by a quote: \\"
        */
        if (*ptr == '\\') {
          charescaped = !charescaped;
        } else
        if (*ptr == '"') {
          int index = opt - 'a';

          if (charescaped) {
            charescaped = 0;
            break;
          }
          if (opt == '-') {
            index = 26;
          }

          if (!args->value[index]) {
            int len = ptr - value_begin;
            args->value[index] = (char*) xdcalloc(1, len + 1);
            memcpy(args->value[index], value_begin, len);
            php_stripcslashes(args->value[index], &len);
            state = STATE_SKIP_CHAR;
          } else {
            goto duplicate_opts;
          }
        }
        break;
      case STATE_SKIP_CHAR:
        state = STATE_NORMAL;
        break;
        
    }
  } while (*ptr);
  *ret_args = args;
  return XDEBUG_ERROR_OK;

parse_error:
  *ret_args = args;
  return XDEBUG_ERROR_PARSE;

duplicate_opts:
  *ret_args = args;
  return XDEBUG_ERROR_DUP_ARG;
}

