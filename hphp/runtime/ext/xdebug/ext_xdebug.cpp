/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2013 Facebook, Inc. (http://www.facebook.com)     |
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "hphp/runtime/ext/xdebug/ext_xdebug.h"
#include "hphp/runtime/ext/xdebug/xdebug_xml.h"
#include "hphp/runtime/ext/xdebug/xdebug_dbgp.h"

#include "hphp/runtime/base/code-coverage.h"
#include "hphp/runtime/base/execution-context.h"
#include "hphp/runtime/base/thread-info.h"
#include "hphp/runtime/vm/unwind.h"
#include "hphp/runtime/vm/vm-regs.h"
#include "hphp/util/logger.h"

TRACE_SET_MOD(xdebug);

namespace HPHP {

#define XDEBUG_COMMAND_DELIM '\0'
#define XDEBUG_AUTHOR "ochotinun+jmarrama@box.com"
#define XDEBUG_URL "unicorns.org"
#define XDEBUG_COPYRIGHT "onmytodolist,facebookownsthis"
#define DBGP_VERSION "1.0"

///////////////////////////////////////////////////////////////////////////////

static const StaticString s_CALL_FN_MAIN("{main}");

/*
 * Returns the frame of the callee's callee. Useful for the xdebug_call_*
 * functions. Only returns nullptr if the callee is the top level pseudo-main
 *
 * If an offset pointer is passed, we store in it it the pc offset of the
 * call to the callee.
 */
static ActRec *get_call_fp(Offset *off = nullptr) {
  // We want the frame of our callee's callee
  VMRegAnchor _; // Ensure consistent state for vmfp
  ActRec* fp0 = g_context->getPrevVMState(vmfp());
  assert(fp0);
  ActRec* fp1 = g_context->getPrevVMState(fp0, off);

  // fp1 should only be NULL if fp0 is the top-level pseudo-main
  if (!fp1) {
    assert(fp0->m_func->isPseudoMain());
    fp1 = nullptr;
  }
  return fp1;
}

static xdebug_str *make_message(xdebug_xml_node *message)
{
  xdebug_str  xml_message = {0, 0, NULL};
  xdebug_str *ret;

  xdebug_str_ptr_init(ret);

  xdebug_xml_return_node(message, &xml_message);

  xdebug_str_add(ret, xdebug_sprintf("%d", xml_message.l + sizeof("<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n") - 1), 1);
  xdebug_str_addl(ret, "\0", 1, 0);
  xdebug_str_add(ret, "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n", 0);
  xdebug_str_add(ret, xml_message.d, 0);
  xdebug_str_addl(ret, "\0", 1, 0);
  xdebug_str_dtor(xml_message);

  return ret;
}

static void send_message(int socketFd, xdebug_xml_node *message)
{
  xdebug_str *tmp = make_message(message);
  write(socketFd, tmp->d, tmp->l);
  xdebug_str_ptr_dtor(tmp);
}


///////////////////////////////////////////////////////////////////////////////

static bool HHVM_FUNCTION(xdebug_break)
  XDEBUG_NOTIMPLEMENTED

static String HHVM_FUNCTION(xdebug_call_class)
  XDEBUG_NOTIMPLEMENTED

static String HHVM_FUNCTION(xdebug_call_file) {
  // PHP5 xdebug returns the top-level file if the callee is top-level
  ActRec *fp = get_call_fp();
  if (fp == nullptr) {
    VMRegAnchor _; // Ensure consistent state for vmfp
    fp = g_context->getPrevVMState(vmfp());
    assert(fp);
  }
  return String(fp->m_func->filename()->data(), CopyString);
}

static int64_t HHVM_FUNCTION(xdebug_call_line) {
  // PHP5 xdebug returns 0 when it can't determine the line number
  Offset pc;
  ActRec *fp = get_call_fp(&pc);
  if (fp == nullptr) {
    return 0;
  }

  Unit *unit = fp->m_func->unit();
  assert(unit);
  return unit->getLineNumber(pc);
}

static Variant HHVM_FUNCTION(xdebug_call_function) {
  // PHP5 xdebug returns false if the callee is top-level
  ActRec *fp = get_call_fp();
  if (fp == nullptr) {
    return false;
  }

  // PHP5 xdebug returns "{main}" for pseudo-main
  if (fp->m_func->isPseudoMain()) {
    return s_CALL_FN_MAIN;
  }
  return String(fp->m_func->name()->data(), CopyString);
}

static bool HHVM_FUNCTION(xdebug_code_coverage_started) {
  ThreadInfo *ti = ThreadInfo::s_threadInfo.getNoCheck();
  return ti->m_reqInjectionData.getCoverage();
}

static TypedValue* HHVM_FN(xdebug_debug_zval)(ActRec* ar)
  XDEBUG_NOTIMPLEMENTED

static TypedValue* HHVM_FN(xdebug_debug_zval_stdout)(ActRec* ar)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_disable)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_dump_superglobals)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_enable)
  XDEBUG_NOTIMPLEMENTED

