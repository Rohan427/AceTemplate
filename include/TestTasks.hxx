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
                // Example: copy or compute into buffer

                std::vector<InputT>* input = static_cast<std::vector<InputT>*>(arg);

//                if (buffer.size() < 100 && ConfigurableTask<DataT, InputT>::m_dataProcessor->m_sharedDataPtr.value())
                {
//                    buffer.resize (50);
                    // Fill with data...

                    TaskOrchestrator<DataT, InputT>::instance().markAndSwap (true);
                }
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

                TaskOrchestrator<DataT, InputT>::instance().setConsumerFinished (true);
            }
        };
} // namespace Manager

#endif
