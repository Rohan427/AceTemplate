#pragma once

#ifndef CONFIG_HXX
#define CONFIG_HXX

#include "Json.hxx"
#include "Configuration.hxx"

// Standard includes
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>


class Config : public configuration::Configuration
{
    public:
        json::Json::JsonType jType;

        int THREAD_SLEEP_TIME; // suggest 1ms tick for performance
        int MAX_THREADS;
        uint64_t MAX_FPU_THREADS = 32;
        int MAX_OBJECTS = 500000; //Fallback for bad configuration files
        int UPDATE_INTERVAL = 500;
        float VALID_LEVEL = 5.0f;
        int MAX_CON_THREADS;
        int MAX_PROD_THREADS;


        struct ConfigErr
        {
            std::string message;
            int err;
            json::Json::JsonType type;
            std::string varName;
        };

        static Config& getInstance()
        {
            static Config instance;
            return instance;
        }

        // Delete copy constructor and assignment operator
        Config (const Config&) = delete;
        Config& operator= (const Config&) = delete;

        void init (const std::string filePath)
        {
            current = false;
            timestamp = 0;
            cfgFilePath = filePath;
            current = Config::update();
        };

        std::string getFilePath()
        {
            return cfgFilePath;
        }

        int getThreadSleepTime(); // 50ms tick
        int getMaxThreads(); // The maximum number of threads allocated to the application
        int getMaxObjects();
        uint64_t getMaxFpuThreads();
        float getLevel();
        int getDataUpdateInterval();
        int getMaxConThreads();
        int getMaxProdThreads();

        //bool isCurrent();
        //time_t getLastReadTime();
        bool update();
        ConfigErr getErrObj();
        bool isCurrent();
        time_t getLastReadTime();
        bool save (const std::string& filename, bool changePath);

    private:
        Config() {};

        std::string getString (const std::string& name);
        double getDouble (const std::string& name);
        int64_t getInt64 (const std::string& name);
        uint64_t getUint64 (const std::string& name);
        bool isBool (const std::string& name);
        bool isNull (const std::string& name);

        bool setString (const std::string& name, const std::string& value);
        bool setDouble (const std::string& name, double value);
        bool setInt64 (const std::string& name, int64_t value);
        bool setUint64 (const std::string& name, uint64_t value);
        bool setBool (const std::string& name, bool value);
        bool setNull (const std::string& name);

        ConfigErr errObj;
        json::Json parser;
};

#endif // CONFIG_HXX
