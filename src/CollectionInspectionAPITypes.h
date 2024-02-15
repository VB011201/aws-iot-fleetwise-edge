// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CANDataTypes.h"
#include "EventTypes.h"
#include "MessageTypes.h"
#include "OBDDataTypes.h"
#include "SignalTypes.h"
#include <mutex>
#include <queue>
#include <vector>

namespace Aws
{
namespace IoTFleetWise
{

struct ExpressionNode;

static constexpr uint32_t MAX_NUMBER_OF_ACTIVE_CONDITION = 256; /**< More active conditions will be ignored */
static constexpr uint32_t ALL_CONDITIONS = 0xFFFFFFFF;
static constexpr uint32_t MAX_EQUATION_DEPTH =
    10; /**< If the AST of the expression is deeper than this value the equation is not accepted */
static constexpr uint32_t MAX_DIFFERENT_SIGNAL_IDS =
    50000; /**< Signal IDs can be distributed over the whole range but never more than 50.000 signals in parallel */

// INPUT to collection and inspection engine:

// This values will be provided by CollectionSchemeManagement:

// The following structs describe an inspection view on all active collection conditions
// As a start these structs are mainly a copy of the data defined in ICollectionScheme
struct PassThroughMetadata
{
    bool compress{ false };
    bool persist{ false };
    uint32_t priority{ 0 };
    std::string decoderID;
    std::string collectionSchemeID;
};

// As a start these structs are mainly a copy of the data defined in ICollectionScheme but as plain old data structures
struct InspectionMatrixSignalCollectionInfo
{
    SignalID signalID;
    uint32_t sampleBufferSize;        /**<  at least this amount of last x samples will be kept in buffer*/
    uint32_t minimumSampleIntervalMs; /**<  zero means all signals are recorded as seen on the bus */
    uint32_t fixedWindowPeriod;       /**< zero means no fixed window sampling would happen */
    bool isConditionOnlySignal;       /**< Should the collected signals be sent to cloud or are the number
                                       * of samples in the buffer only necessary for condition evaluation
                                       */
    SignalType signalType{ SignalType::DOUBLE };
};

struct InspectionMatrixCanFrameCollectionInfo
{
    CANRawFrameID frameID;
    CANChannelNumericID channelID;
    uint32_t sampleBufferSize;        /**<  at least this amount of last x raw can frames will be kept in buffer */
    uint32_t minimumSampleIntervalMs; /**<  0 which all frames are recording as seen on the bus */
};

struct ConditionWithCollectedData
{
    const ExpressionNode *condition; /**< points into InspectionMatrix.expressionNodes;
                                      * Raw pointer is used as needed for efficient AST and ConditionWithCollectedData
                                      * never exists without the relevant InspectionMatrix */
    uint32_t minimumPublishIntervalMs;
    uint32_t afterDuration;
    std::vector<InspectionMatrixSignalCollectionInfo> signals;
    std::vector<InspectionMatrixCanFrameCollectionInfo> canFrames;
    bool includeActiveDtcs;
    bool triggerOnlyOnRisingEdge;
    PassThroughMetadata metadata;
};

struct InspectionMatrix
{
    std::vector<ConditionWithCollectedData> conditions;
    std::vector<ExpressionNode> expressionNodeStorage; /**< A list of Expression nodes from all conditions;
                                                        * to increase performance the expressionNodes from one
                                                        * collectionScheme should be close to each other (memory
                                                        * locality). The traversal is depth first preorder */
};

// These values are provided by the CANDataConsumers
struct CollectedCanRawFrame
{
    CollectedCanRawFrame() = default;
    CollectedCanRawFrame( CANRawFrameID frameIDIn,
                          CANChannelNumericID channelIdIn,
                          Timestamp receiveTimeIn,
                          std::array<uint8_t, MAX_CAN_FRAME_BYTE_SIZE> &dataIn,
                          uint8_t sizeIn )
        : frameID( frameIDIn )
        , channelId( channelIdIn )
        , receiveTime( receiveTimeIn )
        , data( dataIn )
        , size( sizeIn )
    {
    }
    CANRawFrameID frameID{ INVALID_CAN_FRAME_ID };
    CANChannelNumericID channelId{ INVALID_CAN_SOURCE_NUMERIC_ID };
    Timestamp receiveTime{ 0 };
    std::array<uint8_t, MAX_CAN_FRAME_BYTE_SIZE> data{};
    uint8_t size{ 0 };
};

union SignalValue {
    int64_t int64Val;
    float floatVal;
    double doubleVal;
    bool boolVal;
    uint8_t uint8Val;
    int8_t int8Val;
    uint16_t uint16Val;
    int16_t int16Val;
    uint32_t uint32Val;
    int32_t int32Val;
    uint64_t uint64Val;
    SignalValue &
    operator=( const uint8_t value )
    {
        uint8Val = value;
        return *this;
    }

