/** @file

  Implements unit test for SDK APIs

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#include "ink_config.h"
#include <sys/types.h>

#include <errno.h>
//extern int errno;

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "Regression.h"
#include "api/ts/ts.h"
#include "api/ts/experimental.h"
#include "I_RecCore.h"
#include "I_Layout.h"

#include "InkAPITestTool.cc"
#include "http/HttpSM.h"

#define TC_PASS 1
#define TC_FAIL 0

#define UTDBG_TAG "sdk_ut"

/******************************************************************************/

/* Use SDK_RPRINT to report failure or success for each test case */
int
SDK_RPRINT(RegressionTest * t, const char *api_name, const char *testcase_name, int status, const char *err_details_format, ...)
{
  int l;
  char buffer[8192];
  char format2[8192];
  snprintf(format2, sizeof(format2), "[%s] %s : [%s] <<%s>> { %s }\n", t->name,
           api_name, testcase_name, status == TC_PASS ? "PASS" : "FAIL", err_details_format);
  va_list ap;
  va_start(ap, err_details_format);
  l = ink_bvsprintf(buffer, format2, ap);
  va_end(ap);
  fputs(buffer, stderr);
  return (l);
}


/*
  REGRESSION_TEST(SDK_<test_name>)(RegressionTest *t, int atype, int *pstatus)

  RegressionTest *test is a pointer on object that will run the test.
   Do not modify.

  int atype is one of:
   REGRESSION_TEST_NONE
   REGRESSION_TEST_QUICK
   REGRESSION_TEST_NIGHTLY
   REGRESSION_TEST_EXTENDED

  int *pstatus should be set to one of:
   REGRESSION_TEST_PASSED
   REGRESSION_TEST_INPROGRESS
   REGRESSION_TEST_FAILED
   REGRESSION_TEST_NOT_RUN
  Note: pstatus is polled and can be used for asynchroneous tests.

*/

/* Misc */
////////////////////////////////////////////////
//       SDK_API_TSTrafficServerVersionGet
//
// Unit Test for API: TSTrafficServerVersionGet
////////////////////////////////////////////////
REGRESSION_TEST(SDK_API_TSTrafficServerVersionGet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  /* Assume the UT runs on TS5.0 and higher */
  const char *ts_version = TSTrafficServerVersionGet();
  if (!ts_version) {
    SDK_RPRINT(test, "TSTrafficServerVersionGet", "TestCase1", TC_FAIL, "can't get traffic server version");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  int major_ts_version = 0;
  int minor_ts_version = 0;
  int patch_ts_version = 0;
  // coverity[secure_coding]
  if (sscanf(ts_version, "%d.%d.%d", &major_ts_version, &minor_ts_version, &patch_ts_version) != 3) {
    SDK_RPRINT(test, "TSTrafficServerVersionGet", "TestCase2", TC_FAIL, "traffic server version format is incorrect");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  if (major_ts_version < 2) {
    SDK_RPRINT(test, "TSTrafficServerVersionGet", "TestCase3", TC_FAIL, "traffic server major version is incorrect");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  SDK_RPRINT(test, "TSTrafficServerVersionGet", "TestCase1", TC_PASS, "ok");
  *pstatus = REGRESSION_TEST_PASSED;
  return;
}


////////////////////////////////////////////////
//       SDK_API_TSPluginDirGet
//
// Unit Test for API: TSPluginDirGet
//                    TSInstallDirGet
////////////////////////////////////////////////
REGRESSION_TEST(SDK_API_TSPluginDirGet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  const char *plugin_dir = TSPluginDirGet();
  const char *install_dir = TSInstallDirGet();

  if (!plugin_dir) {
    SDK_RPRINT(test, "TSPluginDirGet", "TestCase1", TC_FAIL, "can't get plugin dir");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  if (!install_dir) {
    SDK_RPRINT(test, "TSInstallDirGet", "TestCase1", TC_FAIL, "can't get installation dir");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  // Note: This doesn't have to be true
  //      since the location can be anywhere
  //      We only pass this test, with the default layout.
  if (strstr(plugin_dir, "libexec/trafficserver") == NULL) {
    SDK_RPRINT(test, "TSPluginDirGet", "TestCase2", TC_FAIL,
               "plugin dir(%s) is incorrect, expected (%s) in path. Are you using the default layout?",
               plugin_dir,"libexec/trafficserver");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  if (strstr(plugin_dir, install_dir) == NULL) {
    SDK_RPRINT(test, "TSInstallDirGet", "TestCase2", TC_FAIL, "install dir is incorrect");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  SDK_RPRINT(test, "TSPluginDirGet", "TestCase1", TC_PASS, "ok");
  SDK_RPRINT(test, "TSInstallDirGet", "TestCase1", TC_PASS, "ok");
  *pstatus = REGRESSION_TEST_PASSED;
  return;
}


/* TSConfig */
////////////////////////////////////////////////
//       SDK_API_TSConfig
//
// Unit Test for API: TSConfigSet
//                    TSConfigGet
//                    TSConfigRelease
//                    TSConfigDataGet
////////////////////////////////////////////////
static int my_config_id = -1;
typedef struct
{
  const char *a;
  const char *b;
} ConfigData;

static void
config_destroy_func(void *data)
{
  ConfigData *config = (ConfigData *) data;
  TSfree(config);
  return;
}

REGRESSION_TEST(SDK_API_TSConfig) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  ConfigData *config = (ConfigData *) TSmalloc(sizeof(ConfigData));
  config->a = "unit";
  config->b = "test";

  my_config_id = TSConfigSet(0, config, config_destroy_func);

  TSConfig test_config = NULL;
  test_config = TSConfigGet(my_config_id);

  if (!test_config) {
    SDK_RPRINT(test, "TSConfigSet", "TestCase1", TC_FAIL, "can't correctly set global config structure");
    SDK_RPRINT(test, "TSConfigGet", "TestCase1", TC_FAIL, "can't correctly get global config structure");
    TSConfigRelease(my_config_id, reinterpret_cast<TSConfig>(config));
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  if (TSConfigDataGet(test_config) != config) {
    SDK_RPRINT(test, "TSConfigDataGet", "TestCase1", TC_FAIL, "failed to get config data");
    TSConfigRelease(my_config_id, reinterpret_cast<TSConfig>(config));
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  SDK_RPRINT(test, "TSConfigGet", "TestCase1", TC_PASS, "ok");
  SDK_RPRINT(test, "TSConfigSet", "TestCase1", TC_PASS, "ok");
  SDK_RPRINT(test, "TSConfigDataGet", "TestCase1", TC_PASS, "ok");

  TSConfigRelease(my_config_id, reinterpret_cast<TSConfig>(config));
  *pstatus = REGRESSION_TEST_PASSED;
  return;
}

/* TSNetVConn */
//////////////////////////////////////////////
//       SDK_API_TSNetVConn
//
// Unit Test for API: TSNetVConnRemoteIPGet
//                    TSNetVConnRemotePortGet
//                    TSNetAccept
//                    TSNetConnect
//////////////////////////////////////////////
#define IP(a,b,c,d) htonl((a) << 24 | (b) << 16 | (c) << 8 | (d))
const unsigned short server_port = 12345;
RegressionTest *SDK_NetVConn_test;
int *SDK_NetVConn_pstatus;

int
server_handler(TSCont contp, TSEvent event, void *data)
{
  NOWARN_UNUSED(data);
  if (event == TS_EVENT_VCONN_EOS)
    TSContDestroy(contp);

  return 1;
}

int
client_handler(TSCont contp, TSEvent event, void *data)
{
  if (event == TS_EVENT_NET_CONNECT_FAILED) {
    SDK_RPRINT(SDK_NetVConn_test, "TSNetAccept", "TestCase1", TC_FAIL, "can't connect to server");
    SDK_RPRINT(SDK_NetVConn_test, "TSNetConnect", "TestCase1", TC_FAIL, "can't connect to server");

    // no need to continue, return
    TSContDestroy(contp);
    // Fix me: how to deal with server side cont?
    *SDK_NetVConn_pstatus = REGRESSION_TEST_FAILED;

    return 1;
  } else {
    SDK_RPRINT(SDK_NetVConn_test, "TSNetAccept", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(SDK_NetVConn_test, "TSNetConnect", "TestCase1", TC_PASS, "ok");

    sockaddr const* addr = TSNetVConnRemoteAddrGet(static_cast<TSVConn>(data));
    uint16_t input_server_port = ink_inet_get_port(addr);

    if (!ink_inet_is_loopback(addr)) {
      ip_text_buffer s, ipb;
      ts_ip_endpoint loopback;
      ink_inet_ip4_set(&loopback, htonl(INADDR_LOOPBACK));
      SDK_RPRINT(SDK_NetVConn_test, "TSNetVConnRemoteIPGet", "TestCase1", TC_FAIL, "server ip [%s] is incorrect - expected [%s]",
        ink_inet_ntop(addr, s, sizeof s),
        ink_inet_ntop(&loopback.sa, ipb, sizeof ipb)
      );

      TSContDestroy(contp);
      // Fix me: how to deal with server side cont?
      *SDK_NetVConn_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else
      SDK_RPRINT(SDK_NetVConn_test, "TSNetVConnRemoteIPGet", "TestCase1", TC_PASS, "ok");

    if (input_server_port != server_port) {
      SDK_RPRINT(SDK_NetVConn_test, "TSNetVConnRemotePortGet", "TestCase1", TC_FAIL, "server port [%d] is incorrect -- expected [%d]", input_server_port, server_port);

      TSContDestroy(contp);
      // Fix me: how to deal with server side cont?
      *SDK_NetVConn_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else
      SDK_RPRINT(SDK_NetVConn_test, "TSNetVConnRemotePortGet", "TestCase1", TC_PASS, "ok");

    TSVConnClose((TSVConn) data);
  }

  TSContDestroy(contp);

  *SDK_NetVConn_pstatus = REGRESSION_TEST_PASSED;
  return 1;
}

REGRESSION_TEST(SDK_API_TSNetVConn) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  SDK_NetVConn_test = test;
  SDK_NetVConn_pstatus = pstatus;

  TSMutex server_mutex = TSMutexCreate();
  TSMutex client_mutex = TSMutexCreate();

  TSCont server_cont = TSContCreate(server_handler, server_mutex);
  TSCont client_cont = TSContCreate(client_handler, client_mutex);

  TSNetAccept(server_cont, server_port, -1, 0);

  ts_ip_endpoint addr;
  ink_inet_ip4_set(&addr, htonl(INADDR_LOOPBACK), htons(server_port));
  TSNetConnect(client_cont, &addr.sa);
}

/* TSCache, TSVConn, TSVIO */
//////////////////////////////////////////////
//       SDK_API_TSCache
//
// Unit Test for API: TSCacheReady
//                    TSCacheWrite
//                    TSCacheRead
//                    TSCacheKeyCreate
//                    TSCacheKeyDigestSet
//                    TSVConnCacheObjectSizeGet
//                    TSVConnClose
//                    TSVConnClosedGet
//                    TSVConnRead
//                    TSVConnReadVIOGet
//                    TSVConnWrite
//                    TSVConnWriteVIOGet
//                    TSVIOBufferGet
//                    TSVIOContGet
//                    TSVIOMutexGet
//                    TSVIONBytesGet
//                    TSVIONBytesSet
//                    TSVIONDoneGet
//                    TSVIONDoneSet
//                    TSVIONTodoGet
//                    TSVIOReaderGet
//                    TSVIOReenable
//                    TSVIOVConnGet
//////////////////////////////////////////////

// TSVConnAbort can't be tested
// Fix me: test TSVConnShutdown, TSCacheKeyDataTypeSet,
//         TSCacheKeyHostNameSet, TSCacheKeyPinnedSet

// Logic of the test:
//  - write OBJECT_SIZE bytes in the cache in 3 shots
//    (OBJECT_SIZE/2, then OBJECT_SIZE-100 and finally OBJECT_SIZE)
//  - read object from the cache
//  - remove it from the cache
//  - try to read it (should faild)


#define OBJECT_SIZE 100000      // size of the object we'll write/read/remove in cache


RegressionTest *SDK_Cache_test;
int *SDK_Cache_pstatus;
static char content[OBJECT_SIZE];
static int read_counter = 0;

typedef struct
{
  TSIOBuffer bufp;
  TSIOBuffer out_bufp;
  TSIOBufferReader readerp;
  TSIOBufferReader out_readerp;

  TSVConn write_vconnp;
  TSVConn read_vconnp;
  TSVIO read_vio;
  TSVIO write_vio;

  TSCacheKey key;
} CacheVConnStruct;

int
cache_handler(TSCont contp, TSEvent event, void *data)
{
  Debug("sdk_ut_cache_write", "Event %d data %p", event, data);

  CacheVConnStruct *cache_vconn = (CacheVConnStruct *) TSContDataGet(contp);

  TSIOBufferBlock blockp;
  char *ptr_block;
  int64_t ntodo, ndone, nbytes, towrite, avail, content_length;

  switch (event) {
  case TS_EVENT_CACHE_OPEN_WRITE:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_OPEN_WRITE %d %p", event, data);
    SDK_RPRINT(SDK_Cache_test, "TSCacheWrite", "TestCase1", TC_PASS, "ok");

    // data is write_vc
    cache_vconn->write_vconnp = (TSVConn) data;

    // Create buffers/readers to write and read data into the cache
    cache_vconn->bufp = TSIOBufferCreate();
    cache_vconn->readerp = TSIOBufferReaderAlloc(cache_vconn->bufp);
    cache_vconn->out_bufp = TSIOBufferCreate();
    cache_vconn->out_readerp = TSIOBufferReaderAlloc(cache_vconn->out_bufp);

    // Write content into upstream IOBuffer
    ntodo = OBJECT_SIZE;
    ndone = 0;
    while (ntodo > 0) {
      blockp = TSIOBufferStart(cache_vconn->bufp);
      ptr_block = TSIOBufferBlockWriteStart(blockp, &avail);
      towrite = ((ntodo < avail) ? ntodo : avail);
      memcpy(ptr_block, content + ndone, towrite);
      TSIOBufferProduce(cache_vconn->bufp, towrite);
      ntodo -= towrite;
      ndone += towrite;
    }

    // first write half of the data. To test TSVIOReenable
    cache_vconn->write_vio = TSVConnWrite((TSVConn) data, contp, cache_vconn->readerp, OBJECT_SIZE / 2);
    return 1;

  case TS_EVENT_CACHE_OPEN_WRITE_FAILED:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_OPEN_WRITE_FAILED %d %p", event, data);
    SDK_RPRINT(SDK_Cache_test, "TSCacheWrite", "TestCase1", TC_FAIL, "can't open cache vc, edtata = %p", data);
    TSReleaseAssert(!"cache");

    // no need to continue, return
    *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
    return 1;

  case TS_EVENT_CACHE_OPEN_READ:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_OPEN_READ %d %p", event, data);
    if (read_counter == 2) {
      SDK_RPRINT(SDK_Cache_test, "TSCacheRead", "TestCase2", TC_FAIL, "shouldn't open cache vc");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }

    SDK_RPRINT(SDK_Cache_test, "TSCacheRead", "TestCase1", TC_PASS, "ok");

    cache_vconn->read_vconnp = (TSVConn) data;
    content_length = TSVConnCacheObjectSizeGet(cache_vconn->read_vconnp);
    Debug(UTDBG_TAG "_cache_read", "In cache open read [Content-Length: %d]", content_length);
    if (content_length != OBJECT_SIZE) {
      SDK_RPRINT(SDK_Cache_test, "TSVConnCacheObjectSizeGet", "TestCase1", TC_FAIL, "cached data size is incorrect");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVConnCacheObjectSizeGet", "TestCase1", TC_PASS, "ok");
      cache_vconn->read_vio = TSVConnRead((TSVConn) data, contp, cache_vconn->out_bufp, content_length);
    }
    return 1;

  case TS_EVENT_CACHE_OPEN_READ_FAILED:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_OPEN_READ_FAILED %d %p", event, data);
    if (read_counter == 1) {
      SDK_RPRINT(SDK_Cache_test, "TSCacheRead", "TestCase1", TC_FAIL, "can't open cache vc");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }
    SDK_RPRINT(SDK_Cache_test, "TSCacheRead", "TestCase2", TC_PASS, "ok");

    // ok, all tests passed!
    break;

  case TS_EVENT_CACHE_REMOVE:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_REMOVE %d %p", event, data);
    SDK_RPRINT(SDK_Cache_test, "TSCacheRemove", "TestCase1", TC_PASS, "ok");

    // read the data which has been removed
    read_counter++;
    TSCacheRead(contp, cache_vconn->key);
    return 1;

  case TS_EVENT_CACHE_REMOVE_FAILED:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_CACHE_REMOVE_FAILED %d %p", event, data);
    SDK_RPRINT(SDK_Cache_test, "TSCacheRemove", "TestCase1", TC_FAIL, "can't remove cached item");

    // no need to continue, return
    *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
    return 1;

  case TS_EVENT_VCONN_WRITE_COMPLETE:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_VCONN_WRITE_COMPLETE %d %p", event, data);

    // VConn/VIO APIs
    nbytes = TSVIONBytesGet(cache_vconn->write_vio);
    ndone = TSVIONDoneGet(cache_vconn->write_vio);
    ntodo = TSVIONTodoGet(cache_vconn->write_vio);
    Debug(UTDBG_TAG "_cache_write", "Nbytes=%d Ndone=%d Ntodo=%d", nbytes, ndone, ntodo);

    if (ndone == (OBJECT_SIZE / 2)) {
      TSVIONBytesSet(cache_vconn->write_vio, (OBJECT_SIZE - 100));
      TSVIOReenable(cache_vconn->write_vio);
      Debug(UTDBG_TAG "_cache_write", "Increment write_counter in write_complete [a]");
      return 1;
    } else if (ndone == (OBJECT_SIZE - 100)) {
      TSVIONBytesSet(cache_vconn->write_vio, OBJECT_SIZE);
      TSVIOReenable(cache_vconn->write_vio);
      Debug(UTDBG_TAG "_cache_write", "Increment write_counter in write_complete [b]");
      return 1;
    } else if (ndone == OBJECT_SIZE) {
      Debug(UTDBG_TAG "_cache_write", "finishing up [c]");

      SDK_RPRINT(SDK_Cache_test, "TSVIOReenable", "TestCase2", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVIONBytesSet", "TestCase1", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVConnWrite", "TestCase1", TC_PASS, "ok");
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSCacheWrite", "TestCase1", TC_FAIL, "Did not write expected # of bytes");
      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }

    if ((TSVIO) data != cache_vconn->write_vio) {
      SDK_RPRINT(SDK_Cache_test, "TSVConnWrite", "TestCase1", TC_FAIL, "write_vio corrupted");
      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }
    Debug(UTDBG_TAG "_cache_write", "finishing up [d]");


    if (TSVIOBufferGet(cache_vconn->write_vio) != cache_vconn->bufp) {
      SDK_RPRINT(SDK_Cache_test, "TSVIOBufferGet", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIOBufferGet", "TestCase1", TC_PASS, "ok");
    }

    if (TSVIOContGet(cache_vconn->write_vio) != contp) {
      SDK_RPRINT(SDK_Cache_test, "TSVIOContGet", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIOContGet", "TestCase1", TC_PASS, "ok");
    }

    Debug(UTDBG_TAG "_cache_write", "finishing up [f]");

    if (TSVIOMutexGet(cache_vconn->write_vio) != TSContMutexGet(contp)) {
      SDK_RPRINT(SDK_Cache_test, "TSVIOMutexGet", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIOMutexGet", "TestCase1", TC_PASS, "ok");
    }

    if (TSVIOVConnGet(cache_vconn->write_vio) != cache_vconn->write_vconnp) {
      SDK_RPRINT(SDK_Cache_test, "TSVIOVConnGet", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIOVConnGet", "TestCase1", TC_PASS, "ok");
    }

    Debug(UTDBG_TAG "_cache_write", "finishing up [g]");

    if (TSVIOReaderGet(cache_vconn->write_vio) != cache_vconn->readerp) {
      SDK_RPRINT(SDK_Cache_test, "TSVIOReaderGet", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIOReaderGet", "TestCase1", TC_PASS, "ok");
    }

    // tests for write is done, close write_vconnp
    TSVConnClose(cache_vconn->write_vconnp);
    cache_vconn->write_vconnp = NULL;

    Debug(UTDBG_TAG "_cache_write", "finishing up [h]");


    // start to read data out of cache
    read_counter++;
    TSCacheRead(contp, cache_vconn->key);
    Debug(UTDBG_TAG "_cache_read", "starting read [i]");
    return 1;


  case TS_EVENT_VCONN_WRITE_READY:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_VCONN_WRITE_READY %d %p", event, data);
    if ((TSVIO) data != cache_vconn->write_vio) {
      SDK_RPRINT(SDK_Cache_test, "TSVConnWrite", "TestCase1", TC_FAIL, "write_vio corrupted");
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }

    nbytes = TSVIONBytesGet(cache_vconn->write_vio);
    ndone = TSVIONDoneGet(cache_vconn->write_vio);
    ntodo = TSVIONTodoGet(cache_vconn->write_vio);
    Debug(UTDBG_TAG "_cache_write", "Nbytes=%d Ndone=%d Ntodo=%d", nbytes, ndone, ntodo);

    TSVIOReenable(cache_vconn->write_vio);
    return 1;

  case TS_EVENT_VCONN_READ_COMPLETE:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_VCONN_READ_COMPLETE %d %p", event, data);
    if ((TSVIO) data != cache_vconn->read_vio) {
      SDK_RPRINT(SDK_Cache_test, "TSVConnRead", "TestCase1", TC_FAIL, "read_vio corrupted");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }

    nbytes = TSVIONBytesGet(cache_vconn->read_vio);
    ntodo = TSVIONTodoGet(cache_vconn->read_vio);
    ndone = TSVIONDoneGet(cache_vconn->read_vio);
    Debug(UTDBG_TAG "_cache_read", "Nbytes=%d Ndone=%d Ntodo=%d", nbytes, ndone, ntodo);

    if (nbytes != (ndone + ntodo)) {
      SDK_RPRINT(SDK_Cache_test, "TSVIONBytesGet", "TestCase1", TC_FAIL, "read_vio corrupted");
      SDK_RPRINT(SDK_Cache_test, "TSVIONTodoGet", "TestCase1", TC_FAIL, "read_vio corrupted");
      SDK_RPRINT(SDK_Cache_test, "TSVIONDoneGet", "TestCase1", TC_FAIL, "read_vio corrupted");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIONBytesGet", "TestCase1", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVIONTodoGet", "TestCase1", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVIONDoneGet", "TestCase1", TC_PASS, "ok");

      TSVIONDoneSet(cache_vconn->read_vio, 0);
      if (TSVIONDoneGet(cache_vconn->read_vio) != 0) {
        SDK_RPRINT(SDK_Cache_test, "TSVIONDoneSet", "TestCase1", TC_FAIL, "fail to set");

        // no need to continue, return
        *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
        return 1;
      } else
        SDK_RPRINT(SDK_Cache_test, "TSVIONDoneSet", "TestCase1", TC_PASS, "ok");

      Debug(UTDBG_TAG "_cache_write", "finishing up [i]");

      // now waiting for 100ms to make sure the key is
      // written in directory remove the content
      TSContSchedule(contp, 100, TS_THREAD_POOL_DEFAULT);
    }

    return 1;

  case TS_EVENT_VCONN_READ_READY:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_VCONN_READ_READY %d %p", event, data);
    if ((TSVIO) data != cache_vconn->read_vio) {
      SDK_RPRINT(SDK_Cache_test, "TSVConnRead", "TestCase1", TC_FAIL, "read_vio corrupted");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    }

    nbytes = TSVIONBytesGet(cache_vconn->read_vio);
    ntodo = TSVIONTodoGet(cache_vconn->read_vio);
    ndone = TSVIONDoneGet(cache_vconn->read_vio);
    Debug(UTDBG_TAG "_cache_read", "Nbytes=%d Ndone=%d Ntodo=%d", nbytes, ndone, ntodo);

    if (nbytes != (ndone + ntodo)) {
      SDK_RPRINT(SDK_Cache_test, "TSVIONBytesGet", "TestCase1", TC_FAIL, "read_vio corrupted");
      SDK_RPRINT(SDK_Cache_test, "TSVIONTodoGet", "TestCase1", TC_FAIL, "read_vio corrupted");
      SDK_RPRINT(SDK_Cache_test, "TSVIONDoneGet", "TestCase1", TC_FAIL, "read_vio corrupted");

      // no need to continue, return
      *SDK_Cache_pstatus = REGRESSION_TEST_FAILED;
      return 1;
    } else {
      SDK_RPRINT(SDK_Cache_test, "TSVIONBytesGet", "TestCase1", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVIONTodoGet", "TestCase1", TC_PASS, "ok");
      SDK_RPRINT(SDK_Cache_test, "TSVIONDoneGet", "TestCase1", TC_PASS, "ok");
    }

    // Fix for bug INKqa12276: Must consume data from iobuffer
    nbytes = TSIOBufferReaderAvail(cache_vconn->out_readerp);
    TSIOBufferReaderConsume(cache_vconn->out_readerp, nbytes);
    TSDebug(UTDBG_TAG "_cache_read", "Consuming %d bytes from cache read VC", nbytes);

    TSVIOReenable(cache_vconn->read_vio);
    Debug(UTDBG_TAG "_cache_read", "finishing up [j]");
    return 1;

  case TS_EVENT_TIMEOUT:
    Debug(UTDBG_TAG "_cache_event", "TS_EVENT_TIMEOUT %d %p", event, data);
    // do remove cached doc
    TSCacheRemove(contp, cache_vconn->key);
    return 1;

  default:
    TSReleaseAssert(!"Test SDK_API_TSCache: unexpected event");
  }

  Debug(UTDBG_TAG "_cache_event", "DONE DONE DONE");

  // destroy the data structure
  Debug(UTDBG_TAG "_cache_write", "all tests passed [z]");
  TSIOBufferDestroy(cache_vconn->bufp);
  TSIOBufferDestroy(cache_vconn->out_bufp);
  TSCacheKeyDestroy(cache_vconn->key);
  TSfree(cache_vconn);
  *SDK_Cache_pstatus = REGRESSION_TEST_PASSED;

  return 1;
}

REGRESSION_TEST(SDK_API_TSCache) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  SDK_Cache_test = test;
  SDK_Cache_pstatus = pstatus;
  int is_ready = 0;

  // Check if Cache is ready
  TSCacheReady(&is_ready);
  if (!is_ready) {
    SDK_RPRINT(test, "TSCacheReady", "TestCase1", TC_FAIL, "cache is not ready");

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  } else {
    SDK_RPRINT(test, "TSCacheReady", "TestCase1", TC_PASS, "ok");
  }

  // Create CacheKey
  char key_name[] = "key_for_regression_test";
  TSCacheKey key = TSCacheKeyCreate();
  TSCacheKey key_cmp = TSCacheKeyCreate();
  SDK_RPRINT(test, "TSCacheKeyCreate", "TestCase1", TC_PASS, "ok");
  TSCacheKeyDigestSet(key, key_name, strlen(key_name));
  TSCacheKeyDigestSet(key_cmp, key_name, strlen(key_name));

  if (memcmp(key, key_cmp, sizeof(TSCacheKey)) != 0) {
    SDK_RPRINT(test, "TSCacheKeySetDigest", "TestCase1", TC_FAIL, "digest is wrong");

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    TSCacheKeyDestroy(key);
    TSCacheKeyDestroy(key_cmp);
    return;
  } else {
    SDK_RPRINT(test, "TSCacheKeySetDigest", "TestCase1", TC_PASS, "ok");
    TSCacheKeyDestroy(key_cmp);
  }

  // prepare caching content
  // string, null-terminated.
  for (int i = 0; i < (OBJECT_SIZE - 1); i++) {
    content[i] = 'a';
  }
  content[OBJECT_SIZE - 1] = '\0';

  //Write data to cache.
  TSCont contp = TSContCreate(cache_handler, TSMutexCreate());
  CacheVConnStruct *cache_vconn = (CacheVConnStruct *) TSmalloc(sizeof(CacheVConnStruct));
  cache_vconn->key = key;
  TSContDataSet(contp, cache_vconn);

  TSCacheWrite(contp, key);
}

/* TSfopen */

//////////////////////////////////////////////
//       SDK_API_TSfopen
//
// Unit Test for API: TSfopen
//                    TSclose
//                    TSfflush
//                    TSfgets
//                    TSfread
//                    TSfwrite
//////////////////////////////////////////////

// Used to create tmp file
//#define TMP_DIR "/var/tmp"
#define	PFX	"plugin.config"

REGRESSION_TEST(SDK_API_TSfopen) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  char write_file_name[PATH_NAME_MAX + 1];

  TSFile source_read_file;     // existing file
  TSFile write_file;           // to be created
  TSFile cmp_read_file;        // read & compare

  char input_buffer[BUFSIZ];
  char cmp_buffer[BUFSIZ];
  struct stat stat_buffer_pre, stat_buffer_post, stat_buffer_input;
  char *ret_val;
  int error_counter = 0, read = 0, wrote = 0;
  int64_t read_amount = 0;
  char INPUT_TEXT_FILE[] = "plugin.config";
  char input_file_full_path[BUFSIZ];



  // Set full path to file at run time.
  // TODO: This can never fail since we are
  //       returning the char[]
  //       Better check the dir itself.
  //
  if (TSInstallDirGet() == NULL) {
    error_counter++;
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }
  // Add "etc/trafficserver" to point to config directory
  ink_filepath_make(input_file_full_path, sizeof(input_file_full_path), TSConfigDirGet(), INPUT_TEXT_FILE);

  // open existing file for reading
  if (!(source_read_file = TSfopen(input_file_full_path, "r"))) {
    SDK_RPRINT(test, "TSfopen", "TestCase1", TC_FAIL, "can't open file for reading");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  } else
    SDK_RPRINT(test, "TSfopen", "TestCase1", TC_PASS, "ok");

  // Create unique tmp _file_name_, do not use any TS file_name
  snprintf(write_file_name, PATH_NAME_MAX, "/tmp/%sXXXXXX", PFX);
  int write_file_fd;            // this file will be reopened below
  if ((write_file_fd = mkstemp(write_file_name)) <= 0) {
    SDK_RPRINT(test, "mkstemp", "std func", TC_FAIL, "can't create file for writing");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    return;
  }
  close(write_file_fd);

  // open file for writing, the file doesn't have to exist.
  if (!(write_file = TSfopen(write_file_name, "w"))) {
    SDK_RPRINT(test, "TSfopen", "TestCase2", TC_FAIL, "can't open file for writing");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    return;
  }
  SDK_RPRINT(test, "TSfopen", "TestCase2", TC_PASS, "ok");

  memset(input_buffer, '\0', BUFSIZ);

  // source_read_file and input_file_full_path are the same file
  if (stat(input_file_full_path, &stat_buffer_input) != 0) {
    SDK_RPRINT(test, "stat", "std func", TC_FAIL, "source file and input file messed up");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  read_amount = (stat_buffer_input.st_size <= (off_t)sizeof(input_buffer)) ?
    (stat_buffer_input.st_size) : (sizeof(input_buffer));

  // TSfgets
  if ((ret_val = TSfgets(source_read_file, input_buffer, read_amount))
      == NULL) {
    SDK_RPRINT(test, "TSfgets", "TestCase1", TC_FAIL, "can't read from file");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  } else {
    if (ret_val != input_buffer) {
      SDK_RPRINT(test, "TSfgets", "TestCase2", TC_FAIL, "reading error");
      error_counter++;

      // no need to continue, return
      *pstatus = REGRESSION_TEST_FAILED;
      if (source_read_file != NULL)
        TSfclose(source_read_file);
      if (write_file != NULL)
        TSfclose(write_file);
      return;
    } else
      SDK_RPRINT(test, "TSfgets", "TestCase1", TC_PASS, "ok");
  }

  // TSfwrite
  wrote = TSfwrite(write_file, input_buffer, read_amount);
  if (wrote != read_amount) {
    SDK_RPRINT(test, "TSfwrite", "TestCase1", TC_FAIL, "writing error");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  SDK_RPRINT(test, "TSfwrite", "TestCase1", TC_PASS, "ok");

  // TSfflush
  if (stat(write_file_name, &stat_buffer_pre) != 0) {
    SDK_RPRINT(test, "stat", "std func", TC_FAIL, "TSfwrite error");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  TSfflush(write_file);        // write_file should point to write_file_name

  if (stat(write_file_name, &stat_buffer_post) != 0) {
    SDK_RPRINT(test, "stat", "std func", TC_FAIL, "TSfflush error");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  if ((stat_buffer_pre.st_size == 0) && (stat_buffer_post.st_size == read_amount)) {
    SDK_RPRINT(test, "TSfflush", "TestCase1", TC_PASS, "ok");
  } else {
    SDK_RPRINT(test, "TSfflush", "TestCase1", TC_FAIL, "TSfflush error");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  // TSfread
  // open again for reading
  cmp_read_file = TSfopen(write_file_name, "r");
  if (cmp_read_file == NULL) {
    SDK_RPRINT(test, "TSfopen", "TestCase3", TC_FAIL, "can't open file for reading");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    return;
  }

  read_amount = (stat_buffer_input.st_size <= (off_t)sizeof(cmp_buffer)) ? (stat_buffer_input.st_size) : (sizeof(cmp_buffer));

  // TSfread on read file
  read = TSfread(cmp_read_file, cmp_buffer, read_amount);
  if (read != read_amount) {
    SDK_RPRINT(test, "TSfread", "TestCase1", TC_FAIL, "can't reading");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    if (cmp_read_file != NULL)
      TSfclose(cmp_read_file);
    return;
  } else
    SDK_RPRINT(test, "TSfread", "TestCase1", TC_PASS, "ok");

  // compare input_buffer and cmp_buffer buffers
  if (memcmp(input_buffer, cmp_buffer, read_amount) != 0) {
    SDK_RPRINT(test, "TSfread", "TestCase2", TC_FAIL, "reading error");
    error_counter++;

    // no need to continue, return
    *pstatus = REGRESSION_TEST_FAILED;
    if (source_read_file != NULL)
      TSfclose(source_read_file);
    if (write_file != NULL)
      TSfclose(write_file);
    if (cmp_read_file != NULL)
      TSfclose(cmp_read_file);
    return;
  } else
    SDK_RPRINT(test, "TSfread", "TestCase2", TC_PASS, "ok");

  // remove the tmp file
  if (unlink(write_file_name) != 0) {
    SDK_RPRINT(test, "unlink", "std func", TC_FAIL, "can't remove temp file");
  }
  // TSfclose on read file
  TSfclose(source_read_file);
  SDK_RPRINT(test, "TSfclose", "TestCase1", TC_PASS, "ok");

  // TSfclose on write file
  TSfclose(write_file);
  SDK_RPRINT(test, "TSfclose", "TestCase2", TC_PASS, "ok");

  if (error_counter == 0) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }
  if (cmp_read_file != NULL)
    TSfclose(cmp_read_file);
}

/* TSThread */

//////////////////////////////////////////////
//       SDK_API_TSThread
//
// Unit Test for API: TSThread
//                    TSThreadCreate
//                    TSThreadSelf
//////////////////////////////////////////////
static int thread_err_count = 0;
static RegressionTest *SDK_Thread_test;
static int *SDK_Thread_pstatus;
static void *thread_create_handler(void *arg);

static void *
thread_create_handler(void *arg)
{
  NOWARN_UNUSED(arg);
  TSThread athread;
  //Fix me: do more useful work
  sleep(10);

  athread = TSThreadSelf();
  if (athread == 0) {
    thread_err_count++;
    SDK_RPRINT(SDK_Thread_test, "TSThreadCreate", "TestCase2", TC_FAIL, "can't get thread");
  } else {
    SDK_RPRINT(SDK_Thread_test, "TSThreadCreate", "TestCase2", TC_PASS, "ok");
  }

  if (thread_err_count > 0)
    *SDK_Thread_pstatus = REGRESSION_TEST_FAILED;
  else
    *SDK_Thread_pstatus = REGRESSION_TEST_PASSED;

  return NULL;
}

// Fix me: Solaris threads/Win2K threads tests

// Argument data passed to thread init functions
//  cannot be allocated on the stack.

REGRESSION_TEST(SDK_API_TSThread) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  SDK_Thread_test = test;
  SDK_Thread_pstatus = pstatus;

  TSThread curr_thread = 0;
//    TSThread created_thread = 0;
  pthread_t curr_tid;

  curr_tid = pthread_self();

  // TSThreadSelf
  curr_thread = TSThreadSelf();
  if (curr_thread == 0) {
    SDK_RPRINT(test, "TSThreadSelf", "TestCase1", TC_FAIL, "can't get the current thread");
    thread_err_count++;
  } else {
    SDK_RPRINT(test, "TSThreadSelf", "TestCase1", TC_PASS, "ok");
  }

  // TSThreadCreate
  TSThread created_thread = TSThreadCreate(thread_create_handler, (void *) curr_tid);
  if (created_thread == NULL) {
    thread_err_count++;
    SDK_RPRINT(test, "TSThreadCreate", "TestCase1", TC_FAIL, "can't create thread");
  } else {
    SDK_RPRINT(test, "TSThreadCreate", "TestCase1", TC_PASS, "ok");
  }
}


//////////////////////////////////////////////
//       SDK_API_TSThread
//
// Unit Test for API: TSThreadInit
//                    TSThreadDestroy
//////////////////////////////////////////////
static int thread_init_err_count = 0;
static RegressionTest *SDK_ThreadInit_test;
static int *SDK_ThreadInit_pstatus;
static void *pthread_start_func(void *arg);

static void *
pthread_start_func(void *arg)
{
  NOWARN_UNUSED(arg);
  TSThread temp_thread = 0;

  // TSThreadInit
  temp_thread = TSThreadInit();

  if (!temp_thread) {
    SDK_RPRINT(SDK_ThreadInit_test, "TSThreadInit", "TestCase2", TC_FAIL, "can't init thread");
    thread_init_err_count++;
  } else
    SDK_RPRINT(SDK_ThreadInit_test, "TSThreadInit", "TestCase2", TC_PASS, "ok");

  // Clean up this thread
  if (temp_thread)
    TSThreadDestroy(temp_thread);

  if (thread_init_err_count > 0)
    *SDK_ThreadInit_pstatus = REGRESSION_TEST_FAILED;
  else
    *SDK_ThreadInit_pstatus = REGRESSION_TEST_PASSED;

  return NULL;
}

REGRESSION_TEST(SDK_API_TSThreadInit) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  SDK_ThreadInit_test = test;
  SDK_ThreadInit_pstatus = pstatus;

  pthread_t curr_tid, new_tid;

  curr_tid = pthread_self();

  int ret;
  errno = 0;
  ret = pthread_create(&new_tid, NULL, pthread_start_func, (void *) curr_tid);
  if (ret != 0) {
    thread_init_err_count++;
    SDK_RPRINT(test, "TSThreadInit", "TestCase1", TC_FAIL, "can't create pthread");
  } else
    SDK_RPRINT(test, "TSThreadInit", "TestCase1", TC_PASS, "ok");

}

/* Action */

//////////////////////////////////////////////
//       SDK_API_TSAction
//
// Unit Test for API: TSActionCancel
//////////////////////////////////////////////

static RegressionTest *SDK_ActionCancel_test;
static int *SDK_ActionCancel_pstatus;

int
action_cancel_handler(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(edata);
  if (event == TS_EVENT_IMMEDIATE)     // called from schedule_imm OK
  {
    SDK_RPRINT(SDK_ActionCancel_test, "TSActionCancel", "TestCase1", TC_PASS, "ok");
    *SDK_ActionCancel_pstatus = REGRESSION_TEST_PASSED;
  } else if (event == TS_EVENT_TIMEOUT)        //called from schedule_in Not OK.
  {
    SDK_RPRINT(SDK_ActionCancel_test, "TSActionCancel", "TestCase1", TC_FAIL, "bad action");
    *SDK_ActionCancel_pstatus = REGRESSION_TEST_FAILED;
  } else                        // there is sth wrong
  {
    SDK_RPRINT(SDK_ActionCancel_test, "TSActionCancel", "TestCase1", TC_FAIL, "bad event");
    *SDK_ActionCancel_pstatus = REGRESSION_TEST_FAILED;
  }

  TSContDestroy(contp);
  return 0;
}

REGRESSION_TEST(SDK_API_TSActionCancel) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  SDK_ActionCancel_test = test;
  SDK_ActionCancel_pstatus = pstatus;

  TSMutex cont_mutex = TSMutexCreate();
  TSCont contp = TSContCreate(action_cancel_handler, cont_mutex);
  TSAction actionp = TSContSchedule(contp, 10000, TS_THREAD_POOL_DEFAULT);

  TSMutexLock(cont_mutex);
  if (TSActionDone(actionp)) {
    *pstatus = REGRESSION_TEST_FAILED;
    TSMutexUnlock(cont_mutex);
    return;
  } else {
    TSActionCancel(actionp);
  }
  TSMutexUnlock(cont_mutex);

  TSContSchedule(contp, 0, TS_THREAD_POOL_DEFAULT);
}

//////////////////////////////////////////////
//       SDK_API_TSAction
//
// Unit Test for API: TSActionDone
//////////////////////////////////////////////
/* Currently, don't know how to test it because TSAction
   is at "done" status only "shortly" after finish
   executing action_done_handler. Another possibility is
   to use reentrant call. But in both cases it's not
   guaranteed to get ActionDone.
   */

/* Continuations */

//////////////////////////////////////////////
//       SDK_API_TSCont
//
// Unit Test for API: TSContCreate
//                    TSContCall
//////////////////////////////////////////////

// this is needed for asynchronous APIs
static RegressionTest *SDK_ContCreate_test;
static int *SDK_ContCreate_pstatus;

int
cont_handler(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(contp);
  NOWARN_UNUSED(event);
  NOWARN_UNUSED(edata);
  SDK_RPRINT(SDK_ContCreate_test, "TSContCreate", "TestCase1", TC_PASS, "ok");
  SDK_RPRINT(SDK_ContCreate_test, "TSContCall", "TestCase1", TC_PASS, "ok");

  *SDK_ContCreate_pstatus = REGRESSION_TEST_PASSED;

  return 0;
}


REGRESSION_TEST(SDK_API_TSContCreate) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  // For asynchronous APIs, use static vars to store test and pstatus
  SDK_ContCreate_test = test;
  SDK_ContCreate_pstatus = pstatus;

  TSMutex mutexp = TSMutexCreate();
  TSCont contp = TSContCreate(cont_handler, mutexp);

  if (TS_SUCCESS == TSMutexLockTry(mutexp)) { // Mutex is grabbed succesfully
    TSContCall(contp, (TSEvent) 0, NULL);
    TSMutexUnlock(mutexp);
  } else {                       //mutex has problems
    SDK_RPRINT(SDK_ContCreate_test, "TSContCreate", "TestCase1", TC_FAIL, "continuation creation has problems");
    SDK_RPRINT(SDK_ContCreate_test, "TSContCall", "TestCase1", TC_FAIL, "continuation has problems");

    *pstatus = REGRESSION_TEST_FAILED;
  }

  TSContDestroy(contp);
}


//////////////////////////////////////////////
//       SDK_API_TSCont
//
// Unit Test for API: TSContDataGet
//                    TSContDataSet
//////////////////////////////////////////////

// this is needed for asynchronous APIs
static RegressionTest *SDK_ContData_test;
static int *SDK_ContData_pstatus;

// this is specific for this test
typedef struct
{
  int data1;
  int data2;
} MyData;

int
cont_data_handler(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(event);
  NOWARN_UNUSED(edata);
  MyData *my_data = (MyData *) TSContDataGet(contp);

  if (my_data->data1 == 1 && my_data->data2 == 2) {
    SDK_RPRINT(SDK_ContData_test, "TSContDataSet", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(SDK_ContData_test, "TSContDataGet", "TestCase1", TC_PASS, "ok");

    *SDK_ContData_pstatus = REGRESSION_TEST_PASSED;
  } else {
    // If we get bad data, it's a failure
    SDK_RPRINT(SDK_ContData_test, "TSContDataSet", "TestCase1", TC_FAIL, "bad data");
    SDK_RPRINT(SDK_ContData_test, "TSContDataGet", "TestCase1", TC_FAIL, "bad data");

    *SDK_ContData_pstatus = REGRESSION_TEST_FAILED;
  }

  TSfree(my_data);
  TSContDestroy(contp);
  return 0;
}


REGRESSION_TEST(SDK_API_TSContDataGet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  // For asynchronous APIs, use static vars to store test and pstatus
  SDK_ContData_test = test;
  SDK_ContData_pstatus = pstatus;

  TSCont contp = TSContCreate(cont_data_handler, TSMutexCreate());

  MyData *my_data = (MyData *) TSmalloc(sizeof(MyData));
  my_data->data1 = 1;
  my_data->data2 = 2;

  TSContDataSet(contp, (void *) my_data);

  TSContSchedule(contp, 0, TS_THREAD_POOL_DEFAULT);
}



//////////////////////////////////////////////
//       SDK_API_TSCont
//
// Unit Test for API: TSContMutexGet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSContMutexGet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSMutex mutexp_input;
  TSMutex mutexp_output;
  TSCont contp;

  mutexp_input = TSMutexCreate();
  contp = TSContCreate(cont_handler, mutexp_input);

  mutexp_output = TSContMutexGet(contp);

  if (mutexp_input == mutexp_output) {
    SDK_RPRINT(test, "TSContMutexGet", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else
    SDK_RPRINT(test, "TSContMutexGet", "TestCase1", TC_FAIL, "Continutation's mutex corrupted");

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);

  TSContDestroy(contp);
}

//////////////////////////////////////////////
//       SDK_API_TSCont
//
// Unit Test for API: TSContSchedule
//////////////////////////////////////////////

// this is needed for asynchronous APIs
static RegressionTest *SDK_ContSchedule_test;
static int *SDK_ContSchedule_pstatus;

// this is specific for this test
static int tc1_count = 0;
static int tc2_count = 0;

int
cont_schedule_handler(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(edata);
  if (event == TS_EVENT_IMMEDIATE) {
    // Test Case 1
    SDK_RPRINT(SDK_ContSchedule_test, "TSContSchedule", "TestCase1", TC_PASS, "ok");
    tc1_count++;
  } else if (event == TS_EVENT_TIMEOUT) {
    // Test Case 2
    SDK_RPRINT(SDK_ContSchedule_test, "TSContSchedule", "TestCase2", TC_PASS, "ok");
    tc2_count++;
  } else {
    // If we receive a bad event, it's a failure
    SDK_RPRINT(SDK_ContSchedule_test, "TSContSchedule", "TestCase1|2", TC_FAIL, "received unexpected event number %d", event);
    *SDK_ContSchedule_pstatus = REGRESSION_TEST_FAILED;
    return 0;
  }

  // We expect to be called once for TC1 and once for TC2
  if ((tc1_count == 1) && (tc2_count == 1)) {
    *SDK_ContSchedule_pstatus = REGRESSION_TEST_PASSED;
  }
  // If TC1 or TC2 executed more than once, something is fishy..
  else if (tc1_count + tc2_count >= 2) {
    *SDK_ContSchedule_pstatus = REGRESSION_TEST_FAILED;
  }

  TSContDestroy(contp);
  return 0;
}

/* Mutex */

/*
   Fix me: test for grabbing the mutex from two
   different threads.
   */

//////////////////////////////////////////////
//       SDK_API_TSMutex
//
// Unit Test for API: TSMutexCreate
//                    TSMutexLock
//                    TSMutexUnLock
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSMutexCreate) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSMutex mutexp = TSMutexCreate();

  TSMutexLock(mutexp);

  /* This is normal because all locking is from the same thread */
  TSReturnCode lock = TS_ERROR;

  lock = TSMutexLockTry(mutexp);
  lock = TSMutexLockTry(mutexp);

  if (TS_SUCCESS == lock) {
    SDK_RPRINT(test, "TSMutexCreate", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSMutexLock", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSMutexLockTry", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSMutexCreate", "TestCase1", TC_FAIL, "mutex can't be grabbed twice from the same thread");
    SDK_RPRINT(test, "TSMutexLock", "TestCase1", TC_FAIL, "mutex can't be grabbed twice from the same thread");
    SDK_RPRINT(test, "TSMutexLockTry", "TestCase1", TC_FAIL, "mutex can't be grabbed twice from the same thread");

  }

  TSMutexUnlock(mutexp);
  SDK_RPRINT(test, "TSMutexUnLock", "TestCase1", TC_PASS, "ok");

  if (test_passed) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

}

/* IOBuffer */

//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferCreate
//                    TSIOBufferWaterMarkGet
//                    TSIOBufferWaterMarkSet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferCreate) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  int64_t watermark = 1000;

  TSIOBuffer bufp = TSIOBufferCreate();

  TSIOBufferWaterMarkSet(bufp, watermark);
  watermark = TSIOBufferWaterMarkGet(bufp);

  if (watermark == 1000) {
    SDK_RPRINT(test, "TSIOBufferCreate", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferWaterMarkGet", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferWaterMarkSet", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferCreate", "TestCase1", TC_FAIL, "watermark failed");
    SDK_RPRINT(test, "TSIOBufferWaterMarkGet", "TestCase1", TC_FAIL, "watermark failed");
    SDK_RPRINT(test, "TSIOBufferWaterMarkSet", "TestCase1", TC_FAIL, "watermark failed");
  }

  TSIOBufferDestroy(bufp);

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;

}


//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferSizedCreate
//                    TSIOBufferProduce
//                    TSIOBufferReaderAlloc
//                    TSIOBufferReaderAvail
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferProduce) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSIOBuffer bufp = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);        //size is 4096

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);

  TSIOBufferProduce(bufp, 10);

  int64_t reader_avail = TSIOBufferReaderAvail(readerp);
  if (reader_avail == 10) {
    SDK_RPRINT(test, "TSIOBufferProduce", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferReaderAlloc", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferReaderAvail", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferProduce", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferReaderAlloc", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferReaderAvail", "TestCase1", TC_FAIL, "failed");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;
}


//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferReaderConsume
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferReaderConsume) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSIOBuffer bufp = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);

  TSIOBufferProduce(bufp, 10);
  TSIOBufferReaderConsume(readerp, 10);

  int64_t reader_avail = TSIOBufferReaderAvail(readerp);
  if (reader_avail == 0) {
    SDK_RPRINT(test, "TSIOBufferReaderConsume", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferReaderConsume", "TestCase1", TC_FAIL, "failed");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;
}

//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferReaderClone
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferReaderClone) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSIOBuffer bufp = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);

  TSIOBufferProduce(bufp, 10);
  TSIOBufferReaderConsume(readerp, 5);

  TSIOBufferReader readerp2 = TSIOBufferReaderClone(readerp);

  int64_t reader_avail = TSIOBufferReaderAvail(readerp2);
  if (reader_avail == 5) {
    SDK_RPRINT(test, "TSIOBufferReaderClone", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferReaderClone", "TestCase1", TC_FAIL, "failed");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;
}

//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferStart
//                    TSIOBufferReaderStart
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferStart) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSIOBuffer bufp = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);

  if (TSIOBufferStart(bufp) == TSIOBufferReaderStart(readerp)) {
    SDK_RPRINT(test, "TSIOBufferStart", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferReaderStart", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferStart", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferReaderStart", "TestCase1", TC_FAIL, "failed");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;
}


//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferCopy
//                    TSIOBufferWrite
//                    TSIOBufferReaderCopy
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferCopy) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  char input_buf[] = "This is the test for TSIOBufferCopy, TSIOBufferWrite, TSIOBufferReaderCopy";
  char output_buf[1024];
  TSIOBuffer bufp = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);
  TSIOBuffer bufp2 = TSIOBufferSizedCreate(TS_IOBUFFER_SIZE_INDEX_4K);

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);
  TSIOBufferWrite(bufp, input_buf, (strlen(input_buf) + 1));
  TSIOBufferCopy(bufp2, readerp, (strlen(input_buf) + 1), 0);
  TSIOBufferReaderCopy(readerp, output_buf, (strlen(input_buf) + 1));

  if (strcmp(input_buf, output_buf) == 0) {
    SDK_RPRINT(test, "TSIOBufferWrite", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferCopy", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferReaderCopy", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferWrite", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferCopy", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferReaderCopy", "TestCase1", TC_FAIL, "failed");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);
  return;
}

//////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBuffer
//                    TSIOBufferWrite
//                    TSIOBufferReaderCopy
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferBlockReadAvail) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed_1 = false;
  bool test_passed_2 = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  int i = 10000;
  TSIOBuffer bufp = TSIOBufferCreate();
  TSIOBufferWrite(bufp, (char*)&i, sizeof(int));
  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);

  int64_t avail_write, avail_read;

  // TODO: This is probably not correct any more.
  TSIOBufferBlock blockp = TSIOBufferStart(bufp);

  if ((TSIOBufferBlockWriteStart(blockp, &avail_write) - TSIOBufferBlockReadStart(blockp, readerp, &avail_read)) ==
      sizeof(int)) {
    SDK_RPRINT(test, "TSIOBufferBlockReadStart", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferBlockWriteStart", "TestCase1", TC_PASS, "ok");
    test_passed_1 = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferBlockReadStart", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferBlockWriteStart", "TestCase1", TC_FAIL, "failed");
  }

  if ((TSIOBufferBlockReadAvail(blockp, readerp) + TSIOBufferBlockWriteAvail(blockp)) == 4096) {
    SDK_RPRINT(test, "TSIOBufferBlockReadAvail", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSIOBufferBlockWriteAvail", "TestCase1", TC_PASS, "ok");
    test_passed_2 = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferBlockReadAvail", "TestCase1", TC_FAIL, "failed");
    SDK_RPRINT(test, "TSIOBufferBlockWriteAvail", "TestCase1", TC_FAIL, "failed");
  }

  if (test_passed_1 && test_passed_2) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

  return;
}

//////////////////////////////////////////////////
//       SDK_API_TSIOBuffer
//
// Unit Test for API: TSIOBufferBlockNext
//////////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSIOBufferBlockNext) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  int i = 10000;
  TSIOBuffer bufp = TSIOBufferCreate();
  TSIOBufferWrite(bufp, (char*)&i, sizeof(int));

  TSIOBufferReader readerp = TSIOBufferReaderAlloc(bufp);
  TSIOBufferBlock blockp = TSIOBufferReaderStart(readerp);

  // TODO: This is probaby not the best of regression tests right now ...
  // Note that this assumes block size is > sizeof(int) bytes.
  if (TSIOBufferBlockNext(blockp) == NULL) {
    SDK_RPRINT(test, "TSIOBufferBlockNext", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "TSIOBufferBlockNext", "TestCase1", TC_FAIL, "fail");
  }

  if (test_passed) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

  return;
}



/* Stats */

//////////////////////////////////////////////
//       SDK_API_INKStat
//
// Unit Test for API: INKStatCreate
//                    INKStatIntSet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatIntSet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  INKStat stat = INKStatCreate("stat_is", INKSTAT_TYPE_INT64);

  INKStatIntSet(stat, 100);
  int64_t stat_val;

  stat_val = INKStatIntGet(stat);

  if (stat_val == 100) {
    SDK_RPRINT(test, "INKStatIntSet", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "INKStatIntSet", "TestCase1", TC_FAIL, "can't set to correct integer value");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);

}

//////////////////////////////////////////////
//       SDK_API_INKStat
//
// Unit Test for API: INKStatIntAddTo
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatIntAddTo) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  INKStat stat = INKStatCreate("stat_ia", INKSTAT_TYPE_INT64);

  INKStatIntAddTo(stat, 100);
  int64_t stat_val;

  stat_val = INKStatIntGet(stat);

  if (stat_val == 100) {
    SDK_RPRINT(test, "INKStatIntAddTo", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "INKStatIntAddTo", "TestCase1", TC_FAIL, "can't add to correct integer value");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);

}

//////////////////////////////////////////////
//       SDK_API_INKStat
//
// Unit Test for API: INKStatFloatAddTo
//                    INKStatFloatGet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatFloatAddTo) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  INKStat stat = INKStatCreate("stat_fa", INKSTAT_TYPE_FLOAT);

  INKStatFloatAddTo(stat, 100.0);
  float stat_val = INKStatFloatGet(stat);

  if (stat_val == 100.0) {
    SDK_RPRINT(test, "INKStatFloatAddTo", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "INKStatFloatAddTo", "TestCase1", TC_FAIL, "can't add to correct float value");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);

}

