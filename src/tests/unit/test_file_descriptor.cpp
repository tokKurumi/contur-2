/// @file test_file_descriptor.cpp
/// @brief Unit tests for FileDescriptor / OpenMode / FileDescriptorTable.

#include <set>

#include <gtest/gtest.h>

#include "contur/core/error.h"
#include "contur/fs/file_descriptor.h"
#include "contur/fs/inode.h"

using namespace contur;

namespace {

    OpenFileState makeState(InodeId id, OpenMode mode = OpenMode::Read, std::size_t offset = 0)
    {
        OpenFileState state;
        state.inodeId = id;
        state.mode = mode;
        state.offset = offset;
        return state;
    }

} // namespace

// FileDescriptor value semantics

TEST(FileDescriptorTest, DefaultIsInvalid)
{
    FileDescriptor fd;
    EXPECT_FALSE(fd.valid());
    EXPECT_EQ(fd.value, -1);
}

TEST(FileDescriptorTest, NonNegativeValuesAreValid)
{
    FileDescriptor fd{0};
    EXPECT_TRUE(fd.valid());

    FileDescriptor large{12345};
    EXPECT_TRUE(large.valid());
}

TEST(FileDescriptorTest, NegativeValuesAreInvalid)
{
    FileDescriptor a{-1};
    FileDescriptor b{-42};
    EXPECT_FALSE(a.valid());
    EXPECT_FALSE(b.valid());
}

