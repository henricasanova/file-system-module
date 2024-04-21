#include <gtest/gtest.h>
#include <iostream>

#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Actor.hpp>

#include "../include/PathUtil.hpp"
#include "../include/FileSystem.hpp"
#include "../include/OneDiskStorage.hpp"
#include "../include/FileSystemException.hpp"

#include "./test_util.hpp"

namespace sgfs=simgrid::module::fs;
namespace sg4=simgrid::s4u;

XBT_LOG_NEW_DEFAULT_CATEGORY(fs_test, "File System Test");

class FileSystemTest : public ::testing::Test {
public:
    std::shared_ptr<sgfs::FileSystem> fs_;
    sg4::Host * host_{};

    FileSystemTest() = default;

    void setup_platform() {
        XBT_INFO("Creating a platform with one host and one disk...");
        auto *my_zone = sg4::create_full_zone("zone");
        host_ = my_zone->create_host("my_host", "100Gf");
        auto my_disk = host_->create_disk("my_disk", "1kBps", "2kBps");
        my_zone->seal();

        XBT_INFO("Creating a one-disk storage on the host's disk...");
        auto ods = sgfs::OneDiskStorage::create("my_storage", my_disk);
        XBT_INFO("Creating a file system...");
        fs_ = sgfs::FileSystem::create("my_fs");
        XBT_INFO("Mounting a 100kB partition...");
        fs_->mount_partition("/dev/a/", ods, "100kB");
    }

};

TEST_F(FileSystemTest, FileCreate)  {
    DO_TEST_WITH_FORK([this]() {
        this->setup_platform();

        // Create one actor (for this test we could likely do it all in the maestro but what the hell)
        sg4::Actor::create("FileCreator", host_, [this]() {
            XBT_INFO("Create a 10MB file at /foo/foo.txt, which should fail");
            ASSERT_THROW(this->fs_->create_file("/foo/foo.txt", "10MB"), sgfs::FileSystemException);
            XBT_INFO("Create a 10MB file at /dev/a/foo.txt, which should fail");
            ASSERT_THROW(this->fs_->create_file("/dev/a/foo.txt", "10MB"), sgfs::FileSystemException);
            XBT_INFO("Create a 10kB file at /dev/a/foo.txt, which should work");
            ASSERT_NO_THROW(this->fs_->create_file("/dev/a/foo.txt", "10kB"));
            XBT_INFO("Check remaining space");
            ASSERT_DOUBLE_EQ(this->fs_->partition_by_name("/dev/a")->get_free_space(), 90 * 1000);
        });

        // Run the simulation
        ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
        ASSERT_DOUBLE_EQ(this->fs_->partition_by_name("/dev/a")->get_free_space(), 90 * 1000);
    });
}


TEST_F(FileSystemTest, Directories)  {
    DO_TEST_WITH_FORK([this]() {
        this->setup_platform();

        // Create one actor (for this test we could likely do it all in the maestro but what the hell)
        sg4::Actor::create("FileCreator", host_, [this]() {
            XBT_INFO("Create a 10kB file at /dev/a/foo.txt");
            ASSERT_NO_THROW(fs_->create_file("/dev/a/foo.txt", "10kB"));
            XBT_INFO("Create a 10kB file at /dev/a/b/c/foo.txt");
            ASSERT_NO_THROW(fs_->create_file("/dev/a/b/c/foo.txt", "10kB"));
            ASSERT_TRUE(fs_->file_exists("/dev/a/b/c/foo.txt"));
            XBT_INFO("Create a 10kB file at /dev/a/b/c, which should fail");
            ASSERT_THROW(fs_->create_file("/dev/a/b/c/", "10kB"), sgfs::FileSystemException);
            XBT_INFO("Check that what should exist does");
            ASSERT_FALSE(fs_->file_exists("/dev/a/b/c"));
            XBT_INFO("Check free space");
            ASSERT_DOUBLE_EQ(fs_->partition_by_name("/dev/a//////")->get_free_space(), 80*1000);
        });

        // Run the simulation
        ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    });
}


TEST_F(FileSystemTest, FileMove)  {
    DO_TEST_WITH_FORK([this]() {
        this->setup_platform();

        // Create one actor (for this test we could likely do it all in the maestro but what the hell)
        sg4::Actor::create("FileCreator", host_, [this]() {
            XBT_INFO("Create a 10kB file at /dev/a/foo.txt");
            ASSERT_NO_THROW(fs_->create_file("/dev/a/foo.txt", "10kB"));
            ASSERT_DOUBLE_EQ(fs_->partition_by_name("/dev/a/")->get_free_space(), 90*1000);

            XBT_INFO("Move file /dev/a/foo.txt to /dev/a/b/c/foo.txt");
            ASSERT_NO_THROW(fs_->move_file("/dev/a/foo.txt", "/dev/a/b/c/foo.txt"));
            ASSERT_TRUE(fs_->file_exists("/dev/a/b/c/foo.txt"));
            ASSERT_FALSE(fs_->file_exists("/dev/a/foo.txt"));

            XBT_INFO("Create a 20kB file at /dev/a/stuff.txt");
            ASSERT_NO_THROW(fs_->create_file("/dev/a/stuff.txt", "20kB"));
            ASSERT_DOUBLE_EQ(fs_->partition_by_name("/dev/a/")->get_free_space(), 70*1000);

            XBT_INFO("Move file /dev/a/b/c/foo.txt to /dev/a/stuff.txt");
            ASSERT_NO_THROW(fs_->move_file("/dev/a/b/c/foo.txt", "/dev/a/stuff.txt"));
            ASSERT_FALSE(fs_->file_exists("/dev/a/b/c/foo.txt"));
            ASSERT_TRUE(fs_->file_exists("/dev/a/stuff.txt"));
            ASSERT_DOUBLE_EQ(fs_->partition_by_name("/dev/a/")->get_free_space(), 90*1000);
        });

        // Run the simulation
        ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    });
}