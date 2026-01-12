#pragma once

// Compatibility header for Tracy with older Windows SDK versions
// Defines RelationProcessorDie if not available in the current SDK

#ifdef _WIN32
    #include <windows.h>

    // Check if RelationProcessorDie is already defined
    #ifndef RelationProcessorDie
        // RelationProcessorDie was added in Windows 10, version 1903 (10.0.18362.0)
        // If using an older SDK, define it here
        #define RelationProcessorDie ((LOGICAL_PROCESSOR_RELATIONSHIP)5)
    #endif

#endif
