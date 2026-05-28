#pragma once

// C++
#include <iostream>
#include <string>
#include <chrono>

// Simple high-resolution timer utility for measuring elapsed time
class Timer
{
public:
    // Constructor automatically starts the timer
    Timer() { 
        reset(); 
    }

    // Resets the start time to "now"
    void reset() { 
        m_start = std::chrono::high_resolution_clock::now(); 
    }

    // Returns elapsed time in seconds (as a float)
    float elapsed() { 
        // Compute duration in nanoseconds, then convert to seconds
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_start).count() * 0.001f * 0.001f * 0.001f; 
    }

    // Returns elapsed time in milliseconds
    float elapsedMillis()
    {
        return elapsed() * 1000.0f;
    }
    
// Stores the timestamp when the timer was last started/reset
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

// RAII scoped timer: measures execution time of a code block
class ScopedTimer
{
public:
    ScopedTimer(const std::string& name) : m_name(name) { }

    ~ScopedTimer()
    {
        float time = m_timer.elapsedMillis();
        std::cout << m_name << " - " << time << "ms\n";
    }
private:
    std::string m_name;
    Timer m_timer;
};