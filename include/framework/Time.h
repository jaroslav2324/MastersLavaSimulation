#pragma once

#include "pch.h"

class TimeAccumulator
{
public:
    void add(double ms)
    {
        samples.push_back(ms);
    }

    double average() const
    {
        if (samples.empty())
            return 0.0;

        double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
        return sum / samples.size();
    }

    size_t count() const
    {
        return samples.size();
    }

private:
    std::vector<double> samples;
};

class ScopedTimer
{
public:
    ScopedTimer(TimeAccumulator &acc)
        : accumulator(acc),
          start(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTimer()
    {
        auto end = std::chrono::steady_clock::now();
        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        accumulator.add(ms);
    }

private:
    TimeAccumulator &accumulator;
    std::chrono::steady_clock::time_point start;
};

class ConditionalScopedTimer
{
public:
    ConditionalScopedTimer(TimeAccumulator *acc)
        : accumulator(acc),
          start(acc ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{})
    {
    }

    ~ConditionalScopedTimer()
    {
        if (!accumulator)
            return;

        auto end = std::chrono::steady_clock::now();
        double ms =
            std::chrono::duration<double, std::milli>(end - start).count();
        accumulator->add(ms);
    }

private:
    TimeAccumulator *accumulator;
    std::chrono::steady_clock::time_point start;
};