//////////////////////////////////////////////
//       SDK_API_INKStat
//
// Unit Test for API: INKStatFloatSet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatFloatSet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  INKStat stat = INKStatCreate("stat_fs", INKSTAT_TYPE_FLOAT);

  INKStatFloatSet(stat, 100.0);
  float stat_val = INKStatFloatGet(stat);

  if (stat_val == 100.0) {
    SDK_RPRINT(test, "INKStatFloatSet", "TestCase1", TC_PASS, "ok");
    test_passed = true;
  } else {
    SDK_RPRINT(test, "INKStatFloatSet", "TestCase1", TC_FAIL, "can't set to correct float value");
  }

  // Status of the whole test
  *pstatus = ((test_passed == true) ? REGRESSION_TEST_PASSED : REGRESSION_TEST_FAILED);

}

//////////////////////////////////////////////
//       SDK_API_INKStat
//
// Unit Test for API: INKStatIncrement
//                    INKStatDecrement
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatIncrement) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  bool test_passed_int_increase = false;
  bool test_passed_int_decrease = false;
  bool test_passed_float_increase = false;
  bool test_passed_float_decrease = false;
  *pstatus = REGRESSION_TEST_INPROGRESS;

  INKStat stat_1 = INKStatCreate("stat_1", INKSTAT_TYPE_INT64);
  INKStat stat_2 = INKStatCreate("stat_2", INKSTAT_TYPE_FLOAT);

  INKStatIncrement(stat_1);
  int64_t stat1_val =  INKStatIntGet(stat_1);

  if (stat1_val == 1) {
    SDK_RPRINT(test, "INKStatIncrement", "TestCase1", TC_PASS, "ok for int stat");
    test_passed_int_increase = true;
  } else {
    SDK_RPRINT(test, "INKStatIncrement", "TestCase1", TC_FAIL, "can't increase to correct integer value");
  }

  INKStatDecrement(stat_1);
  stat1_val = INKStatIntGet(stat_1);

  if (stat1_val == 0) {
    SDK_RPRINT(test, "INKStatDecrement", "TestCase1", TC_PASS, "ok for int stat");
    test_passed_int_decrease = true;
  } else {
    SDK_RPRINT(test, "INKStatDecrement", "TestCase1", TC_FAIL, "can't decrease to correct integer value");
  }

  INKStatIncrement(stat_2);
  float stat2_val = INKStatFloatGet(stat_2);

  if (stat2_val == 1.0) {
    SDK_RPRINT(test, "INKStatIncrement", "TestCase2", TC_PASS, "ok for float stat");
    test_passed_float_increase = true;
  } else {
    char message[80];
    snprintf(message, sizeof(message), "can't increase to correct float value (1.0 != %.3f)", stat2_val);
    SDK_RPRINT(test, "INKStatIncrement", "TestCase2", TC_FAIL, &message[0]);
  }

  INKStatDecrement(stat_2);
  stat2_val = INKStatFloatGet(stat_2);

  if (stat2_val == 0.0) {
    SDK_RPRINT(test, "INKStatDecrement", "TestCase2", TC_PASS, "ok for float stat");
    test_passed_float_decrease = true;
  } else {
    SDK_RPRINT(test, "INKStatDecrement", "TestCase2", TC_FAIL, "can't decrease to correct float value");
  }

  // Status of the whole test
  if (test_passed_int_increase && test_passed_int_decrease && test_passed_float_increase && test_passed_float_decrease)
    *pstatus = REGRESSION_TEST_PASSED;
  else
    *pstatus = REGRESSION_TEST_FAILED;

}

////////////////////////////////////////////////////
//       SDK_API_INKCoupledStat
//
// Unit Test for API: INKStatCoupledGlobalCategoryCreate
//                    INKStatCoupledLoacalCopyCreate
//                    INKStatCoupledLoacalCopyDestroy
//                    INKStatCoupledGlobalAdd
//                    INKStatCoupledLocalAdd
//                    INKStatsCoupledUpdate
////////////////////////////////////////////////////

