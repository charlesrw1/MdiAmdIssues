#pragma once

class Profiler
{
public:
	static void init();
	static void start_scope(const char* name, bool include_gpu);
	static void end_scope(const char* name);
	static void end_frame_tick();

	static bool get_time_for_name(const char* name, double& cpu_time, double& gpu_time);
};

struct Profile_Scope_Wrapper
{
	Profile_Scope_Wrapper(const char* name, bool gpu) :myname(name) {
		Profiler::start_scope(name, gpu);
	}
	~Profile_Scope_Wrapper() {
		Profiler::end_scope(myname);
	}

	const char* myname;
};
#define PROFILING

#ifdef PROFILING
#define MAKEPROF_(type, include_gpu) Profile_Scope_Wrapper PROFILEEVENT##__LINE__(type, include_gpu)
#define CPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, false)
#define GPUFUNCTIONSTART MAKEPROF_(__FUNCTION__, true)
#define GPUSCOPESTART(name) MAKEPROF_(name, true)
#define CPUSCOPESTART(name) MAKEPROF_(name, false)


#else
#define CPUPROF(type)
#define GPUPROF(type)
#endif
