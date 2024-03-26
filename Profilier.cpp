
#include "Profilier.h"
#include "glad/glad.h"
#include <chrono>
#include <SDL2/SDL.h>
#include <vector>
#include <cassert>

using std::chrono::steady_clock;
using std::chrono::microseconds;


struct Profile_Event
{
	const char* name = "";
	bool enabled = false;
	uint64_t last_interval_time_cpu = 0;
	uint64_t cpustart = 0;
	uint64_t cputime = 0;	// microseconds
	uint32_t accumulated_cpu = 0;
	bool started = false;

	uint64_t last_interval_time_gpu = 0;
	uint64_t gputime = 0;	// nanoseconds
	uint32_t glquery[2];
	uint32_t accumulated_gpu = 0;
	bool waiting = false;

	bool is_gpu_event = false;
	int parent_event = -1;
};

std::vector<Profile_Event> events;
std::vector<int> stack;
uint64_t intervalstart = 0;
int accumulated = 1;


void Profiler::init()
{
	intervalstart = SDL_GetPerformanceCounter();
}

bool Profiler::get_time_for_name(const char* name, double& cpu_time, double& gpu_time)
{
	for (int i = 0; i < events.size(); i++) {
		if (strcmp(events[i].name, name) == 0) {
			cpu_time = events[i].last_interval_time_cpu / 1000.0;
			if (events[i].is_gpu_event)
				gpu_time = events[i].last_interval_time_gpu / 1000000.0;
			else gpu_time = 0.0;
			return true;
		}
	}
	return false;
}


void Profiler::end_frame_tick()
{
	uint64_t timenow = SDL_GetPerformanceCounter();
	if ((timenow - intervalstart) / (double)SDL_GetPerformanceFrequency() > 1.0) {

		accumulated = 0;
		for (int i = 0; i < events.size(); i++) {
			Profile_Event& e = events[i];
			e.last_interval_time_cpu = (e.accumulated_cpu > 0) ? e.cputime / e.accumulated_cpu : 0;
			e.cputime = 0;
			e.accumulated_cpu = 0;

			if (e.is_gpu_event) {
				e.last_interval_time_gpu = (e.accumulated_gpu > 0) ? e.gputime / e.accumulated_gpu : 0;
				e.gputime = 0;
				e.accumulated_gpu = 0;
			}
		}

		intervalstart = timenow;
	}
}


void Profiler::start_scope(const char* name, bool gpu)
{
	int index = 0;
	for (; index < events.size(); index++) {
		if (strcmp(events[index].name, name) == 0)
			break;
	}
	if (index == events.size()) {
		events.push_back(Profile_Event());
		events[index].name = name;
		if (gpu) {
			events[index].is_gpu_event = true;
			glGenQueries(2, events[index].glquery);
		}
	}
	Profile_Event& e = events[index];
	if (stack.size() > 0)
		e.parent_event = stack.at(stack.size() - 1);
	else
		e.parent_event = -1;

	assert(!e.started);
	e.started = true;
	e.cpustart = std::chrono::duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();

	if (e.is_gpu_event && !e.waiting) {
		glQueryCounter(e.glquery[0], GL_TIMESTAMP);
	}

	if (e.is_gpu_event)
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);

	stack.push_back(index);
}

void Profiler::end_scope(const char* name)
{
	int index = 0;
	for (; index < events.size(); index++) {
		if (strcmp(events[index].name, name) == 0) break;
	}
	assert(index != events.size());
	Profile_Event& e = events[index];
	assert(e.started);
	e.started = false;
	uint64_t timenow = std::chrono::duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	e.cputime += timenow - e.cpustart;
	e.accumulated_cpu++;

	if (e.is_gpu_event) {
		if (!e.waiting) {
			glQueryCounter(e.glquery[1], GL_TIMESTAMP);
			e.waiting = true;
		}
		int available{};
		glGetQueryObjectiv(e.glquery[1], GL_QUERY_RESULT_AVAILABLE, &available);

		if (available == GL_TRUE) {
			uint64_t starttime{}, stoptime{};

			glGetQueryObjectui64v(e.glquery[0], GL_QUERY_RESULT, &starttime);
			glGetQueryObjectui64v(e.glquery[1], GL_QUERY_RESULT, &stoptime);

			e.gputime += stoptime - starttime;
			e.accumulated_gpu++;
			//e.queryback = !e.queryback;
			e.waiting = false;
		}
	}

	if (e.is_gpu_event)
		glPopDebugGroup();

	stack.pop_back();
}