REGRESSION_TEST(SDK_API_INKStatCoupled) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  /* Create global category and its stats */
  INKCoupledStat stat_global_category = INKStatCoupledGlobalCategoryCreate("global.category");
  INKStat global_stat_sum = INKStatCoupledGlobalAdd(stat_global_category, "global.statsum", INKSTAT_TYPE_FLOAT);
  INKStat global_stat_1 = INKStatCoupledGlobalAdd(stat_global_category, "global.stat1", INKSTAT_TYPE_INT64);
  INKStat global_stat_2 = INKStatCoupledGlobalAdd(stat_global_category, "global.stat2", INKSTAT_TYPE_INT64);

  /* Create local category and its stats */
  INKCoupledStat stat_local_copy = INKStatCoupledLocalCopyCreate("local.copy", stat_global_category);
  INKStat local_stat_sum = INKStatCoupledLocalAdd(stat_local_copy, "local.statsum", INKSTAT_TYPE_FLOAT);
  INKStat local_stat_1 = INKStatCoupledLocalAdd(stat_local_copy, "local.stat1", INKSTAT_TYPE_INT64);
  INKStat local_stat_2 = INKStatCoupledLocalAdd(stat_local_copy, "local.stat2", INKSTAT_TYPE_INT64);

  /* stat operation */
  INKStatIntSet(local_stat_1, 100);
  INKStatIntSet(local_stat_2, 100);
  float local_val_1 = INKStatFloatGet(local_stat_1);
  float local_val_2 = INKStatFloatGet(local_stat_2);

  INKStatFloatAddTo(local_stat_sum, local_val_1);
  INKStatFloatAddTo(local_stat_sum, local_val_2);
  float local_val_sum = INKStatFloatGet(local_stat_sum);

  INKStatsCoupledUpdate(stat_local_copy);
  INKStatCoupledLocalCopyDestroy(stat_local_copy);

  float global_val_sum = INKStatFloatGet(global_stat_sum);
  int64_t global_val_1 = INKStatIntGet(global_stat_1);
  int64_t global_val_2 = INKStatIntGet(global_stat_2);

  if (local_val_1 == global_val_1 && local_val_2 == global_val_2 && local_val_sum == global_val_sum) {
    SDK_RPRINT(test, "INKStatCoupledGlobalCategoryCreate", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "INKStatCoupledGlobalAdd", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "INKStatCoupledLocalCopyCreate", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "INKStatCoupledLocalAdd", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "INKStatCoupledLocalCopyDestroy", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "INKStatCoupledUpdate", "TestCase1", TC_PASS, "ok");
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    SDK_RPRINT(test, "INKStatCoupledGlobalCategoryCreate", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");
    SDK_RPRINT(test, "INKStatCoupledGlobalAdd", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");
    SDK_RPRINT(test, "INKStatCoupledLocalCopyCreate", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");
    SDK_RPRINT(test, "INKStatCoupledLocalAdd", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");
    SDK_RPRINT(test, "INKStatCoupledLocalCopyDestroy", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");
    SDK_RPRINT(test, "INKStatCoupledUpdate", "TestCase1", TC_FAIL, "global stats' value is not equal to local one");

    *pstatus = REGRESSION_TEST_FAILED;
  }
}


REGRESSION_TEST(SDK_API_TSContSchedule) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  // For asynchronous APIs, use static vars to store test and pstatus
  SDK_ContSchedule_test = test;
  SDK_ContSchedule_pstatus = pstatus;

  TSCont contp = TSContCreate(cont_schedule_handler, TSMutexCreate());
  TSCont contp2 = TSContCreate(cont_schedule_handler, TSMutexCreate());

  // Test Case 1: schedule immediate
  TSContSchedule(contp, 0, TS_THREAD_POOL_DEFAULT);

  // Test Case 2: schedule in 10ms
  TSContSchedule(contp2, 10, TS_THREAD_POOL_DEFAULT);
}

//////////////////////////////////////////////////////////////////////////////
//     SDK_API_HttpHookAdd
//
// Unit Test for API: TSHttpHookAdd
//                    TSHttpTxnReenable
//                    TSHttpTxnClientIPGet
//                    TSHttpTxnServerIPGet
//                    TSHttpTxnClientIncomingPortGet
//                    TSHttpTxnClientRemotePortGet
//                    TSHttpTxnClientReqGet
//                    TSHttpTxnClientRespGet
//                    TSHttpTxnServerReqGet
//                    TSHttpTxnServerRespGet
//////////////////////////////////////////////////////////////////////////////

#define HTTP_HOOK_TEST_REQUEST_ID  1

typedef struct
{
  RegressionTest *regtest;
  int *pstatus;
  SocketServer *os;
  ClientTxn *browser;
  int hook_mask;
  int reenable_mask;
  bool test_client_ip_get;
  bool test_client_incoming_port_get;
  bool test_client_remote_port_get;
  bool test_client_req_get;
  bool test_client_resp_get;
  bool test_server_ip_get;
  bool test_server_req_get;
  bool test_server_resp_get;
  bool test_next_hop_ip_get;

  unsigned int magic;
} SocketTest;


//This func is called by us from mytest_handler to test TSHttpTxnClientIPGet
static int
checkHttpTxnClientIPGet(SocketTest * test, void *data)
{

  sockaddr const* ptr;
  in_addr_t ip;
  TSHttpTxn txnp = (TSHttpTxn) data;
  in_addr_t actual_ip = htonl(INADDR_LOOPBACK);     /* 127.0.0.1 is expected because the client is on the same machine */

  ptr = TSHttpTxnClientAddrGet(txnp);
  if (ptr == 0 || INADDR_ANY == (ip = ink_inet_ip4_addr_cast(ptr))) {
    test->test_client_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientIPGet", "TestCase1", TC_FAIL, "TSHttpTxnClientIPGet returns 0 %s", ptr ? "address" : "pointer");
    return TS_EVENT_CONTINUE;
  }

  if (ip == actual_ip) {
    test->test_client_ip_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientIPGet", "TestCase1", TC_PASS, "ok [%0.8x]", ip);
  } else {
    test->test_client_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientIPGet", "TestCase1", TC_FAIL, "Value's Mismatch [expected %.8x got %.8x]", actual_ip, ip);
  }
  return TS_EVENT_CONTINUE;

}

//This func is called by us from mytest_handler to check for TSHttpTxnNextHopIPGet
static int
checkHttpTxnNextHopIPGet(SocketTest * test, void *data)
{
  TSHttpTxn txnp = (TSHttpTxn) data;
  in_addr_t actual_ip = htonl(INADDR_LOOPBACK);     /* 127.0.0.1 is expected because the client is on the same machine */
  sockaddr const* ptr;
  in_addr_t nexthopip;

  ptr = TSHttpTxnNextHopAddrGet(txnp);
  if (ptr == 0 || (nexthopip = ink_inet_ip4_addr_cast(ptr)) == 0) {
    test->test_next_hop_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnNextHopIPGet", "TestCase1", TC_FAIL, "TSHttpTxnNextHopIPGet returns 0 %s", ptr ? "address" : "pointer" );
    return TS_EVENT_CONTINUE;
  }

  if (nexthopip == actual_ip) {
    test->test_next_hop_ip_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnNextHopIPGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_next_hop_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnNextHopIPGet", "TestCase1", TC_FAIL, "Value's Mismatch [expected %0.8x got %0.8x]", actual_ip, nexthopip);
  }

  return TS_EVENT_CONTINUE;
}



//This func is called by us from mytest_handler to test TSHttpTxnServerIPGet
static int
checkHttpTxnServerIPGet(SocketTest * test, void *data)
{
  sockaddr const* ptr;
  in_addr_t ip;
  TSHttpTxn txnp = (TSHttpTxn) data;
  in_addr_t actual_ip = htonl(INADDR_LOOPBACK);      /* 127.0.0.1 is expected because the client is on the same machine */

  ptr = TSHttpTxnServerAddrGet(txnp);
  if (0 == ptr || 0 == (ip = ink_inet_ip4_addr_cast(ptr))) {
    test->test_server_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerIPGet", "TestCase1", TC_FAIL, "TSHttpTxnServerIPGet returns 0 %s", ptr ? "address" : "pointer");
    return TS_EVENT_CONTINUE;
  }

  if (ip == actual_ip) {
    test->test_server_ip_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerIPGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_server_ip_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerIPGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }


  return TS_EVENT_CONTINUE;

}

//This func is called by us from mytest_handler to test TSHttpTxnClientIncomingPortGet
static int
checkHttpTxnClientIncomingPortGet(SocketTest * test, void *data)
{
  uint16_t port;
  TSMgmtInt port_from_config_file = -1;
  TSHttpTxn txnp = (TSHttpTxn) data;
  sockaddr const* ptr = TSHttpTxnIncomingAddrGet(txnp);

  if (0 == ptr) {
    SDK_RPRINT(test->regtest, "TSHttpTxnIncomingPortGet", "TestCase1", TC_FAIL,
               "TSHttpTxnClientIncomingPortGet returns 0 pointer");
    test->test_client_incoming_port_get = false;
    return TS_EVENT_CONTINUE;
  }
  port = ink_inet_get_port(ptr);

  if (TSMgmtIntGet("proxy.config.http.server_port", &port_from_config_file) != TS_SUCCESS) {
    port_from_config_file = 8080;
  }

  TSDebug(UTDBG_TAG, "TS HTTP port = %x, Txn incoming client port %x", (int) port_from_config_file, port);

  if (port == static_cast<uint16_t>(port_from_config_file)) {
    SDK_RPRINT(test->regtest, "TSHttpTxnClientIncomingPortGet", "TestCase1", TC_PASS, "ok");
    test->test_client_incoming_port_get = true;
  } else {
    SDK_RPRINT(test->regtest, "TSHttpTxnClientIncomingPortGet", "TestCase1", TC_FAIL,
               "Value's Mismatch. From Funtion: %d  Expected value: %d", port, port_from_config_file);
    test->test_client_incoming_port_get = false;
  }
  return TS_EVENT_CONTINUE;
}

//This func is called by us from mytest_handler to test TSHttpTxnClientRemotePortGet
static int
checkHttpTxnClientRemotePortGet(SocketTest * test, void *data)
{

  uint16_t port;
  uint16_t browser_port;
  TSHttpTxn txnp = (TSHttpTxn) data;
  sockaddr const* ptr = TSHttpTxnClientAddrGet(txnp);

  browser_port = test->browser->local_port;

  if (0 == ptr) {
    SDK_RPRINT(test->regtest, "TSHttpTxnClientClientAddrGet", "TestCase2", TC_FAIL,
               "TSHttpTxnClientAddrGet returned 0 pointer.");
    test->test_client_remote_port_get = false;
    return TS_EVENT_CONTINUE;
  }

  port = ink_inet_get_port(ptr);
  TSDebug(UTDBG_TAG, "Browser port = %x, Txn remote port = %x", browser_port, port);

  if (port == browser_port) {
    SDK_RPRINT(test->regtest, "TSHttpTxnClientAddrGet", "TestCase1", TC_PASS, "ok");
    test->test_client_remote_port_get = true;
  } else {
    SDK_RPRINT(test->regtest, "TSHttpTxnClientAddrGet", "TestCase1", TC_FAIL,
               "Value's Mismatch. From Function: %d Expected Value: %d", port, browser_port);
    test->test_client_remote_port_get = false;
  }
  return TS_EVENT_CONTINUE;
}

// This func is called by us from mytest_handler to test TSHttpTxnClientReqGet
static int
checkHttpTxnClientReqGet(SocketTest * test, void *data)
{

  TSMBuffer bufp;
  TSMLoc mloc;
  TSHttpTxn txnp = (TSHttpTxn) data;

  if (TSHttpTxnClientReqGet(txnp, &bufp, &mloc) != TS_SUCCESS) {
    test->test_client_req_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientReqGet", "TestCase1", TC_FAIL, "Unable to get handle to client request");
    return TS_EVENT_CONTINUE;
  }

  if ((bufp == reinterpret_cast<TSMBuffer>(&((HttpSM *) txnp)->t_state.hdr_info.client_request)) &&
      (mloc == reinterpret_cast<TSMLoc>(((HttpSM *)txnp)->t_state.hdr_info.client_request.m_http))) {
    test->test_client_req_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientReqGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_client_req_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientReqGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }

  return TS_EVENT_CONTINUE;
}

// This func is called by us from mytest_handler to test TSHttpTxnClientRespGet
static int
checkHttpTxnClientRespGet(SocketTest * test, void *data)
{

  TSMBuffer bufp;
  TSMLoc mloc;
  TSHttpTxn txnp = (TSHttpTxn) data;

  if (TSHttpTxnClientRespGet(txnp, &bufp, &mloc) != TS_SUCCESS) {
    test->test_client_resp_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientRespGet", "TestCase1", TC_FAIL, "Unable to get handle to client response");
    return TS_EVENT_CONTINUE;
  }

  if ((bufp == reinterpret_cast<TSMBuffer>(&((HttpSM *) txnp)->t_state.hdr_info.client_response)) &&
      (mloc == reinterpret_cast<TSMLoc>(((HttpSM *) txnp)->t_state.hdr_info.client_response.m_http))) {
    test->test_client_resp_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientRespGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_client_resp_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnClientRespGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }

  return TS_EVENT_CONTINUE;
}

// This func is called by us from mytest_handler to test TSHttpTxnServerReqGet
static int
checkHttpTxnServerReqGet(SocketTest * test, void *data)
{

  TSMBuffer bufp;
  TSMLoc mloc;
  TSHttpTxn txnp = (TSHttpTxn) data;

  if (TSHttpTxnServerReqGet(txnp, &bufp, &mloc) != TS_SUCCESS) {
    test->test_server_req_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerReqGet", "TestCase1", TC_FAIL, "Unable to get handle to server request");
    return TS_EVENT_CONTINUE;
  }

  if ((bufp == reinterpret_cast<TSMBuffer>(&((HttpSM *) txnp)->t_state.hdr_info.server_request)) &&
      (mloc == reinterpret_cast<TSMLoc>(((HttpSM *) txnp)->t_state.hdr_info.server_request.m_http))) {
    test->test_server_req_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerReqGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_server_req_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerReqGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }

  return TS_EVENT_CONTINUE;
}

// This func is called by us from mytest_handler to test TSHttpTxnServerRespGet
static int
checkHttpTxnServerRespGet(SocketTest * test, void *data)
{

  TSMBuffer bufp;
  TSMLoc mloc;
  TSHttpTxn txnp = (TSHttpTxn) data;

  if (TSHttpTxnServerRespGet(txnp, &bufp, &mloc) != TS_SUCCESS) {
    test->test_server_resp_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerRespGet", "TestCase1", TC_FAIL, "Unable to get handle to server response");
    return TS_EVENT_CONTINUE;
  }

  if ((bufp == reinterpret_cast<TSMBuffer>(&((HttpSM *) txnp)->t_state.hdr_info.server_response)) &&
      (mloc == reinterpret_cast<TSMLoc>(((HttpSM *) txnp)->t_state.hdr_info.server_response.m_http))) {
    test->test_server_resp_get = true;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerRespGet", "TestCase1", TC_PASS, "ok");
  } else {
    test->test_server_resp_get = false;
    SDK_RPRINT(test->regtest, "TSHttpTxnServerRespGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }

  return TS_EVENT_CONTINUE;
}


// This func is called both by us when scheduling EVENT_IMMEDIATE
// And by HTTP SM for registered hooks
static int
mytest_handler(TSCont contp, TSEvent event, void *data)
{
  SocketTest *test = (SocketTest *) TSContDataGet(contp);
  if (test == NULL) {
    if ((event == TS_EVENT_IMMEDIATE) || (event == TS_EVENT_TIMEOUT)) {
      return 0;
    }
    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    return 0;
  }
  TSAssert(test->magic == MAGIC_ALIVE);
  TSAssert(test->browser->magic == MAGIC_ALIVE);

  switch (event) {
  case TS_EVENT_HTTP_TXN_START:
    if (test->hook_mask == 0) {
      test->hook_mask |= 1;
    }

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 1;
    break;

  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    if (test->hook_mask == 1) {
      test->hook_mask |= 2;
    }
    TSSkipRemappingSet((TSHttpTxn) data,1);
    checkHttpTxnClientReqGet(test, data);

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 2;
    break;

  case TS_EVENT_HTTP_OS_DNS:
    if (test->hook_mask == 7) {
      test->hook_mask |= 8;
    }

    checkHttpTxnClientIncomingPortGet(test, data);
    checkHttpTxnClientRemotePortGet(test, data);

    checkHttpTxnClientIPGet(test, data);
    checkHttpTxnServerIPGet(test, data);

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 8;
    break;

  case TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE:
    if (test->hook_mask == 3) {
      test->hook_mask |= 4;
    }
    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 4;
    break;

  case TS_EVENT_HTTP_SEND_REQUEST_HDR:
    if (test->hook_mask == 15) {
      test->hook_mask |= 16;
    }

    checkHttpTxnServerReqGet(test, data);
    checkHttpTxnNextHopIPGet(test, data);

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 16;
    break;

  case TS_EVENT_HTTP_READ_RESPONSE_HDR:
    if (test->hook_mask == 31) {
      test->hook_mask |= 32;
    }
    checkHttpTxnServerRespGet(test, data);

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 32;
    break;

  case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
    if (test->hook_mask == 63) {
      test->hook_mask |= 64;
    }

    checkHttpTxnClientRespGet(test, data);

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 64;
    break;

  case TS_EVENT_HTTP_TXN_CLOSE:
    if (test->hook_mask == 127) {
      test->hook_mask |= 128;
    }

    TSHttpTxnReenable((TSHttpTxn) data, TS_EVENT_HTTP_CONTINUE);
    test->reenable_mask |= 128;
    break;

  case TS_EVENT_IMMEDIATE:
  case TS_EVENT_TIMEOUT:
    /* Browser still waiting the response ? */
    if (test->browser->status == REQUEST_INPROGRESS) {
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
    }
    /* Browser got the response. test is over. clean up */
    else {
      /* Note: response is available using test->browser->response pointer */
      if ((test->browser->status == REQUEST_SUCCESS) && (test->hook_mask == 255)) {
        *(test->pstatus) = REGRESSION_TEST_PASSED;
        SDK_RPRINT(test->regtest, "TSHttpHookAdd", "TestCase1", TC_PASS, "ok");

      } else {
        *(test->pstatus) = REGRESSION_TEST_FAILED;
        SDK_RPRINT(test->regtest, "TSHttpHookAdd", "TestCase1", TC_FAIL,
                   "Hooks not called or request failure. Hook mask = %d\n %s", test->hook_mask,test->browser->response);
      }

      if (test->reenable_mask == 255) {
        SDK_RPRINT(test->regtest, "TSHttpTxnReenable", "TestCase1", TC_PASS, "ok");

      } else {
        *(test->pstatus) = REGRESSION_TEST_FAILED;
        SDK_RPRINT(test->regtest, "TSHttpTxnReenable", "TestCase1", TC_FAIL, "Txn not reenabled properly");

      }

      if ((test->test_client_ip_get != true) ||
          (test->test_client_incoming_port_get != true) ||
          (test->test_client_remote_port_get != true) ||
          (test->test_client_req_get != true) ||
          (test->test_client_resp_get != true) ||
          (test->test_server_ip_get != true) ||
          (test->test_server_req_get != true) ||
          (test->test_server_resp_get != true) || (test->test_next_hop_ip_get != true)) {
        *(test->pstatus) = REGRESSION_TEST_FAILED;
      }
      // transaction is over. clean up.
      synclient_txn_delete(test->browser);
      synserver_delete(test->os);

      test->magic = MAGIC_DEAD;
      TSfree(test);
      TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(test->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(test->regtest, "TSHttpHookAdd", "TestCase1", TC_FAIL, "Unexpected event %d", event);
    break;
  }

  return TS_EVENT_IMMEDIATE;
}



EXCLUSIVE_REGRESSION_TEST(SDK_API_HttpHookAdd) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSCont cont = TSContCreate(mytest_handler, TSMutexCreate());
  SocketTest *socktest = (SocketTest *) TSmalloc(sizeof(SocketTest));

  socktest->regtest = test;
  socktest->pstatus = pstatus;
  socktest->hook_mask = 0;
  socktest->reenable_mask = 0;
  socktest->test_client_ip_get = false;
  socktest->test_client_incoming_port_get = false;
  socktest->test_client_req_get = false;
  socktest->test_client_resp_get = false;
  socktest->test_server_ip_get = false;
  socktest->test_server_req_get = false;
  socktest->test_server_resp_get = false;
  socktest->test_next_hop_ip_get = false;
  socktest->magic = MAGIC_ALIVE;
  TSContDataSet(cont, socktest);

  /* Register to HTTP hooks that are called in case of a cache MISS */
  TSHttpHookAdd(TS_HTTP_TXN_START_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_OS_DNS_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_SEND_REQUEST_HDR_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_READ_RESPONSE_HDR_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_SEND_RESPONSE_HDR_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_TXN_CLOSE_HOOK, cont);

  /* Create a new synthetic server */
  socktest->os = synserver_create(SYNSERVER_LISTEN_PORT);
  synserver_start(socktest->os);

  /* Create a client transaction */
  socktest->browser = synclient_txn_create();
  char *request = generate_request(HTTP_HOOK_TEST_REQUEST_ID);  // this request has a no-cache that prevents caching
  synclient_txn_send_request(socktest->browser, request);
  TSfree(request);

  /* Wait until transaction is done */
  if (socktest->browser->status == REQUEST_INPROGRESS) {
    TSContSchedule(cont, 25, TS_THREAD_POOL_DEFAULT);
  }

  return;
}


//////////////////////////////////////////////
//       SDK_API_TSUrl
//
// Unit Test for API: TSUrlCreate
//                    TSUrlDestroy
//                    TSUrlSchemeGet
//                    TSUrlSchemeSet
//                    TSUrlUserGet
//                    TSUrlUserSet
//                    TSUrlPasswordGet
//                    TSUrlPasswordSet
//                    TSUrlHostGet
//                    TSUrlHostSet
//                    TSUrlPortGet
//                    TSUrlPortSet
//                    TSUrlPathGet
//                    TSUrlPathSet
//                    TSUrlHttpParamsGet
//                    TSUrlHttpParamsSet
//                    TSUrlHttpQueryGet
//                    TSUrlHttpQuerySet
//                    TSUrlHttpFragmentGet
//                    TSUrlHttpFragmentSet
//                    TSUrlCopy
//                    TSUrlClone
//                    TSUrlStringGet
//                    TSUrlPrint
//                    TSUrlLengthGet
//                    TSUrlFtpTypeGet
//                    TSUrlFtpTypeSet
//////////////////////////////////////////////

char *
test_url_print(TSMBuffer bufp, TSMLoc hdr_loc)
{
  TSIOBuffer output_buffer;
  TSIOBufferReader reader;
  int64_t total_avail;

  TSIOBufferBlock block;
  const char *block_start;
  int64_t block_avail;

  char *output_string;
  int output_len;

  output_buffer = TSIOBufferCreate();

  if (!output_buffer) {
    TSError("couldn't allocate IOBuffer\n");
  }

  reader = TSIOBufferReaderAlloc(output_buffer);

  /* This will print  just MIMEFields and not
     the http request line */
  TSUrlPrint(bufp, hdr_loc, output_buffer);

  /* Find out how the big the complete header is by
     seeing the total bytes in the buffer.  We need to
     look at the buffer rather than the first block to
     see the size of the entire header */
  total_avail = TSIOBufferReaderAvail(reader);

  /* Allocate the string with an extra byte for the string
     terminator */
  output_string = (char *) TSmalloc(total_avail + 1);
  output_len = 0;

  /* We need to loop over all the buffer blocks to make
     sure we get the complete header since the header can
     be in multiple blocks */
  block = TSIOBufferReaderStart(reader);
  while (block) {

    block_start = TSIOBufferBlockReadStart(block, reader, &block_avail);

    /* We'll get a block pointer back even if there is no data
       left to read so check for this condition and break out of
       the loop. A block with no data to read means we've exhausted
       buffer of data since if there was more data on a later
       block in the chain, this block would have been skipped over */
    if (block_avail == 0) {
      break;
    }

    memcpy(output_string + output_len, block_start, block_avail);
    output_len += block_avail;

    /* Consume the data so that we get to the next block */
    TSIOBufferReaderConsume(reader, block_avail);

    /* Get the next block now that we've consumed the
       data off the last block */
    block = TSIOBufferReaderStart(reader);
  }

  /* Terminate the string */
  output_string[output_len] = '\0';
  output_len++;

  /* Free up the TSIOBuffer that we used to print out the header */
  TSIOBufferReaderFree(reader);
  TSIOBufferDestroy(output_buffer);

  return output_string;
}

REGRESSION_TEST(SDK_API_TSUrl) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  TSMBuffer bufp1 = (TSMBuffer)NULL;
  TSMBuffer bufp2 = (TSMBuffer)NULL;
  TSMBuffer bufp3 = (TSMBuffer)NULL;
  TSMLoc url_loc1;
  TSMLoc url_loc2;
  TSMLoc url_loc3;
  const char *scheme = TS_URL_SCHEME_HTTP;
  const char *scheme_get;
  const char *user = "yyy";
  const char *user_get;
  const char *password = "xxx";
  const char *password_get;
  const char *host = "www.example.com";
  const char *host_get;
  int port = 2021;
  char port_char[10];
  int port_get = 80;
  const char *path = "about/overview.html";
  const char *path_get;
  const char *params = "abcdef";
  const char *params_get;
  const char *query = "name=xxx";
  const char *query_get;
  const char *fragment = "yyy";
  const char *fragment_get;
  char *url_expected_string;
  char *url_string_from_1 = (char *)NULL;
  char *url_string_from_2 = (char *)NULL;
  char *url_string_from_3 = (char *)NULL;
  char *url_string_from_print = (char *)NULL;
  int url_expected_length;
  int url_length_from_1;
  int url_length_from_2;
  int type = 'a';
  int type_get;
  int tmp_len;

  bool test_passed_create = false;
  bool test_passed_destroy = false;
  bool test_passed_scheme = false;
  bool test_passed_user = false;
  bool test_passed_password = false;
  bool test_passed_host = false;
  bool test_passed_port = false;
  bool test_passed_path = false;
  bool test_passed_params = false;
  bool test_passed_query = false;
  bool test_passed_fragment = false;
  bool test_passed_copy = false;
  bool test_passed_clone = false;
  bool test_passed_string1 = false;
  bool test_passed_string2 = false;
  bool test_passed_print = false;
  bool test_passed_length1 = false;
  bool test_passed_length2 = false;
  bool test_passed_type = false;

  int length;

  *pstatus = REGRESSION_TEST_INPROGRESS;

  //Initialization
  memset(port_char, 0, 10);
  snprintf(port_char, sizeof(port_char), "%d", port);

  // HTTP URL

  url_expected_length = strlen(scheme) + strlen("://") +
    ((user == NULL) ? 0 : strlen(user)) +
    ((password == NULL) ? ((user == NULL) ? 0 : strlen("@")) : strlen(":") + strlen(password) + strlen("@")) +
    strlen(host) +
    ((port == 80) ? 0 : strlen(port_char) + strlen(":")) +
    strlen("/") + strlen(path) +
    ((params == NULL) ? 0 : strlen(";") + strlen(params)) +
    ((query == NULL) ? 0 : strlen("?") + strlen(query)) + ((fragment == NULL) ? 0 : strlen("#") + strlen(fragment));

  size_t len = url_expected_length + 1;
  url_expected_string = (char *) TSmalloc(len * sizeof(char));
  memset(url_expected_string, 0, url_expected_length + 1);
  snprintf(url_expected_string, len, "%s://%s%s%s%s%s%s%s/%s%s%s%s%s%s%s",
           scheme,
           ((user == NULL) ? "" : user),
           ((password == NULL) ? "" : ":"),
           ((password == NULL) ? "" : password),
           (((user == NULL) && (password == NULL)) ? "" : "@"),
           host,
           ((port == 80) ? "" : ":"),
           ((port == 80) ? "" : port_char),
           ((path == NULL) ? "" : path),
           ((params == NULL) ? "" : ";"),
           ((params == NULL) ? "" : params),
           ((query == NULL) ? "" : "?"),
           ((query == NULL) ? "" : query), ((fragment == NULL) ? "" : "#"), ((fragment == NULL) ? "" : fragment)
    );


  // Set Functions

  bufp1 = TSMBufferCreate();
  if (TSUrlCreate(bufp1, &url_loc1) != TS_SUCCESS) {
    // Cannot proceed with tests.
    SDK_RPRINT(test, "TSUrlCreate", "TestCase1", TC_FAIL, "unable to create URL within buffer.");
    goto print_results;
  }
  //Scheme
  if (TSUrlSchemeSet(bufp1, url_loc1, scheme, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlSchemeSet", "TestCase1", TC_FAIL, "TSUrlSchemeSet Returned TS_ERROR");
  } else {
    scheme_get = TSUrlSchemeGet(bufp1, url_loc1, &length);
    if (strncmp(scheme_get, scheme, length) == 0) {
      SDK_RPRINT(test, "TSUrlSchemeSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_scheme = true;
    } else {
      SDK_RPRINT(test, "TSUrlSchemeSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //User
  if (TSUrlUserSet(bufp1, url_loc1, user, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlUserSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    user_get = TSUrlUserGet(bufp1, url_loc1, &length);
    if (((user_get == NULL) && (user == NULL)) || (strncmp(user_get, user, length) == 0)) {
      SDK_RPRINT(test, "TSUrlUserSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_user = true;
    } else {
      SDK_RPRINT(test, "TSUrlUserSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  // Password
  if (TSUrlPasswordSet(bufp1, url_loc1, password, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlPasswordSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    password_get = TSUrlPasswordGet(bufp1, url_loc1, &length);
    if (((password_get == NULL) && (password == NULL)) || (strncmp(password_get, password, length) == 0)) {
      SDK_RPRINT(test, "TSUrlPasswordSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_password = true;
    } else {
      SDK_RPRINT(test, "TSUrlPasswordSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Host
  if (TSUrlHostSet(bufp1, url_loc1, host, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlHostSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    host_get = TSUrlHostGet(bufp1, url_loc1, &length);
    if (strncmp(host_get, host, length) == 0) {
      SDK_RPRINT(test, "TSUrlHostSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_host = true;
    } else {
      SDK_RPRINT(test, "TSUrlHostSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Port
  if (TSUrlPortSet(bufp1, url_loc1, port) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlPortSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    port_get = TSUrlPortGet(bufp1, url_loc1);
    if (port_get == port) {
      SDK_RPRINT(test, "TSUrlPortSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_port = true;
    } else {
      SDK_RPRINT(test, "TSUrlPortSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Path
  if (TSUrlPathSet(bufp1, url_loc1, path, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlPathSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    path_get = TSUrlPathGet(bufp1, url_loc1, &length);
    if (((path == NULL) && (path_get == NULL)) || (strncmp(path, path_get, length) == 0)) {
      SDK_RPRINT(test, "TSUrlPathSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_path = true;
    } else {
      SDK_RPRINT(test, "TSUrlPathSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Params
  if (TSUrlHttpParamsSet(bufp1, url_loc1, params, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlHttpParamsSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    params_get = TSUrlHttpParamsGet(bufp1, url_loc1, &length);
    if (((params == NULL) && (params_get == NULL)) || (strncmp(params, params_get, length) == 0)) {
      SDK_RPRINT(test, "TSUrlHttpParamsSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_params = true;
    } else {
      SDK_RPRINT(test, "TSUrlHttpParamsSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Query
  if (TSUrlHttpQuerySet(bufp1, url_loc1, query, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlHttpQuerySet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    query_get = TSUrlHttpQueryGet(bufp1, url_loc1, &length);
    if (((query == NULL) && (query_get == NULL)) || (strncmp(query, query_get, length) == 0)) {
      SDK_RPRINT(test, "TSUrlHttpQuerySet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_query = true;
    } else {
      SDK_RPRINT(test, "TSUrlHttpQuerySet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Fragments
  if (TSUrlHttpFragmentSet(bufp1, url_loc1, fragment, -1) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlHttpFragmentSet", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    fragment_get = TSUrlHttpFragmentGet(bufp1, url_loc1, &length);
    if (((fragment == NULL) && (fragment_get == NULL)) || (strncmp(fragment, fragment_get, length) == 0)) {
      SDK_RPRINT(test, "TSUrlHttpFragmentSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_fragment = true;
    } else {
      SDK_RPRINT(test, "TSUrlHttpFragmentSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }

  //Length
  url_length_from_1 = TSUrlLengthGet(bufp1, url_loc1);
  if (url_length_from_1 == url_expected_length) {
    SDK_RPRINT(test, "TSUrlLengthGet", "TestCase1", TC_PASS, "ok");
    test_passed_length1 = true;
  } else {
    SDK_RPRINT(test, "TSUrlLengthGet", "TestCase1", TC_FAIL, "Values don't match");
  }

  //String
  url_string_from_1 = TSUrlStringGet(bufp1, url_loc1, &tmp_len);
  if (strcmp(url_string_from_1, url_expected_string) == 0) {
    SDK_RPRINT(test, "TSUrlStringGet", "TestCase1", TC_PASS, "ok");
    test_passed_string1 = true;
  } else {
    SDK_RPRINT(test, "TSUrlStringGet", "TestCase1", TC_FAIL, "Values don't match");
  }

  //Copy
  bufp2 = TSMBufferCreate();
  if (TSUrlCreate(bufp2, &url_loc2) != TS_SUCCESS) {
    // Cannot proceed with tests.
    SDK_RPRINT(test, "TSUrlCreate", "TestCase2", TC_FAIL, "unable to create URL within buffer for TSUrlCopy.");
    goto print_results;
  }
  if (TSUrlCopy(bufp2, url_loc2, bufp1, url_loc1) == TS_ERROR) {
    SDK_RPRINT(test, "TSUrlCopy", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    //Length Test Case 2
    url_length_from_2 = TSUrlLengthGet(bufp2, url_loc2);
    if (url_length_from_2 == url_expected_length) {
      SDK_RPRINT(test, "TSUrlLengthGet", "TestCase2", TC_PASS, "ok");
      test_passed_length2 = true;
    } else {
      SDK_RPRINT(test, "TSUrlCopy", "TestCase1", TC_FAIL, "Values don't match");
    }


    //String Test Case 2
    url_string_from_2 = TSUrlStringGet(bufp2, url_loc2, &tmp_len);
    if (strcmp(url_string_from_2, url_expected_string) == 0) {
      SDK_RPRINT(test, "TSUrlStringGet", "TestCase2", TC_PASS, "ok");
      test_passed_string2 = true;
    } else {
      SDK_RPRINT(test, "TSUrlStringGet", "TestCase2", TC_FAIL, "Values don't match");
    }

    // Copy Test Case
    if (strcmp(url_string_from_1, url_string_from_2) == 0) {
      SDK_RPRINT(test, "TSUrlCopy", "TestCase1", TC_PASS, "ok");
      test_passed_copy = true;
    } else {
      SDK_RPRINT(test, "TSUrlCopy", "TestCase1", TC_FAIL, "Values Don't Match");
    }
  }

  //Clone
  bufp3 = TSMBufferCreate();
  if (TSUrlClone(bufp3, bufp1, url_loc1, &url_loc3) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlClone", "TestCase1", TC_FAIL, "Returned TS_ERROR");
  } else {
    //String Test Case 2
    url_string_from_3 = TSUrlStringGet(bufp3, url_loc3, &tmp_len);
    // Copy Test Case
    if (strcmp(url_string_from_1, url_string_from_3) == 0) {
      SDK_RPRINT(test, "TSUrlClone", "TestCase1", TC_PASS, "ok");
      test_passed_clone = true;
    } else {
      SDK_RPRINT(test, "TSUrlClone", "TestCase1", TC_FAIL, "Values Don't Match");
    }
  }

  //UrlPrint
  url_string_from_print = test_url_print(bufp1, url_loc1);
  if (url_string_from_print == NULL) {
    SDK_RPRINT(test, "TSUrlPrint", "TestCase1", TC_FAIL, "TSUrlPrint doesn't return TS_SUCCESS");
  } else {
    if (strcmp(url_string_from_print, url_expected_string) == 0) {
      SDK_RPRINT(test, "TSUrlPrint", "TestCase1", TC_PASS, "ok");
      test_passed_print = true;
    } else {
      SDK_RPRINT(test, "TSUrlPrint", "TestCase1", TC_FAIL, "TSUrlPrint doesn't return TS_SUCCESS");
    }
    TSfree(url_string_from_print);
  }

  if (TSUrlFtpTypeSet(bufp1, url_loc1, type) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlFtpTypeSet", "TestCase1", TC_FAIL, "TSUrlFtpTypeSet Returned TS_ERROR");
  } else {
    type_get = TSUrlFtpTypeGet(bufp1, url_loc1);
    if (type_get == type) {
      SDK_RPRINT(test, "TSUrlFtpTypeSet&Get", "TestCase1", TC_PASS, "ok");
      test_passed_type = true;
    } else {
      SDK_RPRINT(test, "TSUrlFtpTypeSet&Get", "TestCase1", TC_FAIL, "Values don't match");
    }
  }


  if ((TSUrlDestroy(bufp1, url_loc1) == TS_ERROR) &&
      (TSUrlDestroy(bufp2, url_loc2) == TS_ERROR) &&
      (TSUrlDestroy(bufp3, url_loc3) == TS_ERROR)) {
    SDK_RPRINT(test, "TSUrlCreate", "TestCase1&2", TC_PASS, "ok");
    SDK_RPRINT(test, "TSUrlDestroy", "TestCase1|2|3", TC_FAIL, "Returns TS_ERROR");
  } else {
    SDK_RPRINT(test, "TSUrlCreate", "TestCase1&2", TC_PASS, "ok");
    SDK_RPRINT(test, "TSUrlDestroy", "TestCase1&2&3", TC_PASS, "ok");
    TSHandleMLocRelease(bufp1, TS_NULL_MLOC, url_loc1);
    TSHandleMLocRelease(bufp2, TS_NULL_MLOC, url_loc2);
    TSHandleMLocRelease(bufp3, TS_NULL_MLOC, url_loc3);
    test_passed_create = true;
    test_passed_destroy = true;
  }

print_results:
  TSfree(url_expected_string);
  if (url_string_from_1 != NULL) {
    TSfree(url_string_from_1);
  }
  if (url_string_from_2 != NULL) {
    TSfree(url_string_from_2);
  }
  if (url_string_from_3 != NULL) {
    TSfree(url_string_from_3);
  }
  if (bufp1 != NULL) {
    TSMBufferDestroy(bufp1);
  }
  if (bufp2 != NULL) {
    TSMBufferDestroy(bufp2);
  }
  if (bufp3 != NULL) {
    TSMBufferDestroy(bufp3);
  }
  if ((test_passed_create == false) ||
      (test_passed_destroy == false) ||
      (test_passed_scheme == false) ||
      (test_passed_user == false) ||
      (test_passed_password == false) ||
      (test_passed_host == false) ||
      (test_passed_port == false) ||
      (test_passed_path == false) ||
      (test_passed_params == false) ||
      (test_passed_query == false) ||
      (test_passed_fragment == false) ||
      (test_passed_copy == false) ||
      (test_passed_clone == false) ||
      (test_passed_string1 == false) ||
      (test_passed_string2 == false) ||
      (test_passed_print == false) ||
      (test_passed_length1 == false) || (test_passed_length2 == false) || (test_passed_type == false)) {
        /*** Debugging the test itself....
	(test_passed_create == false)?printf("test_passed_create is false\n"):printf("");
	(test_passed_destroy == false)?printf("test_passed_destroy is false\n"):printf("");
	(test_passed_scheme == false)?printf("test_passed_scheme is false\n"):printf("");
	(test_passed_user == false)?printf("test_passed_user is false\n"):printf("");
	(test_passed_password == false)?printf("test_passed_password is false\n"):printf("");
	(test_passed_host == false)?printf("test_passed_host is false\n"):printf("");
	(test_passed_port == false)?printf("test_passed_port is false\n"):printf("");
	(test_passed_path == false)?printf("test_passed_path is false\n"):printf("");
	(test_passed_params == false)?printf("test_passed_params is false\n"):printf("");
	(test_passed_query == false)?printf("test_passed_query is false\n"):printf("");
	(test_passed_fragment == false)?printf("test_passed_fragment is false\n"):printf("");
	(test_passed_copy == false)?printf("test_passed_copy is false\n"):printf("");
	(test_passed_string1 == false)?printf("test_passed_string1 is false\n"):printf("");
	(test_passed_string2 == false)?printf("test_passed_string2 is false\n"):printf("");
	(test_passed_length1 == false)?printf("test_passed_length1 is false\n"):printf("");
	(test_passed_length2 == false)?printf("test_passed_length2 is false\n"):printf("");
	(test_passed_type == false)?printf("test_passed_type is false\n"):printf("");
	.....***********/
    *pstatus = REGRESSION_TEST_FAILED;
  } else {
    *pstatus = REGRESSION_TEST_PASSED;
  }
}

//////////////////////////////////////////////
//       SDK_API_TSHttpHdr
//
// Unit Test for API: TSHttpHdrCreate
//                    TSHttpHdrCopy
//                    TSHttpHdrClone
//                    TSHttpHdrDestroy
//                    TSHttpHdrLengthGet
//                    TSHttpHdrMethodGet
//                    TSHttpHdrMethodSet
//                    TSHttpHdrPrint
//                    TSHttpHdrReasonGet
//                    TSHttpHdrReasonLookup
//                    TSHttpHdrReasonSet
//                    TSHttpHdrStatusGet
//                    TSHttpHdrStatusSet
//                    TSHttpHdrTypeGet
//                    TSHttpHdrUrlGet
//                    TSHttpHdrUrlSet
//////////////////////////////////////////////

/**
 * If you change value of any constant in this function then reflect that change in variable expected_iobuf.
 */
REGRESSION_TEST(SDK_API_TSHttpHdr) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  TSMBuffer bufp1 = (TSMBuffer)NULL;
  TSMBuffer bufp2 = (TSMBuffer)NULL;
  TSMBuffer bufp3 = (TSMBuffer)NULL;
  TSMBuffer bufp4 = (TSMBuffer)NULL;

  TSMLoc hdr_loc1 = (TSMLoc)NULL;
  TSMLoc hdr_loc2 = (TSMLoc)NULL;
  TSMLoc hdr_loc3 = (TSMLoc)NULL;
  TSMLoc hdr_loc4 = (TSMLoc)NULL;

  TSHttpType hdr1type;
  TSHttpType hdr2type;

  const char *methodGet;

  TSMLoc url_loc;
  TSMLoc url_loc_Get;
  const char *url_host = "www.example.com";
  int url_port = 2345;
  const char *url_path = "abcd/efg/hij.htm";

  const char *response_reason = "aefa";
  const char *response_reason_get;

  TSHttpStatus status_get;

  int version_major = 2;
  int version_minor = 1;
  int version_get;

  /* TSHttpType type1; unused: lv */
  /* TSHttpType type2; unused: lv */
  const char *method1;
  const char *method2;
  int length1;
  int length2;
  TSMLoc url_loc1;
  TSMLoc url_loc2;
  /* int version1; unused: lv */
  /* int version2; unused: lv */

  int length;
  const char *expected_iobuf = "GET http://www.example.com:2345/abcd/efg/hij.htm HTTP/2.1\r\n\r\n";
  int actual_length;
  int expected_length;
  bool test_passed_Http_Hdr_Create = false;
  bool test_passed_Http_Hdr_Type = false;
  bool test_passed_Http_Hdr_Method = false;
  bool test_passed_Http_Hdr_Url = false;
  bool test_passed_Http_Hdr_Status = false;
  bool test_passed_Http_Hdr_Reason = false;
  bool test_passed_Http_Hdr_Reason_Lookup = false;
  bool test_passed_Http_Hdr_Version = false;
  bool test_passed_Http_Hdr_Copy = false;
  bool test_passed_Http_Hdr_Clone = false;
  bool test_passed_Http_Hdr_Length = false;
  bool test_passed_Http_Hdr_Print = false;
  bool test_passed_Http_Hdr_Destroy = false;
  bool try_print_function = true;
  bool test_buffer_created = true;


  *pstatus = REGRESSION_TEST_INPROGRESS;

  bufp1 = TSMBufferCreate();
  bufp2 = TSMBufferCreate();
  bufp3 = TSMBufferCreate();
  bufp4 = TSMBufferCreate();

  // Create
  if (test_buffer_created == true) {
    hdr_loc1 = TSHttpHdrCreate(bufp1);
    hdr_loc2 = TSHttpHdrCreate(bufp2);
    hdr_loc3 = TSHttpHdrCreate(bufp3);
    SDK_RPRINT(test, "TSHttpHdrCreate", "TestCase1&2&3", TC_PASS, "ok");
    test_passed_Http_Hdr_Create = true;
  } else {
    SDK_RPRINT(test, "TSHttpHdrCreate", "All Test Cases", TC_FAIL, "Cannot run test as unable to allocate MBuffers");
  }


  // Type
  if (test_passed_Http_Hdr_Create == true) {
    if ((TSHttpHdrTypeSet(bufp1, hdr_loc1, TS_HTTP_TYPE_REQUEST) == TS_ERROR) ||
        (TSHttpHdrTypeSet(bufp2, hdr_loc2, TS_HTTP_TYPE_RESPONSE) == TS_ERROR)) {
      SDK_RPRINT(test, "TSHttpHdrTypeSet", "TestCase1|2", TC_FAIL, "TSHttpHdrTypeSet returns TS_ERROR");
    } else {
      hdr1type = TSHttpHdrTypeGet(bufp1, hdr_loc1);
      hdr2type = TSHttpHdrTypeGet(bufp2, hdr_loc2);
      if ((hdr1type == TS_HTTP_TYPE_REQUEST) && (hdr2type == TS_HTTP_TYPE_RESPONSE)) {
        SDK_RPRINT(test, "TSHttpHdrTypeSet&Get", "TestCase1&2", TC_PASS, "ok");
        test_passed_Http_Hdr_Type = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrTypeSet&Get", "TestCase1&2", TC_FAIL, "Values mismatch");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrTypeSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header Creation Test failed");
  }

  // Method
  if (test_passed_Http_Hdr_Type == true) {
    if (TSHttpHdrMethodSet(bufp1, hdr_loc1, TS_HTTP_METHOD_GET, -1) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrMethodSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrMethodSet returns TS_ERROR");
    } else {
      methodGet = TSHttpHdrMethodGet(bufp1, hdr_loc1, &length);
      if ((strncmp(methodGet, TS_HTTP_METHOD_GET, length) == 0) && (length == (int) strlen(TS_HTTP_METHOD_GET))) {
        SDK_RPRINT(test, "TSHttpHdrMethodSet&Get", "TestCase1", TC_PASS, "ok");
        test_passed_Http_Hdr_Method = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrMethodSet&Get", "TestCase1", TC_FAIL, "Value's mismatch");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrMethodSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header's Type cannot be set");
  }

  // Url
  if (test_passed_Http_Hdr_Type == true) {
    if (TSUrlCreate(bufp1, &url_loc) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "TestCase1", TC_FAIL, "Cannot run test as TSUrlCreate returns TS_ERROR");
    } else {
      if (TSHttpHdrUrlSet(bufp1, hdr_loc1, url_loc) == TS_ERROR) {
        SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrUrlSet returns TS_ERROR");
      } else {
        if (TSHttpHdrUrlGet(bufp1, hdr_loc1, &url_loc_Get) != TS_SUCCESS) {
          SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrUrlGet retuns TS_ERROR");
        } else {
          if (url_loc == url_loc_Get) {
            SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "TestCase1", TC_PASS, "ok");
            test_passed_Http_Hdr_Url = true;
          } else {
            SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "TestCase1", TC_FAIL, "Value's mismatch");
          }
          if (TSHandleMLocRelease(bufp1, hdr_loc1, url_loc_Get) == TS_ERROR) {
            SDK_RPRINT(test, "TSHandleMLocRelease", "", TC_FAIL, "Unable to release handle to URL");
          }
        }
      }

      // Fill up the URL for Copy Test Case.
      if (TSUrlSchemeSet(bufp1, url_loc, TS_URL_SCHEME_HTTP, -1) == TS_ERROR) {
        SDK_RPRINT(test, "TSUrlSchemeSet", "", TC_FAIL, "Unable to set scheme in URL in the HTTP Header");
        try_print_function = false;
      }
      if (TSUrlHostSet(bufp1, url_loc, url_host, -1) == TS_ERROR) {
        SDK_RPRINT(test, "TSUrlHostSet", "", TC_FAIL, "Unable to set host in URL in the HTTP Header");
        try_print_function = false;
      }
      if (TSUrlPortSet(bufp1, url_loc, url_port) == TS_ERROR) {
        SDK_RPRINT(test, "TSUrlPortSet", "", TC_FAIL, "Unable to set port in URL in the HTTP Header");
        try_print_function = false;
      }
      if (TSUrlPathSet(bufp1, url_loc, url_path, -1) == TS_ERROR) {
        SDK_RPRINT(test, "TSUrlPathSet", "", TC_FAIL, "Unable to set path in URL in the HTTP Header");
        try_print_function = false;
      }
      if (TSHandleMLocRelease(bufp1, hdr_loc1, url_loc) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "", TC_FAIL, "Unable to release handle to URL");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrUrlSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header's Type cannot be set");
  }

  // Reason
  if (test_passed_Http_Hdr_Type == true) {
    if (TSHttpHdrReasonSet(bufp2, hdr_loc2, response_reason, -1) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrReasonSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrReasonSet returns TS_ERROR");
    } else {
      response_reason_get = TSHttpHdrReasonGet(bufp2, hdr_loc2, &length);
      if ((strncmp(response_reason_get, response_reason, length) == 0) && (length == (int) strlen(response_reason))) {
        SDK_RPRINT(test, "TSHttpHdrReasonSet&Get", "TestCase1", TC_PASS, "ok");
        test_passed_Http_Hdr_Reason = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrReasonSet&Get", "TestCase1", TC_FAIL, "Value's mismatch");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrReasonSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header's Type cannot be set");
  }

  // Status
  if (test_passed_Http_Hdr_Type == true) {
    if (TSHttpHdrStatusSet(bufp2, hdr_loc2, TS_HTTP_STATUS_OK) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrStatusSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrStatusSet returns TS_ERROR");
    } else {
      status_get = TSHttpHdrStatusGet(bufp2, hdr_loc2);
      if (status_get == TS_HTTP_STATUS_OK) {
        SDK_RPRINT(test, "TSHttpHdrStatusSet&Get", "TestCase1", TC_PASS, "ok");
        test_passed_Http_Hdr_Status = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrStatusSet&Get", "TestCase1", TC_FAIL, "Value's mismatch");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrStatusSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header's Type cannot be set");
  }

  //Version
  if (test_passed_Http_Hdr_Type == true) {
    if (TSHttpHdrVersionSet(bufp1, hdr_loc1, TS_HTTP_VERSION(version_major, version_minor)) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase1", TC_FAIL, "TSHttpHdrVersionSet returns TS_ERROR");
    } else {
      version_get = TSHttpHdrVersionGet(bufp1, hdr_loc1);
      if ((version_major == TS_HTTP_MAJOR(version_get)) && (version_minor == TS_HTTP_MINOR(version_get))) {
        SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase1", TC_PASS, "ok");
        test_passed_Http_Hdr_Version = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase1", TC_FAIL, "Value's mismatch");
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "All Test Case", TC_FAIL, "Cannot run test as Header's Type cannot be set");
  }

  if (test_passed_Http_Hdr_Version == true) {
    if (TSHttpHdrVersionSet(bufp2, hdr_loc2, TS_HTTP_VERSION(version_major, version_minor)) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase2", TC_FAIL, "TSHttpHdrVersionSet returns TS_ERROR");
      test_passed_Http_Hdr_Version = false;
    } else {
      version_get = TSHttpHdrVersionGet(bufp2, hdr_loc2);
      if ((version_major == TS_HTTP_MAJOR(version_get)) && (version_minor == TS_HTTP_MINOR(version_get))) {
        SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase2", TC_PASS, "ok");
      } else {
        SDK_RPRINT(test, "TSHttpHdrVersionSet&Get", "TestCase2", TC_FAIL, "Value's mismatch");
        test_passed_Http_Hdr_Version = false;
      }
    }
  }
  //Reason Lookup
  if (strcmp("None", TSHttpHdrReasonLookup(TS_HTTP_STATUS_NONE)) != 0) {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase1", TC_FAIL,
               "TSHttpHdrReasonLookup returns Value's mismatch");
  } else {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase1", TC_PASS, "ok");
    test_passed_Http_Hdr_Reason_Lookup = true;
  }

  if (strcmp("Ok", TSHttpHdrReasonLookup(TS_HTTP_STATUS_OK)) != 0) {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase2", TC_FAIL,
               "TSHttpHdrReasonLookup returns Value's mismatch");
    if (test_passed_Http_Hdr_Reason_Lookup == true) {
      test_passed_Http_Hdr_Reason_Lookup = false;
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase2", TC_PASS, "ok");
  }

  if (strcmp("Continue", TSHttpHdrReasonLookup(TS_HTTP_STATUS_CONTINUE)) != 0) {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase3", TC_FAIL,
               "TSHttpHdrReasonLookup returns Value's mismatch");
    if (test_passed_Http_Hdr_Reason_Lookup == true) {
      test_passed_Http_Hdr_Reason_Lookup = false;
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase3", TC_PASS, "ok");
  }

  if (strcmp("Not Modified", TSHttpHdrReasonLookup(TS_HTTP_STATUS_NOT_MODIFIED)) != 0) {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase2", TC_FAIL,
               "TSHttpHdrReasonLookup returns Value's mismatch");
    if (test_passed_Http_Hdr_Reason_Lookup == true) {
      test_passed_Http_Hdr_Reason_Lookup = false;
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrReasonLookup", "TestCase4", TC_PASS, "ok");
  }

  // Copy
  if (test_passed_Http_Hdr_Create == true) {
    if (TSHttpHdrCopy(bufp3, hdr_loc3, bufp1, hdr_loc1) == TS_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "TSHttpHdrCopy returns TS_ERROR");
    } else {

      bool flag = true;
      // Check the type
      if (flag == true) {
        TSHttpType type1 = TSHttpHdrTypeGet(bufp1, hdr_loc1);
        TSHttpType type2 = TSHttpHdrTypeGet(bufp3, hdr_loc3);

        if (type1 != type2) {
          SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Type mismatch in both headers");
          flag = false;
        }
      }
      // Check the Version
      if (flag == true) {
        int version1 = TSHttpHdrVersionGet(bufp1, hdr_loc1);
        int version2 = TSHttpHdrVersionGet(bufp3, hdr_loc3);

        if (version1 != version2) {
          SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Version mismatch in both headers");
          flag = false;
        }
      }
      // Check the Method
      if (flag == true) {
        method1 = TSHttpHdrMethodGet(bufp1, hdr_loc1, &length1);
        method2 = TSHttpHdrMethodGet(bufp3, hdr_loc3, &length2);
        if ((length1 != length2) || (strncmp(method1, method2, length1) != 0)) {
          SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Method mismatch in both headers");
          flag = false;
        }
      }
      // Check the URL
      if (flag == true) {

        if ((TSHttpHdrUrlGet(bufp1, hdr_loc1, &url_loc1) != TS_SUCCESS) ||
            (TSHttpHdrUrlGet(bufp3, hdr_loc3, &url_loc2) != TS_SUCCESS)) {
          SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "TSHttpVersionGet returns TS_ERROR");
          flag = false;
        } else {
          const char *scheme1;
          const char *scheme2;

          const char *host1;
          const char *host2;

          int port1;
          int port2;

          const char *path1;
          const char *path2;

          // URL Scheme
          scheme1 = TSUrlSchemeGet(bufp1, url_loc1, &length1);
          scheme2 = TSUrlSchemeGet(bufp3, url_loc2, &length2);
          if ((length1 != length2) || (strncmp(scheme1, scheme2, length1) != 0)) {
            SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Scheme has different values in both headers");
            flag = false;
          }

          // URL Host
          if (flag == true) {
            host1 = TSUrlHostGet(bufp1, url_loc1, &length1);
            host2 = TSUrlHostGet(bufp3, url_loc2, &length2);
            if ((length1 != length2) || (strncmp(host1, host2, length1) != 0)) {
              SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Host has different values in both headers");
              flag = false;
            }
          }
          // URL Port
          if (flag == true) {
            port1 = TSUrlPortGet(bufp1, url_loc1);
            port2 = TSUrlPortGet(bufp3, url_loc2);
            if (port1 != port2) {
              SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Port has different values in both headers");
              flag = false;
            }
          }
          // URL Path
          if (flag == true) {
            path1 = TSUrlPathGet(bufp1, url_loc1, &length1);
            path2 = TSUrlPathGet(bufp3, url_loc2, &length2);
            if ((path1 != NULL) && (path2 != NULL)) {
              if ((length1 != length2) || (strncmp(path1, path2, length1) != 0)) {
                SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Path has different values in both headers");
                flag = false;
              }
            } else {
              if (path1 != path2) {
                SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Host has different values in both headers");
                flag = false;
              }
            }
            if ((TSHandleMLocRelease(bufp1, hdr_loc1, url_loc1) == TS_ERROR) ||
                (TSHandleMLocRelease(bufp3, hdr_loc3, url_loc2) == TS_ERROR)) {
              SDK_RPRINT(test, "TSHandleMLocRelease", "", TC_FAIL, "Unable to release Handle acquired by TSHttpHdrUrlGet");
            }
          }

          if (flag == true) {
            SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_PASS, "ok");
            test_passed_Http_Hdr_Copy = true;
          }
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrCopy", "All Test Cases", TC_PASS, "Cannot run test as TSHttpHdrCreate has failed");
  }

  // Clone
  if (test_passed_Http_Hdr_Create == true) {
    if (TSHttpHdrClone(bufp4, bufp1, hdr_loc1, &hdr_loc4) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "TSHttpHdrClone returns TS_ERROR");
    } else {

      bool flag = true;
      // Check the type
      if (flag == true) {
        TSHttpType type1 = TSHttpHdrTypeGet(bufp1, hdr_loc1);
        TSHttpType type2 = TSHttpHdrTypeGet(bufp4, hdr_loc4);

        if (type1 != type2) {
          SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Type mismatch in both headers");
          flag = false;
        }
      }
      // Check the Version
      if (flag == true) {
        int version1 = TSHttpHdrVersionGet(bufp1, hdr_loc1);
        int version2 = TSHttpHdrVersionGet(bufp4, hdr_loc4);

        if (version1 != version2) {
          SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Version mismatch in both headers");
          flag = false;
        }
      }
      // Check the Method
      if (flag == true) {
        method1 = TSHttpHdrMethodGet(bufp1, hdr_loc1, &length1);
        method2 = TSHttpHdrMethodGet(bufp4, hdr_loc4, &length2);
        if ((length1 != length2) || (strncmp(method1, method2, length1) != 0)) {
          SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Method mismatch in both headers");
          flag = false;
        }
      }
      // Check the URL
      if (flag == true) {

        if ((TSHttpHdrUrlGet(bufp1, hdr_loc1, &url_loc1) != TS_SUCCESS) ||
            (TSHttpHdrUrlGet(bufp4, hdr_loc4, &url_loc2) != TS_SUCCESS)) {
          SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "TSHttpVersionGet returns TS_ERROR");
          flag = false;
        } else {
          const char *scheme1;
          const char *scheme2;

          const char *host1;
          const char *host2;

          int port1;
          int port2;

          const char *path1;
          const char *path2;

          // URL Scheme
          scheme1 = TSUrlSchemeGet(bufp1, url_loc1, &length1);
          scheme2 = TSUrlSchemeGet(bufp4, url_loc2, &length2);
          if ((length1 != length2) || (strncmp(scheme1, scheme2, length1) != 0)) {
            SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Url Scheme has different values in both headers");
            flag = false;
          }

          // URL Host
          if (flag == true) {
            host1 = TSUrlHostGet(bufp1, url_loc1, &length1);
            host2 = TSUrlHostGet(bufp4, url_loc2, &length2);
            if ((length1 != length2) || (strncmp(host1, host2, length1) != 0)) {
              SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Url Host has different values in both headers");
              flag = false;
            }
          }
          // URL Port
          if (flag == true) {
            port1 = TSUrlPortGet(bufp1, url_loc1);
            port2 = TSUrlPortGet(bufp4, url_loc2);
            if (port1 != port2) {
              SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_FAIL, "Url Port has different values in both headers");
              flag = false;
            }
          }
          // URL Path
          if (flag == true) {
            path1 = TSUrlPathGet(bufp1, url_loc1, &length1);
            path2 = TSUrlPathGet(bufp4, url_loc2, &length2);
            if ((path1 != NULL) && (path2 != NULL)) {
              if ((length1 != length2) || (strncmp(path1, path2, length1) != 0)) {
                SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Path has different values in both headers");
                flag = false;
              }
            } else {
              if (path1 != path2) {
                SDK_RPRINT(test, "TSHttpHdrCopy", "TestCase1", TC_FAIL, "Url Host has different values in both headers");
                flag = false;
              }
            }
            if ((TSHandleMLocRelease(bufp1, hdr_loc1, url_loc1) == TS_ERROR) ||
                (TSHandleMLocRelease(bufp4, hdr_loc4, url_loc2) == TS_ERROR)) {
              SDK_RPRINT(test, "TSHandleMLocRelease", "", TC_FAIL, "Unable to release Handle acquired by TSHttpHdrUrlGet");
            }
          }

          if (flag == true) {
            SDK_RPRINT(test, "TSHttpHdrClone", "TestCase1", TC_PASS, "ok");
            test_passed_Http_Hdr_Clone = true;
          }
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrClone", "All Test Cases", TC_PASS, "Cannot run test as TSHttpHdrCreate has failed");
  }


  //LengthGet
  if (test_passed_Http_Hdr_Create == true) {
    actual_length = TSHttpHdrLengthGet(bufp1, hdr_loc1);
    TSIOBuffer iobuf = TSIOBufferCreate();
    TSHttpHdrPrint(bufp1, hdr_loc1, iobuf);
    TSIOBufferReader iobufreader = TSIOBufferReaderAlloc(iobuf);

    expected_length = TSIOBufferReaderAvail(iobufreader);
    if (actual_length == expected_length) {
      SDK_RPRINT(test, "TSHttpHdrLengthGet", "TestCase1", TC_PASS, "ok");
      test_passed_Http_Hdr_Length = true;
    } else {
      SDK_RPRINT(test, "TSHttpHdrLengthGet", "TestCase1", TC_FAIL, "Incorrect value returned.");
    }

    // Print.
    if ((test_passed_Http_Hdr_Method == true) && (test_passed_Http_Hdr_Url == true) &&
        (test_passed_Http_Hdr_Version == true) && (test_passed_Http_Hdr_Length == true) &&
        (try_print_function == true)) {
      char *actual_iobuf = NULL;

      actual_iobuf = (char *) TSmalloc((actual_length + 1) * sizeof(char));

      if (actual_iobuf == NULL) {
        SDK_RPRINT(test, "TSHttpHdrPrint", "TestCase1", TC_FAIL, "Unable to allocate memory");
      } else {

        TSIOBufferBlock iobufblock;
        int64_t bytes_read;

        memset(actual_iobuf, 0, (actual_length + 1) * sizeof(char));
        bytes_read = 0;

        iobufblock = TSIOBufferReaderStart(iobufreader);

        while (iobufblock != NULL) {
          const char *block_start;
          int64_t block_size;

          block_start = TSIOBufferBlockReadStart(iobufblock, iobufreader, &block_size);
          if (block_size <= 0)
            break;

          memcpy(actual_iobuf + bytes_read, block_start, block_size);
          bytes_read += block_size;
          TSIOBufferReaderConsume(iobufreader, block_size);
          iobufblock = TSIOBufferReaderStart(iobufreader);
        }
        if (strcmp(actual_iobuf, expected_iobuf) == 0) {
          SDK_RPRINT(test, "TSHttpHdrPrint", "TestCase1", TC_PASS, "ok");
          test_passed_Http_Hdr_Print = true;
        } else {
          SDK_RPRINT(test, "TSHttpHdrPrint", "TestCase1", TC_FAIL, "Value's mismatch");
        }

        TSfree(actual_iobuf);
        TSIOBufferReaderFree(iobufreader);
        TSIOBufferDestroy(iobuf);
      }
    } else {
      SDK_RPRINT(test, "TSHttpHdrPrint", "TestCase1", TC_FAIL, "Unable to run test for TSHttpHdrPrint");
    }
  } else {
    SDK_RPRINT(test, "TSHttpHdrLengthGet", "All Test Cases", TC_PASS, "Cannot run test as TSHttpHdrCreate has failed");
  }

  // Destroy
  if (test_passed_Http_Hdr_Create == true) {
    TSHttpHdrDestroy(bufp1, hdr_loc1);
    TSHttpHdrDestroy(bufp2, hdr_loc2);
    TSHttpHdrDestroy(bufp3, hdr_loc3);
    TSHttpHdrDestroy(bufp4, hdr_loc4);
    if ((TSHandleMLocRelease(bufp1, TS_NULL_MLOC, hdr_loc1) == TS_ERROR) ||
        (TSHandleMLocRelease(bufp2, TS_NULL_MLOC, hdr_loc2) == TS_ERROR) ||
        (TSHandleMLocRelease(bufp3, TS_NULL_MLOC, hdr_loc3) == TS_ERROR) ||
        (TSHandleMLocRelease(bufp4, TS_NULL_MLOC, hdr_loc4) == TS_ERROR)) {
      SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase1|2|3|4", TC_FAIL, "Unable to release the handle to headers");
    }
    SDK_RPRINT(test, "TSHttpHdrDestroy", "TestCase1&2&3&4", TC_PASS, "ok");
    test_passed_Http_Hdr_Destroy = true;
  } else {
    SDK_RPRINT(test, "TSHttpHdrDestroy", "All Test Cases", TC_FAIL, "Cannot run test as header was not created");
  }

  if (bufp1) {
    if (TSMBufferDestroy(bufp1) == TS_ERROR) {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase1", TC_FAIL, "Unable to destroy MBuffer");
    }
  }

  if (bufp2) {
    if (TSMBufferDestroy(bufp2) == TS_ERROR) {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase2", TC_FAIL, "Unable to destroy MBuffer");
    }
  }

  if (bufp3) {
    if (TSMBufferDestroy(bufp3) == TS_ERROR) {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase3", TC_FAIL, "Unable to destroy MBuffer");
    }
  }

  if (bufp4) {
    if (TSMBufferDestroy(bufp4) == TS_ERROR) {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase4", TC_FAIL, "Unable to destroy MBuffer");
    }
  }

  if ((test_passed_Http_Hdr_Create == true) &&
      (test_passed_Http_Hdr_Type == true) &&
      (test_passed_Http_Hdr_Method == true) &&
      (test_passed_Http_Hdr_Url == true) &&
      (test_passed_Http_Hdr_Status == true) &&
      (test_passed_Http_Hdr_Reason == true) &&
      (test_passed_Http_Hdr_Reason_Lookup == true) &&
      (test_passed_Http_Hdr_Version == true) &&
      (test_passed_Http_Hdr_Copy == true) &&
      (test_passed_Http_Hdr_Clone == true) &&
      (test_passed_Http_Hdr_Length == true) &&
      (test_passed_Http_Hdr_Print == true) && (test_passed_Http_Hdr_Destroy == true)) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

  return;

}


//////////////////////////////////////////////
//       SDK_API_TSMimeHdrField
//
// Unit Test for API: TSMBufferCreate
//                    TSMBufferDestroy
//                    TSMimeHdrCreate
//                    TSMimeHdrDestroy
//                    TSMimeHdrFieldCreate
//                    TSMimeHdrFieldDestroy
//                    TSMimeHdrFieldFind
//                    TSMimeHdrFieldGet
//                    TSMimeHdrFieldAppend
//                    TSMimeHdrFieldNameGet
//                    TSMimeHdrFieldNameSet
//                    TSMimeHdrFieldNext
//                    TSMimeHdrFieldsClear
//                    TSMimeHdrFieldsCount
//                    TSMimeHdrFieldValueAppend
//                    TSMimeHdrFieldValueDelete
//                    TSMimeHdrFieldValueStringGet
//                    TSMimeHdrFieldValueDateGet
//                    TSMimeHdrFieldValueIntGet
//                    TSMimeHdrFieldValueUintGet
//                    TSMimeHdrFieldValueStringInsert
//                    TSMimeHdrFieldValueDateInsert
//                    TSMimeHdrFieldValueIntInsert
//                    TSMimeHdrFieldValueUintInsert
//                    TSMimeHdrFieldValuesClear
//                    TSMimeHdrFieldValuesCount
//                    TSMimeHdrFieldValueStringSet
//                    TSMimeHdrFieldValueDateSet
//                    TSMimeHdrFieldValueIntSet
//                    TSMimeHdrFieldValueUintSet
//                    TSMimeHdrLengthGet
//                    TSMimeHdrPrint
//////////////////////////////////////////////

TSReturnCode
compare_field_names(RegressionTest * test, TSMBuffer bufp1, TSMLoc mime_loc1, TSMLoc field_loc1, TSMBuffer bufp2,
                    TSMLoc mime_loc2, TSMLoc field_loc2)
{
  NOWARN_UNUSED(test);
  const char *name1;
  const char *name2;
  int length1;
  int length2;

  name1 = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc1, &length1);
  name2 = TSMimeHdrFieldNameGet(bufp2, mime_loc2, field_loc2, &length2);

  if ((length1 == length2) && (strncmp(name1, name2, length1) == 0) ) {
    return TS_SUCCESS;
  } else {
    return TS_ERROR;
  }
}

REGRESSION_TEST(SDK_API_TSMimeHdrField) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  TSMBuffer bufp1 = (TSMBuffer)NULL;

  TSMLoc mime_loc1 = (TSMLoc)NULL;

  TSMLoc field_loc11 = (TSMLoc)NULL;
  TSMLoc field_loc12 = (TSMLoc)NULL;
  TSMLoc field_loc13 = (TSMLoc)NULL;
  TSMLoc field_loc14 = (TSMLoc)NULL;
  TSMLoc field_loc15 = (TSMLoc)NULL;


  const char *field1Name = "field1";
  const char *field2Name = "field2";
  const char *field3Name = "field3";
  const char *field4Name = "field4";
  const char *field5Name = "field5";

  const char *field1NameGet;
  const char *field2NameGet;
  const char *field3NameGet;
  const char *field4NameGet;
  const char *field5NameGet;

  int field1NameGetLength;
  int field2NameGetLength;
  int field3NameGetLength;
  int field4NameGetLength;
  int field5NameGetLength;

  int field1_length;
  int field2_length;
  int field3_length;
  int field4_length;
  /* int field5_length; unused: lv */

  TSMLoc test_field_loc11 = (TSMLoc)NULL;
  TSMLoc test_field_loc12 = (TSMLoc)NULL;
  TSMLoc test_field_loc13 = (TSMLoc)NULL;
  TSMLoc test_field_loc14 = (TSMLoc)NULL;
  TSMLoc test_field_loc15 = (TSMLoc)NULL;

  int actualNumberOfFields;
  int numberOfFields;

  const char *field1Value1 = "field1Value1";
  const char *field1Value2 = "field1Value2";
  const char *field1Value3 = "field1Value3";
  const char *field1Value4 = "field1Value4";
  const char *field1Value5 = "field1Value5";
  const char *field1ValueNew = "newfieldValue";

  const char *field1Value1Get;
  const char *field1Value2Get;
  const char *field1Value3Get;
  const char *field1Value4Get;
  const char *field1Value5Get;
  const char *field1ValueNewGet;

  int lengthField1Value1;
  int lengthField1Value2;
  int lengthField1Value3;
  int lengthField1Value4;
  int lengthField1Value5;
  int lengthField1ValueNew;

  time_t field2Value1 = time(NULL);
  time_t field2Value1Get;
  time_t field2ValueNew;
  time_t field2ValueNewGet;

  int field3Value1 = 31;
  int field3Value2 = 32;
  int field3Value3 = 33;
  int field3Value4 = 34;
  int field3Value5 = 35;
  int field3ValueNew = 30;

  int field3Value1Get;
  int field3Value2Get;
  int field3Value3Get;
  int field3Value4Get;
  int field3Value5Get;
  int field3ValueNewGet;

  unsigned int field4Value1 = 41;
  unsigned int field4Value2 = 42;
  unsigned int field4Value3 = 43;
  unsigned int field4Value4 = 44;
  unsigned int field4Value5 = 45;
  unsigned int field4ValueNew = 40;

  unsigned int field4Value1Get;
  unsigned int field4Value2Get;
  unsigned int field4Value3Get;
  unsigned int field4Value4Get;
  unsigned int field4Value5Get;
  unsigned int field4ValueNewGet;

  const char *field5Value1 = "field5Value1";
  const char *field5Value1Append = "AppendedValue";
  const char *fieldValueAppendGet;
  int lengthFieldValueAppended;
  int field5Value2 = 52;
  const char *field5Value3 = "DeleteValue";
  const char *fieldValueDeleteGet;
  int lengthFieldValueDeleteGet;
  unsigned int field5Value4 = 54;
  int numberOfValueInField;

  TSMLoc field_loc;

  bool test_passed_MBuffer_Create = false;
  bool test_passed_Mime_Hdr_Create = false;
  bool test_passed_Mime_Hdr_Field_Create = false;
  bool test_passed_Mime_Hdr_Field_Name = false;
  bool test_passed_Mime_Hdr_Field_Append = false;
  bool test_passed_Mime_Hdr_Field_Get = false;
  bool test_passed_Mime_Hdr_Field_Next = false;
  bool test_passed_Mime_Hdr_Fields_Count = false;
  bool test_passed_Mime_Hdr_Field_Value_String_Insert = false;
  bool test_passed_Mime_Hdr_Field_Value_String_Get = false;
  bool test_passed_Mime_Hdr_Field_Value_String_Set = false;
  bool test_passed_Mime_Hdr_Field_Value_Date_Insert = false;
  bool test_passed_Mime_Hdr_Field_Value_Date_Get = false;
  bool test_passed_Mime_Hdr_Field_Value_Date_Set = false;
  bool test_passed_Mime_Hdr_Field_Value_Int_Insert = false;
  bool test_passed_Mime_Hdr_Field_Value_Int_Get = false;
  bool test_passed_Mime_Hdr_Field_Value_Int_Set = false;
  bool test_passed_Mime_Hdr_Field_Value_Uint_Insert = false;
  bool test_passed_Mime_Hdr_Field_Value_Uint_Get = false;
  bool test_passed_Mime_Hdr_Field_Value_Uint_Set = false;
  bool test_passed_Mime_Hdr_Field_Value_Append = false;
  bool test_passed_Mime_Hdr_Field_Value_Delete = false;
  bool test_passed_Mime_Hdr_Field_Values_Clear = false;
  bool test_passed_Mime_Hdr_Field_Values_Count = false;
  bool test_passed_Mime_Hdr_Field_Destroy = false;
  bool test_passed_Mime_Hdr_Fields_Clear = false;
  bool test_passed_Mime_Hdr_Destroy = false;
  bool test_passed_MBuffer_Destroy = false;
  bool test_passed_Mime_Hdr_Field_Length_Get = false;

  *pstatus = REGRESSION_TEST_INPROGRESS;

  // TSMBufferCreate
  bufp1 = TSMBufferCreate();
  SDK_RPRINT(test, "TSMBufferCreate", "TestCase1", TC_PASS, "ok");
  test_passed_MBuffer_Create = true;

  // TSMimeHdrCreate
  if (test_passed_MBuffer_Create == true) {
    if (TSMimeHdrCreate(bufp1, &mime_loc1) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrCreate", "TestCase1", TC_FAIL, "TSMimeHdrCreate Returns TS_ERROR");
    } else {
      SDK_RPRINT(test, "TSMimeHdrCreate", "TestCase1", TC_PASS, "ok");
      test_passed_Mime_Hdr_Create = true;
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrCreate", "TestCase1", TC_FAIL, "Cannot run test as Test for TSMBufferCreate Failed");
  }

  // TSMimeHdrFieldCreate
  if (test_passed_Mime_Hdr_Create == true) {
    if ((TSMimeHdrFieldCreate(bufp1, mime_loc1, &field_loc11) != TS_SUCCESS) ||
        (TSMimeHdrFieldCreate(bufp1, mime_loc1, &field_loc12) != TS_SUCCESS) ||
        (TSMimeHdrFieldCreate(bufp1, mime_loc1, &field_loc13) != TS_SUCCESS) ||
        (TSMimeHdrFieldCreate(bufp1, mime_loc1, &field_loc14) != TS_SUCCESS) ||
        (TSMimeHdrFieldCreate(bufp1, mime_loc1, &field_loc15) != TS_SUCCESS)) {
      SDK_RPRINT(test, "TSMimeHdrFieldCreate", "TestCase1|2|3|4|5", TC_FAIL, "TSMimeHdrFieldCreate Returns TS_ERROR");
    } else {
      SDK_RPRINT(test, "TSMimeHdrFieldCreate", "TestCase1|2|3|4|5", TC_PASS, "ok");
      test_passed_Mime_Hdr_Field_Create = true;
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldCreate", "All Test Case", TC_FAIL, "Cannot run test as Test for TSMimeHdrCreate Failed");
  }


  //TSMimeHdrFieldNameGet&Set
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((TSMimeHdrFieldNameSet(bufp1, mime_loc1, field_loc11, field1Name, -1) == TS_ERROR) ||
        (TSMimeHdrFieldNameSet(bufp1, mime_loc1, field_loc12, field2Name, -1) == TS_ERROR) ||
        (TSMimeHdrFieldNameSet(bufp1, mime_loc1, field_loc13, field3Name, -1) == TS_ERROR) ||
        (TSMimeHdrFieldNameSet(bufp1, mime_loc1, field_loc14, field4Name, -1) == TS_ERROR) ||
        (TSMimeHdrFieldNameSet(bufp1, mime_loc1, field_loc15, field5Name, -1) == TS_ERROR)) {
      SDK_RPRINT(test, "TSMimeHdrFieldNameSet", "TestCase1|2|3|4|5", TC_FAIL, "TSMimeHdrFieldNameSet Returns TS_ERROR");
    } else {
      field1NameGet = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc11, &field1NameGetLength);
      field2NameGet = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc12, &field2NameGetLength);
      field3NameGet = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc13, &field3NameGetLength);
      field4NameGet = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc14, &field4NameGetLength);
      field5NameGet = TSMimeHdrFieldNameGet(bufp1, mime_loc1, field_loc15, &field5NameGetLength);
      if (((strncmp(field1NameGet, field1Name, field1NameGetLength) == 0) && (field1NameGetLength == (int) strlen(field1Name))) &&
          ((strncmp(field2NameGet, field2Name, field2NameGetLength) == 0) && (field2NameGetLength == (int) strlen(field2Name))) &&
          ((strncmp(field3NameGet, field3Name, field3NameGetLength) == 0) && (field3NameGetLength == (int) strlen(field3Name))) &&
          ((strncmp(field4NameGet, field4Name, field4NameGetLength) == 0) && (field4NameGetLength == (int) strlen(field4Name))) &&
          ((strncmp(field5NameGet, field5Name, field5NameGetLength) == 0) && field5NameGetLength == (int) strlen(field5Name))) {
        SDK_RPRINT(test, "TSMimeHdrFieldNameGet&Set", "TestCase1&2&3&4&5", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Name = true;
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldNameGet&Set", "TestCase1|2|3|4|5", TC_FAIL, "Values Don't Match");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldNameGet&Set", "All Test Case", TC_FAIL,
               "Cannot run test as Test for TSMBufferFieldCreate Failed");
  }


  // TSMimeHdrFieldAppend, TSMimeHdrFieldGet, TSMimeHdrFieldNext
  if (test_passed_Mime_Hdr_Field_Name == true) {
    if ((TSMimeHdrFieldAppend(bufp1, mime_loc1, field_loc11) != TS_SUCCESS) ||
        (TSMimeHdrFieldAppend(bufp1, mime_loc1, field_loc12) != TS_SUCCESS) ||
        (TSMimeHdrFieldAppend(bufp1, mime_loc1, field_loc13) != TS_SUCCESS) ||
        (TSMimeHdrFieldAppend(bufp1, mime_loc1, field_loc14) != TS_SUCCESS) ||
        (TSMimeHdrFieldAppend(bufp1, mime_loc1, field_loc15) != TS_SUCCESS)) {
      SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase1|2|3|4|5", TC_FAIL, "TSMimeHdrFieldAppend Returns TS_ERROR");
    } else {
      if (TS_NULL_MLOC == (test_field_loc11 = TSMimeHdrFieldGet(bufp1, mime_loc1, 0))) {
        SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase1|2|3|4|5", TC_FAIL, "TSMimeHdrFieldGet Returns TS_NULL_MLOC");
        SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase1", TC_FAIL,
                   "Cannot Test TSMimeHdrFieldNext as TSMimeHdrFieldGet Returns TS_NULL_MLOC");
        SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase1", TC_FAIL, "TSMimeHdrFieldGet Returns TS_NULL_MLOC");
      } else {
        if (compare_field_names(test, bufp1, mime_loc1, field_loc11, bufp1, mime_loc1, test_field_loc11) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase1", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase1", TC_FAIL, "Cannot Test TSMimeHdrFieldNext as Values don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase1", TC_FAIL, "Values Don't match");
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase1", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Field_Append = true;
          test_passed_Mime_Hdr_Field_Get = true;
        }
      }

      if (test_passed_Mime_Hdr_Field_Append == true) {
        test_field_loc12 = TSMimeHdrFieldNext(bufp1, mime_loc1, test_field_loc11);
        if (compare_field_names(test, bufp1, mime_loc1, field_loc12, bufp1, mime_loc1, test_field_loc12) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase2", TC_PASS, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase2", TC_PASS, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase2", TC_PASS, "Values Don't match");
          test_passed_Mime_Hdr_Field_Append = false;
          test_passed_Mime_Hdr_Field_Next = false;
          test_passed_Mime_Hdr_Field_Get = false;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase2", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase2", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase2", TC_PASS, "ok");
          test_passed_Mime_Hdr_Field_Next = true;
        }
      }

      if (test_passed_Mime_Hdr_Field_Append == true) {
        test_field_loc13 = TSMimeHdrFieldNext(bufp1, mime_loc1, test_field_loc12);
        if (compare_field_names(test, bufp1, mime_loc1, field_loc13, bufp1, mime_loc1, test_field_loc13) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase3", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase3", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase3", TC_FAIL, "Values Don't match");
          test_passed_Mime_Hdr_Field_Append = false;
          test_passed_Mime_Hdr_Field_Next = false;
          test_passed_Mime_Hdr_Field_Get = false;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase3", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase3", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase3", TC_PASS, "ok");
        }
      }

      if (test_passed_Mime_Hdr_Field_Append == true) {
        test_field_loc14 = TSMimeHdrFieldNext(bufp1, mime_loc1, test_field_loc13);
        if (compare_field_names(test, bufp1, mime_loc1, field_loc14, bufp1, mime_loc1, test_field_loc14) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase4", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase4", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase4", TC_FAIL, "Values Don't match");
          test_passed_Mime_Hdr_Field_Append = false;
          test_passed_Mime_Hdr_Field_Next = false;
          test_passed_Mime_Hdr_Field_Get = false;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase4", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase4", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldGet", "TestCase4", TC_PASS, "ok");
        }
      }

      if (test_passed_Mime_Hdr_Field_Append == true) {
        test_field_loc15 = TSMimeHdrFieldNext(bufp1, mime_loc1, test_field_loc14);
        if (compare_field_names(test, bufp1, mime_loc1, field_loc15, bufp1, mime_loc1, test_field_loc15) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase5", TC_FAIL, "Values Don't match");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase5", TC_FAIL, "Values Don't match");
          test_passed_Mime_Hdr_Field_Append = false;
          test_passed_Mime_Hdr_Field_Next = false;
          test_passed_Mime_Hdr_Field_Get = false;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldAppend", "TestCase5", TC_PASS, "ok");
          SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase5", TC_PASS, "ok");
        }
      }

      if ((TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc11) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc12) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc13) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc14) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc15) == TS_ERROR)) {
        SDK_RPRINT(test, "TSMimeHdrFieldAppend/Next/Get", "", TC_FAIL,
                   "Unable to release handle using TSHandleMLocRelease. Can be bad handle.");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldAppend & TSMimeHdrFieldNext", "All Test Case", TC_FAIL,
               "Cannot run test as Test for TSMimeHdrFieldNameGet&Set Failed");
  }


  //TSMimeHdrFieldsCount
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((numberOfFields = TSMimeHdrFieldsCount(bufp1, mime_loc1)) < 0 ) {
      SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_FAIL, "TSMimeHdrFieldsCount Returns TS_ERROR");
    } else {
      actualNumberOfFields = 0;
      if ((field_loc = TSMimeHdrFieldGet(bufp1, mime_loc1, actualNumberOfFields)) == TS_NULL_MLOC) {
        SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_FAIL, "TSMimeHdrFieldGet Returns TS_NULL_MLOC");
      } else {
        while (field_loc != NULL) {
          TSMLoc next_field_loc;

          actualNumberOfFields++;
          next_field_loc = TSMimeHdrFieldNext(bufp1, mime_loc1, field_loc);
          if (TSHandleMLocRelease(bufp1, mime_loc1, field_loc) == TS_ERROR) {
            SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_FAIL, "Unable to release handle using TSHandleMLocRelease");
          }
          field_loc = next_field_loc;
          next_field_loc = NULL;
        }
        if (actualNumberOfFields == numberOfFields) {
          SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Fields_Count = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_FAIL, "Value's Dont match");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldsCount", "TestCase1", TC_FAIL, "Cannot run Test as TSMimeHdrFieldCreate failed");
  }

  // TSMimeHdrFieldValueStringInsert, TSMimeHdrFieldValueStringGet, TSMimeHdrFieldValueStringSet
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc11, -1, field1Value2, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc11, 0, field1Value1, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc11, -1, field1Value5, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc11, 2, field1Value4, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc11, 2, field1Value3, -1) == TS_ERROR)) {
      SDK_RPRINT(test, "TSMimeHdrFieldValueStringInsert", "TestCase1|2|3|4|5", TC_FAIL,
                 "TSMimeHdrFieldValueStringInsert Returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueStringGet", "TestCase1&2&3&4&5", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueStringInsert returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueStringSet", "TestCase1", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueStringInsert returns TS_ERROR");
    } else {
      field1Value1Get = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 0, &lengthField1Value1);
      field1Value2Get = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 1, &lengthField1Value2);
      field1Value3Get = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 2, &lengthField1Value3);
      field1Value4Get = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 3, &lengthField1Value4);
      field1Value5Get = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 4, &lengthField1Value5);
      if (((strncmp(field1Value1Get, field1Value1, lengthField1Value1) == 0) && lengthField1Value1 == (int) strlen(field1Value1)) &&
          ((strncmp(field1Value2Get, field1Value2, lengthField1Value2) == 0) && lengthField1Value2 == (int) strlen(field1Value2)) &&
          ((strncmp(field1Value3Get, field1Value3, lengthField1Value3) == 0) && lengthField1Value3 == (int) strlen(field1Value3)) &&
          ((strncmp(field1Value4Get, field1Value4, lengthField1Value4) == 0) && lengthField1Value4 == (int) strlen(field1Value4)) &&
          ((strncmp(field1Value5Get, field1Value5, lengthField1Value5) == 0) && lengthField1Value5 == (int) strlen(field1Value5))) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueStringInsert", "TestCase1&2&3&4&5", TC_PASS, "ok");
        SDK_RPRINT(test, "TSMimeHdrFieldValueStringGet", "TestCase1&2&3&4&5", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Value_String_Insert = true;
        test_passed_Mime_Hdr_Field_Value_String_Get = true;

        if ((TSMimeHdrFieldValueStringSet(bufp1, mime_loc1, field_loc11, 3, field1ValueNew, -1)) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueStringSet", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueStringSet returns TS_ERROR");
        } else {
          field1ValueNewGet = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc11, 3, &lengthField1ValueNew);
          if ((strncmp(field1ValueNewGet, field1ValueNew, lengthField1ValueNew) == 0) &&
              (lengthField1ValueNew == (int) strlen(field1ValueNew))) {
            SDK_RPRINT(test, "TSMimeHdrFieldValueStringSet", "TestCase1", TC_PASS, "ok");
            test_passed_Mime_Hdr_Field_Value_String_Set = true;
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldValueStringSet", "TestCase1", TC_FAIL, "Value's Don't match");
          }
        }
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldValueStringInsert", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueStringGet", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueStringSet", "TestCase1", TC_FAIL,
                   "TSMimeHdrFieldValueStringSet cannot be tested as TSMimeHdrFieldValueStringInsert|Get failed");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldValueStringInsert&Set&Get", "All", TC_FAIL, "Cannot run Test as TSMimeHdrFieldCreate failed");
  }


  // TSMimeHdrFieldValueDateInsert, TSMimeHdrFieldValueDateGet, TSMimeHdrFieldValueDateSet
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if (TSMimeHdrFieldValueDateInsert(bufp1, mime_loc1, field_loc12, field2Value1) == TS_ERROR) {
      SDK_RPRINT(test, "TSMimeHdrFieldValueDateInsert", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueDateInsert Returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueDateGet", "TestCase1", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueDateInsert returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueDateSet", "TestCase1", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueDateInsert returns TS_ERROR");
    } else {
      field2Value1Get = TSMimeHdrFieldValueDateGet(bufp1, mime_loc1, field_loc12);
      if (field2Value1Get == field2Value1) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueDateInsert", "TestCase1", TC_PASS, "ok");
        SDK_RPRINT(test, "TSMimeHdrFieldValueDateGet", "TestCase1", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Value_Date_Insert = true;
        test_passed_Mime_Hdr_Field_Value_Date_Get = true;
        field2ValueNew = time(NULL);
        if ((TSMimeHdrFieldValueDateSet(bufp1, mime_loc1, field_loc12, field2ValueNew)) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueDateSet", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueDateSet returns TS_ERROR");
        } else {
          field2ValueNewGet = TSMimeHdrFieldValueDateGet(bufp1, mime_loc1, field_loc12);
          if (field2ValueNewGet == field2ValueNew) {
            SDK_RPRINT(test, "TSMimeHdrFieldValueDateSet", "TestCase1", TC_PASS, "ok");
            test_passed_Mime_Hdr_Field_Value_Date_Set = true;
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldValueDateSet", "TestCase1", TC_FAIL, "Value's Don't match");
          }
        }
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldValueDateInsert", "TestCase1", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueDateGet", "TestCase1", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueDateSet", "TestCase1", TC_FAIL,
                   "TSMimeHdrFieldValueDateSet cannot be tested as TSMimeHdrFieldValueDateInsert|Get failed");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldValueDateInsert&Set&Get", "TestCase1", TC_FAIL, "Cannot run Test as TSMimeHdrFieldCreate failed");
  }


  // TSMimeHdrFieldValueIntInsert, TSMimeHdrFieldValueIntGet, TSMimeHdrFieldValueIntSet
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc13, -1, field3Value2) == TS_ERROR) ||
        (TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc13, 0, field3Value1) == TS_ERROR) ||
        (TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc13, -1, field3Value5) == TS_ERROR) ||
        (TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc13, 2, field3Value4) == TS_ERROR) ||
        (TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc13, 2, field3Value3) == TS_ERROR)) {
      SDK_RPRINT(test, "TSMimeHdrFieldValueIntInsert", "TestCase1|2|3|4|5", TC_FAIL, "TSMimeHdrFieldValueIntInsert Returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueIntGet", "TestCase1&2&3&4&5", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueIntInsert returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueIntSet", "TestCase1", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueIntInsert returns TS_ERROR");
    } else {
      field3Value1Get = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 0);
      field3Value2Get = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 1);
      field3Value3Get = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 2);
      field3Value4Get = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 3);
      field3Value5Get = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 4);
      if ((field3Value1Get == field3Value1) && (field3Value2Get == field3Value2) && (field3Value3Get == field3Value3) &&
          (field3Value4Get == field3Value4) && (field3Value5Get == field3Value5)) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueIntInsert", "TestCase1&2&3&4&5", TC_PASS, "ok");
        SDK_RPRINT(test, "TSMimeHdrFieldValueIntGet", "TestCase1&2&3&4&5", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Value_Int_Insert = true;
        test_passed_Mime_Hdr_Field_Value_Int_Get = true;
        if ((TSMimeHdrFieldValueIntSet(bufp1, mime_loc1, field_loc13, 3, field3ValueNew)) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueIntSet", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueIntSet returns TS_ERROR");
        } else {
          field3ValueNewGet = TSMimeHdrFieldValueIntGet(bufp1, mime_loc1, field_loc13, 3);
          if (field3ValueNewGet == field3ValueNew) {
            SDK_RPRINT(test, "TSMimeHdrFieldValueIntSet", "TestCase1", TC_PASS, "ok");
            test_passed_Mime_Hdr_Field_Value_Int_Set = true;
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldValueIntSet", "TestCase1", TC_FAIL, "Value's Don't match");
          }
        }
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldValueIntInsert", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueIntGet", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueIntSet", "TestCase1", TC_FAIL,
                   "TSMimeHdrFieldValueIntSet cannot be tested as TSMimeHdrFieldValueIntInsert|Get failed");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldValueIntInsert&Set&Get", "All", TC_FAIL, "Cannot run Test as TSMimeHdrFieldCreate failed");
  }

  // TSMimeHdrFieldValueUintInsert, TSMimeHdrFieldValueUintGet, TSMimeHdrFieldValueUintSet
  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc14, -1, field4Value2) == TS_ERROR) ||
        (TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc14, 0, field4Value1) == TS_ERROR) ||
        (TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc14, -1, field4Value5) == TS_ERROR) ||
        (TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc14, 2, field4Value4) == TS_ERROR) ||
        (TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc14, 2, field4Value3) == TS_ERROR)) {
      SDK_RPRINT(test, "TSMimeHdrFieldValueUintInsert", "TestCase1|2|3|4|5", TC_FAIL,
                 "TSMimeHdrFieldValueUintInsert Returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueUintGet", "TestCase1&2&3&4&5", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueUintInsert returns TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrFieldValueUintSet", "TestCase1", TC_FAIL,
                 "Cannot run Test as TSMimeHdrFieldValueUintInsert returns TS_ERROR");
    } else {
      field4Value1Get = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 0);
      field4Value2Get = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 1);
      field4Value3Get = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 2);
      field4Value4Get = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 3);
      field4Value5Get = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 4);
      if ((field4Value1Get == field4Value1) && (field4Value2Get == field4Value2) && (field4Value3Get == field4Value3) &&
          (field4Value4Get == field4Value4) && (field4Value5Get == field4Value5)) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueUintInsert", "TestCase1&2&3&4&5", TC_PASS, "ok");
        SDK_RPRINT(test, "TSMimeHdrFieldValueUintGet", "TestCase1&2&3&4&5", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Value_Uint_Insert = true;
        test_passed_Mime_Hdr_Field_Value_Uint_Get = true;
        if ((TSMimeHdrFieldValueUintSet(bufp1, mime_loc1, field_loc14, 3, field4ValueNew)) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueUintSet", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueUintSet returns TS_ERROR");
        } else {
          field4ValueNewGet = TSMimeHdrFieldValueUintGet(bufp1, mime_loc1, field_loc14, 3);
          if (field4ValueNewGet == field4ValueNew) {
            SDK_RPRINT(test, "TSMimeHdrFieldValueUintSet", "TestCase1", TC_PASS, "ok");
            test_passed_Mime_Hdr_Field_Value_Uint_Set = true;
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldValueUintSet", "TestCase1", TC_FAIL, "Value's Don't match");
          }
        }
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldValueUintInsert", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueUintGet", "TestCase1|2|3|4|5", TC_PASS, "Value's Don't Match");
        SDK_RPRINT(test, "TSMimeHdrFieldValueUintSet", "TestCase1", TC_FAIL,
                   "TSMimeHdrFieldValueUintSet cannot be tested as TSMimeHdrFieldValueUintInsert|Get failed");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldValueUintInsert&Set&Get", "All", TC_FAIL, "Cannot run Test as TSMimeHdrFieldCreate failed");
  }

  // TSMimeHdrFieldLengthGet
  field1_length = TSMimeHdrFieldLengthGet(bufp1, mime_loc1, field_loc11);
  field2_length = TSMimeHdrFieldLengthGet(bufp1, mime_loc1, field_loc12);
  field3_length = TSMimeHdrFieldLengthGet(bufp1, mime_loc1, field_loc13);
  field4_length = TSMimeHdrFieldLengthGet(bufp1, mime_loc1, field_loc14);
  if ((field1_length == 0) || (field2_length == 0) || (field3_length == 0) || (field4_length == 0)) {
    SDK_RPRINT(test, "TSMimeHdrFieldLengthGet", "TestCase1", TC_FAIL, "Returned bad length");
    test_passed_Mime_Hdr_Field_Length_Get = false;
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldLengthGet", "TestCase1", TC_PASS, "ok");
    test_passed_Mime_Hdr_Field_Length_Get = true;
  }



  // TSMimeHdrFieldValueAppend, TSMimeHdrFieldValueDelete, TSMimeHdrFieldValuesCount, TSMimeHdrFieldValuesClear

  if (test_passed_Mime_Hdr_Field_Create == true) {
    if ((TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc15, -1, field5Value1, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueIntInsert(bufp1, mime_loc1, field_loc15, -1, field5Value2) == TS_ERROR) ||
        (TSMimeHdrFieldValueStringInsert(bufp1, mime_loc1, field_loc15, -1, field5Value3, -1) == TS_ERROR) ||
        (TSMimeHdrFieldValueUintInsert(bufp1, mime_loc1, field_loc15, -1, field5Value4) == TS_ERROR)) {
      SDK_RPRINT(test, "TSMimeHdrFieldValueAppend", "TestCase1", TC_FAIL,
                 "TSMimeHdrFieldValueString|Int|UintInsert returns TS_ERROR. Cannot create field for testing.");
      SDK_RPRINT(test, "TSMimeHdrFieldValueDelete", "TestCase1", TC_FAIL,
                 "TSMimeHdrFieldValueString|Int|UintInsert returns TS_ERROR. Cannot create field for testing.");
      SDK_RPRINT(test, "TSMimeHdrFieldValuesCount", "TestCase1", TC_FAIL,
                 "TSMimeHdrFieldValueString|Int|UintInsert returns TS_ERROR. Cannot create field for testing.");
      SDK_RPRINT(test, "TSMimeHdrFieldValuesClear", "TestCase1", TC_FAIL,
                 "TSMimeHdrFieldValueString|Int|UintInsert returns TS_ERROR. Cannot create field for testing.");
    } else {
      if (TSMimeHdrFieldValueAppend(bufp1, mime_loc1, field_loc15, 0, field5Value1Append, -1) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueAppend", "TestCase1", TC_FAIL,
                   "TSMimeHdrFieldValueAppend returns TS_ERROR");
      } else {
        fieldValueAppendGet = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc15, 0, &lengthFieldValueAppended);
        char *expected_value;
        size_t len = strlen(field5Value1) + strlen(field5Value1Append) + 1;
        expected_value = (char *) TSmalloc(len);
        memset(expected_value, 0, strlen(field5Value1) + strlen(field5Value1Append) + 1);
        ink_strncpy(expected_value, field5Value1, len);
        strncat(expected_value, field5Value1Append, len - strlen(expected_value) - 1);
        if ((strncmp(fieldValueAppendGet, expected_value, lengthFieldValueAppended) == 0) &&
            (lengthFieldValueAppended = strlen(expected_value))) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueAppend", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Field_Value_Append = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldValueAppend", "TestCase1", TC_FAIL, "Values mismatch");
        }
        TSfree(expected_value);
      }

      numberOfValueInField = TSMimeHdrFieldValuesCount(bufp1, mime_loc1, field_loc15);
      if (numberOfValueInField == 4) {
        SDK_RPRINT(test, "TSMimeHdrFieldValuesCount", "TestCase1", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Values_Count = true;
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldValuesCount", "TestCase1", TC_FAIL, "Values don't match");
      }

      if (TSMimeHdrFieldValueDelete(bufp1, mime_loc1, field_loc15, 2) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrFieldValueDelete", "TestCase1", TC_FAIL, "TSMimeHdrFieldValueDelete Returns TS_ERROR");
      } else {
        fieldValueDeleteGet = TSMimeHdrFieldValueStringGet(bufp1, mime_loc1, field_loc15, 2, &lengthFieldValueDeleteGet);
        if ((strncmp(fieldValueDeleteGet, field5Value3, lengthFieldValueDeleteGet) == 0) &&
            (lengthFieldValueDeleteGet == (int) strlen(field5Value3))) {
          SDK_RPRINT(test, "TSMimeHdrFieldValueDelete", "TestCase1", TC_FAIL,
                     "Value not deleted from field or incorrect index deleted from field.");
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldValueDelete", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Field_Value_Delete = true;
        }
      }

      if (TSMimeHdrFieldValuesClear(bufp1, mime_loc1, field_loc15) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrFieldValuesClear", "TestCase1", TC_FAIL, "TSMimeHdrFieldValuesClear returns TS_ERROR");
      } else {
        numberOfValueInField = TSMimeHdrFieldValuesCount(bufp1, mime_loc1, field_loc15);
        if (numberOfValueInField == 0) {
          SDK_RPRINT(test, "TSMimeHdrFieldValuesClear", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Field_Values_Clear = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldValuesClear", "TestCase1", TC_FAIL, "Values don't match");
        }
      }
    }

    // TSMimeHdrFieldDestroy
    if (TSMimeHdrFieldDestroy(bufp1, mime_loc1, field_loc15) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase1", TC_FAIL, "TSMimeHdrFieldDestroy returns TS_ERROR");
    } else {
      if ((test_field_loc15 = TSMimeHdrFieldFind(bufp1, mime_loc1, field5Name, -1)) == TS_NULL_MLOC) {
        SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase1", TC_PASS, "ok");
        test_passed_Mime_Hdr_Field_Destroy = true;
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase1", TC_FAIL, "Field not destroyed");
        if (TSHandleMLocRelease(bufp1, mime_loc1, test_field_loc15) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase1", TC_FAIL, "Unable to release handle using TSHandleMLocRelease");
        }
      }
      if (TSHandleMLocRelease(bufp1, mime_loc1, field_loc15) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase2", TC_FAIL, "Unable to release handle using TSHandleMLocRelease");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldValueAppend", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrFieldCreate has failed");
    SDK_RPRINT(test, "TSMimeHdrFieldValueDelete", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrFieldCreate has failed");
    SDK_RPRINT(test, "TSMimeHdrFieldValuesCount", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrFieldCreate has failed");
    SDK_RPRINT(test, "TSMimeHdrFieldValuesClear", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrFieldCreate has failed");
    SDK_RPRINT(test, "TSMimeHdrFieldDestroy", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrFieldCreate has failed");
  }

  // Mime Hdr Fields Clear
  if (test_passed_Mime_Hdr_Field_Append == true) {
    if (TSMimeHdrFieldsClear(bufp1, mime_loc1) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrFieldsClear", "TestCase1", TC_FAIL, "TSMimeHdrFieldsClear returns TS_ERROR");
    } else {
      if ((numberOfFields = TSMimeHdrFieldsCount(bufp1, mime_loc1)) < 0) {
        SDK_RPRINT(test, "TSMimeHdrFieldsClear", "TestCase1", TC_FAIL, "TSMimeHdrFieldsCount returns TS_ERROR");
      } else {
        if (numberOfFields == 0) {
          SDK_RPRINT(test, "TSMimeHdrFieldsClear", "TestCase1", TC_PASS, "ok");
          test_passed_Mime_Hdr_Fields_Clear = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldsClear", "TestCase1", TC_FAIL, "Fields still exist");
        }
      }
      if ((TSHandleMLocRelease(bufp1, mime_loc1, field_loc11) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, field_loc12) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, field_loc13) == TS_ERROR) ||
          (TSHandleMLocRelease(bufp1, mime_loc1, field_loc14) == TS_ERROR)) {
        SDK_RPRINT(test, "TSMimeHdrFieldsDestroy", "", TC_FAIL, "Unable to release handle using TSHandleMLocRelease");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldsClear", "TestCase1", TC_FAIL,
               "Cannot run test as Fields have not been inserted in the mime header");
  }

  // Mime Hdr Destroy
  if (test_passed_Mime_Hdr_Create == true) {
    if (TSMimeHdrDestroy(bufp1, mime_loc1) == TS_ERROR) {
      SDK_RPRINT(test, "TSMimeHdrDestroy", "TestCase1", TC_FAIL, "TSMimeHdrDestroy return TS_ERROR");
      SDK_RPRINT(test, "TSMimeHdrDestroy", "TestCase1", TC_FAIL, "Probably TSMimeHdrCreate failed.");
    } else {
      SDK_RPRINT(test, "TSMimeHdrDestroy", "TestCase1", TC_PASS, "ok");
      test_passed_Mime_Hdr_Destroy = true;
    }
      /** Commented out as Traffic Server was crashing. Will have to look into it. */
    /*
       if (TSHandleMLocRelease(bufp1,TS_NULL_MLOC,mime_loc1)==TS_ERROR) {
       SDK_RPRINT(test,"TSHandleMLocRelease","TSMimeHdrDestroy",TC_FAIL,"unable to release handle using TSHandleMLocRelease");
       }
     */
  } else {
    SDK_RPRINT(test, "TSMimeHdrDestroy", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrCreate failed");
  }

  // MBuffer Destroy
  if (test_passed_MBuffer_Create == true) {
    if (TSMBufferDestroy(bufp1) == TS_ERROR) {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase1", TC_FAIL, "TSMBufferDestroy return TS_ERROR");
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase1", TC_FAIL, "Probably TSMBufferCreate failed.");
    } else {
      SDK_RPRINT(test, "TSMBufferDestroy", "TestCase1", TC_PASS, "ok");
      test_passed_MBuffer_Destroy = true;
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrDestroy", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrCreate failed");
  }


  if ((test_passed_MBuffer_Create == true) &&
      (test_passed_Mime_Hdr_Create == true) &&
      (test_passed_Mime_Hdr_Field_Create == true) &&
      (test_passed_Mime_Hdr_Field_Name == true) &&
      (test_passed_Mime_Hdr_Field_Append == true) &&
      (test_passed_Mime_Hdr_Field_Get == true) &&
      (test_passed_Mime_Hdr_Field_Next == true) &&
      (test_passed_Mime_Hdr_Fields_Count == true) &&
      (test_passed_Mime_Hdr_Field_Value_String_Insert == true) &&
      (test_passed_Mime_Hdr_Field_Value_String_Get == true) &&
      (test_passed_Mime_Hdr_Field_Value_String_Set == true) &&
      (test_passed_Mime_Hdr_Field_Value_Date_Insert == true) &&
      (test_passed_Mime_Hdr_Field_Value_Date_Get == true) &&
      (test_passed_Mime_Hdr_Field_Value_Date_Set == true) &&
      (test_passed_Mime_Hdr_Field_Value_Int_Insert == true) &&
      (test_passed_Mime_Hdr_Field_Value_Int_Get == true) &&
      (test_passed_Mime_Hdr_Field_Value_Int_Set == true) &&
      (test_passed_Mime_Hdr_Field_Value_Uint_Insert == true) &&
      (test_passed_Mime_Hdr_Field_Value_Uint_Get == true) &&
      (test_passed_Mime_Hdr_Field_Value_Uint_Set == true) &&
      (test_passed_Mime_Hdr_Field_Value_Append == true) &&
      (test_passed_Mime_Hdr_Field_Value_Delete == true) &&
      (test_passed_Mime_Hdr_Field_Values_Clear == true) &&
      (test_passed_Mime_Hdr_Field_Values_Count == true) &&
      (test_passed_Mime_Hdr_Field_Destroy == true) &&
      (test_passed_Mime_Hdr_Fields_Clear == true) &&
      (test_passed_Mime_Hdr_Destroy == true) &&
      (test_passed_MBuffer_Destroy == true) && (test_passed_Mime_Hdr_Field_Length_Get == true)) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }
  return;

}


//////////////////////////////////////////////
//       SDK_API_TSHttpHdrParse
//
// Unit Test for API: TSHttpParserCreate
//                    TSHttpParserDestroy
//                    TSHttpParserClear
//                    TSHttpHdrParseReq
//                    TSHttpHdrParseResp
//////////////////////////////////////////////

char *
convert_http_hdr_to_string(TSMBuffer bufp, TSMLoc hdr_loc)
{
  TSIOBuffer output_buffer;
  TSIOBufferReader reader;
  int64_t total_avail;

  TSIOBufferBlock block;
  const char *block_start;
  int64_t block_avail;

  char *output_string;
  int output_len;

  output_buffer = TSIOBufferCreate();

  if (!output_buffer) {
    TSError("couldn't allocate IOBuffer\n");
  }

  reader = TSIOBufferReaderAlloc(output_buffer);

  /* This will print  just MIMEFields and not
     the http request line */
  TSHttpHdrPrint(bufp, hdr_loc, output_buffer);

  /* Find out how the big the complete header is by
     seeing the total bytes in the buffer.  We need to
     look at the buffer rather than the first block to
     see the size of the entire header */
  total_avail = TSIOBufferReaderAvail(reader);

  /* Allocate the string with an extra byte for the string
     terminator */
  output_string = (char *) TSmalloc(total_avail + 1);
  output_len = 0;

  /* We need to loop over all the buffer blocks to make
     sure we get the complete header since the header can
     be in multiple blocks */
  block = TSIOBufferReaderStart(reader);
  while (block) {

    block_start = TSIOBufferBlockReadStart(block, reader, &block_avail);

    /* We'll get a block pointer back even if there is no data
       left to read so check for this condition and break out of
       the loop. A block with no data to read means we've exhausted
       buffer of data since if there was more data on a later
       block in the chain, this block would have been skipped over */
    if (block_avail == 0) {
      break;
    }

    memcpy(output_string + output_len, block_start, block_avail);
    output_len += block_avail;

    /* Consume the data so that we get to the next block */
    TSIOBufferReaderConsume(reader, block_avail);

    /* Get the next block now that we've consumed the
       data off the last block */
    block = TSIOBufferReaderStart(reader);
  }

  /* Terminate the string */
  output_string[output_len] = '\0';
  output_len++;

  /* Free up the TSIOBuffer that we used to print out the header */
  TSIOBufferReaderFree(reader);
  TSIOBufferDestroy(output_buffer);

  return output_string;
}

REGRESSION_TEST(SDK_API_TSHttpHdrParse) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  const char *req =
    "GET http://www.example.com/ HTTP/1.1\r\nmimefield1:field1value1,field1value2\r\nmimefield2:field2value1,field2value2\r\n\r\n";
  const char *resp =
    "HTTP/1.1 200 OK\r\n1mimefield:1field1value,1field2value\r\n2mimefield:2field1value,2field2value\r\n\r\n";
  const char *start;
  const char *end;
  char *temp;

  int retval;

  TSMBuffer reqbufp;
  TSMBuffer respbufp = (TSMBuffer)NULL;

  TSMLoc req_hdr_loc = (TSMLoc)NULL;
  TSMLoc resp_hdr_loc = (TSMLoc)NULL;

  TSHttpParser parser;

  bool test_passed_parse_req = false;
  bool test_passed_parse_resp = false;
  bool test_passed_parser_clear = false;
  bool test_passed_parser_destroy = false;
  bool resp_run = true;


  //Create Parser
  parser = TSHttpParserCreate();
  SDK_RPRINT(test, "TSHttpParserCreate", "TestCase1", TC_PASS, "ok");

  // Request
  reqbufp = TSMBufferCreate();
  req_hdr_loc = TSHttpHdrCreate(reqbufp);
  start = req;
  end = req + strlen(req) + 1;
  if ((retval = TSHttpHdrParseReq(parser, reqbufp, req_hdr_loc, &start, end)) == TS_PARSE_ERROR) {
    SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_FAIL, "TSHttpHdrParseReq returns TS_PARSE_ERROR");
  } else {
    if (retval == TS_PARSE_DONE) {
      test_passed_parse_req = true;
    } else {
      SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_FAIL, "Parsing Error");
    }
  }

  TSHttpParserClear(parser);
  SDK_RPRINT(test, "TSHttpParserClear", "TestCase1", TC_PASS, "ok");
  test_passed_parser_clear = true;

  // Response
  if (test_passed_parser_clear == true) {
    respbufp = TSMBufferCreate();
    resp_hdr_loc = TSHttpHdrCreate(respbufp);
    start = resp;
    end = resp + strlen(resp) + 1;
    if ((retval = TSHttpHdrParseResp(parser, respbufp, resp_hdr_loc, &start, end)) == TS_PARSE_ERROR) {
      SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_FAIL,
                 "TSHttpHdrParseReq returns TS_PARSE_ERROR. Maybe an error with TSHttpParserClear.");
    } else {
      if (retval == TS_PARSE_DONE) {
        test_passed_parse_resp = true;
      } else {
        SDK_RPRINT(test, "TSHttpHdrParseResp", "TestCase1", TC_FAIL, "Parsing Error");
      }
    }
  } else {
    resp_run = false;
  }

  if (test_passed_parse_req == true) {
    temp = convert_http_hdr_to_string(reqbufp, req_hdr_loc);
    if (strcmp(req, temp) == 0) {
      SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_PASS, "ok");
    } else {
      SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_FAIL, "Incorrect parsing");
      test_passed_parse_req = false;
    }
    TSfree(temp);
  }

  if (test_passed_parse_resp == true) {
    temp = convert_http_hdr_to_string(respbufp, resp_hdr_loc);
    if (strcmp(resp, temp) == 0) {
      SDK_RPRINT(test, "TSHttpHdrParseResp", "TestCase1", TC_PASS, "ok");
    } else {
      SDK_RPRINT(test, "TSHttpHdrParseResp", "TestCase1", TC_FAIL, "Incorrect parsing");
      test_passed_parse_resp = false;
    }
    TSfree(temp);
  }

  TSHttpParserDestroy(parser);
  SDK_RPRINT(test, "TSHttpParserDestroy", "TestCase1", TC_PASS, "ok");
  test_passed_parser_destroy = true;

  if ((test_passed_parse_req != true) ||
      (test_passed_parse_resp != true) || (test_passed_parser_clear != true) || (test_passed_parser_destroy != true)) {
    *pstatus = REGRESSION_TEST_FAILED;
  } else {
    *pstatus = REGRESSION_TEST_PASSED;
  }

  TSMimeHdrDestroy(reqbufp, req_hdr_loc);
  if (resp_run == true)
    TSMimeHdrDestroy(respbufp, resp_hdr_loc);

  TSHandleMLocRelease(reqbufp, TS_NULL_MLOC, req_hdr_loc);
  if (resp_run == true)
    TSHandleMLocRelease(respbufp, TS_NULL_MLOC, resp_hdr_loc);

  TSMBufferDestroy(reqbufp);
  if (resp_run == true)
    TSMBufferDestroy(respbufp);

  return;
}




//////////////////////////////////////////////
//       SDK_API_TSMimeHdrParse
//
// Unit Test for API: TSMimeHdrCopy
//                    TSMimeHdrClone
//                    TSMimeHdrFieldCopy
//                    TSMimeHdrFieldClone
//                    TSMimeHdrFieldCopyValues
//                    TSMimeHdrFieldNextDup
//                    TSMimeHdrFieldRemove
//                    TSMimeHdrLengthGet
//                    TSMimeHdrParse
//                    TSMimeHdrPrint
//                    TSMimeParserClear
//                    TSMimeParserCreate
//                    TSMimeParserDestroy
//                    TSHandleMLocRelease
//////////////////////////////////////////////

char *
convert_mime_hdr_to_string(TSMBuffer bufp, TSMLoc hdr_loc)
{
  TSIOBuffer output_buffer;
  TSIOBufferReader reader;
  int64_t total_avail;

  TSIOBufferBlock block;
  const char *block_start;
  int64_t block_avail;

  char *output_string;
  int output_len;

  output_buffer = TSIOBufferCreate();

  if (!output_buffer) {
    TSError("couldn't allocate IOBuffer\n");
  }

  reader = TSIOBufferReaderAlloc(output_buffer);

  /* This will print  just MIMEFields and not
     the http request line */
  TSMimeHdrPrint(bufp, hdr_loc, output_buffer);

  /* Find out how the big the complete header is by
     seeing the total bytes in the buffer.  We need to
     look at the buffer rather than the first block to
     see the size of the entire header */
  total_avail = TSIOBufferReaderAvail(reader);

  /* Allocate the string with an extra byte for the string
     terminator */
  output_string = (char *) TSmalloc(total_avail + 1);
  output_len = 0;

  /* We need to loop over all the buffer blocks to make
     sure we get the complete header since the header can
     be in multiple blocks */
  block = TSIOBufferReaderStart(reader);
  while (block) {

    block_start = TSIOBufferBlockReadStart(block, reader, &block_avail);

    /* We'll get a block pointer back even if there is no data
       left to read so check for this condition and break out of
       the loop. A block with no data to read means we've exhausted
       buffer of data since if there was more data on a later
       block in the chain, this block would have been skipped over */
    if (block_avail == 0) {
      break;
    }

    memcpy(output_string + output_len, block_start, block_avail);
    output_len += block_avail;

    /* Consume the data so that we get to the next block */
    TSIOBufferReaderConsume(reader, block_avail);

    /* Get the next block now that we've consumed the
       data off the last block */
    block = TSIOBufferReaderStart(reader);
  }

  /* Terminate the string */
  output_string[output_len] = '\0';
  output_len++;

  /* Free up the TSIOBuffer that we used to print out the header */
  TSIOBufferReaderFree(reader);
  TSIOBufferDestroy(output_buffer);

  return output_string;
}

TSReturnCode
compare_field_values(RegressionTest * test, TSMBuffer bufp1, TSMLoc hdr_loc1, TSMLoc field_loc1, TSMBuffer bufp2,
                     TSMLoc hdr_loc2, TSMLoc field_loc2, bool first_time)
{
  NOWARN_UNUSED(first_time);
  int no_of_values1;
  int no_of_values2;
  int i;

  const char *str1 = NULL;
  const char *str2 = NULL;

  int length1 = 0;
  int length2 = 0;

  no_of_values1 = TSMimeHdrFieldValuesCount(bufp1, hdr_loc1, field_loc1);
  no_of_values2 = TSMimeHdrFieldValuesCount(bufp2, hdr_loc2, field_loc2);
  if (no_of_values1 != no_of_values2) {
    SDK_RPRINT(test, "compare_field_values", "TestCase", TC_FAIL, "Field Values not equal");
    return TS_ERROR;
  }

  for (i = 0; i < no_of_values1; i++) {
    str1 = TSMimeHdrFieldValueStringGet(bufp1, hdr_loc1, field_loc1, i, &length1);
    str2 = TSMimeHdrFieldValueStringGet(bufp2, hdr_loc2, field_loc2, i, &length2);
    if (!((length1 == length2) && (strncmp(str1, str2, length1) == 0))) {
      SDK_RPRINT(test, "compare_field_values", "TestCase", TC_FAIL, "Field Value %d differ from each other", i);
      return TS_ERROR;
    }
  }

  return TS_SUCCESS;
}


REGRESSION_TEST(SDK_API_TSMimeHdrParse) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  const char *parse_string =
    "field1:field1Value1,field1Value2\r\nfield2:10,-34,45\r\nfield3:field3Value1,23\r\nfield2: 2345, field2Value2\r\n\r\n";
  const char *DUPLICATE_FIELD_NAME = "field2";
  const char *REMOVE_FIELD_NAME = "field3";

  TSMimeParser parser;

  TSMBuffer bufp1 = (TSMBuffer)NULL;
  TSMBuffer bufp2 = (TSMBuffer)NULL;
  TSMBuffer bufp3 = (TSMBuffer)NULL;

  TSMLoc mime_hdr_loc1 = (TSMLoc)NULL;
  TSMLoc mime_hdr_loc2 = (TSMLoc)NULL;
  TSMLoc mime_hdr_loc3 = (TSMLoc)NULL;

  TSMLoc field_loc1 = (TSMLoc)NULL;
  TSMLoc field_loc2 = (TSMLoc)NULL;

  const char *start;
  const char *end;
  char *temp;

  TSParseResult retval;
  int hdrLength;

  bool test_passed_parser_create = false;
  bool test_passed_parse = false;
  bool test_passed_parser_clear = false;
  bool test_passed_parser_destroy = false;
  bool test_passed_mime_hdr_print = false;
  bool test_passed_mime_hdr_length_get = false;
  bool test_passed_mime_hdr_field_next_dup = false;
  bool test_passed_mime_hdr_copy = false;
  bool test_passed_mime_hdr_clone = false;
  bool test_passed_mime_hdr_field_remove = false;
  bool test_passed_mime_hdr_field_copy = false;
  bool test_passed_mime_hdr_field_copy_values = false;
  bool test_passed_handle_mloc_release = false;
  bool test_passed_mime_hdr_field_find = false;

  //Create Parser
  parser = TSMimeParserCreate();
  SDK_RPRINT(test, "TSMimeParserCreate", "TestCase1", TC_PASS, "ok");
  test_passed_parser_create = true;

  if (test_passed_parser_create == true) {
    // Parsing
    bufp1 = TSMBufferCreate();
    if (TSMimeHdrCreate(bufp1, &mime_hdr_loc1) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_FAIL, "Cannot create Mime hdr for parsing");
      SDK_RPRINT(test, "TSMimeHdrPrint", "TestCase1", TC_FAIL, "Cannot run test as unable to create Mime Header for parsing");
      SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL, "Cannot run test as unable to create Mime Header for parsing");

      if (TSMBufferDestroy(bufp1) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_FAIL, "Error in Destroying MBuffer");
      }
    } else {
      start = parse_string;
      end = parse_string + strlen(parse_string) + 1;
      if ((retval = TSMimeHdrParse(parser, bufp1, mime_hdr_loc1, &start, end)) == TS_PARSE_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_FAIL, "TSMimeHdrParse returns TS_PARSE_ERROR");
        SDK_RPRINT(test, "TSMimeHdrPrint", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrParse returned Error.");
        SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrParse returned Error.");
      } else {
        if (retval == TS_PARSE_DONE) {
          temp = convert_mime_hdr_to_string(bufp1, mime_hdr_loc1);    // Implements TSMimeHdrPrint.
          if (strcmp(parse_string, temp) == 0) {
            SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_PASS, "ok");
            SDK_RPRINT(test, "TSMimeHdrPrint", "TestCase1", TC_PASS, "ok");

            // TSMimeHdrLengthGet
            hdrLength = TSMimeHdrLengthGet(bufp1, mime_hdr_loc1);
            if (hdrLength == (int) strlen(temp)) {
              SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_PASS, "ok");
              test_passed_mime_hdr_length_get = true;
            } else {
              SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL, "Value's Mismatch");
            }

            test_passed_parse = true;
            test_passed_mime_hdr_print = true;
          } else {
            SDK_RPRINT(test, "TSMimeHdrParse|TSMimeHdrPrint", "TestCase1", TC_FAIL, "Incorrect parsing or incorrect Printing");
            SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL,
                       "Cannot run test as TSMimeHdrParse|TSMimeHdrPrint failed.");
          }

          TSfree(temp);
        } else {
          SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_FAIL, "Parsing Error");
          SDK_RPRINT(test, "TSMimeHdrPrint", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrParse returned error.");
          SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL, "Cannot run test as TSMimeHdrParse returned error.");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrParse", "TestCase1", TC_FAIL, "Cannot run test as unable to create a parser");
    SDK_RPRINT(test, "TSMimeHdrPrint", "TestCase1", TC_FAIL, "Cannot run test as unable to create a parser");
    SDK_RPRINT(test, "TSMimeHdrLengthGet", "TestCase1", TC_FAIL, "Cannot run test as unable to create a parser");
  }


  // HOW DO I CHECK FOR PARSER CLEAR????
  if (test_passed_parser_create == true) {
    TSMimeParserClear(parser);
    SDK_RPRINT(test, "TSMimeParserClear", "TestCase1", TC_PASS, "ok");
    test_passed_parser_clear = true;
  } else {
    SDK_RPRINT(test, "TSMimeParserClear", "TestCase1", TC_FAIL, "Cannot run test as unable to create a parser");
  }


  if (test_passed_parser_create == true) {
    TSMimeParserDestroy(parser);
    SDK_RPRINT(test, "TSMimeParserDestroy", "TestCase1", TC_PASS, "ok");
    test_passed_parser_destroy = true;
  } else {
    SDK_RPRINT(test, "TSMimeParserDestroy", "TestCase1", TC_FAIL, "Cannot run test as unable to create a parser");
  }

  //TSMimeHdrFieldNextDup
  if (test_passed_parse == true) {
    if ((field_loc1 = TSMimeHdrFieldFind(bufp1, mime_hdr_loc1, DUPLICATE_FIELD_NAME, -1)) == TS_NULL_MLOC) {
      SDK_RPRINT(test, "TSMimeHdrFieldNextDup", "TestCase1", TC_FAIL, "TSMimeHdrFieldFind returns TS_NULL_MLOC");
      SDK_RPRINT(test, "TSMimeHdrFieldFind", "TestCase1", TC_PASS, "TSMimeHdrFieldFind returns TS_NULL_MLOC");
    } else {
      const char *fieldName;
      int length;

      fieldName = TSMimeHdrFieldNameGet(bufp1, mime_hdr_loc1, field_loc1, &length);
      if (strncmp(fieldName, DUPLICATE_FIELD_NAME, length) == 0) {
        SDK_RPRINT(test, "TSMimeHdrFieldFind", "TestCase1", TC_PASS, "ok");
        test_passed_mime_hdr_field_find = true;
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldFind", "TestCase1", TC_PASS, "TSMimeHdrFieldFind returns incorrect field pointer");
      }

      field_loc2 = TSMimeHdrFieldNextDup(bufp1, mime_hdr_loc1, field_loc1);
      if (compare_field_names(test, bufp1, mime_hdr_loc1, field_loc1, bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrFieldNextDup", "TestCase1", TC_FAIL, "Incorrect Pointer");
      } else {
        SDK_RPRINT(test, "TSMimeHdrFieldNextDup", "TestCase1", TC_PASS, "ok");
        test_passed_mime_hdr_field_next_dup = true;
      }

      // TSHandleMLocRelease
      if (TSHandleMLocRelease(bufp1, mime_hdr_loc1, field_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase1", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase1", TC_PASS, "ok");
        test_passed_handle_mloc_release = true;
      }

      if (field_loc2 != NULL) {
        if (TSHandleMLocRelease(bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase2", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
          test_passed_handle_mloc_release = false;
        } else {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase2", TC_PASS, "ok");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase1", TC_FAIL, "Unable to run test as parsing failed.");
  }

  //TSMimeHdrCopy
  if (test_passed_parse == true) {
    // Parsing
    bufp2 = TSMBufferCreate();
    if (TSMimeHdrCreate(bufp2, &mime_hdr_loc2) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_FAIL, "Cannot create Mime hdr for copying");
      if (TSMBufferDestroy(bufp2) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_FAIL, "Error in Destroying MBuffer");
      }
    } else {
      if (TSMimeHdrCopy(bufp2, mime_hdr_loc2, bufp1, mime_hdr_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_FAIL, "TSMimeHdrCopy returns TS_ERROR");
      } else {
        temp = convert_mime_hdr_to_string(bufp2, mime_hdr_loc2);      // Implements TSMimeHdrPrint.
        if (strcmp(parse_string, temp) == 0) {
          SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_PASS, "ok");
          test_passed_mime_hdr_copy = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_FAIL, "Value's Mismatch");
        }
        TSfree(temp);
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrCopy", "TestCase1", TC_FAIL, "Unable to run test as parsing failed.");
  }

  bufp3 = TSMBufferCreate();
  TSMimeHdrCreate(bufp3, &mime_hdr_loc3);
  test_passed_mime_hdr_clone = true;

  // TSMimeHdrFieldRemove
  if (test_passed_mime_hdr_copy == true) {
    if ((field_loc1 = TSMimeHdrFieldFind(bufp2, mime_hdr_loc2, REMOVE_FIELD_NAME, -1)) == TS_NULL_MLOC) {
      SDK_RPRINT(test, "TSMimeHdrFieldRemove", "TestCase1", TC_FAIL, "TSMimeHdrFieldFind returns TS_NULL_MLOC");
    } else {
      if (TSMimeHdrFieldRemove(bufp2, mime_hdr_loc2, field_loc1) != TS_SUCCESS) {
        SDK_RPRINT(test, "TSMimeHdrFieldRemove", "TestCase1", TC_FAIL, "TSMimeHdrFieldRemove returns TS_ERROR");
      } else {
        // Make sure the remove actually took effect
        field_loc2 = TSMimeHdrFieldFind(bufp2, mime_hdr_loc2, REMOVE_FIELD_NAME, -1);
        if ((field_loc2 == TS_NULL_MLOC) || (field_loc1 != field_loc2)) {
          test_passed_mime_hdr_field_remove = true;
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldRemove", "TestCase1", TC_FAIL, "Field Not Removed");
        }

        if ((test_passed_mime_hdr_field_remove == true)) {
          if (TSMimeHdrFieldAppend(bufp2, mime_hdr_loc2, field_loc1) != TS_SUCCESS) {
            SDK_RPRINT(test, "TSMimeHdrFieldRemove", "TestCase1", TC_FAIL,
                       "Unable to readd the field to mime header. Probably destroyed");
            test_passed_mime_hdr_field_remove = false;
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldRemove", "TestCase1", TC_PASS, "ok");
          }
        }
      }

      // TSHandleMLocRelease
      if (TSHandleMLocRelease(bufp2, mime_hdr_loc2, field_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase3", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
        test_passed_handle_mloc_release = false;
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase3", TC_PASS, "ok");
      }

      if (field_loc2 != NULL) {
        if (TSHandleMLocRelease(bufp2, mime_hdr_loc2, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase4", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
          test_passed_handle_mloc_release = false;
        } else {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase4", TC_PASS, "ok");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldNext", "TestCase1", TC_FAIL, "Unable to run test as parsing failed.");
  }

  // TSMimeHdrFieldCopy
  if (test_passed_mime_hdr_copy == true) {
    if (TSMimeHdrFieldCreate(bufp2, mime_hdr_loc2, &field_loc1) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "Unable to create field for Copying");
    } else {
      if ((field_loc2 = TSMimeHdrFieldGet(bufp1, mime_hdr_loc1, 0)) == TS_NULL_MLOC) {
        SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "Unable to get source field for copying");
      } else {
        if (TSMimeHdrFieldCopy(bufp2, mime_hdr_loc2, field_loc1, bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "TSMimeHdrFieldCopy returns TS_ERROR");
        } else {
          if ((compare_field_names(test, bufp2, mime_hdr_loc2, field_loc1, bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) ||
              (compare_field_values(test, bufp2, mime_hdr_loc2, field_loc1, bufp1, mime_hdr_loc1, field_loc2, true) == TS_ERROR)) {
            SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "Value's Mismatch");
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_PASS, "ok");
            test_passed_mime_hdr_field_copy = true;
          }
        }
      }
      if (TSHandleMLocRelease(bufp2, mime_hdr_loc2, field_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase5", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
        test_passed_handle_mloc_release = false;
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase5", TC_PASS, "ok");
      }

      if (field_loc2 != NULL) {
        if (TSHandleMLocRelease(bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase6", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
          test_passed_handle_mloc_release = false;
        } else {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase6", TC_PASS, "ok");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "Unable to run test as bufp2 might not have been created");
  }

  // TSMimeHdrFieldClone
  if (test_passed_mime_hdr_clone == true) {
    field_loc1 = NULL;
    field_loc2 = NULL;
    if ((field_loc2 = TSMimeHdrFieldGet(bufp1, mime_hdr_loc1, 0)) == TS_NULL_MLOC) {
      SDK_RPRINT(test, "TSMimeHdrFieldClone", "TestCase1", TC_FAIL, "Unable to get source field for copying");
    } else {
      if (TSMimeHdrFieldClone(bufp3, mime_hdr_loc3, bufp1, mime_hdr_loc1, field_loc2, &field_loc1) != TS_SUCCESS) {
        SDK_RPRINT(test, "TSMimeHdrFieldClone", "TestCase1", TC_FAIL, "TSMimeHdrFieldClone returns TS_ERROR");
      } else {
        if ((compare_field_names(test, bufp3, mime_hdr_loc3, field_loc1, bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) ||
            (compare_field_values(test, bufp3, mime_hdr_loc3, field_loc1, bufp1, mime_hdr_loc1, field_loc2, true) == TS_ERROR)) {
          SDK_RPRINT(test, "TSMimeHdrFieldClone", "TestCase1", TC_FAIL, "Value's Mismatch");
        } else {
          SDK_RPRINT(test, "TSMimeHdrFieldClone", "TestCase1", TC_PASS, "ok");
        }
      }
    }
    if (field_loc1 != NULL) {
      if (TSHandleMLocRelease(bufp3, mime_hdr_loc3, field_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase7", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
        test_passed_handle_mloc_release = false;
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase7", TC_PASS, "ok");
      }
    }

    if (field_loc2 != NULL) {
      if (TSHandleMLocRelease(bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase8", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
        test_passed_handle_mloc_release = false;
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase8", TC_PASS, "ok");
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldClone", "TestCase1", TC_FAIL, "Unable to run test as bufp3 might not have been created");
  }

  // TSMimeHdrFieldCopyValues
  if (test_passed_mime_hdr_copy == true) {
    if (TSMimeHdrFieldCreate(bufp2, mime_hdr_loc2, &field_loc1) != TS_SUCCESS) {
      SDK_RPRINT(test, "TSMimeHdrFieldCopyValues", "TestCase1", TC_FAIL, "Unable to create field for Copying");
    } else {
      if ((field_loc2 = TSMimeHdrFieldGet(bufp1, mime_hdr_loc1, 0)) == TS_NULL_MLOC) {
        SDK_RPRINT(test, "TSMimeHdrFieldCopyValues", "TestCase1", TC_FAIL, "Unable to get source field for copying");
      } else {
        if (TSMimeHdrFieldCopyValues(bufp2, mime_hdr_loc2, field_loc1, bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSMimeHdrFieldCopyValues", "TestCase1", TC_FAIL, "TSMimeHdrFieldCopy returns TS_ERROR");
        } else {
          if (compare_field_values(test, bufp2, mime_hdr_loc2, field_loc1, bufp1, mime_hdr_loc1, field_loc2, false) == TS_ERROR) {
            SDK_RPRINT(test, "TSMimeHdrFieldCopyValues", "TestCase1", TC_FAIL, "Value's Mismatch");
          } else {
            SDK_RPRINT(test, "TSMimeHdrFieldCopyValues", "TestCase1", TC_PASS, "ok");
            test_passed_mime_hdr_field_copy_values = true;
          }
        }
      }
      if (TSHandleMLocRelease(bufp2, mime_hdr_loc2, field_loc1) == TS_ERROR) {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase9", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
        test_passed_handle_mloc_release = false;
      } else {
        SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase9", TC_PASS, "ok");
      }

      if (field_loc2 != NULL) {
        if (TSHandleMLocRelease(bufp1, mime_hdr_loc1, field_loc2) == TS_ERROR) {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase10", TC_FAIL, "TSHandleMLocRelease returns TS_ERROR");
          test_passed_handle_mloc_release = false;
        } else {
          SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase10", TC_PASS, "ok");
        }
      }
    }
  } else {
    SDK_RPRINT(test, "TSMimeHdrFieldCopy", "TestCase1", TC_FAIL, "Unable to run test as bufp2 might not have been created");
  }

  if ((TSMimeHdrDestroy(bufp1, mime_hdr_loc1) == TS_ERROR) ||
      (TSMimeHdrDestroy(bufp2, mime_hdr_loc2) == TS_ERROR) || (TSMimeHdrDestroy(bufp3, mime_hdr_loc3) == TS_ERROR)) {
    SDK_RPRINT(test, "", "TestCase", TC_FAIL, "TSMimeHdrDestroy returns TS_ERROR");
  }

  if ((TSHandleMLocRelease(bufp1, TS_NULL_MLOC, mime_hdr_loc1) == TS_ERROR) ||
      (TSHandleMLocRelease(bufp2, TS_NULL_MLOC, mime_hdr_loc2) == TS_ERROR) ||
      (TSHandleMLocRelease(bufp3, TS_NULL_MLOC, mime_hdr_loc3) == TS_ERROR)) {
    SDK_RPRINT(test, "TSHandleMLocRelease", "TestCase11|12|13", TC_FAIL, "Unable to release handle to Mime Hdrs");
    test_passed_handle_mloc_release = false;
  }

  if ((TSMBufferDestroy(bufp1) == TS_ERROR) ||
      (TSMBufferDestroy(bufp2) == TS_ERROR) || (TSMBufferDestroy(bufp3) == TS_ERROR)) {
    SDK_RPRINT(test, "", "TestCase", TC_FAIL, "TSMBufferDestroy returns TS_ERROR");
  }

  if ((test_passed_parser_create != true) ||
      (test_passed_parse != true) ||
      (test_passed_parser_clear != true) ||
      (test_passed_parser_destroy != true) ||
      (test_passed_mime_hdr_print != true) ||
      (test_passed_mime_hdr_length_get != true) ||
      (test_passed_mime_hdr_field_next_dup != true) ||
      (test_passed_mime_hdr_copy != true) ||
      (test_passed_mime_hdr_clone != true) ||
      (test_passed_mime_hdr_field_remove != true) ||
      (test_passed_mime_hdr_field_copy != true) ||
      (test_passed_mime_hdr_field_copy_values != true) ||
      (test_passed_handle_mloc_release != true) ||
      (test_passed_mime_hdr_field_find != true)) {
    *pstatus = REGRESSION_TEST_FAILED;
  } else {
    *pstatus = REGRESSION_TEST_PASSED;
  }

}

//////////////////////////////////////////////
//       SDK_API_TSUrlParse
//
// Unit Test for API: TSUrlParse
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSUrlParse) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  const char *url = "http://abc:def@www.example.com:3426/homepage.cgi;ab?abc=def#abc";
  const char *start;
  const char *end;
  char *temp;

  int retval;

  TSMBuffer bufp;
  TSMLoc url_loc = (TSMLoc)NULL;
  bool test_passed_parse_url = false;
  int length;

  *pstatus = REGRESSION_TEST_INPROGRESS;


  bufp = TSMBufferCreate();
  if (TSUrlCreate(bufp, &url_loc) != TS_SUCCESS) {
    SDK_RPRINT(test, "TSUrlParse", "TestCase1", TC_FAIL, "Cannot create Url for parsing the url");
    if (TSMBufferDestroy(bufp) == TS_ERROR) {
      SDK_RPRINT(test, "TSUrlParse", "TestCase1", TC_FAIL, "Error in Destroying MBuffer");
    }
  } else {
    start = url;
    end = url + strlen(url) + 1;
    if ((retval = TSUrlParse(bufp, url_loc, &start, end)) == TS_PARSE_ERROR) {
      SDK_RPRINT(test, "TSUrlParse", "TestCase1", TC_FAIL, "TSUrlParse returns TS_PARSE_ERROR");
    } else {
      if (retval == TS_PARSE_DONE) {
        temp = TSUrlStringGet(bufp, url_loc, &length);
        if (strncmp(url, temp, length) == 0) {
          SDK_RPRINT(test, "TSUrlParse", "TestCase1", TC_PASS, "ok");
          test_passed_parse_url = true;
        } else {
          SDK_RPRINT(test, "TSUrlParse", "TestCase1", TC_FAIL, "Value's Mismatch");
        }
        TSfree(temp);
      } else {
        SDK_RPRINT(test, "TSHttpHdrParseReq", "TestCase1", TC_FAIL, "Parsing Error");
      }
    }
  }

  if (test_passed_parse_url != true) {
    *pstatus = REGRESSION_TEST_FAILED;
  } else {
    *pstatus = REGRESSION_TEST_PASSED;
  }

  TSUrlDestroy(bufp, url_loc);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  TSMBufferDestroy(bufp);

  return;
}

//////////////////////////////////////////////
//       SDK_API_TSTextLog
//
// Unit Test for APIs: TSTextLogObjectCreate
//                     TSTextLogObjectWrite
//                     TSTextLogObjectDestroy
//                     TSTextLogObjectFlush
//////////////////////////////////////////////
#define LOG_TEST_PATTERN "SDK team rocks"

typedef struct
{
  RegressionTest *test;
  int *pstatus;
  char *fullpath_logname;
  unsigned long magic;
} LogTestData;


static int
log_test_handler(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(edata);
  TSFile filep;
  char buf[1024];
  bool str_found;

  TSAssert(event == TS_EVENT_TIMEOUT);

  LogTestData *data = (LogTestData *) TSContDataGet(contp);
  TSAssert(data->magic == MAGIC_ALIVE);

  // Verify content was correctly written into log file

  if ((filep = TSfopen(data->fullpath_logname, "r")) == NULL) {
    SDK_RPRINT(data->test, "TSTextLogObject", "TestCase1", TC_FAIL, "can not open log file %s", data->fullpath_logname);
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    return -1;
  }

  str_found = false;
  while (TSfgets(filep, buf, 1024) != NULL) {
    if (strstr(buf, LOG_TEST_PATTERN) != NULL) {
      str_found = true;
      break;
    }
  }
  TSfclose(filep);
  if (str_found == false) {
    SDK_RPRINT(data->test, "TSTextLogObject", "TestCase1", TC_FAIL, "can not find pattern %s in log file", LOG_TEST_PATTERN);
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    return -1;
  }

  *(data->pstatus) = REGRESSION_TEST_PASSED;

  data->magic = MAGIC_DEAD;
  TSfree(data->fullpath_logname);
  TSfree(data);
  data = NULL;

  return -1;
}

REGRESSION_TEST(SDK_API_TSTextLog) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSTextLogObject log;
  TSReturnCode retVal;

  char logname[128];
  char fullpath_logname[128];

  /* Generate a random log file name, so if we run the test several times, we won't use the
     same log file name. */
  char *tmp = REC_ConfigReadString("proxy.config.log.logfile_dir");
  snprintf(logname, sizeof(logname), "RegressionTestLog%d.log", (int) getpid());
  snprintf(fullpath_logname, sizeof(fullpath_logname), "%s/%s", tmp, logname);
  // xfree(tmp);

  retVal = TSTextLogObjectCreate(logname, TS_LOG_MODE_ADD_TIMESTAMP, &log);
  if (retVal != TS_SUCCESS) {
    SDK_RPRINT(test, "TSTextLogObjectCreate", "TestCase1", TC_FAIL, "can not create log object");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  } else {
    SDK_RPRINT(test, "TSTextLogObjectCreate", "TestCase1", TC_PASS, "ok");
  }

  retVal = TSTextLogObjectWrite(log, (char*)LOG_TEST_PATTERN);
  if (retVal != TS_SUCCESS) {
    SDK_RPRINT(test, "TSTextLogObjectWrite", "TestCase1", TC_FAIL, "can not write to log object");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  } else {
    SDK_RPRINT(test, "TSTextLogObjectWrite", "TestCase1", TC_PASS, "ok");
  }

  TSTextLogObjectFlush(log);
  SDK_RPRINT(test, "TSTextLogObjectFlush", "TestCase1", TC_PASS, "ok");

  retVal = TSTextLogObjectDestroy(log);
  if (retVal != TS_SUCCESS) {
    SDK_RPRINT(test, "TSTextLogObjectDestroy", "TestCase1", TC_FAIL, "can not destroy log object");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  } else {
    SDK_RPRINT(test, "TSTextLogObjectDestroy", "TestCase1", TC_PASS, "ok");
  }


  TSCont log_test_cont = TSContCreate(log_test_handler, TSMutexCreate());
  LogTestData *data = (LogTestData *) TSmalloc(sizeof(LogTestData));
  data->test = test;
  data->pstatus = pstatus;
  data->fullpath_logname = TSstrdup(fullpath_logname);
  data->magic = MAGIC_ALIVE;
  TSContDataSet(log_test_cont, data);

  TSContSchedule(log_test_cont, 5000, TS_THREAD_POOL_DEFAULT);
  return;
}


//////////////////////////////////////////////
//       SDK_API_TSMgmtGet
//
// Unit Test for APIs: TSMgmtCounterGet
//                     TSMgmtFloatGet
//                     TSMgmtIntGet
//                     TSMgmtStringGet
//////////////////////////////////////////////

REGRESSION_TEST(SDK_API_TSMgmtGet) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  const char *CONFIG_PARAM_COUNTER_NAME = "proxy.process.http.total_parent_proxy_connections";
  int CONFIG_PARAM_COUNTER_VALUE = 0;

  const char *CONFIG_PARAM_FLOAT_NAME = "proxy.config.http.background_fill_completed_threshold";
  float CONFIG_PARAM_FLOAT_VALUE = 0.5;

  const char *CONFIG_PARAM_INT_NAME = "proxy.config.http.cache.http";
  int CONFIG_PARAM_INT_VALUE = 1;

  const char *CONFIG_PARAM_STRING_NAME = "proxy.config.product_name";
  const char *CONFIG_PARAM_STRING_VALUE = "Traffic Server";

  *pstatus = REGRESSION_TEST_INPROGRESS;

  int err = 0;
  TSMgmtCounter cvalue = 0;
  TSMgmtFloat fvalue = 0.0;
  TSMgmtInt ivalue = -1;
  TSMgmtString svalue = NULL;

  if (TS_SUCCESS != TSMgmtCounterGet(CONFIG_PARAM_COUNTER_NAME, &cvalue)) {
    SDK_RPRINT(test, "TSMgmtCounterGet", "TestCase1.1", TC_FAIL, "can not get value of param %s", CONFIG_PARAM_COUNTER_NAME);
    err = 1;
  } else if (cvalue != CONFIG_PARAM_COUNTER_VALUE) {
    SDK_RPRINT(test, "TSMgmtCounterGet", "TestCase1.1", TC_FAIL,
               "got incorrect value of param %s, should have been %d, found %d", CONFIG_PARAM_COUNTER_NAME,
               CONFIG_PARAM_COUNTER_VALUE, cvalue);
    err = 1;
  } else {
    SDK_RPRINT(test, "TSMgmtCounterGet", "TestCase1.1", TC_PASS, "ok");
  }

  if ((TS_SUCCESS != TSMgmtFloatGet(CONFIG_PARAM_FLOAT_NAME, &fvalue)) || (fvalue != CONFIG_PARAM_FLOAT_VALUE)) {
    SDK_RPRINT(test, "TSMgmtFloatGet", "TestCase2", TC_FAIL, "can not get value of param %s", CONFIG_PARAM_FLOAT_NAME);
    err = 1;
  } else {
    SDK_RPRINT(test, "TSMgmtFloatGet", "TestCase1.2", TC_PASS, "ok");
  }

  if ((TSMgmtIntGet(CONFIG_PARAM_INT_NAME, &ivalue) != TS_SUCCESS) || (ivalue != CONFIG_PARAM_INT_VALUE)) {
    SDK_RPRINT(test, "TSMgmtIntGet", "TestCase1.3", TC_FAIL, "can not get value of param %s", CONFIG_PARAM_INT_NAME);
    err = 1;
  } else {
    SDK_RPRINT(test, "TSMgmtIntGet", "TestCase1.3", TC_PASS, "ok");
  }

  if (TS_SUCCESS != TSMgmtStringGet(CONFIG_PARAM_STRING_NAME, &svalue)) {
    SDK_RPRINT(test, "TSMgmtStringGet", "TestCase1.4", TC_FAIL, "can not get value of param %s", CONFIG_PARAM_STRING_NAME);
    err = 1;
  } else if (strcmp(svalue, CONFIG_PARAM_STRING_VALUE) != 0) {
    SDK_RPRINT(test, "TSMgmtStringGet", "TestCase1.4", TC_FAIL,
               "got incorrect value of param %s, should have been \"%s\", found \"%s\"", CONFIG_PARAM_STRING_NAME,
               CONFIG_PARAM_STRING_VALUE, svalue);
    err = 1;
  } else {
    SDK_RPRINT(test, "TSMgmtStringGet", "TestCase1.4", TC_PASS, "ok");
  }

  if (err) {
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  *pstatus = REGRESSION_TEST_PASSED;
  SDK_RPRINT(test, "TSMgmtGet", "TestCase1", TC_PASS, "ok");
  return;
}


//////////////////////////////////////////////
//       SDK_API_TSConstant
//
// Unit Test for APIs: All TS_XXX constants
//
//////////////////////////////////////////////

#define PRINT_DIFF( _x ) \
{ \
      if ( _x - ORIG_##_x != 0) { \
          test_passed = false; \
          SDK_RPRINT (test, "##_x", "TestCase1", TC_FAIL, \
		      "%s:Original Value = %d; New Value = %d \n", #_x,_x, ORIG_##_x); \
      } \
}


typedef enum
{
  ORIG_TS_PARSE_ERROR = -1,
  ORIG_TS_PARSE_DONE = 0,
  ORIG_TS_PARSE_OK = 1,
  ORIG_TS_PARSE_CONT = 2
} ORIG_TSParseResult;

typedef enum
{
  ORIG_TS_HTTP_TYPE_UNKNOWN,
  ORIG_TS_HTTP_TYPE_REQUEST,
  ORIG_TS_HTTP_TYPE_RESPONSE
} ORIG_TSHttpType;

typedef enum
{
  ORIG_TS_HTTP_STATUS_NONE = 0,

  ORIG_TS_HTTP_STATUS_CONTINUE = 100,
  ORIG_TS_HTTP_STATUS_SWITCHING_PROTOCOL = 101,

  ORIG_TS_HTTP_STATUS_OK = 200,
  ORIG_TS_HTTP_STATUS_CREATED = 201,
  ORIG_TS_HTTP_STATUS_ACCEPTED = 202,
  ORIG_TS_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
  ORIG_TS_HTTP_STATUS_NO_CONTENT = 204,
  ORIG_TS_HTTP_STATUS_RESET_CONTENT = 205,
  ORIG_TS_HTTP_STATUS_PARTIAL_CONTENT = 206,

  ORIG_TS_HTTP_STATUS_MULTIPLE_CHOICES = 300,
  ORIG_TS_HTTP_STATUS_MOVED_PERMANENTLY = 301,
  ORIG_TS_HTTP_STATUS_MOVED_TEMPORARILY = 302,
  ORIG_TS_HTTP_STATUS_SEE_OTHER = 303,
  ORIG_TS_HTTP_STATUS_NOT_MODIFIED = 304,
  ORIG_TS_HTTP_STATUS_USE_PROXY = 305,

  ORIG_TS_HTTP_STATUS_BAD_REQUEST = 400,
  ORIG_TS_HTTP_STATUS_UNAUTHORIZED = 401,
  ORIG_TS_HTTP_STATUS_PAYMENT_REQUIRED = 402,
  ORIG_TS_HTTP_STATUS_FORBIDDEN = 403,
  ORIG_TS_HTTP_STATUS_NOT_FOUND = 404,
  ORIG_TS_HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
  ORIG_TS_HTTP_STATUS_NOT_ACCEPTABLE = 406,
  ORIG_TS_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
  ORIG_TS_HTTP_STATUS_REQUEST_TIMEOUT = 408,
  ORIG_TS_HTTP_STATUS_CONFLICT = 409,
  ORIG_TS_HTTP_STATUS_GONE = 410,
  ORIG_TS_HTTP_STATUS_LENGTH_REQUIRED = 411,
  ORIG_TS_HTTP_STATUS_PRECONDITION_FAILED = 412,
  ORIG_TS_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE = 413,
  ORIG_TS_HTTP_STATUS_REQUEST_URI_TOO_LONG = 414,
  ORIG_TS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,

  ORIG_TS_HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
  ORIG_TS_HTTP_STATUS_NOT_IMPLEMENTED = 501,
  ORIG_TS_HTTP_STATUS_BAD_GATEWAY = 502,
  ORIG_TS_HTTP_STATUS_SERVICE_UNAVAILABLE = 503,
  ORIG_TS_HTTP_STATUS_GATEWAY_TIMEOUT = 504,
  ORIG_TS_HTTP_STATUS_HTTPVER_NOT_SUPPORTED = 505
} ORIG_TSHttpStatus;

typedef enum
{
  ORIG_TS_HTTP_READ_REQUEST_HDR_HOOK,
  ORIG_TS_HTTP_OS_DNS_HOOK,
  ORIG_TS_HTTP_SEND_REQUEST_HDR_HOOK,
  ORIG_TS_HTTP_READ_CACHE_HDR_HOOK,
  ORIG_TS_HTTP_READ_RESPONSE_HDR_HOOK,
  ORIG_TS_HTTP_SEND_RESPONSE_HDR_HOOK,
  ORIG_TS_HTTP_REQUEST_TRANSFORM_HOOK,
  ORIG_TS_HTTP_RESPONSE_TRANSFORM_HOOK,
  ORIG_TS_HTTP_SELECT_ALT_HOOK,
  ORIG_TS_HTTP_TXN_START_HOOK,
  ORIG_TS_HTTP_TXN_CLOSE_HOOK,
  ORIG_TS_HTTP_SSN_START_HOOK,
  ORIG_TS_HTTP_SSN_CLOSE_HOOK,
  ORIG_TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK,
  ORIG_TS_HTTP_PRE_REMAP_HOOK,
  ORIG_TS_HTTP_POST_REMAP_HOOK,
  ORIG_TS_HTTP_LAST_HOOK
} ORIG_TSHttpHookID;

typedef enum
{
  ORIG_TS_EVENT_NONE = 0,
  ORIG_TS_EVENT_IMMEDIATE = 1,
  ORIG_TS_EVENT_TIMEOUT = 2,
  ORIG_TS_EVENT_ERROR = 3,
  ORIG_TS_EVENT_CONTINUE = 4,

  ORIG_TS_EVENT_VCONN_READ_READY = 100,
  ORIG_TS_EVENT_VCONN_WRITE_READY = 101,
  ORIG_TS_EVENT_VCONN_READ_COMPLETE = 102,
  ORIG_TS_EVENT_VCONN_WRITE_COMPLETE = 103,
  ORIG_TS_EVENT_VCONN_EOS = 104,

  ORIG_TS_EVENT_NET_CONNECT = 200,
  ORIG_TS_EVENT_NET_CONNECT_FAILED = 201,
  ORIG_TS_EVENT_NET_ACCEPT = 202,
  ORIG_TS_EVENT_NET_ACCEPT_FAILED = 204,

  ORIG_TS_EVENT_HOST_LOOKUP = 500,

  ORIG_TS_EVENT_CACHE_OPEN_READ = 1102,
  ORIG_TS_EVENT_CACHE_OPEN_READ_FAILED = 1103,
  ORIG_TS_EVENT_CACHE_OPEN_WRITE = 1108,
  ORIG_TS_EVENT_CACHE_OPEN_WRITE_FAILED = 1109,
  ORIG_TS_EVENT_CACHE_REMOVE = 1112,
  ORIG_TS_EVENT_CACHE_REMOVE_FAILED = 1113,
  ORIG_TS_EVENT_CACHE_SCAN = 1120,
  ORIG_TS_EVENT_CACHE_SCAN_FAILED = 1121,
  ORIG_TS_EVENT_CACHE_SCAN_OBJECT = 1122,
  ORIG_TS_EVENT_CACHE_SCAN_OPERATION_BLOCKED = 1123,
  ORIG_TS_EVENT_CACHE_SCAN_OPERATION_FAILED = 1124,
  ORIG_TS_EVENT_CACHE_SCAN_DONE = 1125,

  ORIG_TS_EVENT_HTTP_CONTINUE = 60000,
  ORIG_TS_EVENT_HTTP_ERROR = 60001,
  ORIG_TS_EVENT_HTTP_READ_REQUEST_HDR = 60002,
  ORIG_TS_EVENT_HTTP_OS_DNS = 60003,
  ORIG_TS_EVENT_HTTP_SEND_REQUEST_HDR = 60004,
  ORIG_TS_EVENT_HTTP_READ_CACHE_HDR = 60005,
  ORIG_TS_EVENT_HTTP_READ_RESPONSE_HDR = 60006,
  ORIG_TS_EVENT_HTTP_SEND_RESPONSE_HDR = 60007,
  ORIG_TS_EVENT_HTTP_REQUEST_TRANSFORM = 60008,
  ORIG_TS_EVENT_HTTP_RESPONSE_TRANSFORM = 60009,
  ORIG_TS_EVENT_HTTP_SELECT_ALT = 60010,
  ORIG_TS_EVENT_HTTP_TXN_START = 60011,
  ORIG_TS_EVENT_HTTP_TXN_CLOSE = 60012,
  ORIG_TS_EVENT_HTTP_SSN_START = 60013,
  ORIG_TS_EVENT_HTTP_SSN_CLOSE = 60014,
  ORIG_TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE = 60015,

  ORIG_TS_EVENT_MGMT_UPDATE = 60100
} ORIG_TSEvent;

typedef enum
{
  ORIG_TS_CACHE_LOOKUP_MISS,
  ORIG_TS_CACHE_LOOKUP_HIT_STALE,
  ORIG_TS_CACHE_LOOKUP_HIT_FRESH
} ORIG_TSCacheLookupResult;

typedef enum
{
  ORIG_TS_CACHE_DATA_TYPE_NONE,
  ORIG_TS_CACHE_DATA_TYPE_HTTP,
  ORIG_TS_CACHE_DATA_TYPE_OTHER
} ORIG_TSCacheDataType;

typedef enum
{
  ORIG_TS_CACHE_ERROR_NO_DOC = -20400,
  ORIG_TS_CACHE_ERROR_DOC_BUSY = -20401,
  ORIG_TS_CACHE_ERROR_NOT_READY = -20407
} ORIG_TSCacheError;

typedef enum
{
  ORIG_TS_CACHE_SCAN_RESULT_DONE = 0,
  ORIG_TS_CACHE_SCAN_RESULT_CONTINUE = 1,
  ORIG_TS_CACHE_SCAN_RESULT_DELETE = 10,
  ORIG_TS_CACHE_SCAN_RESULT_DELETE_ALL_ALTERNATES,
  ORIG_TS_CACHE_SCAN_RESULT_UPDATE,
  ORIG_TS_CACHE_SCAN_RESULT_RETRY
} ORIG_TSCacheScanResult;

typedef enum
{
  ORIG_TS_DATA_ALLOCATE,
  ORIG_TS_DATA_MALLOCED,
  ORIG_TS_DATA_CONSTANT
} ORIG_TSIOBufferDataFlags;

typedef enum
{
  ORIG_TS_VC_CLOSE_ABORT = -1,
  ORIG_TS_VC_CLOSE_NORMAL = 1
} ORIG_TSVConnCloseFlags;

typedef enum
{
  ORIG_TS_SDK_VERSION_2_0 = 0,
  ORIG_TS_SDK_VERSION_3_0
} ORIG_TSSDKVersion;

typedef enum
{
  ORIG_TS_ERROR = -1,
  ORIG_TS_SUCCESS = 0
} ORIG_TSReturnCode;


REGRESSION_TEST(SDK_API_TSConstant) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;
  bool test_passed = true;

  PRINT_DIFF(TS_PARSE_ERROR);
  PRINT_DIFF(TS_PARSE_DONE);
  PRINT_DIFF(TS_PARSE_OK);
  PRINT_DIFF(TS_PARSE_CONT);

  PRINT_DIFF(TS_HTTP_STATUS_NONE);
  PRINT_DIFF(TS_HTTP_STATUS_CONTINUE);
  PRINT_DIFF(TS_HTTP_STATUS_SWITCHING_PROTOCOL);
  PRINT_DIFF(TS_HTTP_STATUS_OK);
  PRINT_DIFF(TS_HTTP_STATUS_CREATED);


  PRINT_DIFF(TS_HTTP_STATUS_ACCEPTED);
  PRINT_DIFF(TS_HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION);
  PRINT_DIFF(TS_HTTP_STATUS_NO_CONTENT);
  PRINT_DIFF(TS_HTTP_STATUS_RESET_CONTENT);
  PRINT_DIFF(TS_HTTP_STATUS_PARTIAL_CONTENT);

  PRINT_DIFF(TS_HTTP_STATUS_MULTIPLE_CHOICES);
  PRINT_DIFF(TS_HTTP_STATUS_MOVED_PERMANENTLY);
  PRINT_DIFF(TS_HTTP_STATUS_MOVED_TEMPORARILY);
  PRINT_DIFF(TS_HTTP_STATUS_SEE_OTHER);
  PRINT_DIFF(TS_HTTP_STATUS_NOT_MODIFIED);
  PRINT_DIFF(TS_HTTP_STATUS_USE_PROXY);
  PRINT_DIFF(TS_HTTP_STATUS_BAD_REQUEST);
  PRINT_DIFF(TS_HTTP_STATUS_UNAUTHORIZED);
  PRINT_DIFF(TS_HTTP_STATUS_FORBIDDEN);
  PRINT_DIFF(TS_HTTP_STATUS_NOT_FOUND);
  PRINT_DIFF(TS_HTTP_STATUS_METHOD_NOT_ALLOWED);
  PRINT_DIFF(TS_HTTP_STATUS_NOT_ACCEPTABLE);
  PRINT_DIFF(TS_HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED);
  PRINT_DIFF(TS_HTTP_STATUS_REQUEST_TIMEOUT);
  PRINT_DIFF(TS_HTTP_STATUS_CONFLICT);
  PRINT_DIFF(TS_HTTP_STATUS_GONE);
  PRINT_DIFF(TS_HTTP_STATUS_PRECONDITION_FAILED);
  PRINT_DIFF(TS_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE);
  PRINT_DIFF(TS_HTTP_STATUS_REQUEST_URI_TOO_LONG);
  PRINT_DIFF(TS_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE);
  PRINT_DIFF(TS_HTTP_STATUS_INTERNAL_SERVER_ERROR);
  PRINT_DIFF(TS_HTTP_STATUS_NOT_IMPLEMENTED);
  PRINT_DIFF(TS_HTTP_STATUS_BAD_GATEWAY);
  PRINT_DIFF(TS_HTTP_STATUS_GATEWAY_TIMEOUT);
  PRINT_DIFF(TS_HTTP_STATUS_HTTPVER_NOT_SUPPORTED);

  PRINT_DIFF(TS_HTTP_READ_REQUEST_HDR_HOOK);
  PRINT_DIFF(TS_HTTP_OS_DNS_HOOK);
  PRINT_DIFF(TS_HTTP_SEND_REQUEST_HDR_HOOK);
  PRINT_DIFF(TS_HTTP_READ_RESPONSE_HDR_HOOK);
  PRINT_DIFF(TS_HTTP_SEND_RESPONSE_HDR_HOOK);
  PRINT_DIFF(TS_HTTP_REQUEST_TRANSFORM_HOOK);
  PRINT_DIFF(TS_HTTP_RESPONSE_TRANSFORM_HOOK);
  PRINT_DIFF(TS_HTTP_SELECT_ALT_HOOK);
  PRINT_DIFF(TS_HTTP_TXN_START_HOOK);
  PRINT_DIFF(TS_HTTP_TXN_CLOSE_HOOK);
  PRINT_DIFF(TS_HTTP_SSN_START_HOOK);
  PRINT_DIFF(TS_HTTP_SSN_CLOSE_HOOK);
  PRINT_DIFF(TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK);
  PRINT_DIFF(TS_HTTP_LAST_HOOK);

  PRINT_DIFF(TS_EVENT_NONE);
  PRINT_DIFF(TS_EVENT_IMMEDIATE);
  PRINT_DIFF(TS_EVENT_TIMEOUT);
  PRINT_DIFF(TS_EVENT_ERROR);

  PRINT_DIFF(TS_EVENT_CONTINUE);
  PRINT_DIFF(TS_EVENT_VCONN_READ_READY);
  PRINT_DIFF(TS_EVENT_VCONN_WRITE_READY);
  PRINT_DIFF(TS_EVENT_VCONN_READ_COMPLETE);
  PRINT_DIFF(TS_EVENT_VCONN_WRITE_COMPLETE);
  PRINT_DIFF(TS_EVENT_VCONN_EOS);

  PRINT_DIFF(TS_EVENT_NET_CONNECT);
  PRINT_DIFF(TS_EVENT_NET_CONNECT_FAILED);
  PRINT_DIFF(TS_EVENT_NET_ACCEPT);
  PRINT_DIFF(TS_EVENT_NET_ACCEPT_FAILED);

  PRINT_DIFF(TS_EVENT_HOST_LOOKUP);

  PRINT_DIFF(TS_EVENT_CACHE_OPEN_READ);
  PRINT_DIFF(TS_EVENT_CACHE_OPEN_READ_FAILED);
  PRINT_DIFF(TS_EVENT_CACHE_OPEN_WRITE);
  PRINT_DIFF(TS_EVENT_CACHE_OPEN_WRITE_FAILED);
  PRINT_DIFF(TS_EVENT_CACHE_REMOVE);
  PRINT_DIFF(TS_EVENT_CACHE_REMOVE_FAILED);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN_FAILED);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN_OBJECT);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN_OPERATION_BLOCKED);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN_OPERATION_FAILED);
  PRINT_DIFF(TS_EVENT_CACHE_SCAN_DONE);

  PRINT_DIFF(TS_EVENT_HTTP_CONTINUE);
  PRINT_DIFF(TS_EVENT_HTTP_ERROR);
  PRINT_DIFF(TS_EVENT_HTTP_READ_REQUEST_HDR);
  PRINT_DIFF(TS_EVENT_HTTP_OS_DNS);
  PRINT_DIFF(TS_EVENT_HTTP_SEND_REQUEST_HDR);
  PRINT_DIFF(TS_EVENT_HTTP_READ_CACHE_HDR);
  PRINT_DIFF(TS_EVENT_HTTP_READ_RESPONSE_HDR);
  PRINT_DIFF(TS_EVENT_HTTP_SEND_RESPONSE_HDR);
  PRINT_DIFF(TS_EVENT_HTTP_REQUEST_TRANSFORM);
  PRINT_DIFF(TS_EVENT_HTTP_RESPONSE_TRANSFORM);
  PRINT_DIFF(TS_EVENT_HTTP_SELECT_ALT);
  PRINT_DIFF(TS_EVENT_HTTP_TXN_START);
  PRINT_DIFF(TS_EVENT_HTTP_TXN_CLOSE);
  PRINT_DIFF(TS_EVENT_HTTP_SSN_START);
  PRINT_DIFF(TS_EVENT_HTTP_SSN_CLOSE);
  PRINT_DIFF(TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE);

  PRINT_DIFF(TS_EVENT_MGMT_UPDATE);

  PRINT_DIFF(TS_CACHE_LOOKUP_MISS);
  PRINT_DIFF(TS_CACHE_LOOKUP_HIT_STALE);
  PRINT_DIFF(TS_CACHE_LOOKUP_HIT_FRESH);

  PRINT_DIFF(TS_CACHE_DATA_TYPE_NONE);
  PRINT_DIFF(TS_CACHE_DATA_TYPE_HTTP);
  PRINT_DIFF(TS_CACHE_DATA_TYPE_OTHER);

  PRINT_DIFF(TS_CACHE_ERROR_NO_DOC);
  PRINT_DIFF(TS_CACHE_ERROR_DOC_BUSY);
  PRINT_DIFF(TS_CACHE_ERROR_NOT_READY);

  PRINT_DIFF(TS_CACHE_SCAN_RESULT_DONE);
  PRINT_DIFF(TS_CACHE_SCAN_RESULT_CONTINUE);
  PRINT_DIFF(TS_CACHE_SCAN_RESULT_DELETE);
  PRINT_DIFF(TS_CACHE_SCAN_RESULT_DELETE_ALL_ALTERNATES);
  PRINT_DIFF(TS_CACHE_SCAN_RESULT_UPDATE);
  PRINT_DIFF(TS_CACHE_SCAN_RESULT_RETRY);

  PRINT_DIFF(TS_DATA_ALLOCATE);
  PRINT_DIFF(TS_DATA_MALLOCED);
  PRINT_DIFF(TS_DATA_CONSTANT);

  PRINT_DIFF(TS_VC_CLOSE_ABORT);
  PRINT_DIFF(TS_VC_CLOSE_NORMAL);

  PRINT_DIFF(TS_SDK_VERSION_2_0);
  PRINT_DIFF(TS_SDK_VERSION_3_0);

  PRINT_DIFF(TS_ERROR);
  PRINT_DIFF(TS_SUCCESS);


  if (test_passed) {
    *pstatus = REGRESSION_TEST_PASSED;
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

}

//////////////////////////////////////////////
//       SDK_API_TSHttpSsn
//
// Unit Test for API: TSHttpSsnHookAdd
//                    TSHttpSsnReenable
//                    TSHttpTxnHookAdd
//                    TSHttpTxnErrorBodySet
//                    TSHttpTxnParentProxyGet
//                    TSHttpTxnParentProxySet
//////////////////////////////////////////////


typedef struct
{
  RegressionTest *test;
  int *pstatus;
  SocketServer *os;
  ClientTxn *browser;
  TSHttpSsn ssnp;
  int test_passed_ssn_hook_add;
  int test_passed_ssn_reenable;
  int test_passed_txn_ssn_get;
  int test_passed_txn_hook_add;
  int test_passed_txn_error_body_set;
  bool test_passed_Parent_Proxy;
  int magic;
} ContData;

static int
checkHttpTxnParentProxy(ContData * data, TSHttpTxn txnp)
{

  const char *hostname = "txnpp.example.com";
  int port = 10180;
  char *hostnameget = NULL;
  int portget = 0;

  TSHttpTxnParentProxySet(txnp, (char*)hostname, port);
  if (TSHttpTxnParentProxyGet(txnp, &hostnameget, &portget) != TS_SUCCESS) {
    SDK_RPRINT(data->test, "TSHttpTxnParentProxySet", "TestCase1", TC_FAIL, "TSHttpTxnParentProxyGet doesn't return TS_SUCCESS");
    SDK_RPRINT(data->test, "TSHttpTxnParentProxyGet", "TestCase1", TC_FAIL, "TSHttpTxnParentProxyGet doesn't return TS_SUCCESS");
    return TS_EVENT_CONTINUE;
  }

  if ((strcmp(hostname, hostnameget) == 0) && (port == portget)) {
    SDK_RPRINT(data->test, "TSHttpTxnParentProxySet", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(data->test, "TSHttpTxnParentProxyGet", "TestCase1", TC_PASS, "ok");
    data->test_passed_Parent_Proxy = true;
  } else {
    SDK_RPRINT(data->test, "TSHttpTxnParentProxySet", "TestCase1", TC_FAIL, "Value's Mismatch");
    SDK_RPRINT(data->test, "TSHttpTxnParentProxyGet", "TestCase1", TC_FAIL, "Value's Mismatch");
  }

  return TS_EVENT_CONTINUE;
}


static int
ssn_handler(TSCont contp, TSEvent event, void *edata)
{

  TSHttpTxn txnp = NULL;
  ContData *data = NULL;
  data = (ContData *)TSContDataGet(contp);
  if (data == NULL) {
    switch (event) {
    case TS_EVENT_HTTP_SSN_START:
      TSHttpSsnReenable((TSHttpSsn) edata, TS_EVENT_HTTP_CONTINUE);
      break;
    case TS_EVENT_IMMEDIATE:
    case TS_EVENT_TIMEOUT:
      break;
    case TS_EVENT_HTTP_TXN_START:
    default:
      TSHttpTxnReenable((TSHttpTxn) edata, TS_EVENT_HTTP_CONTINUE);
      break;
    }
    return 0;
  }

  switch (event) {
  case TS_EVENT_HTTP_SSN_START:
    data->ssnp = (TSHttpSsn) edata;
    TSHttpSsnHookAdd(data->ssnp, TS_HTTP_TXN_START_HOOK, contp);
    TSHttpSsnReenable(data->ssnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_HTTP_TXN_START:
    TSSkipRemappingSet((TSHttpTxn) edata,1);
    SDK_RPRINT(data->test, "TSHttpSsnReenable", "TestCase", TC_PASS, "ok");
    data->test_passed_ssn_reenable++;
    {
      txnp = (TSHttpTxn) edata;
      TSHttpSsn ssnp = TSHttpTxnSsnGet(txnp);
      if (ssnp != data->ssnp) {
        SDK_RPRINT(data->test, "TSHttpSsnHookAdd", "TestCase", TC_FAIL, "Value's mismatch");
        data->test_passed_ssn_hook_add--;
        SDK_RPRINT(data->test, "TSHttpTxnSsnGet", "TestCase", TC_FAIL, "Session doesn't match");
        data->test_passed_txn_ssn_get--;
      } else {
        SDK_RPRINT(data->test, "TSHttpSsnHookAdd", "TestCase1", TC_PASS, "ok");
        data->test_passed_ssn_hook_add++;
        SDK_RPRINT(data->test, "TSHttpTxnSsnGet", "TestCase1", TC_PASS, "ok");
        data->test_passed_txn_ssn_get++;
      }
      TSHttpTxnHookAdd(txnp, TS_HTTP_OS_DNS_HOOK, contp);
      TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    }
    break;

  case TS_EVENT_HTTP_OS_DNS:
    SDK_RPRINT(data->test, "TSHttpTxnHookAdd", "TestCase1", TC_PASS, "ok");
    data->test_passed_txn_hook_add++;
    txnp = (TSHttpTxn) edata;

    TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
    checkHttpTxnParentProxy(data, txnp);

    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
    break;

  case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
    SDK_RPRINT(data->test, "TSHttpTxnHookAdd", "TestCase2", TC_PASS, "ok");
    data->test_passed_txn_hook_add++;
    txnp = (TSHttpTxn) edata;
    if (1) {
      char *temp = TSstrdup(ERROR_BODY);
      TSHttpTxnErrorBodySet(txnp, temp, strlen(temp), NULL);
    }
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_IMMEDIATE:
  case TS_EVENT_TIMEOUT:
    /* Browser still waiting the response ? */
    if (data->browser->status == REQUEST_INPROGRESS) {
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
    }
    /* Browser got the response. test is over. clean up */
    else {
      /* Check if browser response body is the one we expected */
      char *temp = data->browser->response;
      temp = strstr(temp, "\r\n\r\n");
      if (temp != NULL) {
        temp += strlen("\r\n\r\n");
        if ((temp[0] == '\0') || (strncmp(temp, "\r\n\r\n", 4) == 0)) {
          SDK_RPRINT(data->test, "TSHttpTxnErrorBodySet", "TestCase1", TC_FAIL, "No Error Body found");
          data->test_passed_txn_error_body_set--;
        }
        if (strncmp(temp, ERROR_BODY, strlen(ERROR_BODY)) == 0) {
          SDK_RPRINT(data->test, "TSHttpTxnErrorBodySet", "TestCase1", TC_PASS, "ok");
          data->test_passed_txn_error_body_set++;
        }
      } else {
        SDK_RPRINT(data->test, "TSHttpTxnErrorBodySet", "TestCase1", TC_FAIL, "strstr returns NULL. Didn't find end of headers.");
        data->test_passed_txn_error_body_set--;
      }

      /* Note: response is available using test->browser->response pointer */
      if ((data->browser->status == REQUEST_SUCCESS) &&
          (data->test_passed_ssn_hook_add == 1) &&
          (data->test_passed_ssn_reenable == 1) &&
          (data->test_passed_txn_ssn_get == 1) &&
          (data->test_passed_txn_hook_add == 2) && (data->test_passed_txn_error_body_set == 1)
          && (data->test_passed_Parent_Proxy == true)) {
        *(data->pstatus) = REGRESSION_TEST_PASSED;
      } else {
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }

      // transaction is over. clean up.
      synclient_txn_delete(data->browser);
      /* Don't need it as didn't initialize the server
         synserver_delete(data->os);
       */
      data->magic = MAGIC_DEAD;
      TSfree(data);
      TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(data->test, "TSHttpSsn", "TestCase1", TC_FAIL, "Unexpected event %d", event);
    break;
  }
  return 0;
}


EXCLUSIVE_REGRESSION_TEST(SDK_API_HttpSsn) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSCont cont = TSContCreate(ssn_handler, TSMutexCreate());
  if (cont == NULL) {
    SDK_RPRINT(test, "TSHttpSsn", "TestCase1", TC_FAIL, "Unable to create Continuation.");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  ContData *socktest = (ContData *) TSmalloc(sizeof(ContData));
  socktest->test = test;
  socktest->pstatus = pstatus;
  socktest->test_passed_ssn_hook_add = 0;
  socktest->test_passed_ssn_reenable = 0;
  socktest->test_passed_txn_ssn_get = 0;
  socktest->test_passed_txn_hook_add = 0;
  socktest->test_passed_txn_error_body_set = 0;
  socktest->test_passed_Parent_Proxy = false;
  socktest->magic = MAGIC_ALIVE;
  TSContDataSet(cont, socktest);

  /* Register to HTTP hooks that are called in case of a cache MISS */
  TSHttpHookAdd(TS_HTTP_SSN_START_HOOK, cont);

  /* Create a client transaction */
  socktest->browser = synclient_txn_create();
  char *request = generate_request(3);  // response is expected to be error case
  synclient_txn_send_request(socktest->browser, request);
  TSfree(request);

  /* Wait until transaction is done */
  if (socktest->browser->status == REQUEST_INPROGRESS) {
    TSContSchedule(cont, 25, TS_THREAD_POOL_DEFAULT);
  }

  return;
}

/////////////////////////////////////////////////////
//       SDK_API_TSHttpTxnCache
//
// Unit Test for API: TSHttpTxnCachedReqGet
//                    TSHttpTxnCachedRespGet
//                    TSHttpTxnCacheLookupStatusGet
/////////////////////////////////////////////////////

typedef struct
{
  RegressionTest *test;
  int *pstatus;
  SocketServer *os;
  ClientTxn *browser1;
  ClientTxn *browser2;
  char *request;
  bool test_passed_txn_cached_req_get;
  bool test_passed_txn_cached_resp_get;
  bool test_passed_txn_cache_lookup_status;
  bool first_time;
  int magic;
} CacheTestData;

static int
cache_hook_handler(TSCont contp, TSEvent event, void *edata)
{

  TSHttpTxn txnp = NULL;
  CacheTestData *data = (CacheTestData *)TSContDataGet(contp);

  if (data == NULL) {
    switch (event) {
    case TS_EVENT_IMMEDIATE:
    case TS_EVENT_TIMEOUT:
      break;
    case TS_EVENT_HTTP_READ_CACHE_HDR:
    default:
      TSHttpTxnReenable((TSHttpTxn) edata, TS_EVENT_HTTP_CONTINUE);
      break;
    }
    return 0;
  }

  switch (event) {
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    txnp = (TSHttpTxn) edata;
    TSSkipRemappingSet(txnp,1);
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;
  
  
  case TS_EVENT_HTTP_CACHE_LOOKUP_COMPLETE:
    {
      int lookup_status;
      if (data->first_time == true) {
        txnp = (TSHttpTxn) edata;
        if (TSHttpTxnCacheLookupStatusGet(txnp, &lookup_status) != TS_SUCCESS) {
          SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase1", TC_FAIL,
                     "TSHttpTxnCacheLookupStatus doesn't return TS_SUCCESS");
        } else {
          if (lookup_status == TS_CACHE_LOOKUP_MISS) {
            SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase1", TC_PASS, "ok");
            data->test_passed_txn_cache_lookup_status = true;
          } else {
            SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase1", TC_FAIL,
                       "Incorrect Value returned by TSHttpTxnCacheLookupStatusGet");
          }
        }
      } else {
        txnp = (TSHttpTxn) edata;
        if (TSHttpTxnCacheLookupStatusGet(txnp, &lookup_status) != TS_SUCCESS) {
          SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase2", TC_FAIL,
                     "TSHttpTxnCacheLookupStatus doesn't return TS_SUCCESS");
          data->test_passed_txn_cache_lookup_status = false;
        } else {
          if (lookup_status == TS_CACHE_LOOKUP_HIT_FRESH) {
            SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase2", TC_PASS, "ok");
          } else {
            SDK_RPRINT(data->test, "TSHttpTxnCacheLookupStatusGet", "TestCase2", TC_FAIL,
                       "Incorrect Value returned by TSHttpTxnCacheLookupStatusGet");
            data->test_passed_txn_cache_lookup_status = false;
          }
        }
      }
      TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    }
    break;
  case TS_EVENT_HTTP_READ_CACHE_HDR:
    {
      TSMBuffer reqbuf;
      TSMBuffer respbuf;

      TSMLoc reqhdr;
      TSMLoc resphdr;

      txnp = (TSHttpTxn) edata;

      if (TSHttpTxnCachedReqGet(txnp, &reqbuf, &reqhdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpTxnCachedReqGet", "TestCase1", TC_FAIL, "TSHttpTxnCachedReqGet returns 0");
      } else {
        if ((reqbuf == reinterpret_cast<TSMBuffer>(((HttpSM *) txnp)->t_state.cache_req_hdr_heap_handle)) &&
            (reqhdr == reinterpret_cast<TSMLoc>((((HttpSM *) txnp)->t_state.cache_info.object_read->request_get())->m_http))) {
          SDK_RPRINT(data->test, "TSHttpTxnCachedReqGet", "TestCase1", TC_PASS, "ok");
          data->test_passed_txn_cached_req_get = true;
        } else {
          SDK_RPRINT(data->test, "TSHttpTxnCachedReqGet", "TestCase1", TC_FAIL, "Value's Mismatch");
        }
      }

      if (TSHttpTxnCachedRespGet(txnp, &respbuf, &resphdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpTxnCachedRespGet", "TestCase1", TC_FAIL, "TSHttpTxnCachedRespGet returns 0");
      } else {
        if ((respbuf == reinterpret_cast<TSMBuffer>(((HttpSM *) txnp)->t_state.cache_resp_hdr_heap_handle)) &&
            (resphdr == reinterpret_cast<TSMLoc>((((HttpSM *) txnp)->t_state.cache_info.object_read->response_get())->m_http))) {
          SDK_RPRINT(data->test, "TSHttpTxnCachedRespGet", "TestCase1", TC_PASS, "ok");
          data->test_passed_txn_cached_resp_get = true;
        } else {
          SDK_RPRINT(data->test, "TSHttpTxnCachedRespGet", "TestCase1", TC_FAIL, "Value's Mismatch");
        }
      }

      if ((TSHandleMLocRelease(reqbuf, TS_NULL_MLOC, reqhdr) != TS_SUCCESS) ||
          (TSHandleMLocRelease(respbuf, TS_NULL_MLOC, resphdr) != TS_SUCCESS)) {
        SDK_RPRINT(data->test, "TSHttpTxnCache", "", TC_FAIL, "Unable to release handle to headers.");
      }

      TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    }

    break;

  case TS_EVENT_IMMEDIATE:
  case TS_EVENT_TIMEOUT:
    /* Browser still waiting the response ? */
    if (data->first_time == true) {
      if (data->browser1->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
    } else {
      if (data->browser2->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
    }

    /* Browser got the response. test is over. clean up */
    {
      /* If this is the first time, then the response is in cache and we should make */
      /* another request to get cache hit */
      if (data->first_time == true) {
        data->first_time = false;
        /* Kill the origin server */
        synserver_delete(data->os);
        /* Send another similar client request */
        synclient_txn_send_request(data->browser2, data->request);
        TSfree(data->request);
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }

      /* Note: response is available using test->browser->response pointer */
      if ((data->browser1->status == REQUEST_SUCCESS) &&
          (data->browser2->status == REQUEST_SUCCESS) &&
          (data->test_passed_txn_cached_req_get == true) &&
          (data->test_passed_txn_cached_resp_get == true) && (data->test_passed_txn_cache_lookup_status == true)) {
        *(data->pstatus) = REGRESSION_TEST_PASSED;
      } else {
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }

      // transaction is over. clean up.
      synclient_txn_delete(data->browser1);
      synclient_txn_delete(data->browser2);

      data->magic = MAGIC_DEAD;
      TSfree(data);
      TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(data->test, "TSHttpTxnCache", "TestCase1", TC_FAIL, "Unexpected event %d", event);
    break;
  }
  return 0;
}


EXCLUSIVE_REGRESSION_TEST(SDK_API_HttpTxnCache) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSCont cont = TSContCreate(cache_hook_handler, TSMutexCreate());

  if (cont == NULL) {
    SDK_RPRINT(test, "TSHttpSsn", "TestCase1", TC_FAIL, "Unable to create Continuation.");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  CacheTestData *socktest = (CacheTestData *) TSmalloc(sizeof(CacheTestData));
  socktest->test = test;
  socktest->pstatus = pstatus;
  socktest->test_passed_txn_cached_req_get = false;
  socktest->test_passed_txn_cached_resp_get = false;
  socktest->first_time = true;
  socktest->magic = MAGIC_ALIVE;
  TSContDataSet(cont, socktest);
  
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont);
  /* Register to HTTP hooks that are called in case of a cache MISS */
  TSHttpHookAdd(TS_HTTP_READ_CACHE_HDR_HOOK, cont);
  TSHttpHookAdd(TS_HTTP_CACHE_LOOKUP_COMPLETE_HOOK, cont);

  /* Create a new synthetic server */
  socktest->os = synserver_create(SYNSERVER_LISTEN_PORT);
  synserver_start(socktest->os);

  /* Create a client transaction */
  socktest->browser1 = synclient_txn_create();
  socktest->browser2 = synclient_txn_create();
  socktest->request = generate_request(2);
  synclient_txn_send_request(socktest->browser1, socktest->request);

  /* Wait until transaction is done */
  TSContSchedule(cont, 25, TS_THREAD_POOL_DEFAULT);

  return;
}

///////////////////////////////////////////////////////
//       SDK_API_TSHttpTxnTransform
//
// Unit Test for API: TSHttpTxnTransformRespGet
//                    TSHttpTxnTransformedRespCache
//                    TSHttpTxnUntransformedRespCache
///////////////////////////////////////////////////////

/** Append Transform Data Structure **/
typedef struct
{
  TSVIO output_vio;
  TSIOBuffer output_buffer;
  TSIOBufferReader output_reader;
  int append_needed;
} MyTransformData;
/** Append Transform Data Structure Ends **/

typedef struct
{
  RegressionTest *test;
  int *pstatus;
  SocketServer *os;
  ClientTxn *browser1;
  ClientTxn *browser2;
  ClientTxn *browser3;
  ClientTxn *browser4;
  char *request1;
  char *request2;
  bool test_passed_txn_transform_resp_get;
  bool test_passed_txn_transformed_resp_cache;
  bool test_passed_txn_untransformed_resp_cache;
  bool test_passed_transform_create;
  int req_no;
  MyTransformData *transformData;
  int magic;
} TransformTestData;

/**** Append Transform Code (Tailored to needs)****/

static TSIOBuffer append_buffer;
static TSIOBufferReader append_buffer_reader;
static int64_t append_buffer_length;

static MyTransformData *
my_data_alloc()
{
  MyTransformData *data;

  data = (MyTransformData *) TSmalloc(sizeof(MyTransformData));
  data->output_vio = NULL;
  data->output_buffer = NULL;
  data->output_reader = NULL;
  data->append_needed = 1;

  return data;
}

static void
my_data_destroy(MyTransformData * data)
{
  if (data) {
    if (data->output_buffer) {
      TSIOBufferDestroy(data->output_buffer);
    }
    TSfree(data);
  }
}

static void
handle_transform(TSCont contp)
{
  TSVConn output_conn;
  TSVIO write_vio;
  TransformTestData *contData;
  MyTransformData *data;
  int64_t towrite;
  int64_t avail;

  /* Get the output connection where we'll write data to. */
  output_conn = TSTransformOutputVConnGet(contp);

  /* Get the write VIO for the write operation that was performed on
     ourself. This VIO contains the buffer that we are to read from
     as well as the continuation we are to call when the buffer is
     empty. */
  write_vio = TSVConnWriteVIOGet(contp);

  /* Get our data structure for this operation. The private data
     structure contains the output VIO and output buffer. If the
     private data structure pointer is NULL, then we'll create it
     and initialize its internals. */
  contData = (TransformTestData *) TSContDataGet(contp);
  data = contData->transformData;
  if (!data) {
    towrite = TSVIONBytesGet(write_vio);
    if (towrite != INT64_MAX) {
      towrite += append_buffer_length;
    }
    contData->transformData = my_data_alloc();
    data = contData->transformData;
    data->output_buffer = TSIOBufferCreate();
    data->output_reader = TSIOBufferReaderAlloc(data->output_buffer);
    data->output_vio = TSVConnWrite(output_conn, contp, data->output_reader, towrite);
    // Don't need this as the structure is encapsulated in another structure
    // which is set to be Continuation's Data.
    // TSContDataSet (contp, data);
  }

  /* We also check to see if the write VIO's buffer is non-NULL. A
     NULL buffer indicates that the write operation has been
     shutdown and that the continuation does not want us to send any
     more WRITE_READY or WRITE_COMPLETE events. For this simplistic
     transformation that means we're done. In a more complex
     transformation we might have to finish writing the transformed
     data to our output connection. */
  if (!TSVIOBufferGet(write_vio)) {
    if (data->append_needed) {
      data->append_needed = 0;
      TSIOBufferCopy(TSVIOBufferGet(data->output_vio), append_buffer_reader, append_buffer_length, 0);
    }

    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(write_vio) + append_buffer_length);
    TSVIOReenable(data->output_vio);
    return;
  }

  /* Determine how much data we have left to read. For this append
     transform plugin this is also the amount of data we have left
     to write to the output connection. */
  towrite = TSVIONTodoGet(write_vio);
  if (towrite > 0) {
    /* The amount of data left to read needs to be truncated by
       the amount of data actually in the read buffer. */
    avail = TSIOBufferReaderAvail(TSVIOReaderGet(write_vio));
    if (towrite > avail) {
      towrite = avail;
    }

    if (towrite > 0) {
      /* Copy the data from the read buffer to the output buffer. */
      TSIOBufferCopy(TSVIOBufferGet(data->output_vio), TSVIOReaderGet(write_vio), towrite, 0);

      /* Tell the read buffer that we have read the data and are no
         longer interested in it. */
      TSIOBufferReaderConsume(TSVIOReaderGet(write_vio), towrite);

      /* Modify the write VIO to reflect how much data we've
         completed. */
      TSVIONDoneSet(write_vio, TSVIONDoneGet(write_vio) + towrite);
    }
  }

  /* Now we check the write VIO to see if there is data left to
     read. */
  if (TSVIONTodoGet(write_vio) > 0) {
    if (towrite > 0) {
      /* If there is data left to read, then we reenable the output
         connection by reenabling the output VIO. This will wakeup
         the output connection and allow it to consume data from the
         output buffer. */
      TSVIOReenable(data->output_vio);

      /* Call back the write VIO continuation to let it know that we
         are ready for more data. */
      TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_READY, write_vio);
    }
  } else {
    if (data->append_needed) {
      data->append_needed = 0;
      TSIOBufferCopy(TSVIOBufferGet(data->output_vio), append_buffer_reader, append_buffer_length, 0);
    }

    /* If there is no data left to read, then we modify the output
       VIO to reflect how much data the output connection should
       expect. This allows the output connection to know when it
       is done reading. We then reenable the output connection so
       that it can consume the data we just gave it. */
    TSVIONBytesSet(data->output_vio, TSVIONDoneGet(write_vio) + append_buffer_length);
    TSVIOReenable(data->output_vio);

    /* Call back the write VIO continuation to let it know that we
       have completed the write operation. */
    TSContCall(TSVIOContGet(write_vio), TS_EVENT_VCONN_WRITE_COMPLETE, write_vio);
  }
}

static int
transformtest_transform(TSCont contp, TSEvent event, void *edata)
{
  NOWARN_UNUSED(edata);
  TransformTestData *contData = (TransformTestData *) TSContDataGet(contp);
  if (contData->test_passed_transform_create == false) {
    contData->test_passed_transform_create = true;
    SDK_RPRINT(contData->test, "TSTransformCreate", "TestCase1", TC_PASS, "ok");
  }
  /* Check to see if the transformation has been closed by a call to
     TSVConnClose. */
  if (TSVConnClosedGet(contp)) {
    my_data_destroy(contData->transformData);
    contData->transformData = NULL;
    TSContDestroy(contp);
    return 0;
  } else {
    switch (event) {
    case TS_EVENT_ERROR:
      {
        TSVIO write_vio;

        /* Get the write VIO for the write operation that was
           performed on ourself. This VIO contains the continuation of
           our parent transformation. */
        write_vio = TSVConnWriteVIOGet(contp);

        /* Call back the write VIO continuation to let it know that we
           have completed the write operation. */
        TSContCall(TSVIOContGet(write_vio), TS_EVENT_ERROR, write_vio);
      }
      break;
    case TS_EVENT_VCONN_WRITE_COMPLETE:
      /* When our output connection says that it has finished
         reading all the data we've written to it then we should
         shutdown the write portion of its connection to
         indicate that we don't want to hear about it anymore. */
      TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);
      break;
    case TS_EVENT_VCONN_WRITE_READY:
    default:
      /* If we get a WRITE_READY event or any other type of
         event (sent, perhaps, because we were reenabled) then
         we'll attempt to transform more data. */
      handle_transform(contp);
      break;
    }
  }

  return 0;
}

static int
transformable(TSHttpTxn txnp, TransformTestData * data)
{
  TSMBuffer bufp;
  TSMLoc hdr_loc;

  if (TSHttpTxnServerRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
    SDK_RPRINT(data->test, "TSHttpTxnTransform", "", TC_FAIL, "[transformable]: TSHttpTxnServerRespGet return 0");
  }

  /*
   *  We are only interested in "200 OK" responses.
   */

  if (TS_HTTP_STATUS_OK == TSHttpHdrStatusGet(bufp, hdr_loc)) {
    return 1;
  }
// XXX - Can't return TS_ERROR because that is a different type
// -bcall 7/24/07
//     if (resp_status == TS_ERROR) {
//      SDK_RPRINT(data->test,"TSHttpTxnTransform","",TC_FAIL,"[transformable]: TSHttpHdrStatusGet returns TS_ERROR");
//     }

  return 0;                     /* not a 200 */
}

static void
transform_add(TSHttpTxn txnp, TransformTestData * data)
{
  TSVConn connp;

  connp = TSTransformCreate(transformtest_transform, txnp);
  TSContDataSet(connp, data);
  if (connp == NULL) {
    SDK_RPRINT(data->test, "TSHttpTxnTransform", "", TC_FAIL, "Unable to create Transformation.");
    return;
  }

  TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, connp);
  return;
}

static int
load(const char *append_string)
{
  TSIOBufferBlock blk;
  char *p;
  int64_t avail;

  append_buffer = TSIOBufferCreate();
  append_buffer_reader = TSIOBufferReaderAlloc(append_buffer);

  blk = TSIOBufferStart(append_buffer);
  p = TSIOBufferBlockWriteStart(blk, &avail);

  ink_strncpy(p, append_string, avail);
  if (append_string != NULL) {
    TSIOBufferProduce(append_buffer, strlen(append_string));
  }

  append_buffer_length = TSIOBufferReaderAvail(append_buffer_reader);

  return 1;
}

/**** Append Transform Code Ends ****/

static int
transform_hook_handler(TSCont contp, TSEvent event, void *edata)
{
  TSHttpTxn txnp = NULL;
  TransformTestData *data = NULL;
  data = (TransformTestData *) TSContDataGet(contp);
  if (data == NULL) {
    switch (event) {
    case TS_EVENT_IMMEDIATE:
    case TS_EVENT_TIMEOUT:
      break;
    case TS_EVENT_HTTP_READ_RESPONSE_HDR:
    default:
      TSHttpTxnReenable((TSHttpTxn) edata, TS_EVENT_HTTP_CONTINUE);
      break;
    }
    return 0;
  }

  switch (event) {
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    txnp = (TSHttpTxn) edata;
    TSSkipRemappingSet(txnp,1);
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;
  case TS_EVENT_HTTP_READ_RESPONSE_HDR:
    txnp = (TSHttpTxn) edata;
    /* Setup hooks for Transformation */
    if (transformable(txnp, data)) {
      transform_add(txnp, data);
    }
    /* Call TransformedRespCache or UntransformedRespCache depending on request */
    {
      TSMBuffer bufp;
      TSMLoc hdr;
      TSMLoc field;

      if (TSHttpTxnClientReqGet(txnp, &bufp, &hdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase", TC_FAIL, "TSHttpTxnClientReqGet returns 0");
      } else {
        if (TS_NULL_MLOC == (field = TSMimeHdrFieldFind(bufp, hdr, "Request", -1))) {
          SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase", TC_FAIL, "Didn't find field request");
        } else {
          int reqid =TSMimeHdrFieldValueIntGet(bufp, hdr, field, 0);
          if (reqid == 1) {
            TSHttpTxnTransformedRespCache(txnp, 0);
            TSHttpTxnUntransformedRespCache(txnp, 1);
          }
          if (reqid == 2) {
            TSHttpTxnTransformedRespCache(txnp, 1);
            TSHttpTxnUntransformedRespCache(txnp, 0);
          }
          if (TSHandleMLocRelease(bufp, hdr, field) != TS_SUCCESS) {
            SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase", TC_FAIL, "Unable to release handle to field in Client request");
          }
        }
        if (TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr) != TS_SUCCESS) {
          SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase", TC_FAIL, "Unable to release handle to Client request");
        }
      }
    }

    /* Add the transaction hook to SEND_RESPONSE_HDR_HOOK */
    TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
    /* Reenable the transaction */
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
    {
      TSMBuffer bufp;
      TSMLoc hdr;
      txnp = (TSHttpTxn) edata;
      if (TSHttpTxnTransformRespGet(txnp, &bufp, &hdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpTxnTransformRespGet", "TestCase", TC_FAIL, "TSHttpTxnTransformRespGet returns 0");
        data->test_passed_txn_transform_resp_get = false;
      } else {
        if ((bufp == reinterpret_cast<TSMBuffer>(&(((HttpSM *) txnp)->t_state.hdr_info.transform_response))) &&
            (hdr == reinterpret_cast<TSMLoc>((&(((HttpSM *) txnp)->t_state.hdr_info.transform_response))->m_http))) {
          SDK_RPRINT(data->test, "TSHttpTxnTransformRespGet", "TestCase", TC_PASS, "ok");
        } else {
          SDK_RPRINT(data->test, "TSHttpTxnTransformRespGet", "TestCase", TC_FAIL, "Value's Mismatch");
          data->test_passed_txn_transform_resp_get = false;
        }
        if (TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr) != TS_SUCCESS) {
          SDK_RPRINT(data->test, "TSHttpTxnTransformRespGet", "TestCase", TC_FAIL,
                     "Unable to release handle to Transform header handle");
        }

      }
    }
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_IMMEDIATE:
  case TS_EVENT_TIMEOUT:

    switch (data->req_no) {
    case 1:
      if (data->browser1->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
      data->req_no++;
      Debug(UTDBG_TAG "_transform", "Running Browser 2");
      synclient_txn_send_request(data->browser2, data->request2);
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
      return 0;
    case 2:
      if (data->browser2->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
      data->req_no++;
      Debug(UTDBG_TAG "_transform", "Running Browser 3");
      synclient_txn_send_request(data->browser3, data->request1);
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
      return 0;
    case 3:
      if (data->browser3->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
      data->req_no++;
      Debug(UTDBG_TAG "_transform", "Running Browser 4");
      synclient_txn_send_request(data->browser4, data->request2);
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
      return 0;
    case 4:
      if (data->browser4->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
      synserver_delete(data->os);
      data->req_no++;
      TSfree(data->request1);
      TSfree(data->request2);
      // for squid log: if this is the last (or only) test in your
      // regression run you will not see any log entries in squid
      // (because logging is buffered and not flushed before
      // termination when running regressions)
      // sleep(10);
      break;
    default:
      SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase", TC_FAIL, "Something terribly wrong with the test");
      exit(0);

    }
    /* Browser got the response. test is over */
    {
      /* Check if we got the response we were expecting or not */
      if ((strstr(data->browser1->response, TRANSFORM_APPEND_STRING) != NULL) &&
          (strstr(data->browser3->response, TRANSFORM_APPEND_STRING) == NULL)) {
        SDK_RPRINT(data->test, "TSHttpTxnUntransformedResponseCache", "TestCase1", TC_PASS, "ok");
        data->test_passed_txn_untransformed_resp_cache = true;
      } else {
        SDK_RPRINT(data->test, "TSHttpTxnUntransformedResponseCache", "TestCase1", TC_FAIL, "Value's Mismatch");
      }

      if ((strstr(data->browser2->response, TRANSFORM_APPEND_STRING) != NULL) &&
          (strstr(data->browser4->response, TRANSFORM_APPEND_STRING) != NULL)) {
        SDK_RPRINT(data->test, "TSHttpTxnTransformedResponseCache", "TestCase1", TC_PASS, "ok");
        data->test_passed_txn_transformed_resp_cache = true;
      } else {
        SDK_RPRINT(data->test, "TSHttpTxnTransformedResponseCache", "TestCase1", TC_FAIL, "Value's Mismatch");
      }
      
      /* Note: response is available using test->browser->response pointer */
      *(data->pstatus) = REGRESSION_TEST_PASSED;
      if (data->browser1->status != REQUEST_SUCCESS) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "Browser 1 status was not REQUEST_SUCCESS");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->browser2->status != REQUEST_SUCCESS) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "Browser 2 status was not REQUEST_SUCCESS");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->browser3->status != REQUEST_SUCCESS) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "Browser 3 status was not REQUEST_SUCCESS");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->browser4->status != REQUEST_SUCCESS) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "Browser 4 status was not REQUEST_SUCCESS");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->test_passed_txn_transform_resp_get != true) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "did not pass transform_resp_get");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->test_passed_txn_transformed_resp_cache != true) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "did not pass transformed_resp_cache");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->test_passed_txn_untransformed_resp_cache != true) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "did not pass untransformed_resp_cache");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      if (data->test_passed_transform_create != true) {
        SDK_RPRINT(data->test, "TSTransformCreate", "TestCase1", TC_FAIL, "did not pass transform_create");
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }
      // transaction is over. clean up.
      synclient_txn_delete(data->browser1);
      synclient_txn_delete(data->browser2);
      synclient_txn_delete(data->browser3);
      synclient_txn_delete(data->browser4);

      data->magic = MAGIC_DEAD;
      TSfree(data);
      TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(data->test, "TSHttpTxnTransform", "TestCase1", TC_FAIL, "Unexpected event %d", event);
    break;
  }
  return 0;
}


