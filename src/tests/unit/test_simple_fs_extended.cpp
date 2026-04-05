/// @file test_simple_fs_extended.cpp
/// @brief Extended unit tests for SimpleFS — descriptor lifecycle, directory
///        nesting, multi-block files, stat/remove edge cases.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "contur/fs/simple_fs.h"

using namespace contur;

namespace {

    std::vector<std::byte> asBytes(const std::vector<std::uint8_t> &src)
    {
        std::vector<std::byte> out;
        out.reserve(src.size());
        for (auto v : src)
            out.push_back(static_cast<std::byte>(v));
        return out;
    }

    std::vector<std::uint8_t> asUints(const std::vector<std::byte> &src)
    {
        std::vector<std::uint8_t> out;
        out.reserve(src.size());
        for (auto b : src)
            out.push_back(static_cast<std::uint8_t>(b));
        return out;
    }

} // namespace

// 128 blocks × 64 bytes = 8 KB — large enough to test multi-block files
class FsExtTest : public ::testing::Test
{
    protected:
    SimpleFS fs{128, 64};
};

// ---------------------------------------------------------------------------
// Descriptor lifecycle error paths
// ---------------------------------------------------------------------------

TEST_F(FsExtTest, WriteToReadOnlyDescriptorFails)
{
    auto fdCreate = fs.open("/ro.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdCreate.isOk());
    ASSERT_TRUE(fs.close(fdCreate.value()).isOk());

    auto fdRead = fs.open("/ro.bin", OpenMode::Read);
    ASSERT_TRUE(fdRead.isOk());

    auto payload = asBytes({1, 2, 3});
    auto result = fs.write(fdRead.value(), {payload.data(), payload.size()});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::PermissionDenied);

    ASSERT_TRUE(fs.close(fdRead.value()).isOk());
}

