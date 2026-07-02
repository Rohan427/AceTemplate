#pragma once

#ifndef OBJECTDATA_HXX
#define OBJECTDATA_HXX

#include <ace/Log_Msg.h>

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
            int objectId;      // unique ID for object
            time_t timestamp;  // Time of data collection
            bool isValid;
            float level;

            ObjectData (int pobjectid, time_t ptimestamp, bool pisValid, float plevel)
                        //: objectId (pobjectid),
                        //  timestamp (ptimestamp),
                        //  isValid (pisValid),
                        //  level (plevel)
            {

                objectId = pobjectid;
                timestamp = ptimestamp;
                isValid = pisValid;
                level = plevel;

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("ObjectData %i: level: %f, isValid %i\n"), objectId, level, isValid));
            }

            ObjectData() : objectId (0),
                           timestamp (0.0f),
                           isValid (false),
                           level (5.0f)
            {}   // or whatever your fields are
    };

    class TestData
    {
        public:
            int objectId;
            float level;
            bool flagged;
            float x;
            float y;
            float z;

            TestData (int pobjectId, float plevel, bool pflagged, float px, float py, float pz)
                 //: objectId (pobjectId),
                //  level (plevel),
                //  flagged (pflagged),
                //  x (px),
                //  y (py),
                //  z (pz)
            {
                objectId = pobjectId;
                level = plevel;
                flagged = pflagged;
                x = px;
                y = py;
                z = pz;

                ACE_DEBUG ((LM_DEBUG, ACE_TEXT ("TestData %i: level: %f, flagged %i\n"), objectId, level, flagged));
            }
    };
} // namespace Manager

#endif // OBJECTDATA_HXX
