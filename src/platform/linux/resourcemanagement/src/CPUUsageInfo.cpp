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
#include "CPUUsageInfo.h"
#include <climits>
#include <dirent.h>
#include <fstream>
#include <regex>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

namespace Aws
{
namespace IoTFleetWise
{
namespace Platform
{
bool
CPUUsageInfo::reportCPUUsageInfo()
{

    struct rusage currentProcessUsage = {};
    if ( getrusage( RUSAGE_SELF, &currentProcessUsage ) != 0 )
    {
        return false;
    }
    // Compute user and system time
    mUserSpaceTime = static_cast<double>( currentProcessUsage.ru_utime.tv_sec ) +
                     1e-6 * static_cast<double>( currentProcessUsage.ru_utime.tv_usec );
    mKernelSpaceTime = static_cast<double>( currentProcessUsage.ru_stime.tv_sec ) +
                       1e-6 * static_cast<double>( currentProcessUsage.ru_stime.tv_usec );
    // Compute Idle time
    std::string stat;
    std::ifstream uptimeFile;
    uptimeFile.open( "/proc/uptime" );
    if ( uptimeFile.is_open() )
    {
        std::getline( uptimeFile, stat );
        unsigned long uptimeSeconds = 0U;
        unsigned long uptimeFraction = 0U;
        unsigned long idleTimeSeconds = 0U;
        unsigned long idleTimeFraction = 0U;
        if ( sscanf( stat.c_str(),
                     "%lu.%lu %lu.%lu",
                     &uptimeSeconds,
                     &uptimeFraction,
                     &idleTimeSeconds,
                     &idleTimeFraction ) == 4 )
        {
            mIdleTime = double( uptimeSeconds ) + double( idleTimeFraction ) / 100.0;
        }
    }

    return true;
}

bool
CPUUsageInfo::reportPerThreadUsageData( CPUUsageInfo::ThreadCPUUsageInfos &threadCPUUsageInfos )
{
    // Iterate through all the tasks, and extracts each task info and push it into the
    // a structure per task. Each task is a thread in this context.
    DIR *taskDir = nullptr;
    struct dirent *dp = nullptr;
    taskDir = opendir( "/proc/self/task/." );
    if ( taskDir == nullptr )
    {
        return false;
    }
    // Info for each thread is cleared before any new report
    threadCPUUsageInfos.clear();
    // Clock Frequency is needed to compute the user and system execution cpu cycles
    double clockTickFrequency = 1.0 / static_cast<double>( sysconf( _SC_CLK_TCK ) );
    while ( ( dp = readdir( taskDir ) ) != nullptr )
    {
        std::string taskFileName = dp->d_name;
        if ( taskFileName.length() > 0 && taskFileName[0] != '.' )
        {
            FILE *fp = nullptr;
            std::string pathToThreadCPUUsageInfo = "/proc/self/task/" + taskFileName + "/stat";
            fp = fopen( pathToThreadCPUUsageInfo.c_str(), "r" );

            if ( fp != nullptr )
            {
                CPUUsageInfo::ThreadId tid =
                    static_cast<CPUUsageInfo::ThreadId>( strtol( taskFileName.c_str(), NULL, 10 ) );
                char statContent[MAX_PROC_STAT_FILE_SIZE_READ];
                if ( fgets( statContent, MAX_PROC_STAT_FILE_SIZE_READ - 1, fp ) != nullptr )
                {
                    statContent[MAX_PROC_STAT_FILE_SIZE_READ - 1] = '\0'; // fgets should already null terminate string
                    char *c = &statContent[0];
                    /* proc man page:
                     * (1) pid  %d
                     * (2) comm  %s
                     * (3) state  %c
                     * (4) ppid  %d
                     * (5) pgrp  %d
                     * (6) session  %d
                     * (7) tty_nr  %d
                     * (8) tpgid  %d
                     * (9) flags  %u
                     * (10) minflt  %lu
                     * (11) cminflt  %lu
                     * (12) majflt  %lu
                     * (13) cmajflt  %lu
                     * (14) utime  %lu
                     * (15) stime  %lu
                     */
                    uint32_t currentField = 1; // start at 1 like proc man page
                    // as short string optimization often preallocates 15 characters most likely string will not use
                    // heap allocation as strings will by < 15 chars
                    std::string comm = "";
                    bool commStringFinished = false;
                    std::string utimeString = "";
                    std::string stimeString = "";

                    while ( *c != '\0' )
                    {
                        if ( currentField == 2 )
                        {
                            if ( commStringFinished && *c == ' ' )
                            {
                                currentField++;
                            }
                            else if ( *c == ')' )
                            {
                                commStringFinished = true;
                            }
                            else if ( *c != '(' )
                            {
                                comm += *c;
                            }
                        }
                        else if ( *c == ' ' )
                        {
                            currentField++;
                        }
                        else if ( currentField == 14 )
                        {
                            utimeString += *c;
                        }
                        else if ( currentField == 15 )
                        {
                            stimeString += *c;
                        }
                        c++;
                    }

                    int64_t uTime = strtoll( utimeString.c_str(), NULL, 10 );
                    int64_t sTime = strtoll( stimeString.c_str(), NULL, 10 );
                    if ( uTime != LONG_MAX && uTime >= 0 && sTime != LONG_MAX && sTime >= 0 )
                    {
                        threadCPUUsageInfos.emplace_back(
                            CPUUsageInfo::ThreadCPUUsageInfo{ tid,
                                                              comm,
                                                              static_cast<double>( uTime ) * clockTickFrequency,
                                                              static_cast<double>( sTime ) * clockTickFrequency } );
                    }
                }
                fclose( fp );
            }
        }
    }
    closedir( taskDir );
    return true;
}

} // namespace Platform
} // namespace IoTFleetWise
} // namespace Aws
#endif // IOTFLEETWISE_LINUX
