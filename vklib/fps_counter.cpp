#include "fps_counter.h"

FPSCounter::FPSCounter(size_t histroyFrameTimeCnt /*= 100*/) : m_maxHistoryFrameCnt(histroyFrameTimeCnt)
{
	m_prevFrameTimePoint = std::chrono::high_resolution_clock::now();
}

double FPSCounter::CalcFrameSeconds()
{
	// Calculate the time of the last frame.
	std::chrono::time_point currFrameTimePoint = std::chrono::high_resolution_clock::now();
	std::chrono::duration frameDuration = currFrameTimePoint - m_prevFrameTimePoint;
	double frameSeconds = std::chrono::duration_cast<std::chrono::nanoseconds>(frameDuration).count() * 1e-9;

	// Maintain frame time history.
	if (m_historyFrameSeconds.size() > m_maxHistoryFrameCnt) {
		double oldestFrameTime = m_historyFrameSeconds.front();
		m_historyFrameSeconds.pop_front();
		frameSecondSum = std::max(frameSecondSum - oldestFrameTime, 0.0);
	}
	m_historyFrameSeconds.push_back(frameSeconds);
	frameSecondSum += frameSeconds;

	// Calculate average frame time.
	m_avgFrameSeconds = static_cast<double>(frameSecondSum / m_historyFrameSeconds.size());
	m_avgFPS = 1.0 / m_avgFrameSeconds;

	// Smooth the fps value.
	m_FPS = std::lerp(m_FPS, m_avgFPS, 0.05);

	// Update the previous frame time point.
	m_prevFrameTimePoint = currFrameTimePoint;

	return frameSeconds;
}

double FPSCounter::CalcElapsedSeconds()
{
	std::chrono::time_point currFrameEndTimePoint = std::chrono::high_resolution_clock::now();
	std::chrono::duration duration = currFrameEndTimePoint - m_prevFrameTimePoint;
	double seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() * 1e-9;
	return seconds;
}