EXCLUSIVE_REGRESSION_TEST(SDK_API_HttpTxnTransform) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  Debug(UTDBG_TAG "_transform", "Starting test");

  TSCont cont = TSContCreate(transform_hook_handler, TSMutexCreate());
  if (cont == NULL) {
    SDK_RPRINT(test, "TSHttpSsn", "TestCase1", TC_FAIL, "Unable to create Continuation.");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  TransformTestData *socktest = (TransformTestData *) TSmalloc(sizeof(TransformTestData));
  socktest->test = test;
  socktest->pstatus = pstatus;
  socktest->test_passed_txn_transform_resp_get = true;
  socktest->test_passed_txn_transformed_resp_cache = false;
  socktest->test_passed_txn_transformed_resp_cache = false;
  socktest->test_passed_transform_create = false;
  socktest->transformData = NULL;
  socktest->req_no = 1;
  socktest->magic = MAGIC_ALIVE;
  TSContDataSet(cont, socktest);

  /* Prepare the buffer to be appended to responses */
  load(TRANSFORM_APPEND_STRING);
  
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont); //so we can skip remapping
  
  /* Register to HTTP hooks that are called in case of a cache MISS */
  TSHttpHookAdd(TS_HTTP_READ_RESPONSE_HDR_HOOK, cont);

  /* Create a new synthetic server */
  socktest->os = synserver_create(SYNSERVER_LISTEN_PORT);
  synserver_start(socktest->os);

  /* Create a client transaction */
  socktest->browser1 = synclient_txn_create();
  socktest->browser2 = synclient_txn_create();
  socktest->browser3 = synclient_txn_create();
  socktest->browser4 = synclient_txn_create();
  socktest->request1 = generate_request(4);
  socktest->request2 = generate_request(5);
  Debug(UTDBG_TAG "_transform", "Running Browser 1");
  synclient_txn_send_request(socktest->browser1, socktest->request1);
  // synclient_txn_send_request(socktest->browser2, socktest->request2);

  /* Wait until transaction is done */
  TSContSchedule(cont, 25, TS_THREAD_POOL_DEFAULT);

  return;
}

