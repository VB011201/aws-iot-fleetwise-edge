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
#include "CameraPubSubTypes.h"
#include "ClockHandler.h"
#include "IDDSSubscriber.h"
#include "LoggingModule.h"
#include "Signal.h"
#include "Thread.h"
#include "Timer.h"
#include <iostream>

namespace Aws
{
namespace IoTFleetWise
{
namespace VehicleNetwork
{

using namespace Aws::IoTFleetWise::Platform;

/**
 * @brief IDDSSusbcriber implementation for the Camera Sensor.
 * This object listens to Camera frame data on a DDS Topic and shares the
 * resulting camera artifact data with the Data Inspection DDS Module via the SensorDataListener
 * notification.
 */
class CameraDataSubscriber : public IDDSSubscriber
{
public:
    CameraDataSubscriber();
    ~CameraDataSubscriber();

    bool init( const DDSDataSourceConfig &dataSourceConfig ) override;

    bool connect() override;

    bool disconnect() override;

    bool isAlive() override;

    /**
     * @brief From DataReaderListener.
     * method to be called by the Data Reader  when the subscriber is matched with a new Writer on the Publisher side.
     * @param reader DataReader, not used.
     * @param info The subscription matched status. The attribute we focus on current_count_change
     */
    void on_subscription_matched( DataReader *reader, const SubscriptionMatchedStatus &info ) override;

    /**
     * @brief From DataReaderListener.
     * method to be called by the Data Reader   when a new message is put on the topic. After this
     * call, the message is copied into the subscriber and it's removed from the topic.
     * @param reader DataReader, temporary one that helps retrieving the data.
     */
    void on_data_available( DataReader *reader ) override;

private:
    // Start the bus thread
    bool start();
    // Stop the bus thread
    bool stop();
    // atomic state of the bus. If true, we should stop
    bool shouldStop() const;
    /**
     * @brief Main work function.
     * Typically this function waits on conditional variable until it's set.
     * The conditional variable gets set when on_data_available is called by the DDS Stack.
     * After that, in the current phase of implementation,
     *  we simply raise onSensorArtifactAvailable.
     *  Next step is to persist the data into a physical location.
     * @param data data pointer from the stack.
     */
    static void doWork( void *data );
    // stores a Camera frame buffer on disk, in the same location provided in the fileName
    // TODO: In the current form of the code, the CameraFrames received are appended to the
    // the same file on disk. So the consumer of this file would not know how to recover the
    // frames again/Split the file into frames. We want to do this correctly in future versions
    // of this code, where we would store as a metadata the frame size and/or store the frames
    // in separate artifacts.
    bool persistToStorage( const std::vector<CameraFrame> &buffer, const std::string &fileName );

    Thread mThread;
    std::atomic<bool> mShouldStop{ false };
    std::atomic<bool> mIsAlive{ false };
    std::atomic<bool> mNewResponseReceived{ false };
    mutable std::recursive_mutex mThreadMutex;
    Timer mTimer;
    LoggingModule mLogger;
    std::shared_ptr<const Clock> mClock = ClockHandler::getClock();
    Platform::Signal mWait;
    CameraDataItem mDataItem;
    DomainParticipant *mDDSParticipant;
    Subscriber *mDDSSubscriber;
    Topic *mDDSTopic;
    DataReader *mDDSReader;
    TypeSupport mDDStype;
    std::string mCachePath;
    uint32_t mSourceID;
};
} // namespace VehicleNetwork
} // namespace IoTFleetWise
} // namespace Aws
