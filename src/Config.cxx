#include "Config.hxx"

#define DEBUG


int Config::getThreadSleepTime()
{
    THREAD_SLEEP_TIME = (int)getInt64 ("threadSleepTime");

    return THREAD_SLEEP_TIME;
}

int Config::getMaxThreads()
{
    MAX_THREADS = (int)getInt64 ("maxThreads");

    return MAX_THREADS;
}


int Config::getMaxObjects()
{
    MAX_OBJECTS = (int)getUint64 ("maxObjects");

    return MAX_OBJECTS;
}

uint64_t Config::getMaxFpuThreads()
{
    MAX_FPU_THREADS = (uint64_t)getUint64 ("maxFpuThreads");

    return MAX_FPU_THREADS;
}

float Config::getLevel()
{
    UPDATE_INTERVAL = (float)getDouble ("level");

    return UPDATE_INTERVAL;
}

int Config::getDataUpdateInterval()
{
    UPDATE_INTERVAL = (int)getUint64 ("dataUpdateInterval");

    return UPDATE_INTERVAL;
}

int Config::getMaxConThreads()
{
    MAX_CON_THREADS = (int)getInt64 ("maxConThreads");

    return MAX_CON_THREADS;
}

int Config::getMaxProdThreads()
{
    MAX_PROD_THREADS = (int)getUint64 ("maxProTHreads");

    return MAX_PROD_THREADS;
}




std::string Config::getString (const std::string& name)
{
    return parser.getString (name, "");
}

double Config::getDouble (const std::string& name)
{
    return parser.getDouble (name, 0.0);
}

int64_t Config::getInt64 (const std::string& name)
{
    return parser.getInt64 (name, 0);
}

uint64_t Config::getUint64 (const std::string& name)
{
    return parser.getUint64 (name, 0);
}

bool Config::isBool (const std::string& name)
{
    return parser.isBool (name, false);
}

bool Config::isCurrent()
{
    return current;
}

bool Config::isNull (const std::string& name)
{
    return parser.isNull (name);
}

time_t Config::getLastReadTime()
{
    return timestamp;
}


// Setters

bool Config::setString (const std::string& name, const std::string& value)
{
    return parser.setString (name, value);
}

bool Config::setDouble (const std::string& name, double value)
{
    return parser.setDouble (name, value);
}

bool Config::setInt64 (const std::string& name, int64_t value)
{
    return parser.setInt64 (name, value);
}

bool Config::setUint64 (const std::string& name, uint64_t value)
{
    return parser.setUint64 (name, value);
}

bool Config::setBool (const std::string& name, bool value)
{
    return parser.setBool (name, value);
}

bool Config::setNull (const std::string& name)
{
    return parser.setNull (name);
}

bool Config::update()
{
    struct stat fileInfo;

    if (!current)
    {
        // read file if exists
#ifdef DEBUG
        std::cout << "Parse JSON file with comments" << std::endl;
#endif
        if (stat (getFilePath().c_str(), &fileInfo) == 0)
        {
            // st_mtime is the time of last modification of file content
            time_t mtime = fileInfo.st_mtime;

            if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
            {
                if ((current = parser.read2 (getFilePath().c_str())))
                {
                    
                    getThreadSleepTime(); // 50ms tick
                    getMaxThreads(); // The maximum number of threads allocated to the application
                    getMaxObjects();
                    getMaxFpuThreads();
                    getLevel();
                    getDataUpdateInterval();
                    getMaxConThreads();
                    getMaxProdThreads();

                    std::time (&timestamp);
                }
            } // if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
            else
            {
                std::cerr << getFilePath() << " not updated" << std::endl;
                errObj.err = parser.getJsonError().err;
                errObj.message = parser.getJsonError().message;
                errObj.varName = parser.getJsonError().varName;
                errObj.type = parser.getJsonError().type;
            } // END IF-ELSE: if ((getLastReadTime() == 0) || (mtime > getLastReadTime()))
        }
        else
        {
            std::cerr << "Error getting file info " << getFilePath() << ": " << strerror (errno) << std::endl;
            errObj.err = errno;
            errObj.message = strerror (errno);
            errObj.varName = getFilePath();
            errObj.type = json::Json::TYPE_NULL;
        } // END IF-ELSE: if (stat (getFilePath(), &fileInfo) != 0)
#ifdef DEBUG
        std::cout << "File update complete: " << std::endl << std::endl;
#endif
    }
    // else don't read

    return (current);
}

Config::ConfigErr Config::getErrObj()
{
    errObj.err = parser.getJsonError().err;
    errObj.message = parser.getJsonError().message;
    errObj.varName = parser.getJsonError().varName;
    errObj.type = parser.getJsonError().type;

    return (errObj);
}

bool Config::save (const std::string& filename, bool changePath)
{
    return parser.save (filename, changePath);
}
