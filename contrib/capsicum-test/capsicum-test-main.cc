#include <sys/types.h>
#ifdef __linux__
#include <sys/vfs.h>
#include <linux/magic.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include "gtest/gtest.h"
#include "capsicum-test.h"

std::string tmpdir;

class SetupEnvironment : public ::testing::Environment
{
public:
  SetupEnvironment() : teardown_tmpdir_(false) {}
  void SetUp() override {
    CheckCapsicumSupport();
    if (tmpdir.empty()) {
      std::cerr << "Generating temporary directory root: ";
      CreateTemporaryRoot();
    } else {
      std::cerr << "User provided temporary directory root: ";
    }
    std::cerr << tmpdir << std::endl;
  }
  void CheckCapsicumSupport() {
#ifdef __FreeBSD__
    size_t trap_enotcap_enabled_len;
    int rc;
    bool trap_enotcap_enabled;

    trap_enotcap_enabled_len = sizeof(trap_enotcap_enabled);

    if (feature_present("security_capabilities") == 0) {
      GTEST_SKIP() << "Tests require a CAPABILITIES enabled kernel";
    } else {
      std::cerr << "Running on a CAPABILITIES enabled kernel" << std::endl;
    }
    const char *oid = "kern.trap_enotcap";
    rc = sysctlbyname(oid, &trap_enotcap_enabled, &trap_enotcap_enabled_len,
      nullptr, 0);
    if (rc != 0) {
      GTEST_FAIL() << "sysctlbyname failed: " << strerror(errno);
    }
    if (trap_enotcap_enabled) {
      GTEST_SKIP() << "Sysctl " << oid << " enabled. "
                   << "Skipping tests to avoid non-determinism with results";
    } else {
      std::cerr << "Sysctl " << oid << " not enabled." << std::endl;
    }
#endif
  }
  void CreateTemporaryRoot() {
    char *tmpdir_name = tempnam(nullptr, "cptst");

    ASSERT_NE(tmpdir_name, nullptr);
    ASSERT_EQ(mkdir(tmpdir_name, 0700), 0) <<
        "Could not create temp directory, " << tmpdir_name << ": " <<
        strerror(errno);
    tmpdir = std::string(tmpdir_name);
    free(tmpdir_name);
    teardown_tmpdir_ = true;
  }
  void TearDown() override {
    if (teardown_tmpdir_) {
      rmdir(tmpdir.c_str());
    }
  }
private:
  bool teardown_tmpdir_;
};

std::string capsicum_test_bindir;

int main(int argc, char* argv[]) {
  // Set up the test program path, so capsicum-test can find programs, like
  // mini-me* when executed from an absolute path.
  {
    char *new_path, *old_path, *program_name;

    program_name = strdup(argv[0]);
    assert(program_name);
    capsicum_test_bindir = std::string(dirname(program_name));
    free(program_name);

    old_path = getenv("PATH");
    assert(old_path);

    assert(asprintf(&new_path, "%s:%s", capsicum_test_bindir.c_str(),
      old_path) > 0);
    assert(setenv("PATH", new_path, 1) == 0);
  }

  ::testing::InitGoogleTest(&argc, argv);
  for (int ii = 1; ii < argc; ii++) {
    if (strcmp(argv[ii], "-v") == 0) {
      verbose = true;
    } else if (strcmp(argv[ii], "-T") == 0) {
      ii++;
      assert(ii < argc);
      tmpdir = argv[ii];
      struct stat info;
      stat(tmpdir.c_str(), &info);
      assert(S_ISDIR(info.st_mode));
    } else if (strcmp(argv[ii], "-t") == 0) {
      force_mt = true;
    } else if (strcmp(argv[ii], "-F") == 0) {
      force_nofork = true;
    } else if (strcmp(argv[ii], "-u") == 0) {
      if (++ii >= argc) {
        std::cerr << "-u needs argument" << std::endl;
        exit(1);
      }
      if (isdigit(argv[ii][0])) {
        other_uid = atoi(argv[ii]);
      } else {
        struct passwd *p = getpwnam(argv[ii]);
        if (!p) {
          std::cerr << "Failed to get entry for " << argv[ii] << ", errno=" << errno << std::endl;
          exit(1);
        }
        other_uid = p->pw_uid;
      }
    }
  }
  if (other_uid == 0) {
    struct stat info;
    if (stat(argv[0], &info) == 0) {
      other_uid = info.st_uid;
    }
  }

#ifdef __linux__
  // Check whether our temporary directory is on a tmpfs volume.
  struct statfs fsinfo;
  statfs(tmpdir.c_str(), &fsinfo);
  tmpdir_on_tmpfs = (fsinfo.f_type == TMPFS_MAGIC);
#endif

  testing::AddGlobalTestEnvironment(new SetupEnvironment());
  int rc = RUN_ALL_TESTS();
  ShowSkippedTests(std::cerr);
  return rc;
}