static Array HHVM_FUNCTION(xdebug_get_code_coverage) {
  ThreadInfo *ti = ThreadInfo::s_threadInfo.getNoCheck();
  if (ti->m_reqInjectionData.getCoverage()) {
    return ti->m_coverage->Report(false);
  }
  return Array::Create();
}

static Array HHVM_FUNCTION(xdebug_get_collected_errors,
                           bool clean /* = false */)
  XDEBUG_NOTIMPLEMENTED

static Array HHVM_FUNCTION(xdebug_get_declared_vars)
  XDEBUG_NOTIMPLEMENTED

static Array HHVM_FUNCTION(xdebug_get_function_stack)
  XDEBUG_NOTIMPLEMENTED

static Array HHVM_FUNCTION(xdebug_get_headers)
  XDEBUG_NOTIMPLEMENTED

static String HHVM_FUNCTION(xdebug_get_profiler_filename)
  XDEBUG_NOTIMPLEMENTED

static int64_t HHVM_FUNCTION(xdebug_get_stack_depth)
  XDEBUG_NOTIMPLEMENTED

static String HHVM_FUNCTION(xdebug_get_tracefile_name)
  XDEBUG_NOTIMPLEMENTED

static bool HHVM_FUNCTION(xdebug_is_enabled)
  XDEBUG_NOTIMPLEMENTED

static int64_t HHVM_FUNCTION(xdebug_memory_usage) {
  // With jemalloc, the usage can go negative (see ext_std_options.cpp:831)
  int64_t usage = MM().getStats().usage;
  assert(use_jemalloc || usage >= 0);
  return std::max<int64_t>(usage, 0);
}

static int64_t HHVM_FUNCTION(xdebug_peak_memory_usage) {
  return MM().getStats().peakUsage;
}

static void HHVM_FUNCTION(xdebug_print_function_stack,
                          const String& message /* = "user triggered" */,
                          int64_t options /* = 0 */)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_start_code_coverage,
                          int64_t options /* = 0 */) {
  // XDEBUG_CC_UNUSED and XDEBUG_CC_DEAD_CODE not supported right now primarily
  // because the internal CodeCoverage class does support either unexecuted line
  // tracking or dead code analysis
  if (options != 0) {
    raise_error("XDEBUG_CC_UNUSED and XDEBUG_CC_DEAD_CODE constants are not "
                "currently supported.");
    return;
  }

  // If we get here, turn on coverage
  ThreadInfo *ti = ThreadInfo::s_threadInfo.getNoCheck();
  ti->m_reqInjectionData.setCoverage(true);
  if (g_context->isNested()) {
    raise_notice("Calling xdebug_start_code_coverage from a nested VM instance "
                 "may cause unpredicable results");
  }
  throw VMSwitchModeBuiltin();
}

