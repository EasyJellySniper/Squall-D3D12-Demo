#pragma once
#define GRAPHICTIME
#if defined(GRAPHICTIME)
// timer
/** Use to init the clock */
#define TIMER_INIT \
    LARGE_INTEGER frequency; \
    LARGE_INTEGER t1,t2; \
    double elapsedTime; \
    QueryPerformanceFrequency(&frequency);


/** Use to start the performance timer */
#define TIMER_START QueryPerformanceCounter(&t1);

/** Use to stop the performance timer and output the result to the standard stream. Less verbose than \c TIMER_STOP_VERBOSE */
#define TIMER_STOP \
    QueryPerformanceCounter(&t2); \
    elapsedTime=(float)(t2.QuadPart-t1.QuadPart)/frequency.QuadPart; \
	elapsedTime *= 1000.0f;
#endif

struct GameTime
{
	double updateTime;
	double renderTime;
	double renderThreadTime;
	double gpuTime;
};