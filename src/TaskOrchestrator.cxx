#include "TaskOrchestrator.hxx"
#include "Config.hxx"
#include <ace/OS_NS_stdio.h>

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
    void TaskOrchestrator<DT, IT>::runProducer (std::string name, void* args)
    {
        if (m_producerStarted)
        {
            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("[%T][%M][TID:%t] open() failed,%s task already running\n"), 
                        name.c_str())
                      );
            return;
        }

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Starting registered Producer task %s\n"), name.c_str()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_producerStarted)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Producer task %s already started\n"), name.c_str()));
                return;
            }

            if (m_tasks[i]->getConfig().name == name)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Task %s found\n"), name.c_str()));

                if (m_tasks[i]->getConfig().role == TaskRole::ROLE_PRODUCER)
                {
                    ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] Opening task: %s\n"), name.c_str()));
                    int ret = m_tasks[i]->open (args);

                    if (ret < 0)
                    {
                        ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t] open() failed and returned %d for %s\n"), 
                                    ret, name.c_str())
                                  );
                    }
                    else
                    {
                        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Producer task %s started\n"), name.c_str()));
                        m_producerStarted = true;
                    }
                }
                else
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t]: Task: %s not registered as a Producer\n"), name.c_str()));
                }
            }
        }

        if (!m_producerStarted)
        {
            ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t]: Producer task %s not found or not started\n"), name.c_str()));
        }
    }

    template<typename DT, typename IT>
    void TaskOrchestrator<DT, IT>::runConsumer (std::string name, void* args)
    {
        if (m_consumerStarted)
        {
            ACE_DEBUG ((LM_WARNING, ACE_TEXT ("[%T][%M][TID:%t] open() failed,%s task already running\n"), 
                        name.c_str())
                      );
            return;
        }

        ACE_DEBUG ((LM_DEBUG, ACE_TEXT("[%T][%M][TID:%t] Starting registered Consumer task %s\n"), name.c_str()));

        for (size_t i = 0; i < m_tasks.size(); ++i)
        {
            if (m_consumerStarted)
            {
                return;
            }

            if ((m_tasks[i]->getConfig().name == name) && (m_tasks[i]->getConfig().role == TaskRole::ROLE_CONSUMER))
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] Opening task: %s\n"), name.c_str()));
                int ret = m_tasks[i]->open (args);

                if (ret < 0)
                {
                    ACE_DEBUG ((LM_ERROR, ACE_TEXT ("[%T][%M][TID:%t] open() failed and returned %d for %s\n"), 
                                ret, name.c_str())
                              );
                }
                else
                {
                    m_consumerStarted = true;
                    return;
                }
            }
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

    template<typename DT, typename IT>
    ObjectPool<std::vector<DT> >* TaskOrchestrator<DT, IT>::getObjectPool()
    {
        return &m_objectPool;
    }
} // namespace Manager

// Explicit template instantiation for the types used in the application
template class Manager::TaskOrchestrator<Manager::ObjectData, Manager::TestData>;