//////////////////////////////////////////////
//       SDK_API_TSHttpTxnAltInfo
//
// Unit Test for API: TSHttpTxnCachedReqGet
//                    TSHttpTxnCachedRespGet
//////////////////////////////////////////////

typedef struct
{
  RegressionTest *test;
  int *pstatus;
  SocketServer *os;
  ClientTxn *browser1;
  ClientTxn *browser2;
  ClientTxn *browser3;
  char *request1;
  char *request2;
  char *request3;
  bool test_passed_txn_alt_info_client_req_get;
  bool test_passed_txn_alt_info_cached_req_get;
  bool test_passed_txn_alt_info_cached_resp_get;
  bool test_passed_txn_alt_info_quality_set;
  bool run_at_least_once;
  bool first_time;
  int magic;
} AltInfoTestData;

static int
altinfo_hook_handler(TSCont contp, TSEvent event, void *edata)
{
  AltInfoTestData *data = NULL;
  TSHttpTxn txnp = NULL;

  data = (AltInfoTestData *) TSContDataGet(contp);
  if (data == NULL) {
    switch (event) {
    case TS_EVENT_IMMEDIATE:
    case TS_EVENT_TIMEOUT:
      break;
    case TS_EVENT_HTTP_SELECT_ALT:
      break;
    default:
      TSHttpTxnReenable((TSHttpTxn) edata, TS_EVENT_HTTP_CONTINUE);
      break;
    }
    return 0;
  }

  switch (event) {
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    txnp = (TSHttpTxn) edata;
    TSSkipRemappingSet(txnp,1);
    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_HTTP_SELECT_ALT:
    {
      TSMBuffer clientreqbuf;
      TSMBuffer cachereqbuf;
      TSMBuffer cacherespbuf;

      TSMLoc clientreqhdr;
      TSMLoc cachereqhdr;
      TSMLoc cacheresphdr;

      TSHttpAltInfo infop = (TSHttpAltInfo) edata;

      data->run_at_least_once = true;
      if (TSHttpAltInfoClientReqGet(infop, &clientreqbuf, &clientreqhdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpAltInfoClientReqGet", "TestCase", TC_FAIL,
                   "TSHttpAltInfoClientReqGet doesn't return TS_SUCCESS");
        data->test_passed_txn_alt_info_client_req_get = false;
      } else {
        if ((clientreqbuf == reinterpret_cast<TSMBuffer>(&(((HttpAltInfo *)infop)->m_client_req))) &&
            (clientreqhdr == reinterpret_cast<TSMLoc>(((HttpAltInfo *)infop)->m_client_req.m_http))) {
          SDK_RPRINT(data->test, "TSHttpAltInfoClientReqGet", "TestCase", TC_PASS, "ok");
        } else {
          SDK_RPRINT(data->test, "TSHttpAltInfoClientReqGet", "TestCase", TC_FAIL, "Value's Mismatch");
          data->test_passed_txn_alt_info_client_req_get = false;
        }
      }

      if (TSHttpAltInfoCachedReqGet(infop, &cachereqbuf, &cachereqhdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpAltInfoCachedReqGet", "TestCase", TC_FAIL,
                   "TSHttpAltInfoCachedReqGet doesn't return TS_SUCCESS");
        data->test_passed_txn_alt_info_cached_req_get = false;
      } else {
        if ((cachereqbuf == reinterpret_cast<TSMBuffer>(&(((HttpAltInfo *) infop)->m_cached_req))) &&
            (cachereqhdr == reinterpret_cast<TSMLoc>(((HttpAltInfo *) infop)->m_cached_req.m_http))) {
          SDK_RPRINT(data->test, "TSHttpAltInfoCachedReqGet", "TestCase", TC_PASS, "ok");
        } else {
          SDK_RPRINT(data->test, "TSHttpAltInfoCachedReqGet", "TestCase", TC_FAIL, "Value's Mismatch");
          data->test_passed_txn_alt_info_cached_req_get = false;
        }
      }

      if (TSHttpAltInfoCachedRespGet(infop, &cacherespbuf, &cacheresphdr) != TS_SUCCESS) {
        SDK_RPRINT(data->test, "TSHttpAltInfoCachedRespGet", "TestCase", TC_FAIL,
                   "TSHttpAltInfoCachedRespGet doesn't return TS_SUCCESS");
        data->test_passed_txn_alt_info_cached_resp_get = false;
      } else {
        if ((cacherespbuf == reinterpret_cast<TSMBuffer>(&(((HttpAltInfo *) infop)->m_cached_resp))) &&
            (cacheresphdr == reinterpret_cast<TSMLoc>(((HttpAltInfo *) infop)->m_cached_resp.m_http))) {
          SDK_RPRINT(data->test, "TSHttpAltInfoCachedRespGet", "TestCase", TC_PASS, "ok");
        } else {
          SDK_RPRINT(data->test, "TSHttpAltInfoCachedRespGet", "TestCase", TC_FAIL, "Value's Mismatch");
          data->test_passed_txn_alt_info_cached_resp_get = false;
        }
      }

      TSHttpAltInfoQualitySet(infop, 0.5);
      SDK_RPRINT(data->test, "TSHttpAltInfoQualitySet", "TestCase", TC_PASS, "ok");
    }

    break;

  case TS_EVENT_IMMEDIATE:
  case TS_EVENT_TIMEOUT:
    /* Browser still waiting the response ? */
    if (data->first_time == true) {
      if ((data->browser1->status == REQUEST_INPROGRESS) || (data->browser2->status == REQUEST_INPROGRESS)) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
    } else {
      if (data->browser3->status == REQUEST_INPROGRESS) {
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }
    }

    /* Browser got the response. test is over. clean up */
    {
      /* If this is the first time, then both the responses are in cache and we should make */
      /* another request to get cache hit */
      if (data->first_time == true) {
        data->first_time = false;
        /* Kill the origin server */
        synserver_delete(data->os);
// ink_release_assert(0);
        /* Send another similar client request */
        synclient_txn_send_request(data->browser3, data->request3);

        /* Register to HTTP hooks that are called in case of alternate selection */
        TSHttpHookAdd(TS_HTTP_SELECT_ALT_HOOK, contp);
        TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
        return 0;
      }

      /* Note: response is available using test->browser->response pointer */
      if ((data->browser3->status == REQUEST_SUCCESS) &&
          (data->test_passed_txn_alt_info_client_req_get == true) &&
          (data->test_passed_txn_alt_info_cached_req_get == true) &&
          (data->test_passed_txn_alt_info_cached_resp_get == true) &&
          (data->test_passed_txn_alt_info_quality_set == true) && (data->run_at_least_once == true)) {
        *(data->pstatus) = REGRESSION_TEST_PASSED;
      } else {
        if (data->run_at_least_once == false) {
          SDK_RPRINT(data->test, "TSHttpAltInfo", "All", TC_FAIL, "Test not executed even once");
        }
        *(data->pstatus) = REGRESSION_TEST_FAILED;
      }

      // transaction is over. clean up.
      synclient_txn_delete(data->browser1);
      synclient_txn_delete(data->browser2);
      synclient_txn_delete(data->browser3);

      TSfree(data->request1);
      TSfree(data->request2);
      TSfree(data->request3);

      data->magic = MAGIC_DEAD;
      TSfree(data);
      TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(data->test, "TSHttpTxnCache", "TestCase1", TC_FAIL, "Unexpected event %d", event);
    break;
  }
  return 0;
}



EXCLUSIVE_REGRESSION_TEST(SDK_API_HttpAltInfo) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSCont cont = TSContCreate(altinfo_hook_handler, TSMutexCreate());
  if (cont == NULL) {
    SDK_RPRINT(test, "TSHttpSsn", "TestCase1", TC_FAIL, "Unable to create Continuation.");
    *pstatus = REGRESSION_TEST_FAILED;
    return;
  }

  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont); //so we can skip remapping

  AltInfoTestData *socktest = (AltInfoTestData *) TSmalloc(sizeof(AltInfoTestData));
  socktest->test = test;
  socktest->pstatus = pstatus;
  socktest->test_passed_txn_alt_info_client_req_get = true;
  socktest->test_passed_txn_alt_info_cached_req_get = true;
  socktest->test_passed_txn_alt_info_cached_resp_get = true;
  socktest->test_passed_txn_alt_info_quality_set = true;
  socktest->run_at_least_once = false;
  socktest->first_time = true;
  socktest->magic = MAGIC_ALIVE;
  TSContDataSet(cont, socktest);

  /* Create a new synthetic server */
  socktest->os = synserver_create(SYNSERVER_LISTEN_PORT);
  synserver_start(socktest->os);

  /* Create a client transaction */
  socktest->browser1 = synclient_txn_create();
  socktest->browser2 = synclient_txn_create();
  socktest->browser3 = synclient_txn_create();
  socktest->request1 = generate_request(6);
  socktest->request2 = generate_request(7);
  socktest->request3 = generate_request(8);
  synclient_txn_send_request(socktest->browser1, socktest->request1);
  synclient_txn_send_request(socktest->browser2, socktest->request2);

  /* Wait until transaction is done */
  TSContSchedule(cont, 25, TS_THREAD_POOL_DEFAULT);

  return;
}


