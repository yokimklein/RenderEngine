// https://github.com/yokimklein/engine/blob/master/DX11Framework/time/time.cpp

#include "time.h"
#include <reporting/report.h>

c_time_manager::c_time_manager()
	: m_frame_counter(0)
	, m_frame_timer(0.0f)
	, m_frames_per_second(0)
{
	m_last_frame = steady_clock::now();
}

const float c_time_manager::delta_time()
{
	float delta_time = duration<float>(steady_clock::now() - m_last_frame).count();
	m_last_frame = steady_clock::now();

	m_frame_timer += delta_time;

	if (m_frame_timer >= 1.0f)
	{
		m_frame_timer -= 1.0f;
		m_frames_per_second = m_frame_counter;
		m_frame_counter = 0;
	}

	m_accumulator += delta_time;
	if (m_accumulator >= SECONDS_PER_TICK)
	{
		m_accumulator = m_accumulator - SECONDS_PER_TICK;
		delta_time = SECONDS_PER_TICK;
	}
	else
	{
		delta_time = 0.0;
	}
	//LOG_MESSAGE("dt: %f acc: %f", delta_time, m_accumulator);
	return delta_time;
}