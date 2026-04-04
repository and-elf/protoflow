#include <protoflow/config/config_paths.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace protoflow::config;

// ---------------------------------------------------------------------------
// Helper: RAII temp directory
// ---------------------------------------------------------------------------
class TempDir {
public:
    TempDir() {
        path_ = fs::temp_directory_path() / ("protoflow_test_" + std::to_string(counter_++));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
    static inline int counter_ = 0; // NOLINT
};

// Helper: RAII env var override
class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) {
        const char* old = std::getenv(name); // NOLINT
        if (old != nullptr) {
            had_value_ = true;
            old_value_ = old;
        }
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        ::setenv(name, value, 1);
    }
    ~ScopedEnv() {
        if (had_value_) {
            ::setenv(name_.c_str(), old_value_.c_str(), 1); // NOLINT
        } else {
            ::unsetenv(name_.c_str()); // NOLINT
        }
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    std::string name_;
    bool had_value_ = false;
    std::string old_value_;
};

// ---------------------------------------------------------------------------
// Tests: config_directory_candidates
// ---------------------------------------------------------------------------

TEST(ConfigPaths, CandidatesUseXdgConfigHome) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto dirs = config_directory_candidates("test-app");
    ASSERT_FALSE(dirs.empty());
    EXPECT_EQ(dirs.front(), tmp.path() / "protoflow" / "test-app");
}

TEST(ConfigPaths, CandidatesFallbackToHomeConfig) {
    TempDir tmp;
    // Unset XDG_CONFIG_HOME so it falls back to $HOME/.config
    ::unsetenv("XDG_CONFIG_HOME"); // NOLINT
    ScopedEnv env("HOME", tmp.path().c_str());

    auto dirs = config_directory_candidates("my-app");
    ASSERT_FALSE(dirs.empty());
    EXPECT_EQ(dirs.front(), tmp.path() / ".config" / "protoflow" / "my-app");
}

TEST(ConfigPaths, CandidatesIncludeSystemDir) {
    auto dirs = config_directory_candidates("some-app");
    // The system dir /etc/protoflow/some-app should always be a candidate.
    bool found_etc = false;
    for (const auto& d : dirs) {
        if (d == fs::path("/etc/protoflow/some-app")) {
            found_etc = true;
        }
    }
    EXPECT_TRUE(found_etc) << "/etc/protoflow/some-app not in candidates";
}

// ---------------------------------------------------------------------------
// Tests: config_directories (only existing)
// ---------------------------------------------------------------------------

TEST(ConfigPaths, DirectoriesReturnsOnlyExisting) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    // Before creating the directory – should not appear
    auto dirs = config_directories("dir-test");
    for (const auto& d : dirs) {
        EXPECT_NE(d, tmp.path() / "protoflow" / "dir-test");
    }

    // Now create it
    fs::create_directories(tmp.path() / "protoflow" / "dir-test");
    dirs = config_directories("dir-test");
    EXPECT_FALSE(dirs.empty());
    EXPECT_EQ(dirs.front(), tmp.path() / "protoflow" / "dir-test");
}

// ---------------------------------------------------------------------------
// Tests: find_config_file
// ---------------------------------------------------------------------------

TEST(ConfigPaths, FindConfigFileFindsExisting) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto conf_dir = tmp.path() / "protoflow" / "find-app";
    fs::create_directories(conf_dir);

    // Write a config file
    {
        std::ofstream out(conf_dir / "app.conf");
        out << "[general]\nname = test\n";
    }

    auto result = find_config_file("find-app", "app.conf");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, conf_dir / "app.conf");
}

TEST(ConfigPaths, FindConfigFileReturnsNulloptWhenMissing) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto result = find_config_file("nonexistent-app", "nope.conf");
    EXPECT_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// Tests: find_all_config_files
// ---------------------------------------------------------------------------

TEST(ConfigPaths, FindAllConfigFilesReturnsMultiple) {
    TempDir tmp;
    // Put the same filename in both a "user" and a fake "system" dir.
    // We simulate this by pointing XDG to one dir and creating /etc equiv
    // under the same tree is not possible, so we just test with one layer.
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto conf_dir = tmp.path() / "protoflow" / "multi-app";
    fs::create_directories(conf_dir);
    {
        std::ofstream out(conf_dir / "common.conf");
        out << "data=user\n";
    }

    auto files = find_all_config_files("multi-app", "common.conf");
    EXPECT_GE(files.size(), 1U);
    EXPECT_EQ(files.front(), conf_dir / "common.conf");
}

// ---------------------------------------------------------------------------
// Tests: user_config_directory
// ---------------------------------------------------------------------------

TEST(ConfigPaths, UserConfigDirectoryReturnsPath) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto dir = user_config_directory("user-app");
    ASSERT_TRUE(dir.has_value());
    EXPECT_EQ(*dir, tmp.path() / "protoflow" / "user-app");
}

TEST(ConfigPaths, UserConfigDirectoryCreatesOnDemand) {
    TempDir tmp;
    ScopedEnv env("XDG_CONFIG_HOME", tmp.path().c_str());

    auto dir = user_config_directory("created-app", /*create=*/true);
    ASSERT_TRUE(dir.has_value());
    EXPECT_TRUE(fs::is_directory(*dir));
}

// ---------------------------------------------------------------------------
// Tests: system_config_directory
// ---------------------------------------------------------------------------

TEST(ConfigPaths, SystemConfigDirectoryReturnsEtc) {
    auto dir = system_config_directory("sys-app");
    EXPECT_EQ(dir, fs::path("/etc/protoflow/sys-app"));
}
