#ifndef _FPS_COUNTER_H_
#define _FPS_COUNTER_H_

#include <deque>
#include <chrono>

class FPSCounter
{
public:

	FPSCounter(size_t histroyFrameTimeCnt = 100);

	double CalcFrameSeconds();
	double GetFPS() const { return m_FPS; }

	double GetAverageFrameSeconds() const { return m_avgFrameSeconds; }
	double GetAverageFPS() const { return m_avgFPS; }
	
	double CalcElapsedSeconds();
private:
	//const int MaxFrameFPS = 120;
	//const double MinFrameTime = 1.0e9 / MaxFrameFPS;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_prevFrameTimePoint;

	// Keep the latest some frame times.
	const size_t m_maxHistoryFrameCnt = 100;
	std::deque<double> m_historyFrameSeconds;  // In nanosecond
	double frameSecondSum = 0.0;	// Sum of all time in frameTimeHistory.

	double m_avgFrameSeconds = 0.0;
	double m_avgFPS = 0.0;

	double m_FPS = 0.0;		// Smoothed FPS
};


#endif