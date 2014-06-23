#ifndef STD_THREAD_H_
#define STD_THREAD_H_

#if !defined(_WIN32)
// GCC 4.4 provides <thread>
#ifndef _GLIBCXX_USE_SCHED_YIELD
#define _GLIBCXX_USE_SCHED_YIELD
#endif
#include <thread>
#else

// partial std::thread implementation for win32/pthread

#include <algorithm>

#if (_MSC_VER >= 1600)
#define USE_RVALUE_REFERENCES
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#if defined(_MSC_VER) && defined(_MT)
// When linking with LIBCMT (the multithreaded C library), Microsoft recommends
// using _beginthreadex instead of CreateThread.
#define USE_BEGINTHREADEX
#include <process.h>
#endif

#ifdef USE_BEGINTHREADEX
#define THREAD_ID unsigned
#define THREAD_RETURN unsigned __stdcall
#else
#define THREAD_ID DWORD
#define THREAD_RETURN DWORD WINAPI
#endif
#define THREAD_HANDLE HANDLE

namespace std
{

class thread
{
public:
	typedef THREAD_HANDLE native_handle_type;

	class id
	{
		friend class thread;
	public:
		id() : m_thread(0) {}
		id(THREAD_ID _id) : m_thread(_id) {}

		bool operator==(const id& rhs) const
		{
			return m_thread == rhs.m_thread;
		}

		bool operator!=(const id& rhs) const
		{
			return !(*this == rhs);
		}
		
		bool operator<(const id& rhs) const
		{
			return m_thread < rhs.m_thread;
		}

	private:
		THREAD_ID m_thread;
	};

	// no variadic template support in msvc
	//template <typename C, typename... A>
	//thread(C&& func, A&&... args);

	template <typename C>
	thread(C func)
	{
		StartThread(new Func<C>(func));
	}

	template <typename C, typename A>
	thread(C func, A arg)
	{
		StartThread(new FuncArg<C, A>(func, arg));
	}

	thread() /*= default;*/ {}

#ifdef USE_RVALUE_REFERENCES
	thread(const thread&) /*= delete*/;

	thread(thread&& other)
	{
#else
	thread(const thread& t)
	{
		// ugly const_cast to get around lack of rvalue references
		thread& other = const_cast<thread&>(t);
#endif
		swap(other);
	}

#ifdef USE_RVALUE_REFERENCES
	thread& operator=(const thread&) /*= delete*/;

	thread& operator=(thread&& other)
	{
#else
	thread& operator=(const thread& t)
	{
		// ugly const_cast to get around lack of rvalue references
		thread& other = const_cast<thread&>(t);
#endif
		if (joinable())
			detach();
		swap(other);
		return *this;
	}

	~thread()
	{
		if (joinable())
			detach();
	}

	bool joinable() const
	{
		return m_id != id();
	}

	id get_id() const
	{
		return m_id;
	}

	native_handle_type native_handle()
	{
		return m_handle;
	}

	void join()
	{
		WaitForSingleObject(m_handle, INFINITE);
		detach();
	}

	void detach()
	{
		CloseHandle(m_handle);
		m_id = id();
	}

	void swap(thread& other)
	{
		std::swap(m_id, other.m_id);
		std::swap(m_handle, other.m_handle);
	}
	
	static unsigned hardware_concurrency()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return static_cast<unsigned>(sysinfo.dwNumberOfProcessors);
	}

private:
	id m_id;
	
	native_handle_type m_handle;

	template <typename F>
	void StartThread(F* param)
	{
#ifdef USE_BEGINTHREADEX
		m_handle = (HANDLE)_beginthreadex(NULL, 0, &RunAndDelete<F>, param, 0, &m_id.m_thread);
#else
		m_handle = CreateThread(NULL, 0, &RunAndDelete<F>, param, 0, &m_id.m_thread);
#endif
	}
	
	template <typename C>
	class Func
	{
	public:
		Func(C _func) : func(_func) {}

		void Run() { func(); }

	private:
// Both Visual Studio 2012 & 2013 need the following ifdef, or else they complain about losing const-volatile qualifiers:
// Error	23	error C3848: expression having type 'const std::_Bind<true,void,void 
//(__cdecl *const )(PrioritizedWorkQueue *),PrioritizedWorkQueue *&>' would lose some const-volatile qualifiers in order to call 'void std::_Bind<true,void,void (__cdecl *const )(PrioritizedWorkQueue *),PrioritizedWorkQueue *&>::operator ()<>(void)' (thread\prioritizedworkqueue.cpp)

// Error	24	error C3848: expression having type 'const std::_Bind<true,void,std::_Pmf_wrap<void (__cdecl WorkerThread::* )(void),void,WorkerThread,>,WorkerThread *const >'
// would lose some const-volatile qualifiers in order to call 
// 'void std::_Bind<true,void,std::_Pmf_wrap<void (__cdecl WorkerThread::* )(void),void,WorkerThread,>,WorkerThread *const >::operator ()<>(void)' (thread\threadpool.cpp)

// Error	25	error C3848 : expression having type 'const std::_Bind<true,void,std::_Pmf_wrap<void (__cdecl http::Download::* )(std::shared_ptr<http::Download>),void,http::Download,std::shared_ptr<http::Download>>,http::Download *const ,std::shared_ptr<http::Download> &>' 
// would lose some const - volatile qualifiers in order to call 
// 'void std::_Bind<true,void,std::_Pmf_wrap<void (__cdecl http::Download::* )(std::shared_ptr<http::Download>),void,http::Download,std::shared_ptr<http::Download>>,http::Download *const ,std::shared_ptr<http::Download> &>::operator ()<>(void)' (net\http_client.cpp)
#if _MSC_VER >= 1700
		C func;
#else
		C const func;
#endif
	};

	template <typename C, typename A>
	class FuncArg
	{
	public:
		FuncArg(C _func, A _arg) : func(_func), arg(_arg) {}

		void Run() { func(arg); }

	private:
		C const func;
		A arg;
	};

	template <typename F>
	static THREAD_RETURN RunAndDelete(void* param)
	{
		static_cast<F*>(param)->Run();
		delete static_cast<F*>(param);

		return 0;
	}
};

namespace this_thread
{

inline void yield()
{
	SwitchToThread();
}

inline thread::id get_id()
{
	return GetCurrentThreadId();
}

}	// namespace this_thread

}	// namespace std

#undef USE_RVALUE_REFERENCES
#undef USE_BEGINTHREADEX
#undef THREAD_ID
#undef THREAD_RETURN
#undef THREAD_HANDLE

#endif
#endif
