/**
 * Copyright 2020 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: LicenseRef-.amazon.com.-AmznSL-1.0
 * Licensed under the Amazon Software License (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 * http://aws.amazon.com/asl/
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#pragma once

// Includes
#include "IReceiver.h"
#include "ISender.h"
#include "LoggingModule.h"
#include "PayloadManager.h"
#include <atomic>
#include <aws/crt/Api.h>
#include <memory>
#include <mutex>
#include <string>

namespace Aws
{
namespace IoTFleetWise
{
/**
 * @brief Namespace depending on Aws Iot SDK headers
 */
namespace OffboardConnectivityAwsIot
{

class IConnectivityModule
{
public:
    virtual std::shared_ptr<Aws::Crt::Mqtt::MqttConnection> getConnection() const = 0;
    /**
     * @brief returns the current memory usage in bytes by the SDK.
     * @return number of bytes.
     */
    virtual uint64_t getCurrentMemoryUsage() = 0;

    /**
     * @brief Increases atomically the memory usage
     * @param bytes number of bytes to reserve
     * @return number of bytes after the increase.
     */
    virtual uint64_t reserveMemoryUsage( uint64_t bytes ) = 0;

    /**
     * @brief Decreases atomically the memory usage
     * @param bytes number of bytes to release
     * @return number of bytes after the decrease.
     */
    virtual uint64_t releaseMemoryUsage( uint64_t bytes ) = 0;

    virtual bool isAlive() const = 0;

    virtual ~IConnectivityModule() = 0;
};

using Aws::IoTFleetWise::OffboardConnectivity::CollectionSchemeParams;
using Aws::IoTFleetWise::OffboardConnectivity::ConnectivityError;

/**
 * @brief a channel that can be used as IReceiver or ISender or both
 *
 * If the Channel should be used for receiving data subscribe must be called.
 * setTopic must be called always. There can be multiple AwsIotChannels
 * from one AwsIotConnectivityModule. The channel of the connectivityModule passed in the
 * constructor must be established before anything meaningful can be done with this class
 * @see AwsIotConnectivityModule
 */
class AwsIotChannel : public Aws::IoTFleetWise::OffboardConnectivity::ISender,
                      public Aws::IoTFleetWise::OffboardConnectivity::IReceiver
{
public:
    static const uint64_t MAXIMUM_IOT_SDK_HEAP_MEMORY_BYTES =
        10000000; /**< After the SDK allocated more than the here defined 10MB we will stop pushing data to the SDK to
                     avoid increasing heap consumption */

    AwsIotChannel( IConnectivityModule *connectivityModule,
                   const std::shared_ptr<PayloadManager> &payloadManager,
                   uint64_t maximumIotSDKHeapMemoryBytes = MAXIMUM_IOT_SDK_HEAP_MEMORY_BYTES );
    ~AwsIotChannel();

    /**
     * @brief the topic must be set always before using any functionality of this class
     * @param topicNameRef MQTT topic that will be used for sending or receiving data
     *                      if subscribe was called
     * @param subscribeAsynchronously if true the channel will be subscribed to the topic asynchronously so that the
     * channel can receive data
     *
     */
    void setTopic( const std::string &topicNameRef, bool subscribeAsynchronously = false );

    /**
     * @brief Subscribe to the MQTT topic from setTopic. Necessary if data is received on the topic
     *
     * This function blocks until subscribe succeeded or failed and should be done in the setup form
     * the bootstrap thread. The connection of the connectivityModule passed in the constructor
     * must be established otherwise subscribe will fail. No retries are done to try to subscribe
     * this needs to be done in the boostrap during the setup.
     * @return Success if subscribe finished correctly
     */
    ConnectivityError subscribe();

    /**
     * @brief After unsubscribe no data will be received over the channel
     */
    bool unsubscribe();

    bool isAlive() override;

    size_t getMaxSendSize() const override;

    ConnectivityError send( const std::uint8_t *buf,
                            size_t size,
                            struct CollectionSchemeParams collectionSchemeParams = CollectionSchemeParams() ) override;

    bool
    isTopicValid()
    {
        return !mTopicName.empty();
    }

    void
    invalidateConnection()
    {
        std::lock_guard<std::mutex> connectivityLock( mConnectivityMutex );
        std::lock_guard<std::mutex> connectivityLambdaLock( mConnectivityLambdaMutex );
        mConnectivityModule = nullptr;
    }

    bool
    shouldSubscribeAsynchronously()
    {
        return mSubscribeAsynchronously;
    }

private:
    bool isAliveNotThreadSafe();

    /** See "Message size" : "The payload for every publish request can be no larger
     * than 128 KB. AWS IoT Core rejects publish and connect requests larger than this size."
     * https://docs.aws.amazon.com/general/latest/gr/iot-core.html#limits_iot
     */
    static const size_t AWS_IOT_MAX_MESSAGE_SIZE = 131072; // = 128 KiB
    uint64_t mMaximumIotSDKHeapMemoryBytes; /**< If the iot device sdk heap memory usage from all channels exceeds this
                                               threshold this channel stops publishing data*/
    IConnectivityModule *mConnectivityModule;
    std::mutex mConnectivityMutex;
    std::mutex mConnectivityLambdaMutex;
    std::shared_ptr<PayloadManager> mPayloadManager;
    std::string mTopicName;
    std::atomic<bool> mSubscribed;
    Aws::IoTFleetWise::Platform::LoggingModule mLogger;

    bool mSubscribeAsynchronously;
};
} // namespace OffboardConnectivityAwsIot
} // namespace IoTFleetWise
} // namespace Aws