    SignalValue &
    operator=( const uint16_t value )
    {
        uint16Val = value;
        return *this;
    }

    SignalValue &
    operator=( const uint32_t value )
    {
        uint32Val = value;
        return *this;
    }

    SignalValue &
    operator=( const uint64_t value )
    {
        uint64Val = value;
        return *this;
    }

    SignalValue &
    operator=( const int8_t value )
    {
        int8Val = value;
        return *this;
    }

    SignalValue &
    operator=( const int16_t value )
    {
        int16Val = value;
        return *this;
    }
    SignalValue &
    operator=( const int32_t value )
    {
        int32Val = value;
        return *this;
    }
    SignalValue &
    operator=( const int64_t value )
    {
        int64Val = value;
        return *this;
    }

    SignalValue &
    operator=( const float value )
    {
        floatVal = value;
        return *this;
    }

    SignalValue &
    operator=( const double value )
    {
        doubleVal = value;
        return *this;
    }
    SignalValue &
    operator=( const bool value )
    {
        boolVal = value;
        return *this;
    }
};

struct SignalValueWrapper
{
    SignalValue value{ 0 };
    SignalType type{ SignalType::DOUBLE };

    SignalValueWrapper() = default;
    ~SignalValueWrapper() = default;
    SignalValueWrapper( const SignalValueWrapper & ) = default;
    SignalValueWrapper &operator=( const SignalValueWrapper & ) = default;
    SignalValueWrapper( SignalValueWrapper && ) = default;
    SignalValueWrapper &operator=( SignalValueWrapper && ) = default;

    // For Backward Compatibility
    SignalValueWrapper &
    operator=( const double sigValue )
    {
        value = sigValue;
        type = SignalType::DOUBLE;
        return *this;
    }

    template <typename T>
    void
    setVal( const T sigValue, SignalType sigType )
    {
        value = sigValue;
        type = sigType;
    }

    SignalType
    getType() const
    {
        return type;
    }
};

struct CollectedSignal
{
    SignalID signalID{ INVALID_SIGNAL_ID };
    Timestamp receiveTime{ 0 };
    SignalValueWrapper value;

    CollectedSignal() = default;

    // Backward Compatibility
    template <typename T>
    CollectedSignal( SignalID signalIDIn, Timestamp receiveTimeIn, T sigValue )
        : signalID( signalIDIn )
        , receiveTime( receiveTimeIn )
    {
        value.setVal<double>( static_cast<double>( sigValue ), SignalType::DOUBLE );
    }

    template <typename T>
    CollectedSignal( SignalID signalIDIn, Timestamp receiveTimeIn, T sigValue, SignalType sigType )
        : signalID( signalIDIn )
        , receiveTime( receiveTimeIn )
    {
        switch ( sigType )
        {
        case SignalType::UINT8:
            value.setVal<uint8_t>( static_cast<uint8_t>( sigValue ), sigType );
            break;
        case SignalType::INT8:
            value.setVal<int8_t>( static_cast<int8_t>( sigValue ), sigType );
            break;
        case SignalType::UINT16:
            value.setVal<uint16_t>( static_cast<uint16_t>( sigValue ), sigType );
            break;
        case SignalType::INT16:
            value.setVal<int16_t>( static_cast<int16_t>( sigValue ), sigType );
            break;
        case SignalType::UINT32:
            value.setVal<uint32_t>( static_cast<uint32_t>( sigValue ), sigType );
            break;
        case SignalType::INT32:
            value.setVal<int32_t>( static_cast<int32_t>( sigValue ), sigType );
            break;
        case SignalType::UINT64:
            value.setVal<uint64_t>( static_cast<uint64_t>( sigValue ), sigType );
            break;
        case SignalType::INT64:
            value.setVal<int64_t>( static_cast<int64_t>( sigValue ), sigType );
            break;
        case SignalType::FLOAT:
            value.setVal<float>( static_cast<float>( sigValue ), sigType );
            break;
        case SignalType::DOUBLE:
            value.setVal<double>( static_cast<double>( sigValue ), sigType );
            break;
        case SignalType::BOOLEAN:
            value.setVal<bool>( static_cast<bool>( sigValue ), sigType );
            break;
#ifdef FWE_FEATURE_VISION_SYSTEM_DATA
        case SignalType::RAW_DATA_BUFFER_HANDLE:
            value.setVal<uint32_t>( static_cast<uint32_t>( sigValue ), sigType );
            break;
#endif
        }
    }

