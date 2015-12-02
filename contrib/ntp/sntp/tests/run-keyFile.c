/* AUTOGENERATED FILE. DO NOT EDIT. */

//=======Test Runner Used To Run Each Test Below=====
#define RUN_TEST(TestFunc, TestLineNum) \
{ \
  Unity.CurrentTestName = #TestFunc; \
  Unity.CurrentTestLineNumber = TestLineNum; \
  Unity.NumberOfTests++; \
  if (TEST_PROTECT()) \
  { \
      setUp(); \
      TestFunc(); \
  } \
  if (TEST_PROTECT() && !TEST_IS_IGNORED) \
  { \
    tearDown(); \
  } \
  UnityConcludeTest(); \
}

//=======Automagically Detected Files To Include=====
#include "unity.h"
#include <setjmp.h>
#include <stdio.h>
#include "config.h"
#include "fileHandlingTest.h"
#include "ntp_stdlib.h"
#include "ntp_types.h"
#include "crypto.h"

//=======External Functions This Runner Calls=====
extern void setUp(void);
extern void tearDown(void);
extern void test_ReadEmptyKeyFile(void);
extern void test_ReadASCIIKeys(void);
extern void test_ReadHexKeys(void);
extern void test_ReadKeyFileWithComments(void);
extern void test_ReadKeyFileWithInvalidHex(void);


//=======Test Reset Option=====
void resetTest(void);
void resetTest(void)
{
  tearDown();
  setUp();
}

char const *progname;


//=======MAIN=====
int main(int argc, char *argv[])
{
  progname = argv[0];
  UnityBegin("keyFile.c");
  RUN_TEST(test_ReadEmptyKeyFile, 12);
  RUN_TEST(test_ReadASCIIKeys, 13);
  RUN_TEST(test_ReadHexKeys, 14);
  RUN_TEST(test_ReadKeyFileWithComments, 15);
  RUN_TEST(test_ReadKeyFileWithInvalidHex, 16);

  return (UnityEnd());
}
