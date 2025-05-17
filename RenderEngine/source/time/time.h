#pragma once
#include <chrono>
#include <types.h>
using namespace std::chrono;

constexpr dword TICK_RATE = 60;
constexpr float SECONDS_PER_TICK = 1.0f / TICK_RATE;

class c_time_manager
{
public:
	c_time_manager();

	const float delta_time();
	inline void increment_frame_count() { m_frame_counter++; };
	inline const dword get_frames_per_second() const { return m_frames_per_second; };

private:
	steady_clock::time_point m_last_frame;
	float m_accumulator = 0.0;

	dword m_frame_counter;
	float m_frame_timer;
	dword m_frames_per_second;
};