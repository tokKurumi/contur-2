/// @file test_page_table.cpp
/// @brief Unit tests for PageTable.

#include <gtest/gtest.h>

#include "contur/memory/page_table.h"

using namespace contur;

TEST(PageTableTest, ConstructionWithPageCount)
{
    PageTable pt(16);
    EXPECT_EQ(pt.pageCount(), 16u);
    EXPECT_EQ(pt.presentCount(), 0u);
}

TEST(PageTableTest, MapAndTranslate)
{
    PageTable pt(4);
    auto mapResult = pt.map(0, 10);
    ASSERT_TRUE(mapResult.isOk());

    auto translateResult = pt.translate(0);
    ASSERT_TRUE(translateResult.isOk());
    EXPECT_EQ(translateResult.value(), 10u);
}

TEST(PageTableTest, MapSetsPresent)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(1, 5).isOk());
    EXPECT_EQ(pt.presentCount(), 1u);

    auto entry = pt.getEntry(1);
    ASSERT_TRUE(entry.isOk());
    EXPECT_TRUE(entry.value().present);
    EXPECT_EQ(entry.value().frameId, 5u);
    EXPECT_FALSE(entry.value().dirty);
    EXPECT_TRUE(entry.value().referenced);
}

TEST(PageTableTest, UnmapClearsEntry)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(2, 7).isOk());
    EXPECT_EQ(pt.presentCount(), 1u);

    auto unmapResult = pt.unmap(2);
    ASSERT_TRUE(unmapResult.isOk());
    EXPECT_EQ(pt.presentCount(), 0u);

    auto entry = pt.getEntry(2);
    ASSERT_TRUE(entry.isOk());
    EXPECT_FALSE(entry.value().present);
    EXPECT_EQ(entry.value().frameId, INVALID_FRAME);
}

TEST(PageTableTest, TranslateUnmappedPage)
{
    PageTable pt(4);
    auto result = pt.translate(0);
    ASSERT_TRUE(result.isError());
    // Unmapped page should return SegmentationFault
    EXPECT_EQ(result.errorCode(), ErrorCode::SegmentationFault);
}

TEST(PageTableTest, MapOutOfRange)
{
    PageTable pt(4);
    auto result = pt.map(10, 0);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PageTableTest, UnmapOutOfRange)
{
    PageTable pt(4);
    auto result = pt.unmap(10);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PageTableTest, TranslateOutOfRange)
{
    PageTable pt(4);
    auto result = pt.translate(10);
    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidAddress);
}

TEST(PageTableTest, SetReferencedBit)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(0, 1).isOk());

    auto setResult = pt.setReferenced(0, true);
    ASSERT_TRUE(setResult.isOk());

    auto entry = pt.getEntry(0);
    ASSERT_TRUE(entry.isOk());
    EXPECT_TRUE(entry.value().referenced);

    ASSERT_TRUE(pt.setReferenced(0, false).isOk());
    entry = pt.getEntry(0);
    ASSERT_TRUE(entry.isOk());
    EXPECT_FALSE(entry.value().referenced);
}

TEST(PageTableTest, SetDirtyBit)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(0, 1).isOk());

    auto setResult = pt.setDirty(0, true);
    ASSERT_TRUE(setResult.isOk());

    auto entry = pt.getEntry(0);
    ASSERT_TRUE(entry.isOk());
    EXPECT_TRUE(entry.value().dirty);
}

TEST(PageTableTest, ClearReferenced)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(0, 1).isOk());
    ASSERT_TRUE(pt.setReferenced(0, true).isOk());

    auto clearResult = pt.clearReferenced(0);
    ASSERT_TRUE(clearResult.isOk());

    auto entry = pt.getEntry(0);
    ASSERT_TRUE(entry.isOk());
    EXPECT_FALSE(entry.value().referenced);
}

TEST(PageTableTest, Clear)
{
    PageTable pt(4);
    ASSERT_TRUE(pt.map(0, 10).isOk());
    ASSERT_TRUE(pt.map(1, 11).isOk());
    ASSERT_TRUE(pt.map(2, 12).isOk());
    EXPECT_EQ(pt.presentCount(), 3u);

    pt.clear();
    EXPECT_EQ(pt.presentCount(), 0u);

    for (std::size_t i = 0; i < 4; ++i)
    {
        auto entry = pt.getEntry(i);
        ASSERT_TRUE(entry.isOk());
        EXPECT_FALSE(entry.value().present);
    }
}

TEST(PageTableTest, MultipleMapUnmap)
{
    PageTable pt(8);

    ASSERT_TRUE(pt.map(0, 100).isOk());
    ASSERT_TRUE(pt.map(3, 103).isOk());
    ASSERT_TRUE(pt.map(7, 107).isOk());
    EXPECT_EQ(pt.presentCount(), 3u);

    ASSERT_TRUE(pt.unmap(3).isOk());
    EXPECT_EQ(pt.presentCount(), 2u);

    auto result0 = pt.translate(0);
    ASSERT_TRUE(result0.isOk());
    EXPECT_EQ(result0.value(), 100u);

    auto result3 = pt.translate(3);
    ASSERT_TRUE(result3.isError());

    auto result7 = pt.translate(7);
    ASSERT_TRUE(result7.isOk());
    EXPECT_EQ(result7.value(), 107u);
}

TEST(PageTableTest, MoveSemantics)
{
    PageTable pt1(4);
    ASSERT_TRUE(pt1.map(0, 10).isOk());
    ASSERT_TRUE(pt1.map(1, 20).isOk());

    PageTable pt2 = std::move(pt1);
    EXPECT_EQ(pt2.pageCount(), 4u);
    EXPECT_EQ(pt2.presentCount(), 2u);

    auto result = pt2.translate(0);
    ASSERT_TRUE(result.isOk());
    EXPECT_EQ(result.value(), 10u);
}