static void HHVM_FUNCTION(xdebug_start_error_collection)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_start_trace,
                          const String& trace_file,
                          int64_t options /* = 0 */)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_stop_code_coverage,
                          bool cleanup /* = true */) {
  ThreadInfo *ti = ThreadInfo::s_threadInfo.getNoCheck();
  ti->m_reqInjectionData.setCoverage(false);
  if (cleanup) {
    ti->m_coverage->Reset();
  }
}

static void HHVM_FUNCTION(xdebug_stop_error_collection)
  XDEBUG_NOTIMPLEMENTED

static void HHVM_FUNCTION(xdebug_stop_trace)
  XDEBUG_NOTIMPLEMENTED

static double HHVM_FUNCTION(xdebug_time_index)
  XDEBUG_NOTIMPLEMENTED

static TypedValue* HHVM_FN(xdebug_var_dump)(ActRec* ar)
  XDEBUG_NOTIMPLEMENTED

///////////////////////////////////////////////////////////////////////////////

static const StaticString s_XDEBUG_CC_UNUSED("XDEBUG_CC_UNUSED");
static const StaticString s_XDEBUG_CC_DEAD_CODE("XDEBUG_CC_DEAD_CODE");

bool XDebugExtension::connectRemoteDebugSocket() {
  struct sockaddr_in serverAddress;
  struct hostent *serverHost;
  int sockfd;
  int portno = 9000;

  bzero((char *) &serverAddress, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;

  serverHost = gethostbyname("localhost");
  bcopy((char *)serverHost->h_addr,
      (char *)&serverAddress.sin_addr.s_addr,
      serverHost->h_length);

  serverAddress.sin_port = htons(portno);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    return false;
  if (connect(sockfd,(struct sockaddr *) &serverAddress,sizeof(serverAddress)) < 0)
    return false;

  m_debugSocketFd = sockfd;
  return true;
}

char* XDebugExtension::read_next_command(int sock_fd, fd_buf *context)
{
  int size = 0, newl = 0, nbufsize = 0;
  char *tmp;
  char *tmp_buf = NULL;
  char *ptr;
  char buffer[128 + 1];

  if (!context->buffer) {
    context->buffer = (char*) calloc(1,1);
    context->buffer_size = 0;
  }

  while (context->buffer_size < 1 || context->buffer[context->buffer_size - 1] != XDEBUG_COMMAND_DELIM) {
    ptr = context->buffer + context->buffer_size;
    newl = recv(sock_fd, buffer, 128, 0);
    if (newl > 0) {
      context->buffer = (char*) realloc(context->buffer, context->buffer_size + newl + 1);
      memcpy(context->buffer + context->buffer_size, buffer, newl);
      context->buffer_size += newl;
      context->buffer[context->buffer_size] = '\0';
    } else {
      return NULL;
    }
  }

  ptr = (char*) memchr(context->buffer, XDEBUG_COMMAND_DELIM, context->buffer_size);
  size = ptr - context->buffer;
  /* Copy that line into tmp */
  tmp = (char*) malloc(size + 1);
  tmp[size] = '\0';
  memcpy(tmp, context->buffer, size);
  /* Rewrite existing buffer */
  if ((nbufsize = context->buffer_size - size - 1)  > 0) {
    tmp_buf = (char*) malloc(nbufsize + 1);
    memcpy(tmp_buf, ptr + 1, nbufsize);
    tmp_buf[nbufsize] = 0;
  }
  free(context->buffer);
  context->buffer = tmp_buf;
  context->buffer_size = context->buffer_size - (size + 1);

  return tmp;
}

void XDebugExtension::setupXDebugSession() {
  xdebug_xml_node *response = xdebug_xml_node_init("init");
  xdebug_xml_add_attribute(response, "xmlns", "urn:debugger_protocol_v1");
  xdebug_xml_add_attribute(response, "xmlns:xdebug", "http://xdebug.org/dbgp/xdebug");

/* {{{ XML Init Stuff*/
  xdebug_xml_node *child = xdebug_xml_node_init("engine");
  xdebug_xml_add_attribute(child, "version", XDEBUG_VERSION);
  xdebug_xml_add_text(child, xdstrdup(XDEBUG_NAME));
  xdebug_xml_add_child(response, child);

  child = xdebug_xml_node_init("author");
  xdebug_xml_add_text(child, xdstrdup(XDEBUG_AUTHOR));
  xdebug_xml_add_child(response, child);

  child = xdebug_xml_node_init("url");
  xdebug_xml_add_text(child, xdstrdup(XDEBUG_URL));
  xdebug_xml_add_child(response, child);

  child = xdebug_xml_node_init("copyright");
  xdebug_xml_add_text(child, xdstrdup(XDEBUG_COPYRIGHT));
  xdebug_xml_add_child(response, child);

  xdebug_xml_add_attribute_ex(response, "fileuri", xdstrdup("dbgp://stdin"), 0, 1);
  xdebug_xml_add_attribute_ex(response, "language", "PHP", 0, 0);
  xdebug_xml_add_attribute_ex(response, "protocol_version", DBGP_VERSION, 0, 0);
  xdebug_xml_add_attribute_ex(response, "appid", xdebug_sprintf("%d", getpid()), 0, 1);

  send_message(m_debugSocketFd, response);
  xdebug_xml_node_dtor(response);
}

void XDebugExtension::handleDebuggingConnections() {
  setupXDebugSession();
  TRACE(0, "Handling debugging connections!\n");
  fd_buf buffer { nullptr, 0 };

  while (true) {
    char* line = read_next_command(m_debugSocketFd, &buffer);
    if (!line) {
      TRACE(0, "Got a null command!!!!\n");
      sleep(1);
      continue;
    }

    xdebug_xml_node *response = xdebug_xml_node_init("response");
    xdebug_dbgp_arg *args;
    char* cmd = nullptr;
    int statuscode = xdebug_dbgp_parse_cmd(line, (char**) &cmd, (xdebug_dbgp_arg**) &args);

    xdebug_dbgp_cmd *command = lookup_cmd(cmd);

    // TODO - probably should handle errors like xdebug does.... 
    xdebug_xml_add_attribute_ex(response, "command", xdstrdup(cmd), 0, 1);
    xdebug_xml_add_attribute_ex(response, "transaction_id", xdstrdup(CMD_OPTION('i')), 0, 1);

    if (0 == strcmp(command->name, "feature_set")) {
      // always return success? lol
      xdebug_xml_add_attribute_ex(response, "feature", xdstrdup(CMD_OPTION('n')), 0, 1);
      xdebug_xml_add_attribute_ex(response, "success", "1", 0, 0);
    } else if (0 == strcmp(command->name, "status")) {
      xdebug_xml_add_attribute(response, "status", "starting");
      xdebug_xml_add_attribute(response, "reason", "ok");
    } else if (0 == strcmp(command->name, "step_into")) {
      xdebug_xml_add_attribute(response, "status", "running");
      xdebug_xml_add_attribute(response, "reason", "ok");
    } else if (0 == strcmp(command->name, "eval")) {
      int new_length;
      TRACE(0, "Eval data decoded: %s\n", (char*) my_php_base64_decode((const unsigned char *)CMD_OPTION('-'), strlen(CMD_OPTION('-')), &new_length));
    }

    TRACE(0, "Got input %s!\n", line);
    TRACE(0, "parsed with status code %d\n", statuscode);
    TRACE(0, "command has name %s\n", command->name);
    TRACE(0, "Handling debugging connections in sleep loop!\n");

    // send it! TODO - dont send on errors
    send_message(m_debugSocketFd, response);

    // cleanup!
    xdfree(cmd);
    xdebug_dbgp_arg_dtor(args);
  }
}

void XDebugExtension::moduleLoad(const IniSetting::Map& ini, Hdf xdebug_hdf) {
  TRACE(0, "Xdebug being loaded!\n");
  Hdf hdf = xdebug_hdf[XDEBUG_NAME];
  Enable = Config::GetBool(ini, hdf["enable"], false);
  Enable = true;
  if (!Enable) {
    TRACE(0, "XDebug NOT loaded!");
    return;
  }

  // Standard config options
  #define XDEBUG_OPT(T, name, sym, val) Config::Bind(sym, ini, hdf[name], val);
  XDEBUG_CFG
  #undef XDEBUG_OPT

  // hhvm.xdebug.dump.*
  Hdf dump = hdf["dump"];
  Config::Bind(DumpCookie, ini, dump["COOKIE"], "");
  Config::Bind(DumpFiles, ini, dump["FILES"], "");
  Config::Bind(DumpGet, ini, dump["GET"], "");
  Config::Bind(DumpPost, ini, dump["POST"], "");
  Config::Bind(DumpRequest, ini, dump["REQUEST"], "");
  Config::Bind(DumpServer, ini, dump["SERVER"], "");
  Config::Bind(DumpSession, ini, dump["SESSION"], "");
  TRACE(0, "XDebug finished loading!\n");
}

void XDebugExtension::moduleInit() {
  TRACE(0, "XDebug starting init!\n");
  if (!Enable) {
    return;
  }
  Native::registerConstant<KindOfInt64>(
    s_XDEBUG_CC_UNUSED.get(), k_XDEBUG_CC_UNUSED
  );
  Native::registerConstant<KindOfInt64>(
    s_XDEBUG_CC_DEAD_CODE.get(), k_XDEBUG_CC_DEAD_CODE
  );
  HHVM_FE(xdebug_break);
  HHVM_FE(xdebug_call_class);
  HHVM_FE(xdebug_call_file);
  HHVM_FE(xdebug_call_function);
  HHVM_FE(xdebug_call_line);
  HHVM_FE(xdebug_code_coverage_started);
  HHVM_FE(xdebug_debug_zval);
  HHVM_FE(xdebug_debug_zval_stdout);
  HHVM_FE(xdebug_disable);
  HHVM_FE(xdebug_dump_superglobals);
  HHVM_FE(xdebug_enable);
  HHVM_FE(xdebug_get_code_coverage);
  HHVM_FE(xdebug_get_collected_errors);
  HHVM_FE(xdebug_get_declared_vars);
  HHVM_FE(xdebug_get_function_stack);
  HHVM_FE(xdebug_get_headers);
  HHVM_FE(xdebug_get_profiler_filename);
  HHVM_FE(xdebug_get_stack_depth);
  HHVM_FE(xdebug_get_tracefile_name);
  HHVM_FE(xdebug_is_enabled);
  HHVM_FE(xdebug_memory_usage);
  HHVM_FE(xdebug_peak_memory_usage);
  HHVM_FE(xdebug_print_function_stack);
  HHVM_FE(xdebug_start_code_coverage);
  HHVM_FE(xdebug_start_error_collection);
  HHVM_FE(xdebug_start_trace);
  HHVM_FE(xdebug_stop_code_coverage);
  HHVM_FE(xdebug_stop_error_collection);
  HHVM_FE(xdebug_stop_trace);
  HHVM_FE(xdebug_time_index);
  HHVM_FE(xdebug_var_dump);
  loadSystemlib("xdebug");

  if (!connectRemoteDebugSocket()) {
    TRACE(0, "DEBUG SOCKET FAILED TO CONNECT!!!!!");
    return;
  }

  m_remoteDebugThread.start();
  // TODO - make this die gracefully on shutdown

  TRACE(0, "XDebug finished init!\n");
}

// Non-bind config options and edge-cases
bool XDebugExtension::Enable = false;
string XDebugExtension::DumpCookie = "";
string XDebugExtension::DumpFiles = "";
string XDebugExtension::DumpGet = "";
string XDebugExtension::DumpPost = "";
string XDebugExtension::DumpRequest = "";
string XDebugExtension::DumpServer = "";
string XDebugExtension::DumpSession = "";

// Standard config options
#define XDEBUG_OPT(T, name, sym, val) T XDebugExtension::sym = val;
XDEBUG_CFG
#undef XDEBUG_OPT

static XDebugExtension s_xdebug_extension;

///////////////////////////////////////////////////////////////////////////////
}
