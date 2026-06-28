#pragma once

#ifndef OBJECTDATA_HXX
#define OBJECTDATA_HXX

namespace Manager
{
    struct BufferState
    {
        bool valid;                    // Producer sets this
        unsigned long long version;    // Optional monotonic counter
        time_t timestamp;              // For freshness checks

        BufferState() : valid (false), version (0), timestamp (0) {}
    };

    class ObjectData
    {
        public:
            int objectid;      // unique ID for object
            time_t timestame;  // Time of data collection
            bool isValid;
            float level;

            ObjectData (int pobjectid, time_t ptimestame, bool pisValid, float plevel)
                        : objectid (pobjectid),
                          timestame (ptimestame),
                          isValid (pisValid),
                          level (plevel)
            {}

            ObjectData() : objectid (0),
                           timestame (0.0f),
                           isValid (0.0f),
                           level (0.0f)
            {}   // or whatever your fields are
    };

    class TestData
    {
        public:
            int objectId;
            float level;
            float x;
            float y;
            float z;

            TestData (int pobjectId, float plevel, float px, float py, float pz)
                : objectId (pobjectId),
                  level (plevel),
                  x (px),
                  y (py),
                  z (pz)
            {}
    };
} // namespace Manager

#endif // OBJECTDATA_HXX
