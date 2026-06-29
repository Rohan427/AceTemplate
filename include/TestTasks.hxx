#pragma once
#ifndef TESTTASKS_HXX
#define TESTTASKS_HXX

#include "ConfigurableTask.hxx"
#include <ace/Task.h>
#include <ace/Barrier.h>
#include <ace/Thread.h>
#include <ace/Condition_T.h>
#include <ace/Thread_Mutex.h>
#include <ace/OS_NS_stdio.h>

namespace Manager
{
    enum ProducerState { STOPPED, STARTED, RUNNING, FINISHED };

    struct TaskConfig;

    template<typename T>
    class ObjectPool;

    template<typename DataT, typename InputT>
    class ConfigurableTask;

    template<typename DT, typename IT>
    class TaskOrchestrator;

    template<typename DataT, typename InputT>
    class TestProducerTask : public ConfigurableTask<DataT, InputT>
    {
        public:
            TestProducerTask (const TaskConfig& cfg, TaskOrchestrator<DataT, InputT>* processor)
                : ConfigurableTask<DataT, InputT> (cfg, processor)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] TestProducerTask initialized\n")));
            }

        protected:
            virtual void processWorkload (int threadId, void* arg) override
            {
                // Heavy producer work - replace with your real logic
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] Producer processWorkload running\n")));

                this->m_orchestrator->setProducerFinished (ProducerState::RUNNING);
                // Example: copy or compute into buffer

                std::vector<InputT>* input = static_cast<std::vector<InputT>*>(arg);

                this->m_orchestrator->markAndSwap (true);
                this->m_orchestrator->m_nextRun = ACE_OS::gettimeofday() + ACE_Time_Value (0, this->m_config.producerIntervalMs * 1000);
                this->m_orchestrator->setProducerFinished (ProducerState::STOPPED);
            }
    };

    template<typename DataT, typename InputT>
    class TestConsumerTask : public ConfigurableTask<DataT, InputT>
    {
        public:
            TestConsumerTask (const TaskConfig& cfg, TaskOrchestrator<DataT, InputT>* processor)
                : ConfigurableTask<DataT, InputT>(cfg, processor)
            {
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t] TestConsumerTask initialized\n")));
            }

        protected:
            virtual void processWorkload (int threadId, void* arg) override
            {
                // Fast consumer - modify shared dataPtr
                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("[%T][%M][TID:%t]: Consumer processWorkload running\n")));

                std::vector<InputT>* input = static_cast<std::vector<InputT>*>(arg);

                //if (TaskOrchestrator<DataT, InputT>::instance().m_sharedDataPtr.value() && !buffer.empty())
                //{
                //    // Example: remove processed items from shared data
                //    if (!TaskOrchestrator<DataT, InputT>::instance().m_sharedDataPtr.value()->empty())
                //    {
                //        TaskOrchestrator<DataT, InputT>::instance().m_sharedDataPtr.value()->pop_back();
                //    }
                //}

                this->m_orchestrator->setConsumerFinished (true);
            }
        };
} // namespace Manager

#endif
