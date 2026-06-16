#pragma once

#ifndef TASKORCHESTRATOR_HXX
#define TASKORCHESTRATOR_HXX

#include "ObjectPool.hxx"
#include <ace/Thread_Mutex.h>
#include <vector>
#include "ConfigurableTask.hxx"
#include "ObjectData.hxx"

namespace Manager
{
    class TaskOrchestrator
    {
        public:
            static TaskOrchestrator& instance();

            void initialize (int maxThreads, ObjectPool<std::vector<ObjectData> >& objectPool);

            // Factory methods for common patterns
            ConfigurableTask<ObjectData>* createProducerTask(
                const std::string& name,
                int numChildThreads,
                int basePriority,
                WorkloadType workloadType,
                bool useRealTime,
                unsigned long intervalMs);

            ConfigurableTask<ObjectData> *createConsumerTask (const std::string& name,
                                                              int numChildThreads,
                                                              int basePriority,
                                                              WorkloadType workloadType,
                                                              bool useRealTime
                                                             );

            void registerTask (ConfigurableTask<ObjectData> *task);

            template<typename DerivedTask>
            DerivedTask* createAndRegisterTask (const TaskConfig& cfg)
            {
                DerivedTask* task = new DerivedTask(cfg, *m_objectPool);
                registerTask(task);
                return task;
            }

            void startAll();
            void stopAll();

            // Access to shared pool
            ObjectPool<std::vector<ObjectData>> &getObjectPool();
            SchedulingTier getSchedulingTier() const
            {
                return m_selectedTier;
            
            }

            const std::vector<HardwareCore>& getHardwarePool() const
            { 
                return m_hardwarePool;
            }

        private:
            TaskOrchestrator();
            ~TaskOrchestrator();

            static TaskOrchestrator *s_instance;
            static ACE_Thread_Mutex s_creationLock;

            std::vector<ConfigurableTask<ObjectData>*> m_tasks;
            ObjectPool<std::vector<ObjectData>> *m_objectPool;
            bool m_initialized;

            SchedulingTier m_selectedTier;
            std::vector<HardwareCore> m_hardwarePool;
            size_t m_numThreads;
    };
} // namespace Manager

#endif // TASKORCHESTRATOR_HXX