TEST(FileDescriptorTest, EqualityComparesValue)
{
    FileDescriptor a{3};
    FileDescriptor b{3};
    FileDescriptor c{4};

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

// OpenMode bit-flag semantics

TEST(OpenModeTest, BitwiseOrCombinesFlags)
{
    auto combined = OpenMode::Read | OpenMode::Write;
    EXPECT_TRUE(hasOpenMode(combined, OpenMode::Read));
    EXPECT_TRUE(hasOpenMode(combined, OpenMode::Write));
    EXPECT_FALSE(hasOpenMode(combined, OpenMode::Append));
    EXPECT_FALSE(hasOpenMode(combined, OpenMode::Create));
    EXPECT_FALSE(hasOpenMode(combined, OpenMode::Truncate));
}

TEST(OpenModeTest, BitwiseAndIsolatesFlags)
{
    auto combined = OpenMode::Read | OpenMode::Write | OpenMode::Create;
    auto isolated = combined & OpenMode::Create;
    EXPECT_TRUE(hasOpenMode(isolated, OpenMode::Create));
    EXPECT_FALSE(hasOpenMode(isolated, OpenMode::Read));
}

TEST(OpenModeTest, AllFiveFlagsCombinable)
{
    auto all = OpenMode::Read | OpenMode::Write | OpenMode::Create | OpenMode::Truncate | OpenMode::Append;
    EXPECT_TRUE(hasOpenMode(all, OpenMode::Read));
    EXPECT_TRUE(hasOpenMode(all, OpenMode::Write));
    EXPECT_TRUE(hasOpenMode(all, OpenMode::Create));
    EXPECT_TRUE(hasOpenMode(all, OpenMode::Truncate));
    EXPECT_TRUE(hasOpenMode(all, OpenMode::Append));
}

TEST(OpenModeTest, NoneFlagHasNothing)
{
    EXPECT_FALSE(hasOpenMode(OpenMode::None, OpenMode::Read));
    EXPECT_FALSE(hasOpenMode(OpenMode::None, OpenMode::Write));
}

// FileDescriptorTable behavior

TEST(FileDescriptorTableTest, InitiallyEmpty)
{
    FileDescriptorTable table;
    EXPECT_EQ(table.openCount(), 0u);
    EXPECT_FALSE(table.contains(FileDescriptor{0}));
}

TEST(FileDescriptorTableTest, OpenReturnsValidDescriptor)
{
    FileDescriptorTable table;
    auto fdResult = table.open(makeState(7));
    ASSERT_TRUE(fdResult.isOk());

    auto fd = fdResult.value();
    EXPECT_TRUE(fd.valid());
    EXPECT_EQ(table.openCount(), 1u);
    EXPECT_TRUE(table.contains(fd));
}

TEST(FileDescriptorTableTest, GetReturnsStoredState)
{
    FileDescriptorTable table;
    auto fd = table.open(makeState(11, OpenMode::Read | OpenMode::Append, 64)).value();

    auto state = table.get(fd);
    ASSERT_TRUE(state.isOk());
    EXPECT_EQ(state.value().inodeId, 11u);
    EXPECT_EQ(state.value().offset, 64u);
    EXPECT_TRUE(hasOpenMode(state.value().mode, OpenMode::Read));
    EXPECT_TRUE(hasOpenMode(state.value().mode, OpenMode::Append));
}

TEST(FileDescriptorTableTest, SetUpdatesState)
{
    FileDescriptorTable table;
    auto fd = table.open(makeState(1, OpenMode::Read, 0)).value();

    OpenFileState updated;
    updated.inodeId = 1;
    updated.mode = OpenMode::Write;
    updated.offset = 1024;
    ASSERT_TRUE(table.set(fd, updated).isOk());

    auto state = table.get(fd);
    ASSERT_TRUE(state.isOk());
    EXPECT_EQ(state.value().offset, 1024u);
    EXPECT_TRUE(hasOpenMode(state.value().mode, OpenMode::Write));
    EXPECT_FALSE(hasOpenMode(state.value().mode, OpenMode::Read));
}

TEST(FileDescriptorTableTest, CloseRemovesDescriptor)
{
    FileDescriptorTable table;
    auto fd = table.open(makeState(1)).value();

    ASSERT_TRUE(table.close(fd).isOk());
    EXPECT_EQ(table.openCount(), 0u);
    EXPECT_FALSE(table.contains(fd));
}

TEST(FileDescriptorTableTest, GetMissingDescriptorReturnsNotFound)
{
    FileDescriptorTable table;
    auto state = table.get(FileDescriptor{99});
    ASSERT_TRUE(state.isError());
    EXPECT_EQ(state.errorCode(), ErrorCode::NotFound);
}

TEST(FileDescriptorTableTest, SetMissingDescriptorReturnsNotFound)
{
    FileDescriptorTable table;
    auto result = table.set(FileDescriptor{99}, makeState(1));
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(FileDescriptorTableTest, CloseMissingDescriptorReturnsNotFound)
{
    FileDescriptorTable table;
    auto result = table.close(FileDescriptor{99});
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::NotFound);
}

TEST(FileDescriptorTableTest, CloseTwiceReturnsNotFoundSecondTime)
{
    FileDescriptorTable table;
    auto fd = table.open(makeState(1)).value();

    ASSERT_TRUE(table.close(fd).isOk());
    auto second = table.close(fd);
    ASSERT_TRUE(second.isError());
    EXPECT_EQ(second.errorCode(), ErrorCode::NotFound);
}

TEST(FileDescriptorTableTest, DescriptorsHaveUniqueValues)
{
    FileDescriptorTable table;

    std::set<std::int32_t> values;
    for (int i = 0; i < 16; ++i)
    {
        auto fd = table.open(makeState(static_cast<InodeId>(i)));
        ASSERT_TRUE(fd.isOk());
        EXPECT_TRUE(values.insert(fd.value().value).second);
    }
    EXPECT_EQ(table.openCount(), 16u);
}

TEST(FileDescriptorTableTest, OpenCountTracksInsertionsAndDeletions)
{
    FileDescriptorTable table;

    auto fd1 = table.open(makeState(1)).value();
    auto fd2 = table.open(makeState(2)).value();
    auto fd3 = table.open(makeState(3)).value();
    EXPECT_EQ(table.openCount(), 3u);

    ASSERT_TRUE(table.close(fd2).isOk());
    EXPECT_EQ(table.openCount(), 2u);
    EXPECT_TRUE(table.contains(fd1));
    EXPECT_FALSE(table.contains(fd2));
    EXPECT_TRUE(table.contains(fd3));
}

TEST(FileDescriptorTableTest, MoveConstructionPreservesEntries)
{
    FileDescriptorTable original;
    auto fd = original.open(makeState(5)).value();

    FileDescriptorTable moved(std::move(original));
    EXPECT_EQ(moved.openCount(), 1u);
    EXPECT_TRUE(moved.contains(fd));

    auto state = moved.get(fd);
    ASSERT_TRUE(state.isOk());
    EXPECT_EQ(state.value().inodeId, 5u);
}

TEST(FileDescriptorTableTest, IndependentTablesDoNotShareState)
{
    FileDescriptorTable a;
    FileDescriptorTable b;

    auto fdA = a.open(makeState(1)).value();
    EXPECT_FALSE(b.contains(fdA));
    EXPECT_EQ(b.openCount(), 0u);
}
