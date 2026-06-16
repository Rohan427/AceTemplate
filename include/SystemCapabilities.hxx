#pragma once

#ifndef SYSTEMCAPABILITIES_HXX
#define SYSTEMCAPABILITIES_HXX

#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/resource.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>


enum class SchedulingTier
{
    RealTimeAndAffinity, // SCHED_FIFO + Physical P-Core / Low-Latency CCX Pinning
    AffinityOnly,        // Standard Timesharing + Physical Core Pinning
    StandardFallback     // Default OS management (Single-core fallback or blocked API)
};

enum class WorkloadType
{
    PRIMARY_WORKER,      // Dense, uniform FPU arithmetic
    SIBLING_WORKER,      // Intercept path calculations (Takes mathematical precedence)
    MAIN_TASK            // Low-math, high-string background processing
};

struct HardwareCore
{
    int logicalId;       // OS mapping index (e.g., CPU 0 to CPU 31)
    int physicalCoreId;  // Internal hardware core ID
    int socketId;        // Physical processor socket index (NUMA nodes)
    int coreType;        // Intel: 0 = E-Core, 1 = P-Core. AMD/Xeon: Always 1
    bool isHTSibling;    // Identifies if this is a secondary hyperthreaded logical thread
};

namespace Manager
{
    class SystemCapabilities
    {
        public:
            static SchedulingTier AnalyzeTopologyAndPermissions (std::vector<HardwareCore>& outOptimizedPool)
            {
                outOptimizedPool.clear();
                std::vector<HardwareCore> rawCores;
                long totalCores = ::sysconf (_SC_NPROCESSORS_ONLN);

                if (totalCores <= 1)
                {
                    return SchedulingTier::StandardFallback;
                }

                for (int i = 0; i < totalCores; ++i)
                {
                    HardwareCore core;
                    core.logicalId = i;
                    core.physicalCoreId = i;
                    core.socketId = 0;
                    core.coreType = 1; 
                    core.isHTSibling = false;

                    std::string basePath = "/sys/devices/system/cpu/cpu" + std::to_string (i) + "/topology/";
                    
                    std::ifstream socketFile (basePath + "physical_package_id");

                    if (socketFile.is_open())
                    {
                        socketFile >> core.socketId;
                    }

                    std::ifstream coreIdFile (basePath + "core_id");

                    if (coreIdFile.is_open())
                    {
                        coreIdFile >> core.physicalCoreId;
                    }

                    std::ifstream siblingsFile (basePath + "thread_siblings_list");

                    if (siblingsFile.is_open())
                    {
                        std::string line;
                        std::getline (siblingsFile, line);
                        std::stringstream ss (line);
                        std::string token;

                        if (std::getline (ss, token, ','))
                        {
                            int primarySibling = std::stoi (token);

                            if (i != primarySibling)
                            {
                                core.isHTSibling = true;
                            }
                        }
                    }

                    std::ifstream typeFile ("/sys/devices/system/cpu/cpu" + std::to_string (i) + "/topology/intel_punit/core_type");

                    if (typeFile.is_open())
                    {
                        std::string typeStr;
                        typeFile >> typeStr;

                        if (typeStr == "atom")
                        {
                            core.coreType = 0;
                        }
                    }

                    rawCores.push_back (core);
                }

                // =====================================================================
                // FIXED: MATHEMATICALLY CORRECT STRICT WEAK ORDERING COMPARATOR
                // =====================================================================
                std::sort (rawCores.begin(), rawCores.end(), [](const HardwareCore& a, const HardwareCore& b)
                {
                    // Rule 1: Non-HT physical cores always come before secondary HT siblings
                    if (a.isHTSibling != b.isHTSibling)
                    {
                        return !a.isHTSibling; 
                    }

                    // Rule 2: High-performance cores (P-Cores) come before E-Cores
                    if (a.coreType != b.coreType)
                    {
                        return a.coreType > b.coreType; 
                    }

                    // Rule 3: Prioritize Socket 0 to maintain local NUMA cache structures
                    if (a.socketId != b.socketId)
                    {
                        return a.socketId < b.socketId;
                    }

                    // Rule 4: Fall back to unique logical ID comparison to ensure strict weak ordering
                    return a.logicalId < b.logicalId;
                });

                outOptimizedPool = rawCores;

                // Verify Real-Time Priority Privileges
                struct sched_param param;
                param.sched_priority = 10;
                int rtStatus = ::pthread_setschedparam (::pthread_self(), SCHED_FIFO, &param);
                
                if (rtStatus == 0)
                {
                    param.sched_priority = 0;
                    ::pthread_setschedparam (::pthread_self(), SCHED_OTHER, &param);
                    return SchedulingTier::RealTimeAndAffinity;
                }

                return SchedulingTier::AffinityOnly;
            }
    }; // class SystemCapabilities
} // namespace SimCore

#endif // SYSTEMCAPABILITIES_HXX