//////////////////////////////////////////////
//       SDK_API_TSHttpConnect
//
// Unit Test for APIs:
//      - TSHttpConnect
//      - TSHttpTxnIntercept
//      - TSHttpTxnInterceptServer
//
//
// 2 Test cases.
//
// Same test strategy:
//  - create a synthetic server listening on port A
//  - use HttpConnect to send a request to TS for an url on a remote host H, port B
//  - use TxnIntercept or TxnServerIntercept to forward the request
//    to the synthetic server on local host, port A
//  - make sure response is correct
//
//////////////////////////////////////////////

// Important: we create servers listening on different port than the default one
// to make sure our synthetix servers are called

#define TEST_CASE_CONNECT_ID1 9 //TSHttpTxnIntercept
#define TEST_CASE_CONNECT_ID2 10        //TSHttpTxnServerIntercept

#define SYNSERVER_DUMMY_PORT -1

typedef struct
{
  RegressionTest *test;
  int *pstatus;
  int test_case;
  TSVConn vc;
  SocketServer *os;
  ClientTxn *browser;
  char *request;
  unsigned long magic;
} ConnectTestData;


static int
cont_test_handler(TSCont contp, TSEvent event, void *edata)
{
  TSHttpTxn txnp = (TSHttpTxn) edata;
  ConnectTestData *data = (ConnectTestData *) TSContDataGet(contp);
  int request_id = -1;

  TSReleaseAssert(data->magic == MAGIC_ALIVE);
  TSReleaseAssert((data->test_case == TEST_CASE_CONNECT_ID1) || (data->test_case == TEST_CASE_CONNECT_ID2));

  TSDebug(UTDBG_TAG, "Calling cont_test_handler with event %d", event);

  switch (event) {
  case TS_EVENT_HTTP_READ_REQUEST_HDR:
    TSDebug(UTDBG_TAG, "cont_test_handler: event READ_REQUEST");

    // First make sure we're getting called for either request 9 or txn 10
    // Otherwise, this is a request sent by another test. do nothing.
    request_id = get_request_id(txnp);
    TSReleaseAssert(request_id != -1);

    TSDebug(UTDBG_TAG, "cont_test_handler: Request id = %d", request_id);

    if ((request_id != TEST_CASE_CONNECT_ID1) && (request_id != TEST_CASE_CONNECT_ID2)) {
      TSDebug(UTDBG_TAG, "This is not an event for this test !");
      TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
      goto done;
    }

    if ((request_id == TEST_CASE_CONNECT_ID1) && (data->test_case == TEST_CASE_CONNECT_ID1)) {
      TSDebug(UTDBG_TAG, "Calling TSHttpTxnIntercept");
      TSHttpTxnIntercept(data->os->accept_cont, txnp);
    } else if ((request_id == TEST_CASE_CONNECT_ID2) && (data->test_case == TEST_CASE_CONNECT_ID2)) {
      TSDebug(UTDBG_TAG, "Calling TSHttpTxnServerIntercept");
      TSHttpTxnServerIntercept(data->os->accept_cont, txnp);
    }

    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
    break;

  case TS_EVENT_TIMEOUT:
    /* Browser still waiting the response ? */
    if (data->browser->status == REQUEST_INPROGRESS) {
      TSDebug(UTDBG_TAG, "Browser still waiting response...");
      TSContSchedule(contp, 25, TS_THREAD_POOL_DEFAULT);
    }
    /* Browser got the response */
    else {
      /* Check if browser response body is the one we expected */
      char *body_response = get_body_ptr(data->browser->response);
      const char *body_expected;
      if (data->test_case == TEST_CASE_CONNECT_ID1) {
        body_expected = "Body for response 9";
      } else {
        body_expected = "Body for response 10";
      }
      TSDebug(UTDBG_TAG, "Body Response = \n|%s|\nBody Expected = \n|%s|", body_response ? body_response : "*NULL*", body_expected);

      if (!body_response
        || strncmp(body_response, body_expected, strlen(body_expected)) != 0
      ) {
        if (data->test_case == TEST_CASE_CONNECT_ID1) {
          SDK_RPRINT(data->test, "TSHttpConnect", "TestCase1", TC_FAIL, "Unexpected response");
          SDK_RPRINT(data->test, "TSHttpTxnIntercept", "TestCase1", TC_FAIL, "Unexpected response");
        } else {
          SDK_RPRINT(data->test, "TSHttpConnect", "TestCase2", TC_FAIL, "Unexpected response");
          SDK_RPRINT(data->test, "TSHttpTxnServerIntercept", "TestCase2", TC_FAIL, "Unexpected response");
        }
        *(data->pstatus) = REGRESSION_TEST_FAILED;

      } else {
        if (data->test_case == TEST_CASE_CONNECT_ID1) {
          SDK_RPRINT(data->test, "TSHttpConnect", "TestCase1", TC_PASS, "ok");
          SDK_RPRINT(data->test, "TSHttpTxnIntercept", "TestCase1", TC_PASS, "ok");
        } else {
          SDK_RPRINT(data->test, "TSHttpConnect", "TestCase2", TC_PASS, "ok");
          SDK_RPRINT(data->test, "TSHttpTxnServerIntercept", "TestCase2", TC_PASS, "ok");
        }
        *(data->pstatus) = REGRESSION_TEST_PASSED;
      }

      // transaction is over. clean it up.
      synclient_txn_delete(data->browser);
      synserver_delete(data->os);

      // As we registered to a global hook, we may be called back again.
      // Do not destroy the continuation...
      // data->magic = MAGIC_DEAD;
      // TSfree(data);
      // TSContDataSet(contp, NULL);
    }
    break;

  default:
    *(data->pstatus) = REGRESSION_TEST_FAILED;
    SDK_RPRINT(data->test, "TSHttpConnect", "TestCase1 or 2", TC_FAIL, "Unexpected event %d", event);
    break;
  }

done:
  return TS_EVENT_IMMEDIATE;
}


