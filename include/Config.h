#pragma once

// UTIME Input Method Configuration
// All magic numbers and configuration constants are centralized here

// ===================================================================
// Candidate Window Configuration
// ===================================================================
namespace Config {
    namespace CandidateWindow {
        const int MIN_WIDTH = 200;          // Minimum window width in pixels
        const int Y_OFFSET = 25;            // Y-axis offset from cursor position
        const int PADDING = 5;              // Internal padding
        const int LINE_HEIGHT = 24;         // Height of each candidate line
    }

// ===================================================================
// Caret Position Configuration
// ===================================================================
    namespace CaretPosition {
        const int MOUSE_FALLBACK_Y_OFFSET = 20;  // Y-axis offset when using mouse position
        const int FALLBACK_WIDTH = 2;            // Default caret width
        const int FALLBACK_HEIGHT = 20;          // Default caret height
        const int DEFAULT_POSITION_X = 100;      // Default X position if all strategies fail
        const int DEFAULT_POSITION_Y = 100;      // Default Y position if all strategies fail
    }

// ===================================================================
// Dictionary Query Configuration
// ===================================================================
    namespace Dictionary {
        const int MAX_FUZZY_VARIANTS = 5;   // Maximum number of fuzzy pinyin variants
        const int MAX_QUERY_RESULTS = 20;   // Maximum SQL query result limit
    }

// ===================================================================
// Log Configuration
// ===================================================================
    namespace Log {
        const wchar_t* const FILE_PATH = L"C:\\Windows\\Temp\\UTIME_Debug.log";
        
        enum Level {
            LOG_LEVEL_DEBUG = 0,    // Detailed debug information (development only)
            LOG_LEVEL_INFO = 1,     // Key operations (production)
            LOG_LEVEL_WARN = 2,     // Warning messages
            LOG_LEVEL_ERROR = 3     // Error messages (always output)
        };
        
        const Level DEFAULT_LEVEL = LOG_LEVEL_INFO;
    }
}
