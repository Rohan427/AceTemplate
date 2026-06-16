#include "TaskOrchestrator.hxx"
#include "Config.hxx"
#include <ace/OS_NS_stdio.h>

namespace Manager
{
    TaskOrchestrator* TaskOrchestrator::s_instance = 0;
    ACE_Thread_Mutex TaskOrchestrator::s_creationLock;

    TaskOrchestrator& TaskOrchestrator::instance()
    {
        if (s_instance == 0)
        {
            ACE_Guard<ACE_Thread_Mutex> guard (s_creationLock);

            if (s_instance == 0)
            {
                s_instance = new TaskOrchestrator();
            }
        }

        return *s_instance;
    }

    TaskOrchestrator::TaskOrchestrator() : m_objectPool (0), m_initialized (false)
    {
    }

    TaskOrchestrator::~TaskOrchestrator()
    {
        stopAll();
        // Note: Do not delete tasks here - ownership is with caller or explicit cleanup
    }

    void TaskOrchestrator::initialize (int maxThreads, ObjectPool<std::vector<ObjectData> >& objectPool)
    {
        if (m_initialized) return;

        m_numThreads = maxThreads;

        m_objectPool = &objectPool;

        m_selectedTier = SystemCapabilities::AnalyzeTopologyAndPermissions (m_hardwarePool);
        m_initialized = true;

        if (m_hardwarePool.size() < maxThreads)
        {
            ACE_DEBUG ((LM_INFO, ACE_TEXT ("Found less than %i cores available, setting thread count to %i\n"), 
                        maxThreads,
                        m_hardwarePool.size())
                      );

            m_numThreads = m_hardwarePool.size();
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

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("System Initialization: Selected Operating Framework: %s\n"), tierName.c_str()));
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("Detected Total Hardware Units: %i available processing paths.\n"), m_numThreads));

        // Inside EntityManager::startSimulation() right after parsing topology
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("HETEROGENEOUS POOL MAPPING SUMMARY:\n")));
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Main Thread: Reserved exclusively for Core 0\n")));

        size_t physicalCount = 0;
        size_t siblingCount  = 0;

        for (std::vector<HardwareCore>::const_iterator it = m_hardwarePool.begin(); it != m_hardwarePool.end(); ++it)
        {
            const HardwareCore& core = *it;

            if (core.isHTSibling) siblingCount++;
            else physicalCount++;
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Pool 1: %i Physical Cores allocated (Cores 1-15)\n"), physicalCount - 1));
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("  -> Pool 2: %i Sibling Cores allocated (Cores 17-31)\n"), siblingCount - 1));

        //// 3. SYNCHRONIZE BARRIER LIFECYCLE HANDSHAKE
        //m_barrier = new ACE_Barrier (m_numThreads + 1);
        
        //// Launch threads as dedicated kernel-level entities (THR_BOUND)
        //this->activate (THR_NEW_LWP | THR_JOINABLE | THR_BOUND, m_numThreads);
        
        //m_barrier->wait();
        //ACE_DEBUG ((LM_INFO, ACE_TEXT ("All %1 Primary threads pinned, scaled, and synchronized.").arg (m_numThreads));

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] TaskOrchestrator initialized with maxThreads=%d\n"), m_numThreads));
    }


    void TaskOrchestrator::registerTask (ConfigurableTask<ObjectData>* task)
    {
        if (!task) return;

        m_tasks.push_back (task);
    }

    void TaskOrchestrator::startAll()
    {
        ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Starting %d registered tasks...\n"), (int)m_tasks.size()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Opening task: %s\n"), m_tasks[i]->getConfig().name.c_str()));
                int ret = m_tasks[i]->open (0);
                ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t] open() returned %d for %s\n"), 
                           ret, m_tasks[i]->getConfig().name.c_str()));
            }
        }

        // === CRITICAL: Release all barriers ===
        ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Releasing all task barriers...\n")));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                // Each task has its own barrier; we need to signal them
                // (The +1 in barrier count was waiting for this main thread signal)
                m_tasks[i]->m_barrier->wait();   // Release the workers
            }
        }

        ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] All task barriers released - workers should now run.\n")));
    }

    void TaskOrchestrator::stopAll()
    {
        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] Stopping all tasks...\n")));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i]) m_tasks[i]->stop();
        }

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                m_tasks[i]->wait();   // Wait for threads to exit
                delete m_tasks[i];    // Clean up
                m_tasks[i] = 0;
            }
        }

        m_tasks.clear();
        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] All tasks stopped and cleaned up.\n")));
    }

    ObjectPool<std::vector<ObjectData> >& TaskOrchestrator::getObjectPool()
    {
        return *m_objectPool;
    }
} // namespace Manager
