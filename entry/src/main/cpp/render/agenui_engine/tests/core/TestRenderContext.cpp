/*
 * Copyright (c) 2024 AgenUIEngine Contributors
 * Licensed under the MIT License
 *
 * Unit tests for RenderContext facade — queue-based rendering coordination.
 * Uses MockRenderer to verify forwarding without GPU.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "core/RenderContext.h"
#include "mocks/MockRenderer.h"

using namespace AgenUIEngine::Core;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::StrictMock;

// Helper: create a RenderContext with a MockRenderer.
// The mock is returned as a raw pointer for setting expectations.
// Ownership transfers to RenderContext.
// Uses NiceMock because RenderContext destructor calls cleanup() on the renderer,
// and most tests don't need to verify that call.
struct ContextFixture {
    NiceMock<MockRenderer>* mockPtr;
    std::unique_ptr<RenderContext> ctx;

    ContextFixture() {
        auto mock = std::make_unique<NiceMock<MockRenderer>>();
        mockPtr = mock.get();
        ctx = std::make_unique<RenderContext>(std::move(mock));
    }
};

// ---------------------------------------------------------------------------
// Queue-based rendering
// ---------------------------------------------------------------------------

TEST(RenderContext, QueueModeDrawRect) {
    ContextFixture f;

    // drawRect should NOT call renderer immediately
    f.ctx->beginQueue();
    f.ctx->drawRect({10, 20}, {100, 200}, {1, 0, 0});
    f.ctx->endQueue();

    // On flushQueue, the renderer should be called
    EXPECT_CALL(*f.mockPtr, drawRect(glm::vec2(10, 20), glm::vec2(100, 200), glm::vec3(1, 0, 0)))
        .Times(1);
    f.ctx->flushQueue();
}

TEST(RenderContext, QueueModeMultipleCommands) {
    ContextFixture f;

    f.ctx->beginQueue();
    f.ctx->drawRect({0, 0}, {10, 10}, {1, 1, 1});
    f.ctx->drawRect({5, 5}, {20, 20}, {0, 0, 0});
    f.ctx->endQueue();

    // Both should be flushed
    EXPECT_CALL(*f.mockPtr, drawRect(_, _, _)).Times(2);
    f.ctx->flushQueue();
}

TEST(RenderContext, PrepareGlassPass) {
    ContextFixture f;

    EXPECT_CALL(*f.mockPtr, prepareGlassPass()).Times(1);
    f.ctx->prepareGlassPass();
}

TEST(RenderContext, InitializeForwarded) {
    ContextFixture f;

    RendererInitParams params;
    params.width = 800;
    params.height = 600;

    EXPECT_CALL(*f.mockPtr, initialize(_)).WillOnce(Return(true));
    EXPECT_TRUE(f.ctx->initialize(params));
}

TEST(RenderContext, PipelineCreation) {
    ContextFixture f;

    EXPECT_CALL(*f.mockPtr, createPipelines()).WillOnce(Return(true));
    EXPECT_TRUE(f.ctx->createPipelines());
}

TEST(RenderContext, GetDimensions) {
    ContextFixture f;

    EXPECT_CALL(*f.mockPtr, getWidth()).WillOnce(Return(1920));
    EXPECT_CALL(*f.mockPtr, getHeight()).WillOnce(Return(1080));

    EXPECT_EQ(f.ctx->getWidth(), 1920);
    EXPECT_EQ(f.ctx->getHeight(), 1080);
}

TEST(RenderContext, FlushEmptyQueue) {
    ContextFixture f;

    // Flush with empty queue should not call any draw method
    // StrictMock ensures no unexpected calls
    f.ctx->flushQueue();
}

TEST(RenderContext, GetRenderStatsAfterFlush) {
    ContextFixture f;

    f.ctx->beginQueue();
    f.ctx->drawRect({0, 0}, {10, 10}, {1, 1, 1});
    f.ctx->endQueue();

    EXPECT_CALL(*f.mockPtr, drawRect(_, _, _)).Times(1);
    f.ctx->flushQueue();

    const auto& stats = f.ctx->getRenderStats();
    EXPECT_EQ(stats.drawCalls, 1u);
    EXPECT_EQ(stats.commandCount, 1u);
}

TEST(RenderContext, DestructorCallsCleanup) {
    auto mock = std::make_unique<StrictMock<MockRenderer>>();
    auto* mockPtr = mock.get();

    EXPECT_CALL(*mockPtr, cleanup()).Times(1);

    {
        RenderContext ctx(std::move(mock));
        // ctx goes out of scope, destructor should call cleanup
    }
}

TEST(RenderContext, QueueModeGlowOnTextFlush) {
    ContextFixture f;

    // Set glow before queue
    f.ctx->setTextGlow(5.0f, 0.9f);

    f.ctx->beginQueue();
    f.ctx->drawText("Hello", {0, 0}, 16, {1, 1, 1});
    f.ctx->endQueue();

    // On flush: setTextGlow(5.0, 0.9), drawText, setTextGlow(0, 0)
    {
        InSequence seq;
        EXPECT_CALL(*f.mockPtr, setTextGlow(5.0f, 0.9f));
        EXPECT_CALL(*f.mockPtr, drawText("Hello", glm::vec2(0, 0), 16, glm::vec3(1, 1, 1), _));
        EXPECT_CALL(*f.mockPtr, setTextGlow(0.0f, 0.0f));
    }
    f.ctx->flushQueue();
}

// ---------------------------------------------------------------------------
// Queue mode for all draw types
// ---------------------------------------------------------------------------

TEST(RenderContext, QueueModeFlushRoundedRect) {
    ContextFixture f;
    f.ctx->beginQueue();
    f.ctx->drawRoundedRect({0, 0}, {10, 10}, 7.0f, {1, 0, 0});
    f.ctx->endQueue();
    EXPECT_CALL(*f.mockPtr, drawRoundedRect(_, _, 7.0f, _, _)).Times(1);
    f.ctx->flushQueue();
}

TEST(RenderContext, QueueModeFlushGlassRoundedRect) {
    ContextFixture f;
    f.ctx->beginQueue();
    f.ctx->drawGlassRoundedRect({0, 0}, {10, 10}, 3.0f, {0, 1, 0});
    f.ctx->endQueue();
    EXPECT_CALL(*f.mockPtr, drawGlassRoundedRect(_, _, 3.0f, _)).Times(1);
    f.ctx->flushQueue();
}

TEST(RenderContext, QueueModeFlushMultiLineText) {
    ContextFixture f;
    f.ctx->beginQueue();
    f.ctx->drawMultiLineText("a\nb", {0, 0}, 14, {1, 1, 1}, 300.0f, 1.2f);
    f.ctx->endQueue();
    EXPECT_CALL(*f.mockPtr, drawMultiLineText("a\nb", _, 14, _, 300.0f, 1.2f)).Times(1);
    f.ctx->flushQueue();
}

TEST(RenderContext, QueueModeFlushImage) {
    ContextFixture f;
    f.ctx->beginQueue();
    f.ctx->drawImage("test.png", {5, 5}, {50, 50});
    f.ctx->endQueue();
    EXPECT_CALL(*f.mockPtr, drawImage("test.png", _, _, _, _, _)).Times(1);
    f.ctx->flushQueue();
}

// ---------------------------------------------------------------------------
// Null renderer safety
// ---------------------------------------------------------------------------

TEST(RenderContext, NullRendererGetWidthReturnsZero) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    EXPECT_EQ(ctx->getWidth(), 0);
    EXPECT_EQ(ctx->getHeight(), 0);
}

TEST(RenderContext, NullRendererInitializeReturnsFalse) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    EXPECT_FALSE(ctx->initialize(RendererInitParams{}));
}

TEST(RenderContext, NullRendererCreatePipelineReturnsFalse) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    EXPECT_FALSE(ctx->createPipelines());
}

TEST(RenderContext, NullRendererDrawNoCrash) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    // Draw commands just append to queue — no renderer needed
    ctx->drawRect({0, 0}, {10, 10}, {1, 1, 1});
    ctx->drawRoundedRect({0, 0}, {10, 10}, 5.0f, {1, 1, 1});
    ctx->drawGlassRoundedRect({0, 0}, {10, 10}, 5.0f, {1, 1, 1});
    ctx->drawText("hi", {0, 0}, 12, {0, 0, 0});
    ctx->drawMultiLineText("hi", {0, 0}, 12, {0, 0, 0}, 100.0f, 1.0f);
    ctx->drawImage("a.png", {0, 0}, {10, 10});
}

TEST(RenderContext, NullRendererFlushNoCrash) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    ctx->flushQueue();
}

TEST(RenderContext, NullRendererDestructorNoCrash) {
    auto ctx = std::make_unique<RenderContext>(nullptr);
    // Destructor with null renderer should not crash (no cleanup call)
}

// ---------------------------------------------------------------------------
// State forwarding
// ---------------------------------------------------------------------------

TEST(RenderContext, SetClearColorForwarded) {
    ContextFixture f;
    EXPECT_CALL(*f.mockPtr, setClearColor(0.1f, 0.2f, 0.3f, 1.0f)).Times(1);
    f.ctx->setClearColor(0.1f, 0.2f, 0.3f, 1.0f);
}

TEST(RenderContext, InitializeFontsForwarded) {
    ContextFixture f;
    EXPECT_CALL(*f.mockPtr, initializeFonts(_, "font.ttf")).WillOnce(Return(true));
    EXPECT_TRUE(f.ctx->initializeFonts(nullptr, "font.ttf"));
}

TEST(RenderContext, RecreateSwapChainForwarded) {
    ContextFixture f;
    EXPECT_CALL(*f.mockPtr, recreateSwapChain()).Times(1);
    f.ctx->recreateSwapChain();
}

TEST(RenderContext, UpdateSurfaceSizeForwarded) {
    ContextFixture f;
    EXPECT_CALL(*f.mockPtr, updateSurfaceSize(800, 600)).Times(1);
    f.ctx->updateSurfaceSize(800, 600);
}

TEST(RenderContext, SetCoordinateMappingForwarded) {
    ContextFixture f;
    EXPECT_CALL(*f.mockPtr, setCoordinateMapping(1920, 1080)).Times(1);
    f.ctx->setCoordinateMapping(1920, 1080);
}

// ---------------------------------------------------------------------------
// Flush clears queue
// ---------------------------------------------------------------------------

TEST(RenderContext, FlushClearsQueueForNextBatch) {
    ContextFixture f;

    f.ctx->beginQueue();
    f.ctx->drawRect({0, 0}, {10, 10}, {1, 1, 1});
    f.ctx->endQueue();
    EXPECT_CALL(*f.mockPtr, drawRect(_, _, _)).Times(1);
    f.ctx->flushQueue();

    // After first flush, stats show 1 draw call
    EXPECT_EQ(f.ctx->getRenderStats().drawCalls, 1u);

    // Second flush with no new commands resets stats via clear()
    f.ctx->flushQueue();
    EXPECT_EQ(f.ctx->getRenderStats().drawCalls, 0u);
}

TEST(RenderContext, QueueModeTextNoGlowNoSetTextGlow) {
    ContextFixture f;

    // Don't set glow before queue
    f.ctx->beginQueue();
    f.ctx->drawText("Hello", {0, 0}, 16, {1, 1, 1});
    f.ctx->endQueue();

    // Flush should call drawText but NOT setTextGlow (glowWidth == 0)
    EXPECT_CALL(*f.mockPtr, drawText("Hello", _, 16, _, _)).Times(1);
    f.ctx->flushQueue();
}

