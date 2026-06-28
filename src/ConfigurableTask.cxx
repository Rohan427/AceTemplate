#include "ConfigurableTask.hxx"
#include "DataProcessor.hxx"
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
    template<typename DataT, typename InputT>
    int ConfigurableTask<DataT, InputT>::open (void* args)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: ConfigurableTask<DataT>::open on %s\n"), m_config.name.c_str()));

        m_tier = m_config.tier;
        m_barrier = new ACE_Barrier (m_config.numThreads);
        return this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_config.numThreads);
    }

    template<typename DataT, typename InputT>
    int ConfigurableTask<DataT, InputT>::close (u_long flags)
    {
        stop();
        return 0;
    }


    template<typename DataT, typename InputT>
    int ConfigurableTask<DataT, InputT>::svc()
    {
//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] === svc() ENTERED for %s (isProducer=%d) ===\n"), m_config.name.c_str(), m_config.role));

        m_barrier->wait();

//        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Barrier passed - starting work loop for %s\n"), m_config.name.c_str()));

        int localThreadId = m_threadIndexer++;
        localThreadId = localThreadId % m_config.numThreads;

        // === Core Pinning + Priority (adapted from TaskManager) ===
        ACE_thread_t nativeThreadHandle = ACE_OS::thr_self();

        size_t availableCores = m_hardwareCorePool.size();

        // =========================================================================
        // STEP 1: HETEROGENEOUS TOPOLOGY WORKLOAD PARTITIONING
        // =========================================================================
        if (m_tier != SchedulingTier::StandardFallback && availableCores > 0)
        {
            try
            {
                int targetCpuId = 0;
                WorkloadType myWorkload = WorkloadType::PRIMARY_WORKER;

                // Dynamically assign thread types based on the application lifecycle allocation
                // Example: Split pool so higher index blocks handle trajectory predictions
                if (localThreadId >= (m_config.numThreads / 2))
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
                if (m_tier == SchedulingTier::RealTimeAndAffinity)
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
        if (m_config.role == ROLE_PRODUCER)
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
//             ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: %s in svc() while loop.\n"), m_config.name.c_str()));

            std::vector<InputT>* currentPtr = m_orchestrator->getSharedDataPtr().value();

            if (m_config.role == ROLE_PRODUCER)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Producer %i.\n"), localThreadId));

                if (currentPtr)
                {
                    if (ACE_OS::gettimeofday() >= m_orchestrator->m_nextRun)
                    {
                        processWorkload (localThreadId, currentPtr);
                        bool useful = !m_orchestrator->m_activeWriteBuffer->empty();
//                        m_orchestrator->markAndSwap (useful);
                        m_orchestrator->m_nextRun = ACE_OS::gettimeofday() + ACE_Time_Value (0, m_config.producerIntervalMs * 1000);
                    }
                }
                else
                {
                    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Producer %i m_sharedDataPtr is NULL\n"), localThreadId));
                }
            }
            else if (m_config.role == ROLE_CONSUMER)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Consumer %i.\n"), localThreadId));

                ACE_Guard<ACE_Thread_Mutex> guard (m_orchestrator->m_bufferLock);

                if (m_orchestrator->m_activeReadState->valid)
                {
                    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Consumer %i m_activeReadState is valid.\n"), localThreadId));

                    if (currentPtr)
                    {
                        processWorkload (localThreadId, currentPtr);

                        notifyConsumerDone();
                    }
                    else
                    {
                        ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: Consumer %i currentPtr NOT valid.\n"), localThreadId));
                    }
                }
                else
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: Consumer %i m_activeReadState NOT valid.\n"), localThreadId));
                }
            }
            else
            {
                ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: No role detected for thread %i.\n"), localThreadId));
            }

            ACE_OS::sleep (ACE_Time_Value (0, ::Config::getInstance().THREAD_SLEEP_TIME)); // 5ms yield
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] %s Thread %d exiting cleanly\n"),
                   m_config.role ? ROLE_PRODUCER : ROLE_CONSUMER, localThreadId));

        return 0;
    }


    template<typename DataT, typename InputT>
    void ConfigurableTask<DataT, InputT>::stop()
    {
        m_done = true;
        this->msg_queue()->deactivate();

        // Optional immediate affinity reset for this task's threads
        if (m_tier == SchedulingTier::RealTimeAndAffinity)
        {
            resetCoreAffinityOnShutdown();
        }
    }


    template<typename DataT, typename InputT>
    void ConfigurableTask<DataT, InputT>::resetCoreAffinityOnShutdown()
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

    template<typename DataT, typename InputT>
    std::vector<DataT>* ConfigurableTask<DataT, InputT>::getReadBuffer()
    {
        ACE_Guard<ACE_Thread_Mutex> guard (m_orchestrator->m_bufferLock);
        return (m_orchestrator->m_activeWriteBuffer == m_orchestrator->m_bufferA) ? m_orchestrator->m_bufferB : m_orchestrator->m_bufferA;
    }

    template<typename DataT, typename InputT>
    void ConfigurableTask<DataT, InputT>::triggerProducer()
    {
        // Force immediate execution
        m_orchestrator->m_nextRun = ACE_OS::gettimeofday() - ACE_Time_Value (0, 1000000);  // 1 second in the past
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("Producer triggered for immediate run\n")));
    }

    template<typename DataT, typename InputT>
    void ConfigurableTask<DataT, InputT>::triggerConsumer()
    {
        m_orchestrator->m_notifyCond.broadcast();
    }

    template<typename DataT, typename InputT>
    void ConfigurableTask<DataT, InputT>::notifyConsumerDone()
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: ConfigurableTask<DataT>::notifyConsumerDone\n")));

        if (m_orchestrator)
        {
            ACE_Guard<ACE_Thread_Mutex> guard (m_orchestrator->getAcceptLock());  // May need friend or public setter
            m_orchestrator->setConsumerFinished (true);
            m_orchestrator->getConsumerCond().broadcast();
        }
    }
} // namespace Manager

// ====================== FULL TEMPLATE INSTANTIATION ======================
// This forces generation of all member functions (constructor, svc, getReadBuffer, etc.)
template class Manager::ConfigurableTask<Manager::ObjectData, Manager::TestData>;