TEST_F(FsExtTest, CloseInvalidDescriptorFails)
{
    auto result = fs.close(static_cast<FileDescriptor>(9999));
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, ReadAfterCloseDescriptorFails)
{
    auto payload = asBytes({10, 20, 30});
    auto fdW = fs.open("/closeme.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdW.isOk());
    ASSERT_TRUE(fs.write(fdW.value(), {payload.data(), payload.size()}).isOk());
    ASSERT_TRUE(fs.close(fdW.value()).isOk());

    auto fdR = fs.open("/closeme.bin", OpenMode::Read);
    ASSERT_TRUE(fdR.isOk());
    FileDescriptor closedFd = fdR.value();
    ASSERT_TRUE(fs.close(closedFd).isOk());

    std::vector<std::byte> buf(3, std::byte{0});
    auto result = fs.read(closedFd, {buf.data(), buf.size()});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, WriteAfterCloseDescriptorFails)
{
    auto fdW = fs.open("/wclose.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdW.isOk());
    FileDescriptor closedFd = fdW.value();
    ASSERT_TRUE(fs.close(closedFd).isOk());

    auto payload = asBytes({5, 6, 7});
    auto result = fs.write(closedFd, {payload.data(), payload.size()});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

// ---------------------------------------------------------------------------
// Directory operations
// ---------------------------------------------------------------------------

TEST_F(FsExtTest, MkdirAlreadyExistingFails)
{
    ASSERT_TRUE(fs.mkdir("/dup").isOk());
    auto result = fs.mkdir("/dup");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::AlreadyExists);
}

TEST_F(FsExtTest, MkdirInNonExistentParentFails)
{
    auto result = fs.mkdir("/ghost/child");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, NestedDirectoryCreateAndList)
{
    ASSERT_TRUE(fs.mkdir("/a").isOk());
    ASSERT_TRUE(fs.mkdir("/a/b").isOk());
    ASSERT_TRUE(fs.mkdir("/a/b/c").isOk());

    auto listing = fs.listDir("/a/b");
    ASSERT_TRUE(listing.isOk());
    ASSERT_EQ(listing.value().size(), 1u);
    EXPECT_EQ(listing.value()[0].name, "c");
    EXPECT_EQ(listing.value()[0].type, InodeType::Directory);
}

TEST_F(FsExtTest, RemoveEmptyDirectorySucceeds)
{
    ASSERT_TRUE(fs.mkdir("/empty").isOk());
    ASSERT_TRUE(fs.remove("/empty").isOk());

    auto root = fs.listDir("/");
    ASSERT_TRUE(root.isOk());
    EXPECT_TRUE(root.value().empty());
}

TEST_F(FsExtTest, ListDirOnFileFails)
{
    auto fd = fs.open("/afile.txt", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd.isOk());
    ASSERT_TRUE(fs.close(fd.value()).isOk());

    auto result = fs.listDir("/afile.txt");
    ASSERT_TRUE(result.isError());
}

TEST_F(FsExtTest, ListDirOnNonExistentPathFails)
{
    auto result = fs.listDir("/nonexistent");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, CreateFileInSubdirectory)
{
    ASSERT_TRUE(fs.mkdir("/subdir").isOk());
    auto fd = fs.open("/subdir/file.txt", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd.isOk());
    auto payload = asBytes({10, 20, 30});
    ASSERT_TRUE(fs.write(fd.value(), {payload.data(), payload.size()}).isOk());
    ASSERT_TRUE(fs.close(fd.value()).isOk());

    auto children = fs.listDir("/subdir");
    ASSERT_TRUE(children.isOk());
    ASSERT_EQ(children.value().size(), 1u);
    EXPECT_EQ(children.value()[0].name, "file.txt");
    EXPECT_EQ(children.value()[0].type, InodeType::File);
}

TEST_F(FsExtTest, ReadFileFromSubdirectory)
{
    ASSERT_TRUE(fs.mkdir("/data").isOk());
    auto fdW = fs.open("/data/sample.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdW.isOk());
    auto payload = asBytes({7, 8, 9, 10});
    ASSERT_TRUE(fs.write(fdW.value(), {payload.data(), payload.size()}).isOk());
    ASSERT_TRUE(fs.close(fdW.value()).isOk());

    auto fdR = fs.open("/data/sample.bin", OpenMode::Read);
    ASSERT_TRUE(fdR.isOk());
    std::vector<std::byte> buf(4, std::byte{0});
    auto read = fs.read(fdR.value(), {buf.data(), buf.size()});
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 4u);
    EXPECT_EQ(asUints(buf), (std::vector<std::uint8_t>{7, 8, 9, 10}));
    ASSERT_TRUE(fs.close(fdR.value()).isOk());
}

TEST_F(FsExtTest, MultipleFilesInSameDirectory)
{
    for (int i = 0; i < 5; ++i)
    {
        std::string name = "/file" + std::to_string(i) + ".txt";
        auto fd = fs.open(name, OpenMode::Create | OpenMode::Write);
        ASSERT_TRUE(fd.isOk()) << "Failed for " << name;
        ASSERT_TRUE(fs.close(fd.value()).isOk());
    }
    auto listing = fs.listDir("/");
    ASSERT_TRUE(listing.isOk());
    EXPECT_EQ(listing.value().size(), 5u);
}

// ---------------------------------------------------------------------------
// Write / read accumulation
// ---------------------------------------------------------------------------

TEST_F(FsExtTest, MultipleWritesAccumulateInOrder)
{
    auto fdW = fs.open("/accum.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdW.isOk());

    auto a = asBytes({1, 2, 3});
    auto b = asBytes({4, 5});
    auto c = asBytes({6});
    ASSERT_TRUE(fs.write(fdW.value(), {a.data(), a.size()}).isOk());
    ASSERT_TRUE(fs.write(fdW.value(), {b.data(), b.size()}).isOk());
    ASSERT_TRUE(fs.write(fdW.value(), {c.data(), c.size()}).isOk());
    ASSERT_TRUE(fs.close(fdW.value()).isOk());

    auto fdR = fs.open("/accum.bin", OpenMode::Read);
    ASSERT_TRUE(fdR.isOk());
    std::vector<std::byte> buf(6, std::byte{0});
    auto read = fs.read(fdR.value(), {buf.data(), buf.size()});
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), 6u);
    EXPECT_EQ(asUints(buf), (std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6}));
    ASSERT_TRUE(fs.close(fdR.value()).isOk());
}

