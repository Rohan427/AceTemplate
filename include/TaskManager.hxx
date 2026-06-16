#pragma once

#ifndef TASKMANAGER_HXX
#define TASKMANAGER_HXX

#include "ObjectPool.hxx"
#include <ace/Task.h>
#include <ace/Barrier.h>
#include "ace/Thread.h"
#include <vector>
#include <algorithm>
#include <unordered_set>
#include "SystemCapabilities.hxx"
#include "ObjectData.hxx"

namespace Manager
{
    class TaskManager : public ACE_Task<ACE_MT_SYNCH>
    {
        private:
            static TaskManager* s_instance;
            bool m_done = false;
            int64_t m_currentTime;
            std::atomic<int> m_threadIndexer {0};
            ACE_Barrier* m_barrier = nullptr;
            int m_numThreads;

            ::SchedulingTier m_selectedTier;
            std::vector<HardwareCore> m_hardwareCorePool;

            std::unordered_set<std::string> m_activeIds; // Fast lookup for duplicates


        public:
            static ACE_Thread_Mutex m_vectorLock; // Protects the vector itself

            /************* Functions ******************/

            static TaskManager* instance();

            void startApplication (int numThreads);
            void stopApplication();

            TaskManager()
            {
                delete m_barrier;
            }

            // Signal the svc() loop to terminate
            void stop()
            {
                m_done = true;
                // Also nudge the message queue in case threads are blocked on it
                this->msg_queue()->deactivate();
            }

            // ACE worker thread
            virtual int svc() override;
    };
}

#endif // TASKMANAGER_HXX
