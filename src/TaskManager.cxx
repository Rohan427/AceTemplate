#include "TaskManager.hxx"

Manager::ObjectPool<std::vector<ObjectData>> m_ObjectPool;

namespace Manager
{
    ACE_Thread_Mutex TaskManager::m_vectorLock;

    TaskManager* TaskManager::s_instance = nullptr;

    TaskManager* TaskManager::instance()
    {
        return s_instance;
    }

    void TaskManager::initializeManager (int numThreads)
    {
        // Memory pool
        m_ObjectPool = ObjectPool<std::vector<ObjectData>> (::Config::getInstance().MAX_OBJECTS * 2); // safety margin

        s_instance = this;
        m_done = false;
        m_threadIndexer = 0;
        m_numThreads = numThreads;
        
        // 1. DETERMINE SYSTEM HARDWARE AND EXECUTION PERMISSIONS
        // The resulting list can be used for spawning other threads and tasks later
        m_selectedTier = SystemCapabilities::AnalyzeTopologyAndPermissions (m_hardwareCorePool);

        if (m_hardwareCorePool.size() < m_numThreads)
        {
            ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"),
                        "Found less than %i cores available, setting thread count to %i",
                              m_numThreads, m_hardwareCorePool.size())
                      );

            m_numThreads = m_hardwareCorePool.size();
        }

        // 2. VERBOSE LOGGING FOR ENTERPRISE ENVIRONMENT MANAGEMENT
        std::string tierName;

        switch (m_selectedTier)
        {
            case SchedulingTier::RealTimeAndAffinity:
                tierName = "Tier 1: [REAL-TIME SCHED_FIFO + INTUITIVE HARDWARE CORE PINNING]";
                break;

            case SchedulingTier::AffinityOnly:
                tierName = "Tier 2: [STANDARD SCHEDULER TIMESHARING + INTUITIVE HARDWARE CORE PINNING]";
                break;

            case SchedulingTier::StandardFallback:
                tierName = "Tier 3: [STANDARD FALLBACK - SINGLE CORE OR OS CONTEXT ACCESS BLOCKED]";
                break;
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "System Initialization: Selected Operating Framework: %s", tierName));
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "Detected Total Hardware Units: %s available processing paths.", m_numThreads));

        // Inside EntityManager::startSimulation() right after parsing topology
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "HETEROGENEOUS POOL MAPPING SUMMARY:"));
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"), "  -> Main Thread / UI: Reserved exclusively for Core 0"));

        size_t physicalCount = 0;
        size_t siblingCount  = 0;

        for (const auto& core : m_hardwareCorePool)
        {
            if (core.isHTSibling) siblingCount++;
            else physicalCount++;
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"),
                    "  -> Pool 1 (Primary core Workers): %i Physical Cores allocated (Cores 1-15)",
                    (physicalCount - 1))
                  );
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"),
                    "  -> Pool 2 (Sibling core workers): %i Sibling Cores allocated (Cores 17-31)",
                    (siblingCount - 1))
                  );

        // 3. SYNCHRONIZE BARRIER LIFECYCLE HANDSHAKE
        m_barrier = new ACE_Barrier (m_numThreads + 1);
        
        // Launch threads as dedicated kernel-level entities (THR_BOUND)
        this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_numThreads);
        
        m_barrier->wait();
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s\n"),
                    "All %i Primary Core threads pinned, scaled, and synchronized.", m_numThreads)
                  );
    }

    int TaskManager::svc() 
    {
        // Initial startup sync boundary handshake
        m_barrier->wait();

        // Secure a unique, bound-safe ID matching your active worker pool size
        int localThreadId = m_threadIndexer.fetch_add(1) % m_numThreads;
        ACE_thread_t nativeThreadHandle = ACE_OS::thr_self();

        size_t availableCores = m_hardwareCorePool.size();

        // =========================================================================
        // STEP 1: HETEROGENEOUS TOPOLOGY WORKLOAD PARTITIONING
        // =========================================================================
        if (m_selectedTier != SchedulingTier::StandardFallback && availableCores > 0)
        {
            try
            {
                int targetCpuId = 0;
                WorkloadType myWorkload = WorkloadType::PRIMARY_WORKER;

                // Dynamically assign thread types based on your application lifecycle allocation
                // Example: Split pool so higher index blocks handle trajectory predictions
                if (localThreadId >= (m_numThreads / 2))
                {
                    myWorkload = WorkloadType::PATH_PREDICTOR;
                }

                std::vector<int> primaryPhysicalCores;
                std::vector<int> hyperthreadedSiblingCores;

                // Divide the unfiltered pool into independent hardware computing pools
                for (size_t i = 0; i < availableCores; ++i)
                {
                    if (m_hardwareCorePool.at (i).isHTSibling)
                    {
                        hyperthreadedSiblingCores.push_back (m_hardwareCorePool.at (i).logicalId);
                    }
                    else
                    {
                        primaryPhysicalCores.push_back (m_hardwareCorePool.at (i).logicalId);
                    }
                }

                // CORE ROUTING ENGINE
                if (myWorkload == WorkloadType::PRIMARY_WORKER && !primaryPhysicalCores.empty())
                {
                    // SGP4 Threads: Pinned to physical cores, skipping Core 0 to protect graphics
                    size_t poolOffset = (static_cast<size_t> (localThreadId) % (primaryPhysicalCores.size() - 1)) + 1;
                    targetCpuId = primaryPhysicalCores.at (poolOffset);
                } 
                else if (myWorkload == WorkloadType::SIBLING_WORKER && !hyperthreadedSiblingCores.empty())
                {
                    // Path Prediction: Maps to HT sibling units to share FPU execution blocks
                    size_t poolOffset = (static_cast<size_t> (localThreadId) % (hyperthreadedSiblingCores.size() - 1)) + 1;
                    targetCpuId = hyperthreadedSiblingCores.at (poolOffset);
                } 
                else
                {
                    // Fallback to basic linear stride if HT is completely disabled in system BIOS
                    targetCpuId = m_hardwareCorePool.at (static_cast<size_t>(localThreadId) % availableCores).logicalId;
                }

                cpu_set_t cpuset;
                CPU_ZERO (&cpuset);
                CPU_SET (targetCpuId, &cpuset);
                ::pthread_setaffinity_np (nativeThreadHandle, sizeof (cpu_set_t), &cpuset);

                // =========================================================================
                // STEP 2: REAL-TIME ESCALATION & SCHEDULER TUNING
                // =========================================================================
                if (m_selectedTier == SchedulingTier::RealTimeAndAffinity)
                {
                    struct sched_param param;
                    
                    // MISSILE COMMAND PRIORITY HIERARCHY:
                    // Intercept path calculations take precedence over background satellite rendering
                    if (myWorkload == WorkloadType::SIBLING_WORKER)
                    {
                        param.sched_priority = 35; // Higher real-time tier
                    }
                    else
                    {
                        param.sched_priority = 20; // Standard background real-time tier
                    }

                    int rtStatus = ::pthread_setschedparam (nativeThreadHandle, SCHED_FIFO, &param);

                    if (rtStatus != 0)
                    {
                        // System-level block caught: drop back down to safe time-sharing niceness
#if defined (__linux__)
                        ::setpriority (PRIO_PROCESS, 0, 5);
#endif
                    }
                } 
                else
                {
                    // Tier 2 Fallback: Apply relative niceness under SCHED_OTHER
#if defined (__linux__)
                    int targetNice = (myWorkload == WorkloadType::SIBLING_WORKER) ? 2 : 6;
                    ::setpriority (PRIO_PROCESS, 0, targetNice);
#endif
                }
            } 
            catch (const std::out_of_range& e)
            {
                SIM_LOG (LM_ERROR, QString ("Core allocation exception on Thread %1.").arg (localThreadId));
            }
        }

        // =========================================================================
        // 2. DATA PROCESSING PIPELINE
        // =========================================================================
        auto lastTickTime = std::chrono::high_resolution_clock::now();

        while (!m_done)// && !this->msg_queue()->deactivated())
        {
            auto now = std::chrono::high_resolution_clock::now();

            // SGP4 times
            auto duration = now.time_since_epoch();
            qint64 msecs = std::chrono::duration_cast<std::chrono::milliseconds> (duration).count();


            // Missile times
            auto m_duration = std::chrono::duration_cast<std::chrono::microseconds> (now - lastTickTime).count();
            lastTickTime = now;
            // Convert microseconds to fractional elapsed seconds parameter, passed to missile physicis engine
            float frameDeltaSeconds = static_cast<float> (m_duration) / 1000000.0f;


            SIM_LOG (LM_DEBUG, QString ("Aquire lock %1").arg (localThreadId));

            // Use tryacquire() to prevent the "Mutex Storm" from blocking the GUI
            if (m_vectorLock.tryacquire() == 0)
            {
                size_t currentSize = 500; // Magic number For testing only
                size_t satCount     = 250; // Magic number For testing only
                size_t missileCount = 250; / Magic number For testing only

                // Establish strict architectural division bounds based on your thread type assignment
                int halfPool = m_numThreads / 2; // Split threshold (e.g., index 16)
                
                // =====================================================================
                // WORKLOAD DIVISION 1: (PHYSICAL CORES)
                // =====================================================================
                // Only threads 1 to 15 handle raw computations
                if (localThreadId < halfPool && satCount > 0 && this->m_persistentBufferPtr != nullptr)
                {
                    SIM_LOG (LM_DEBUG, QString ("Loop updatePhysics %1").arg (localThreadId));

                    for (size_t i = static_cast<size_t>(localThreadId); i < satCount; i += static_cast<size_t>(halfPool))
                    {
                        // Do some long-running math
                    }
                }

                // =====================================================================
                // WORKLOAD DIVISION 2: (HYPERTHREADED SIBLINGS)
                // =====================================================================
                // Only threads 16 to 31 handle guided weapon paths and missile trail geometry
                if (localThreadId >= halfPool && missileCount > 0)
                {
                    // Calculate a localized, zero-based indexing offset for the missile loops (0 to 15)
                    size_t missileThreadOffset = static_cast<size_t> (localThreadId - halfPool);

                    // Stride explicitly by the width of the path predictor pool (top half of pool)
                    for (size_t m = missileThreadOffset; m < missileCount; m += static_cast<size_t>(halfPool))
                    {
                        // Do some faster calculations
                    }
                }

                SIM_LOG (LM_DEBUG, QString ("Release lock %1").arg (localThreadId));

                m_vectorLock.release();
            }
            else
            {
                // If the lock is busy, yield immediately to let the GUI or Parser in
                ACE_Thread::yield();
            }

            if (::Config::getInstance().THREAD_SLEEP_TIME > 0)
            {
                ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().THREAD_SLEEP_TIME));
            }
            else
            {
                ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().DEFAULT_THREAD_SLEEP));
            }
        }

        return 0;
    }

    void TaskManager::stopApplication()
    {
        SIM_LOG (LM_INFO, "Initiating Multi-Threaded Simulation Shutdown Sequence...");
        
        // 1. RAISE TRANSITION COOPERATIVE LIFECYCLE FLAGS
        m_done = true;
        
        // Deactivate the underlying message queue to wake any threads blocked on it
        this->msg_queue()->deactivate();

        // 2. EXPLICITLY RETURN AFFINITY RESOURCING BACK TO THE GENERAL OPERATING POOL
        // On systems running true real-time priorities (SCHED_FIFO), resetting the 
        // scheduling parameters on shutdown ensures the cores are cleanly returned 
        // to standard OS management, preventing lockups.
        if (m_selectedTier == SchedulingTier::RealTimeAndAffinity)
        {
            long totalCores = ::sysconf (_SC_NPROCESSORS_ONLN);
            cpu_set_t fullSystemMask;
            CPU_ZERO (&fullSystemMask);

            for (int i = 0; i < totalCores; ++i)
            {
                CPU_SET (i, &fullSystemMask);
            }

            struct sched_param standardParam;
            standardParam.sched_priority = 0;

            // Reset the main thread's parameters safely
            ::pthread_setschedparam (ACE_OS::thr_self(), SCHED_OTHER, &standardParam);
            ::pthread_setaffinity_np (ACE_OS::thr_self(), sizeof (cpu_set_t), &fullSystemMask);
        }

        // 3. REAP THE COMPUTE VECTOR THREAD POOL
        // This blocks the shutdown sequence until all worker threads break 
        // their loops and exit cleanly, avoiding memory leaks or dangling pointers.
        this->wait(); 

        // 4. CLEANUP DYNAMIC COMPONENT MEMORY ALLOCATIONS
        if (m_barrier)
        {
            delete m_barrier;
            m_barrier = nullptr;
        }

        SIM_LOG (LM_INFO, "Simulation Shutdown Finalized Successfully. All threads reaped.");
    }
} // namespace SimCore
