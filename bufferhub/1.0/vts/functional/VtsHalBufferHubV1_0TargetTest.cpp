/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "VtsHalBufferHubV1_0TargetTest"

#include <VtsHalHidlTargetTestBase.h>
#include <android/frameworks/bufferhub/1.0/IBufferClient.h>
#include <android/frameworks/bufferhub/1.0/IBufferHub.h>
#include <android/hardware_buffer.h>
#include <gtest/gtest.h>

using ::android::frameworks::bufferhub::V1_0::BufferHubStatus;
using ::android::frameworks::bufferhub::V1_0::BufferTraits;
using ::android::frameworks::bufferhub::V1_0::IBufferClient;
using ::android::frameworks::bufferhub::V1_0::IBufferHub;
using ::android::hardware::hidl_handle;
using ::android::hardware::graphics::common::V1_2::HardwareBufferDescription;

namespace android {

// Stride is an output that unknown before allocation.
const AHardwareBuffer_Desc kDesc = {
    /*width=*/640UL, /*height=*/480UL,
    /*layers=*/1,    /*format=*/AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
    /*usage=*/0ULL,  /*stride=*/0UL,
    /*rfu0=*/0UL,    /*rfu1=*/0ULL};
const size_t kUserMetadataSize = 1;

class HalBufferHubVts : public ::testing::VtsHalHidlTargetTestBase {};

// Helper function to verify that given bufferTrais:
// 1. is consistent with kDesc
// 2. have a non-null gralloc handle
// 3. have a null buffer info handle (not implemented)
bool isValidTraits(const BufferTraits& bufferTraits) {
    AHardwareBuffer_Desc desc;
    memcpy(&desc, &bufferTraits.bufferDesc, sizeof(AHardwareBuffer_Desc));
    // Not comparing stride because it's unknown before allocation
    return desc.format == kDesc.format && desc.height == kDesc.height &&
           desc.layers == kDesc.layers && desc.usage == kDesc.usage && desc.width == kDesc.width &&
           bufferTraits.bufferHandle.getNativeHandle() != nullptr &&
           // TODO(b/116681016): check bufferInfo once implemented
           bufferTraits.bufferInfo.getNativeHandle() == nullptr;
}

// Test IBufferHub::allocateBuffer then IBufferClient::close
TEST_F(HalBufferHubVts, AllocateAndFreeBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    HardwareBufferDescription desc;
    memcpy(&desc, &kDesc, sizeof(HardwareBufferDescription));

    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::allocateBuffer_cb callback = [&](const auto& status, const auto& outClient,
                                                 const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());
    EXPECT_TRUE(isValidTraits(bufferTraits));

    ASSERT_EQ(BufferHubStatus::NO_ERROR, client->close());
    EXPECT_EQ(BufferHubStatus::CLIENT_CLOSED, client->close());
}

// Test IBufferClient::duplicate after IBufferClient::close
TEST_F(HalBufferHubVts, DuplicateFreedBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    HardwareBufferDescription desc;
    memcpy(&desc, &kDesc, sizeof(HardwareBufferDescription));

    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::allocateBuffer_cb callback = [&](const auto& status, const auto& outClient,
                                                 const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());
    EXPECT_TRUE(isValidTraits(bufferTraits));

    ASSERT_EQ(BufferHubStatus::NO_ERROR, client->close());

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    ASSERT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::CLIENT_CLOSED);
    EXPECT_EQ(token.getNativeHandle(), nullptr);
}

// Test normal import process using IBufferHub::import function
TEST_F(HalBufferHubVts, DuplicateAndImportBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    HardwareBufferDescription desc;
    memcpy(&desc, &kDesc, sizeof(HardwareBufferDescription));

    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::allocateBuffer_cb callback = [&](const auto& status, const auto& outClient,
                                                 const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());
    EXPECT_TRUE(isValidTraits(bufferTraits));

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    ASSERT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(token.getNativeHandle(), nullptr);
    EXPECT_GT(token->numInts, 1);
    EXPECT_EQ(token->numFds, 0);

    sp<IBufferClient> client2;
    BufferTraits bufferTraits2 = {};
    IBufferHub::importBuffer_cb import_cb = [&](const auto& status, const auto& outClient,
                                                const auto& traits) {
        ret = status;
        client2 = outClient;
        bufferTraits2 = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->importBuffer(token, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    EXPECT_NE(nullptr, client2.get());
    EXPECT_TRUE(isValidTraits(bufferTraits2));
    // TODO(b/116681016): check id, user metadata size and client state mask
}

// Test calling IBufferHub::import with nullptr. Must not crash the service
TEST_F(HalBufferHubVts, ImportNullToken) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    hidl_handle nullToken;
    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::importBuffer_cb import_cb = [&](const auto& status, const auto& outClient,
                                                const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->importBuffer(nullToken, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client.get());
    EXPECT_FALSE(isValidTraits(bufferTraits));
}

// Test calling IBufferHub::import with an nonexistant token.
TEST_F(HalBufferHubVts, ImportInvalidToken) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    native_handle_t* tokenHandle = native_handle_create(/*numFds=*/0, /*numInts=*/2);
    tokenHandle->data[0] = 0;
    // Assign a random number since we cannot know the HMAC value.
    tokenHandle->data[1] = 42;

    hidl_handle invalidToken(tokenHandle);
    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::importBuffer_cb import_cb = [&](const auto& status, const auto& outClient,
                                                const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->importBuffer(invalidToken, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client.get());
    EXPECT_FALSE(isValidTraits(bufferTraits));
}

// Test calling IBufferHub::import after the original IBufferClient is closed
TEST_F(HalBufferHubVts, ImportFreedBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    HardwareBufferDescription desc;
    memcpy(&desc, &kDesc, sizeof(HardwareBufferDescription));

    BufferHubStatus ret;
    sp<IBufferClient> client;
    BufferTraits bufferTraits = {};
    IBufferHub::allocateBuffer_cb callback = [&](const auto& status, const auto& outClient,
                                                 const auto& traits) {
        ret = status;
        client = outClient;
        bufferTraits = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());
    EXPECT_TRUE(isValidTraits(bufferTraits));

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    ASSERT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(token.getNativeHandle(), nullptr);
    EXPECT_GT(token->numInts, 1);
    EXPECT_EQ(token->numFds, 0);

    // Close the client. Now the token should be invalid.
    ASSERT_EQ(BufferHubStatus::NO_ERROR, client->close());

    sp<IBufferClient> client2;
    BufferTraits bufferTraits2 = {};
    IBufferHub::importBuffer_cb import_cb = [&](const auto& status, const auto& outClient,
                                                const auto& traits) {
        ret = status;
        client2 = outClient;
        bufferTraits2 = std::move(traits);
    };
    ASSERT_TRUE(bufferHub->importBuffer(token, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client2.get());
    EXPECT_FALSE(isValidTraits(bufferTraits2));
}

}  // namespace android
