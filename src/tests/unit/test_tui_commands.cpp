/// @file test_tui_commands.cpp
/// @brief Unit tests for TUI command contracts and validation helpers.

#include <gtest/gtest.h>

#include "contur/core/error.h"

#include "contur/tui/tui_commands.h"

using namespace contur;

TEST(TuiCommandsTest, ValidatePlaybackConfigRejectsZeroStep)
{
    TuiPlaybackConfig config;
    config.step = 0;

    auto result = validatePlaybackConfig(config);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(TuiCommandsTest, ValidatePlaybackConfigRejectsZeroInterval)
{
    TuiPlaybackConfig config;
    config.intervalMs = 0;

    auto result = validatePlaybackConfig(config);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(TuiCommandsTest, ValidateCommandRejectsZeroStepForTick)
{
    TuiCommand command;
    command.kind = TuiCommandKind::Tick;
    command.step = 0;

    auto result = validateCommand(command);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(TuiCommandsTest, ValidateCommandRejectsZeroIntervalForAutoplayStart)
{
    TuiCommand command;
    command.kind = TuiCommandKind::AutoPlayStart;
    command.step = 2;
    command.intervalMs = 0;

    auto result = validateCommand(command);

    ASSERT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST(TuiCommandsTest, ValidateCommandAllowsPauseWithoutStepValidation)
{
    TuiCommand command;
    command.kind = TuiCommandKind::Pause;
    command.step = 0;

    auto result = validateCommand(command);

    ASSERT_TRUE(result.isOk());
}

TEST(TuiCommandsTest, PlaybackConfigFromCommandCopiesFields)
{
    TuiCommand command;
    command.kind = TuiCommandKind::AutoPlayStart;
    command.step = 4;
    command.intervalMs = 250;

    TuiPlaybackConfig config = playbackConfigFromCommand(command);

    EXPECT_EQ(config.step, 4u);
    EXPECT_EQ(config.intervalMs, 250u);
}