EXCLUSIVE_REGRESSION_TEST(SDK_API_TSHttpConnectIntercept) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);

  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSDebug(UTDBG_TAG, "Starting test TSHttpConnectIntercept");

  TSCont cont_test = TSContCreate(cont_test_handler, TSMutexCreate());
  ConnectTestData *data = (ConnectTestData *) TSmalloc(sizeof(ConnectTestData));
  TSContDataSet(cont_test, data);

  data->test = test;
  data->pstatus = pstatus;
  data->magic = MAGIC_ALIVE;
  data->test_case = TEST_CASE_CONNECT_ID1;

  /* Register to hook READ_REQUEST */
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont_test);

  // Create a synthetic server which won't really listen on a socket port
  // It will be called by the Http SM with a VC
  data->os = synserver_create(SYNSERVER_DUMMY_PORT);

  data->browser = synclient_txn_create();
  data->request = generate_request(9);

  /* Now send a request to the OS via TS using TSHttpConnect */

  /* ip and log do not matter as it is used for logging only */
  sockaddr_in addr;
  ink_inet_ip4_set(&addr, 1, 1);
  data->vc = TSHttpConnect(ink_inet_sa_cast(&addr));
  synclient_txn_send_request_to_vc(data->browser, data->request, data->vc);

  /* Wait until transaction is done */
  TSContSchedule(cont_test, 25, TS_THREAD_POOL_DEFAULT);

  return;
}