    SignalType
    getType() const
    {
        return value.getType();
    }

    SignalValueWrapper
    getValue() const
    {
        return value;
    }
};

#ifdef FWE_FEATURE_VISION_SYSTEM_DATA
enum class UploadedS3ObjectDataFormat
{
    Unknown = 0,
    Cdr = 1,
};

struct UploadedS3Object
{
    std::string key;
    UploadedS3ObjectDataFormat dataFormat;
};
#endif

// Vector of collected decoded signals or raw buffer handles
using CollectedSignalsGroup = std::vector<CollectedSignal>;
using CollectedRawCanFramePtr = std::shared_ptr<CollectedCanRawFrame>;
using DTCInfoPtr = std::shared_ptr<DTCInfo>;

// Each collected data frame is processed and evaluated separately by collection inspection engine
struct CollectedDataFrame
{
    CollectedDataFrame() = default;
    CollectedDataFrame( CollectedSignalsGroup collectedSignals )
        : mCollectedSignals( std::move( collectedSignals ) )
    {
    }
    CollectedDataFrame( CollectedSignalsGroup collectedSignals, CollectedRawCanFramePtr collectedCanRawFrame )
        : mCollectedSignals( std::move( collectedSignals ) )
        , mCollectedCanRawFrame( std::move( collectedCanRawFrame ) )
    {
    }
    CollectedDataFrame( DTCInfoPtr dtcInfo )
        : mActiveDTCs( std::move( dtcInfo ) )
    {
    }
    CollectedSignalsGroup mCollectedSignals;
    CollectedRawCanFramePtr mCollectedCanRawFrame;
    DTCInfoPtr mActiveDTCs;
};

// Thread-safe queue with mutex
template <typename T>
struct LockedQueue
{
public:
    LockedQueue( size_t maxSize )
        : mMaxSize( maxSize )
    {
    }
    bool
    push( const T &&element )
    {
        std::lock_guard<std::mutex> lock( mMutex );
        if ( ( mQueue.size() + 1 ) > mMaxSize )
        {
            return false;
        }
        mQueue.push( element );
        return true;
    }
    bool
    pop( T &element )
    {
        std::lock_guard<std::mutex> lock( mMutex );
        if ( mQueue.empty() )
        {
            return false;
        }
        element = mQueue.front();
        mQueue.pop();
        return true;
    }
    template <typename Functor>
    size_t
    consumeAll( const Functor &functor )
    {
        size_t consumed = 0;
        T element;
        while ( pop( element ) )
        {
            functor( element );
            consumed++;
        }
        return consumed;
    }
    // coverity[misra_cpp_2008_rule_14_7_1_violation] Required in unit tests
    bool
    isEmpty()
    {
        std::lock_guard<std::mutex> lock( mMutex );
        return mQueue.empty();
    }

private:
    std::mutex mMutex;
    size_t mMaxSize;
    std::queue<T> mQueue;
};

// Buffer that sends data to Collection Engine
using SignalBuffer = LockedQueue<CollectedDataFrame>;
// Shared Pointer type to the buffer that sends data to Collection Engine
using SignalBufferPtr = std::shared_ptr<SignalBuffer>;

// Output of collection Inspection Engine
struct TriggeredCollectionSchemeData
{
    PassThroughMetadata metadata;
    Timestamp triggerTime;
    std::vector<CollectedSignal> signals;
    std::vector<CollectedCanRawFrame> canFrames;
#ifdef FWE_FEATURE_VISION_SYSTEM_DATA
    std::vector<UploadedS3Object> uploadedS3Objects;
#endif
    DTCInfo mDTCInfo;
    EventID eventID;
};

using TriggeredCollectionSchemeDataPtr = std::shared_ptr<const TriggeredCollectionSchemeData>;
using CollectedDataReadyToPublish = LockedQueue<TriggeredCollectionSchemeDataPtr>;

} // namespace IoTFleetWise
} // namespace Aws