TEST_F(FsExtTest, LargeFileSpanningMultipleBlocks)
{
    // 200 bytes with 64-byte blocks → 4 blocks needed
    const std::size_t dataSize = 200;
    std::vector<std::uint8_t> src(dataSize);
    for (std::size_t i = 0; i < dataSize; ++i)
        src[i] = static_cast<std::uint8_t>(i % 256);
    auto payload = asBytes(src);

    auto fdW = fs.open("/large.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fdW.isOk());
    auto wrote = fs.write(fdW.value(), {payload.data(), payload.size()});
    ASSERT_TRUE(wrote.isOk());
    EXPECT_EQ(wrote.value(), dataSize);
    ASSERT_TRUE(fs.close(fdW.value()).isOk());

    auto fdR = fs.open("/large.bin", OpenMode::Read);
    ASSERT_TRUE(fdR.isOk());
    std::vector<std::byte> buf(dataSize, std::byte{0});
    auto read = fs.read(fdR.value(), {buf.data(), buf.size()});
    ASSERT_TRUE(read.isOk());
    EXPECT_EQ(read.value(), dataSize);
    EXPECT_EQ(asUints(buf), src);
    ASSERT_TRUE(fs.close(fdR.value()).isOk());

    auto info = fs.stat("/large.bin");
    ASSERT_TRUE(info.isOk());
    EXPECT_EQ(info.value().size, dataSize);
    EXPECT_GT(info.value().blockCount, 1u);
}

// ---------------------------------------------------------------------------
// Stat edge cases
// ---------------------------------------------------------------------------

TEST_F(FsExtTest, StatOnNonExistentPathFails)
{
    auto result = fs.stat("/ghost.txt");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, StatOnDirectoryReturnsDirectoryType)
{
    ASSERT_TRUE(fs.mkdir("/statdir").isOk());
    auto result = fs.stat("/statdir");
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value().type, InodeType::Directory);
}

TEST_F(FsExtTest, StatSizeMatchesWrittenBytes)
{
    auto payload = asBytes({1, 2, 3, 4, 5});
    auto fd = fs.open("/sized.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd.isOk());
    ASSERT_TRUE(fs.write(fd.value(), {payload.data(), payload.size()}).isOk());
    ASSERT_TRUE(fs.close(fd.value()).isOk());

    auto info = fs.stat("/sized.bin");
    ASSERT_TRUE(info.isOk());
    EXPECT_EQ(info.value().size, 5u);
}

// ---------------------------------------------------------------------------
// Remove edge cases
// ---------------------------------------------------------------------------

TEST_F(FsExtTest, RemoveNonExistentPathFails)
{
    auto result = fs.remove("/nothing");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST_F(FsExtTest, CanRecreateFileAfterRemove)
{
    auto fd1 = fs.open("/temp.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd1.isOk());
    ASSERT_TRUE(fs.close(fd1.value()).isOk());
    ASSERT_TRUE(fs.remove("/temp.bin").isOk());

    auto fd2 = fs.open("/temp.bin", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd2.isOk());
    auto payload = asBytes({42});
    ASSERT_TRUE(fs.write(fd2.value(), {payload.data(), payload.size()}).isOk());
    ASSERT_TRUE(fs.close(fd2.value()).isOk());

    auto info = fs.stat("/temp.bin");
    ASSERT_TRUE(info.isOk());
    EXPECT_EQ(info.value().size, 1u);
}

TEST_F(FsExtTest, RemoveNonEmptyDirectoryFails)
{
    ASSERT_TRUE(fs.mkdir("/nonempty").isOk());
    auto fd = fs.open("/nonempty/child.txt", OpenMode::Create | OpenMode::Write);
    ASSERT_TRUE(fd.isOk());
    ASSERT_TRUE(fs.close(fd.value()).isOk());

    auto result = fs.remove("/nonempty");
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidState);
}