EXCLUSIVE_REGRESSION_TEST(SDK_API_TSHttpConnectServerIntercept) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);

  *pstatus = REGRESSION_TEST_INPROGRESS;

  TSDebug(UTDBG_TAG, "Starting test TSHttpConnectServerintercept");

  TSCont cont_test = TSContCreate(cont_test_handler, TSMutexCreate());
  ConnectTestData *data = (ConnectTestData *) TSmalloc(sizeof(ConnectTestData));
  TSContDataSet(cont_test, data);

  data->test = test;
  data->pstatus = pstatus;
  data->magic = MAGIC_ALIVE;
  data->test_case = TEST_CASE_CONNECT_ID2;

  /* Register to hook READ_REQUEST */
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, cont_test);

  /* This is cool ! we can use the code written for the synthetic server and client in InkAPITest.cc */
  data->os = synserver_create(SYNSERVER_DUMMY_PORT);

  data->browser = synclient_txn_create();
  data->request = generate_request(10);

  /* Now send a request to the OS via TS using TSHttpConnect */

  /* ip and log do not matter as it is used for logging only */
  sockaddr_in addr;
  ink_inet_ip4_set(&addr, 2, 2);
  data->vc = TSHttpConnect(ink_inet_sa_cast(&addr));

  synclient_txn_send_request_to_vc(data->browser, data->request, data->vc);

  /* Wait until transaction is done */
  TSContSchedule(cont_test, 25, TS_THREAD_POOL_DEFAULT);

  return;
}


////////////////////////////////////////////////
// SDK_API_OVERRIDABLE_CONFIGS
//
// Unit Test for API: TSHttpTxnConfigFind
//                    TSHttpTxnConfigIntSet
//                    TSHttpTxnConfigIntGet
//                    TSHttpTxnConfigFloatSet
//                    TSHttpTxnConfigFloatGet
//                    TSHttpTxnConfigStringSet
//                    TSHttpTxnConfigStringGet
////////////////////////////////////////////////

// The order of these should be the same as TSOverridableConfigKey
const char *SDK_Overridable_Configs[] = {
  "proxy.config.url_remap.pristine_host_hdr",
  "proxy.config.http.chunking_enabled",
  "proxy.config.http.negative_caching_enabled",
  "proxy.config.http.negative_caching_lifetime",
  "proxy.config.http.cache.when_to_revalidate",
  "proxy.config.http.keep_alive_enabled_in",
  "proxy.config.http.keep_alive_enabled_out",
  "proxy.config.http.keep_alive_post_out",
  "proxy.config.net.sock_recv_buffer_size_out",
  "proxy.config.net.sock_send_buffer_size_out",
  "proxy.config.net.sock_option_flag_out",
  "proxy.config.http.forward.proxy_auth_to_parent",
  "proxy.config.http.anonymize_remove_from",
  "proxy.config.http.anonymize_remove_referer",
  "proxy.config.http.anonymize_remove_user_agent",
  "proxy.config.http.anonymize_remove_cookie",
  "proxy.config.http.anonymize_remove_client_ip",
  "proxy.config.http.anonymize_insert_client_ip",
  "proxy.config.http.append_xforwards_header",
  "proxy.config.http.response_server_enabled",
  "proxy.config.http.insert_squid_x_forwarded_for",
  "proxy.config.http.send_http11_requests",
  "proxy.config.http.cache.http",
  "proxy.config.http.cache.ignore_client_no_cache",
  "proxy.config.http.cache.ignore_client_cc_max_age",
  "proxy.config.http.cache.ims_on_client_no_cache",
  "proxy.config.http.cache.ignore_server_no_cache",
  "proxy.config.http.cache.cache_responses_to_cookies",
  "proxy.config.http.cache.ignore_authentication",
  "proxy.config.http.cache.cache_urls_that_look_dynamic",
  "proxy.config.http.cache.required_headers",
  "proxy.config.http.insert_request_via_str",
  "proxy.config.http.insert_response_via_str",
  "proxy.config.http.cache.heuristic_min_lifetime",
  "proxy.config.http.cache.heuristic_max_lifetime",
  "proxy.config.http.cache.guaranteed_min_lifetime",
  "proxy.config.http.cache.guaranteed_max_lifetime",
  "proxy.config.http.cache.max_stale_age",
  "proxy.config.http.keep_alive_no_activity_timeout_in",
  "proxy.config.http.keep_alive_no_activity_timeout_out",
  "proxy.config.http.transaction_no_activity_timeout_in",
  "proxy.config.http.transaction_no_activity_timeout_out",
  "proxy.config.http.transaction_active_timeout_out",
  "proxy.config.http.origin_max_connections",
  "proxy.config.http.connect_attempts_max_retries",
  "proxy.config.http.connect_attempts_max_retries_dead_server",
  "proxy.config.http.connect_attempts_rr_retries",
  "proxy.config.http.connect_attempts_timeout",
  "proxy.config.http.post_connect_attempts_timeout",
  "proxy.config.http.down_server.cache_time",
  "proxy.config.http.down_server.abort_threshold",
  "proxy.config.http.cache.fuzz.time",
  "proxy.config.http.cache.fuzz.min_time",
  "proxy.config.http.doc_in_cache_skip_dns",

  // These are "special", since they are not MgmtInt's
  "proxy.config.http.response_server_str",
  "proxy.config.http.cache.heuristic_lm_factor",
  "proxy.config.http.cache.fuzz.probability",

  NULL
};

REGRESSION_TEST(SDK_API_OVERRIDABLE_CONFIGS) (RegressionTest * test, int atype, int *pstatus)
{
  NOWARN_UNUSED(atype);
  const char* conf;
  TSOverridableConfigKey key;
  TSRecordDataType type;
  HttpSM* s = HttpSM::allocate();
  bool success = true;
  TSHttpTxn txnp = reinterpret_cast<TSHttpTxn>(s);
  TSMgmtInt ival;
  TSMgmtFloat fval;
  const char* sval;
  const char* test_string = "The Apache Traffic Server";
  int tmp_int;

  s->init();

  *pstatus = REGRESSION_TEST_INPROGRESS;
  for (int i=TS_CONFIG_NULL + 1; i < TS_CONFIG_LAST_ENTRY; ++i) {
    conf = SDK_Overridable_Configs[i];
    if (TS_SUCCESS == TSHttpTxnConfigFind(conf, -1, &key, &type)) {
      if (key != i) {
        SDK_RPRINT(test, "TSHttpTxnConfigFind", "TestCase1", TC_FAIL, "Failed on %s, expected %d, got %d", conf, i, key);
        success = false;
        continue;
      }
    } else {
      SDK_RPRINT(test, "TSHttpTxnConfigFind", "TestCase1", TC_FAIL, "Called returned unexpected TS_ERROR");
      success = false;
      continue;
    }
    // Now check the getters / setters
    switch (type) {
    case TS_RECORDDATATYPE_INT:
      TSHttpTxnConfigIntSet(txnp, key, 17);
      TSHttpTxnConfigIntGet(txnp, key, &ival);
      if (17 != ival) {
        SDK_RPRINT(test, "TSHttpTxnConfigIntSet", "TestCase1", TC_FAIL, "Failed on %s, expected 17, got %d", conf, ival);
        success = false;
        continue;
      }
      break;

    case TS_RECORDDATATYPE_FLOAT:
      TSHttpTxnConfigFloatSet(txnp, key, 17.17);
      TSHttpTxnConfigFloatGet(txnp, key, &fval);
      if (17.17 != fval) {
        SDK_RPRINT(test, "TSHttpTxnConfigFloatSet", "TestCase1", TC_FAIL, "Failed on %s, expected 17, got %d", conf, ival);
        success = false;
        continue;
      }
      break;

    case TS_RECORDDATATYPE_STRING:
      TSHttpTxnConfigStringSet(txnp, key, test_string, -1);
      TSHttpTxnConfigStringGet(txnp, key, &sval, &tmp_int);
      if (test_string != sval) {
        SDK_RPRINT(test, "TSHttpTxnConfigStringSet", "TestCase1", TC_FAIL, "Failed on %s, expected 17, got %s", conf, sval);
        success = false;
        continue;
      }
      break;

    default:
      break;
    }
  }

  s->destroy();
  if (success) {
    *pstatus = REGRESSION_TEST_PASSED;
    SDK_RPRINT(test, "TSHttpTxnConfigFind", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSHttpTxnConfigIntSet", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSHttpTxnConfigFloatSet", "TestCase1", TC_PASS, "ok");
    SDK_RPRINT(test, "TSHttpTxnConfigStringSet", "TestCase1", TC_PASS, "ok");
  } else {
    *pstatus = REGRESSION_TEST_FAILED;
  }

  return;
}
