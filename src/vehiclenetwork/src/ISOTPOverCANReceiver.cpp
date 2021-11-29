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

#if defined( IOTFLEETWISE_LINUX )
// Includes
#include "businterfaces/ISOTPOverCANReceiver.h"
#include "ClockHandler.h"
#include <iostream>
#include <linux/can.h>
#include <linux/can/isotp.h>
#include <net/if.h>
#include <poll.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// ISO TP maximum PDU size is 4095, additional bytes are needed
// for the Linux Networking stack internals.
#define MAX_PDU_SIZE 5000

namespace Aws
{
namespace IoTFleetWise
{
namespace VehicleNetwork
{

ISOTPOverCANReceiver::ISOTPOverCANReceiver()
{
}

ISOTPOverCANReceiver::~ISOTPOverCANReceiver()
{
}

bool
ISOTPOverCANReceiver::init( const ISOTPOverCANReceiverOptions &receiverOptions )
{
    mTimer.reset();
    mReceiverOptions = receiverOptions;
    return true;
}

bool
ISOTPOverCANReceiver::connect()
{
    // Socket CAN parameters
    struct sockaddr_can interfaceAddress = {};
    struct can_isotp_options optionalFlags = {};
    struct can_isotp_fc_options frameControlFlags = {};

    // Set the source and the destination
    interfaceAddress.can_addr.tp.tx_id = mReceiverOptions.mSourceCANId;
    interfaceAddress.can_addr.tp.rx_id = mReceiverOptions.mDestinationCANId;
    // Both source and destination are extended CANIDs
    if ( mReceiverOptions.mIsExtendedId )
    {
        optionalFlags.flags |= ( CAN_ISOTP_EXTEND_ADDR | CAN_ISOTP_RX_EXT_ADDR );
    }
    // Set the block size
    frameControlFlags.bs = mReceiverOptions.mBlockSize & 0xFF;
    // Set the Separation time
    frameControlFlags.stmin = mReceiverOptions.mFrameSeparationTimeMs & 0xFF;
    // Number of wait frames. Set to zero in FWE case as we can wait on reception.
    frameControlFlags.wftmax = 0x0;

    // Open a Socket in default mode ( Blocking )
    mSocket = socket( PF_CAN, SOCK_DGRAM /*| SOCK_NONBLOCK*/, CAN_ISOTP );
    if ( mSocket < 0 )
    {
        mLogger.error( "ISOTPOverCANReceiver::connect",
                       " Failed to create the ISOTP Socket to IF:" + mReceiverOptions.mSocketCanIFName );
        return false;
    }

    // Set the Frame Control Flags
    int retOptFlag = setsockopt( mSocket, SOL_CAN_ISOTP, CAN_ISOTP_OPTS, &optionalFlags, sizeof( optionalFlags ) );
    int retFrameCtrFlag =
        setsockopt( mSocket, SOL_CAN_ISOTP, CAN_ISOTP_RECV_FC, &frameControlFlags, sizeof( frameControlFlags ) );

    if ( retOptFlag < 0 || retFrameCtrFlag < 0 )
    {
        mLogger.error( "ISOTPOverCANReceiver::connect", " Failed to set ISO-TP socket option flags" );
        return false;
    }
    // CAN PF and Interface Index
    interfaceAddress.can_family = AF_CAN;
    interfaceAddress.can_ifindex = static_cast<int>( if_nametoindex( mReceiverOptions.mSocketCanIFName.c_str() ) );

    // Bind the socket
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
    if ( bind( mSocket, (struct sockaddr *)&interfaceAddress, sizeof( interfaceAddress ) ) < 0 )
    {
        mLogger.error( "ISOTPOverCANReceiver::connect",
                       " Failed to bind the ISOTP Socket to IF:" + mReceiverOptions.mSocketCanIFName );
        close( mSocket );
        return false;
    }
    mLogger.trace( "ISOTPOverCANReceiver::connect",
                   " ISOTP Socket connected to IF:" + mReceiverOptions.mSocketCanIFName );
    return true;
}

bool
ISOTPOverCANReceiver::disconnect()
{
    if ( close( mSocket ) < 0 )
    {
        mLogger.error( "ISOTPOverCANReceiver::connect",
                       " Failed to disconnect the ISOTP Socket from IF:" + mReceiverOptions.mSocketCanIFName );
        return false;
    }
    mLogger.trace( "ISOTPOverCANReceiver::disconnect",
                   " ISOTP Socket disconnected from IF:" + mReceiverOptions.mSocketCanIFName );
    return true;
}

bool
ISOTPOverCANReceiver::isAlive() const
{
    int error = 0;
    socklen_t len = sizeof( error );
    // Get the error status of the socket
    int retSockOpt = getsockopt( mSocket, SOL_SOCKET, SO_ERROR, &error, &len );
    return ( retSockOpt == 0 && error == 0 );
}

bool
ISOTPOverCANReceiver::receivePDU( std::vector<uint8_t> &pduData )
{
    if ( mReceiverOptions.mP2TimeoutMs > P2_TIMEOUT_INFINITE )
    {
        struct pollfd pfd = { mSocket, POLLIN, 0 };
        int res = poll( &pfd, 1U, static_cast<int>( mReceiverOptions.mP2TimeoutMs ) );
        if ( res <= 0 )
        {
            // Error (<0) or timeout (==0):
            return false;
        }
    }
    // To pass on the vector to read, we need to reserve some bytes.
    pduData.resize( MAX_PDU_SIZE );
    // coverity[check_return : SUPPRESS]
    int bytesRead = static_cast<int>( read( mSocket, pduData.data(), MAX_PDU_SIZE ) );
    mLogger.trace( "ISOTPOverCANReceiver::receivePDU", " Received a PDU of size:" + std::to_string( bytesRead ) );
    // Remove the unnecessary bytes from the PDU container.
    if ( bytesRead > 0 )
    {
        pduData.resize( static_cast<size_t>( bytesRead ) );
    }
    else
    {
        pduData.resize( 0 );
    }

    return bytesRead > 0;
}

} // namespace VehicleNetwork
} // namespace IoTFleetWise
} // namespace Aws
#endif // IOTFLEETWISE_LINUX
