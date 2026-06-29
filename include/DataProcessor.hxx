#pragma once
#ifndef DATAPROCESSOR_HXX
#define DATAPROCESSOR_HXX

#include "ObjectPool.hxx"
#include "Compatibility.hxx"
#include "ConfigurableTask.hxx"
#include "ace/Singleton.h"
#include <vector>

namespace Manager
{
    class DataProcessor;

    class DataProcessor
    {
        DECLARE_SINGLETON (DataProcessor)   // Use the macro

        public:
            void initialize (size_t poolCapacity);
            void acceptData (std::vector<TestData>* dataPtr);

        private:
            std::vector<TestData>* m_sharedDataPtr;
            TaskOrchestrator<ObjectData, TestData>* m_orchestrator;
    };

} // namespace Manager

#endif // DATAPROCESSOR_HXX
