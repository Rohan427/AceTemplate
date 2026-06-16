#include "ConfigurableTask.hxx"
#include "TaskOrchestrator.hxx"
#include "Compatibility.hxx"
#include "Config.hxx"
#include <ace/OS_NS_unistd.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <ace/OS_NS_stdio.h>

namespace Manager
{
    template<typename ObjectData>  
    ACE_Thread_Mutex ConfigurableTask<ObjectData>::m_sharedVectorLock;

    template<typename DataT>
    ConfigurableTask<DataT>::ConfigurableTask (const TaskConfig& cfg, ObjectPool<std::vector<DataT> >& pool)
        : m_config (cfg),
          m_pool (pool),
          m_barrier (0),
          m_done (false),
          m_threadIndexer (0),
          m_hardwareCorePool (TaskOrchestrator::instance().getHardwarePool()),
          m_selectedTier (TaskOrchestrator::instance().getSchedulingTier()),
          m_bufferA (0),
          m_bufferB (0),
          m_activeWriteBuffer (0)
    {
        m_bufferA = m_pool.acquire();
        m_bufferB = m_pool.acquire();

        if (m_bufferA)
        {
            m_activeWriteBuffer = m_bufferA;
        }
    }

    template<typename DataT>
    ConfigurableTask<DataT>::~ConfigurableTask()
    {
        if (m_barrier)
        {
            delete m_barrier;
            m_barrier = 0;
        }
    }

    template<typename DataT>
    int ConfigurableTask<DataT>::open (void* /*args*/)
    {
//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] open() called for %s - activating %d threads\n"),
//                   m_config.name.c_str(), m_config.numChildThreads));

        m_barrier = new ACE_Barrier (m_config.numChildThreads + 1);
        int ret = this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_config.numChildThreads);

//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] activate() returned %d for %s\n"), ret, m_config.name.c_str()));

        return ret;
    }


    template<typename DataT>
    int ConfigurableTask<DataT>::close (u_long /*flags*/)
    {
        // Cleanup if needed
        return 0;
    }


    template<typename DataT>
    int ConfigurableTask<DataT>::svc()
    {
//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] === svc() ENTERED for %s (isProducer=%d) ===\n"),
//                   m_config.name.c_str(), (int)m_config.isProducer));

        m_barrier->wait();

//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Barrier passed - starting work loop for %s\n"),
//                   m_config.name.c_str()));

        int localThreadId = m_threadIndexer++;
        localThreadId = localThreadId % m_config.numChildThreads;

        // === Core Pinning + Priority (adapted from your TaskManager) ===
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

                // Dynamically assign thread types based on the application lifecycle allocation
                // Example: Split pool so higher index blocks handle trajectory predictions
                if (localThreadId >= (m_config.numChildThreads / 2))
                {
                    myWorkload = WorkloadType::SIBLING_WORKER;
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
                    // Primary  calculation threads: Pinned to physical cores, skipping Core 0 to protect main thredad
                    size_t poolOffset = (static_cast<size_t> (localThreadId) % (primaryPhysicalCores.size() - 1)) + 1;
                    targetCpuId = primaryPhysicalCores.at (poolOffset);
                } 
                else if (myWorkload == WorkloadType::SIBLING_WORKER && !hyperthreadedSiblingCores.empty())
                {
                    // Faster, short-term threads: Maps to HT sibling units to share FPU execution blocks
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
                    
                    // COMMAND PRIORITY HIERARCHY:
                    // Higher priority threads take precedence over background processing
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
                ACE_DEBUG ((LM_ERROR, ACE_TEXT ("Core allocation exception on Thread %d.\n"), localThreadId));
            }
        }

        // Low-priority producer adjustment
        if (m_config.isProducer)
        {
        // Force lower niceness / priority
#if defined(__linux__)
            ::setpriority (PRIO_PROCESS, 0, 10);  // Positive = lower priority
#endif
        }

        // =========================================================================
        // 2. DATA PROCESSING PIPELINE
        // =========================================================================
        auto lastTickTime = std::chrono::high_resolution_clock::now();

        while (!m_done)
        {
            // Producer periodic sleep with early exit check
            if (m_config.intervalMs > 0 && m_config.isProducer)
            {
                // Break sleep into smaller chunks to respond faster to shutdown
                unsigned long remaining = m_config.intervalMs;

                while (remaining > 0 && !m_done)
                {
                    unsigned long slice = (remaining > 1000) ? 1000 : remaining;  // 1 second slices
                    ACE_OS::sleep (ACE_Time_Value (0, slice * 1000));
                    remaining -= slice;
                }

                if (m_done) break;
            }

            if (m_sharedVectorLock.tryacquire() == 0)
            {
                processWorkload (localThreadId, *m_activeWriteBuffer);

                if (m_config.isProducer)
                {
                    swapBuffers();
                }

                m_sharedVectorLock.release();

                ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s Worker %d completed work\n"),
                           m_config.isProducer ? "Producer" : "Consumer", localThreadId));
            }
            else
            {
                ACE_Thread::yield();
            }

            // Consumer light sleep
            if (!m_config.isProducer)
            {
                int sleepUs = ::Config::getInstance().THREAD_SLEEP_TIME > 0 
                            ? ::Config::getInstance().THREAD_SLEEP_TIME : 10000;
                ACE_OS::sleep(ACE_Time_Value(0, sleepUs));
            }
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] %s Thread %d exiting cleanly\n"),
                   m_config.isProducer ? "Producer" : "Consumer", localThreadId));

        return 0;
    }


    template<typename DataT>
    void ConfigurableTask<DataT>::stop()
    {
        m_done = true;
        this->msg_queue()->deactivate();

        // Optional immediate affinity reset for this task's threads
        if (m_selectedTier == SchedulingTier::RealTimeAndAffinity)
        {
            resetCoreAffinityOnShutdown();
        }
    }


    template<typename DataT>
    void ConfigurableTask<DataT>::resetCoreAffinityOnShutdown()
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

        ACE_thread_t self = ACE_OS::thr_self();
        ::pthread_setschedparam (self, SCHED_OTHER, &standardParam);
        ::pthread_setaffinity_np (self, sizeof (cpu_set_t), &fullSystemMask);

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] Core affinity reset to full system pool\n")));
    }

    template<typename DataT>
    void ConfigurableTask<DataT>::swapBuffers()
    {
        ACE_Guard<ACE_Thread_Mutex> guard (m_bufferLock);
        m_activeWriteBuffer = (m_activeWriteBuffer == m_bufferA) ? m_bufferB : m_bufferA;
    }

    template<typename DataT>
    std::vector<DataT>* ConfigurableTask<DataT>::getReadBuffer()
    {
        ACE_Guard<ACE_Thread_Mutex> guard (m_bufferLock);
        return (m_activeWriteBuffer == m_bufferA) ? m_bufferB : m_bufferA;
    }

    template<typename DataT>
    void ConfigurableTask<DataT>::triggerUpdate()
    {
        if (!m_config.isProducer) return;
        // Wake producer if sleeping
        this->msg_queue()->pulse();
    }

    // Static shared lock definition (must match the template instantiation)
//    template<typename ObjectData> 
//    ACE_Thread_Mutex ConfigurableTask<ObjectData>::m_sharedVectorLock;

// Close the explicit definition block
} // namespace Manager

// ====================== FULL TEMPLATE INSTANTIATION ======================
// This forces generation of all member functions (constructor, svc, getReadBuffer, etc.)
template class Manager::ConfigurableTask<Manager::ObjectData>;
