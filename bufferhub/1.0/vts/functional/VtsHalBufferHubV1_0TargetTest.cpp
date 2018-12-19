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

namespace android {
namespace {

const int kWidth = 640;
const int kHeight = 480;
const int kLayerCount = 1;
const int kFormat = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
const int kUsage = 0;
const size_t kUserMetadataSize = 0;

using frameworks::bufferhub::V1_0::BufferHubStatus;
using frameworks::bufferhub::V1_0::IBufferClient;
using frameworks::bufferhub::V1_0::IBufferHub;
using hardware::hidl_handle;
using hardware::graphics::common::V1_2::HardwareBufferDescription;

class HalBufferHubVts : public ::testing::VtsHalHidlTargetTestBase {};

TEST_F(HalBufferHubVts, AllocateAndFreeBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    // Stride is an output, rfu0 and rfu1 are reserved data slot for future use.
    AHardwareBuffer_Desc aDesc = {kWidth, kHeight,        kLayerCount,  kFormat,
                                  kUsage, /*stride=*/0UL, /*rfu0=*/0UL, /*rfu1=*/0ULL};
    HardwareBufferDescription desc;
    memcpy(&desc, &aDesc, sizeof(HardwareBufferDescription));

    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::allocateBuffer_cb callback = [&](const auto& outClient, const auto& outStatus) {
        client = outClient;
        ret = outStatus;
    };
    EXPECT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());

    EXPECT_EQ(BufferHubStatus::NO_ERROR, client->close());
    EXPECT_EQ(BufferHubStatus::CLIENT_CLOSED, client->close());
}

TEST_F(HalBufferHubVts, DuplicateFreedBuffer) {
    sp<IBufferHub> bufferHub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferHub.get());

    // Stride is an output, rfu0 and rfu1 are reserved data slot for future use.
    AHardwareBuffer_Desc aDesc = {kWidth, kHeight,        kLayerCount,  kFormat,
                                  kUsage, /*stride=*/0UL, /*rfu0=*/0UL, /*rfu1=*/0ULL};
    HardwareBufferDescription desc;
    memcpy(&desc, &aDesc, sizeof(HardwareBufferDescription));

    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::allocateBuffer_cb callback = [&](const auto& outClient, const auto& outStatus) {
        client = outClient;
        ret = outStatus;
    };
    EXPECT_TRUE(bufferHub->allocateBuffer(desc, kUserMetadataSize, callback).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());

    EXPECT_EQ(BufferHubStatus::NO_ERROR, client->close());

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    EXPECT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::CLIENT_CLOSED);
    EXPECT_EQ(token.getNativeHandle(), nullptr);
}

TEST_F(HalBufferHubVts, DuplicateAndImportBuffer) {
    sp<IBufferHub> bufferhub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferhub.get());

    // Stride is an output, rfu0 and rfu1 are reserved data slot for future use.
    AHardwareBuffer_Desc aDesc = {kWidth, kHeight,        kLayerCount,  kFormat,
                                  kUsage, /*stride=*/0UL, /*rfu0=*/0UL, /*rfu1=*/0ULL};
    HardwareBufferDescription desc;
    memcpy(&desc, &aDesc, sizeof(HardwareBufferDescription));

    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::allocateBuffer_cb alloc_cb = [&](const auto& outClient, const auto& status) {
        client = outClient;
        ret = status;
    };
    ASSERT_TRUE(bufferhub->allocateBuffer(desc, kUserMetadataSize, alloc_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    ASSERT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(token.getNativeHandle(), nullptr);
    EXPECT_EQ(token->numInts, 1);
    EXPECT_EQ(token->numFds, 0);

    sp<IBufferClient> client2;
    IBufferHub::importBuffer_cb import_cb = [&](const auto& outClient, const auto& status) {
        ret = status;
        client2 = outClient;
    };
    ASSERT_TRUE(bufferhub->importBuffer(token, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    EXPECT_NE(nullptr, client2.get());
    // TODO(b/116681016): once BufferNode.id() is exposed via BufferHubBuffer, check origin.id =
    // improted.id here.
}

// nullptr must not crash the service
TEST_F(HalBufferHubVts, ImportNullToken) {
    sp<IBufferHub> bufferhub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferhub.get());

    hidl_handle nullToken;
    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::importBuffer_cb import_cb = [&](const auto& outClient, const auto& status) {
        client = outClient;
        ret = status;
    };
    ASSERT_TRUE(bufferhub->importBuffer(nullToken, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client.get());
}

// This test has a very little chance to fail (number of existing tokens / 2 ^ 32)
TEST_F(HalBufferHubVts, ImportInvalidToken) {
    sp<IBufferHub> bufferhub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferhub.get());

    native_handle_t* tokenHandle = native_handle_create(/*numFds=*/0, /*numInts=*/1);
    tokenHandle->data[0] = 0;

    hidl_handle invalidToken(tokenHandle);
    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::importBuffer_cb import_cb = [&](const auto& outClient, const auto& status) {
        client = outClient;
        ret = status;
    };
    ASSERT_TRUE(bufferhub->importBuffer(invalidToken, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client.get());

    native_handle_delete(tokenHandle);
}

TEST_F(HalBufferHubVts, ImportFreedBuffer) {
    sp<IBufferHub> bufferhub = IBufferHub::getService();
    ASSERT_NE(nullptr, bufferhub.get());

    // Stride is an output, rfu0 and rfu1 are reserved data slot for future use.
    AHardwareBuffer_Desc aDesc = {kWidth, kHeight,        kLayerCount,  kFormat,
                                  kUsage, /*stride=*/0UL, /*rfu0=*/0UL, /*rfu1=*/0ULL};
    HardwareBufferDescription desc;
    memcpy(&desc, &aDesc, sizeof(HardwareBufferDescription));

    sp<IBufferClient> client;
    BufferHubStatus ret;
    IBufferHub::allocateBuffer_cb alloc_cb = [&](const auto& outClient, const auto& status) {
        client = outClient;
        ret = status;
    };
    ASSERT_TRUE(bufferhub->allocateBuffer(desc, kUserMetadataSize, alloc_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(nullptr, client.get());

    hidl_handle token;
    IBufferClient::duplicate_cb dup_cb = [&](const auto& outToken, const auto& status) {
        token = outToken;
        ret = status;
    };
    ASSERT_TRUE(client->duplicate(dup_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::NO_ERROR);
    ASSERT_NE(token.getNativeHandle(), nullptr);
    EXPECT_EQ(token->numInts, 1);
    EXPECT_EQ(token->numFds, 0);

    // Close the client. Now the token should be invalid.
    client->close();

    sp<IBufferClient> client2;
    IBufferHub::importBuffer_cb import_cb = [&](const auto& outClient, const auto& status) {
        client2 = outClient;
        ret = status;
    };
    EXPECT_TRUE(bufferhub->importBuffer(token, import_cb).isOk());
    EXPECT_EQ(ret, BufferHubStatus::INVALID_TOKEN);
    EXPECT_EQ(nullptr, client2.get());
}

}  // namespace
}  // namespace android
