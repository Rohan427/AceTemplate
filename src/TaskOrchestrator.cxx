#include "TaskOrchestrator.hxx"
#include "Config.hxx"

namespace Manager
{
    template<typename DT, typename IT>
    TaskOrchestrator<DT, IT>* TaskOrchestrator<DT, IT>::s_instance = 0;

    template<typename DT, typename IT>
    ACE_Thread_Mutex TaskOrchestrator<DT, IT>::s_creationLock;

    template<typename DT, typename IT>
    TaskOrchestrator<DT, IT>& TaskOrchestrator<DT, IT>::instance()
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

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::registerTask (ConfigurableTask<DT, IT>* task)
    {
        if (!task)
        {
            ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::registerTask task is NULL, aborting\n"))
                      );
            return;
        }

        m_tasks.push_back (task);
    }

    template<typename DT, typename IT>
    ConfigurableTask<DT, IT>* TaskOrchestrator<DT, IT>::findTask (std::string name, int role)
    {
        ConfigurableTask<DT, IT>* result = nullptr;

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if ((m_tasks[i]->getConfig().name == name) && (m_tasks[i]->getConfig().role == role))
            {
                result = m_tasks[i];
                break;
            }
        }

        return result;
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::runProducer (std::string name, void* data, void* args)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer m_producerFinished = %i\n"), 
                    m_producerFinished)
                  );

        if (m_producerFinished >= ProducerState::RUNNING)
        {
            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer open() failed, %s task already running\n"), 
                        name.c_str())
                      );
            return;
        }

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runProducer starting registered Producer task %s\n"), name.c_str()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_producerFinished == ProducerState::RUNNING)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Producer task %s already started and running\n"), name.c_str()));
                return;
            }

            if (m_tasks[i]->getConfig().name == name)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Task %s found\n"), name.c_str()));

                if (m_tasks[i]->getConfig().role == TaskRole::ROLE_PRODUCER)
                {
                    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Opening task: %s\n"), name.c_str()));

                    if (ACE_OS::gettimeofday() >= m_nextRun)
                    {
                        m_currentInputData = *static_cast<std::vector<IT>*> (data);

                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Producer input data size %i\n"), m_currentInputData.capacity()));

                        m_activeProducerThreads = m_tasks[i]->getConfig().numThreads;

                        int ret = m_tasks[i]->open (args);

                        if (ret < 0)
                        {
                            ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer open() failed and returned %d for %s\n"), 
                                        ret, name.c_str())
                                      );
                        }
                        else
                        {
                            ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Producer task %s started\n"), name.c_str()));
                            m_producerStarted = true;
                            m_producerFinished = ProducerState::STARTED;
                        }
                    }
                    else
                    {
                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Producer task %s timeout not hit\n Producer not started\n"), name.c_str()));
                    }
                }
                else
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runProducer Task: %s not registered as a Producer\n"), name.c_str()));
                }
            }
        }

        if (!m_producerStarted)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Producer task %s not found or not started\n"), name.c_str()));
        }
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::runConsumer (std::string name, void* data, void* args)
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer m_consumerFinished = %i\n"), 
                    m_consumerFinished)
                  );

        if (m_consumerFinished)
        {
            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer open() failed, %s task already running\n"), 
                        name.c_str())
                      );
            return;
        }

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer starting registered Producer task %s\n"), name.c_str()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_consumerFinished)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Consumer task %s already started and running\n"), name.c_str()));
                return;
            }

            if (m_tasks[i]->getConfig().name == name)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Task %s found\n"), name.c_str()));

                if (m_tasks[i]->getConfig().role == TaskRole::ROLE_CONSUMER)
                {
                    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Opening task: %s\n"), name.c_str()));

                    {
                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Consumer input data size %i\n"), m_currentInputData.capacity()));

                        m_activeConsumerThreads = m_tasks[i]->getConfig().numThreads;

                        int ret = m_tasks[i]->open (args);

                        if (ret < 0)
                        {
                            ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer open() failed and returned %d for %s\n"), 
                                        ret, name.c_str())
                                      );
                        }
                        else
                        {
                            ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Consumer task %s started\n"), name.c_str()));
                            m_consumerStarted = true;
                            m_consumerFinished = false;
                        }
                    }
                }
                else
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: TaskOrchestrator::runConsumer Task: %s not registered as a Consumer\n"), name.c_str()));
                }
            }
        }

        if (!m_consumerStarted)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Consumer task %s not found or not started\n"), name.c_str()));
        }
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::startAll()
    {
        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t] Starting %d registered tasks...\n"), (int)m_tasks.size()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] Opening task: %s\n"), m_tasks[i]->getConfig().name.c_str()));
                int ret = m_tasks[i]->open (0);
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] open() returned %d for %s\n"), 
                           ret, m_tasks[i]->getConfig().name.c_str()));
            }
        }

        ACE_DEBUG((LM_INFO, ACE_TEXT("[%T][%M][TID:%t] All task barriers released - workers should now run.\n")));
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::stopAll()
    {
        ACE_DEBUG ((LM_INFO, ACE_TEXT("[%T][%M][TID:%t]: Stopping all tasks...\n")));

        // Signal all tasks to stop
        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                m_tasks[i]->stop();   // Sets m_done = true and deactivates queue
            }
        }

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Waiting for all tasks to complete...\n")));

        // Wait for all threads to exit cleanly
        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_tasks[i])
            {
                m_tasks[i]->wait();   // Block until thread exits
                delete m_tasks[i];
                m_tasks[i] = 0;
            }
        }

        m_tasks.clear();

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Clearing buffers...\n")));
        // Release buffers back to pool
        if (m_bufferA) m_objectPool.release (m_bufferA);
        if (m_bufferB) m_objectPool.release (m_bufferB);
        m_bufferA = m_bufferB = nullptr;

        resetCoreAffinityOnShutdown();

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: All tasks stopped and cleaned up.\n")));
    }

    template<typename DT, typename IT>
    ObjectPool<std::vector<DT> >* TaskOrchestrator<DT, IT>::getObjectPool()
    {
        return &m_objectPool;
    }

    template<typename DT, typename IT>
    bool TaskOrchestrator<DT, IT>::isTimeForProducer() const
    {
        return ACE_OS::gettimeofday() >= m_nextRun;
    }

    template<typename DT, typename IT>
    bool TaskOrchestrator<DT, IT>::hasValidData() const {
        return m_activeReadState && m_activeReadState->valid;
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::resetCoreAffinityOnShutdown()
    {
        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Restore affinity...\n")));

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

        ACE_DEBUG ((LM_INFO, ACE_TEXT ("[%T][%M][TID:%t]: Core affinity reset to full system pool\n")));
    }
} // namespace Manager

// Explicit template instantiation for the types used in the application
template class Manager::TaskOrchestrator<Manager::ObjectData, Manager::TestData>;
