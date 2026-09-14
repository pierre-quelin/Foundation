/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Thread.cpp
 * @brief Win32 thread implementation.
 */
#include "tools/os/thread/Thread.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <windows.h>

namespace
{

void setCurrentThreadName(const char* name)
{
#if defined(_MSC_VER)
    constexpr DWORD MsVcException = 0x406D1388;

#pragma pack(push, 8)
    struct ThreadNameInfo
    {
        DWORD type;
        LPCSTR name;
        DWORD threadId;
        DWORD flags;
    };
#pragma pack(pop)

    ThreadNameInfo info{};
    info.type     = 0x1000;
    info.name     = name;
    info.threadId = static_cast<DWORD>(-1);
    info.flags    = 0;

    __try
    {
        RaiseException(MsVcException, 0, sizeof(info) / sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
#else
    (void)name;
#endif
}

} // namespace

namespace tools::os::thread
{

unsigned long Thread::entryPoint(void* arg)
{
    static_cast<Thread*>(arg)->runEntry();
    return 0;
}

void IThread::sleep_for(util::chrono::Delay d)
{
    const auto ns = d.toNanoseconds();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(ns).count();
    if (ms > 0)
    {
        Sleep(static_cast<DWORD>(ms));
        return;
    }

    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(ns).count();
    if (us > 0)
    {
        Sleep(1);
    }
}

Thread::Thread() = default;

Thread::Thread(std::function<void()> fn, std::string name) : _fn(std::move(fn)), _name(std::move(name))
{
}

Thread::~Thread()
{
    ensureJoined();
    if (_handle != nullptr)
    {
        CloseHandle(static_cast<HANDLE>(_handle));
        _handle = nullptr;
    }
}

void Thread::setName(std::string_view name)
{
    _name.assign(name);
}

void Thread::setSchedPolicy(SchedPolicy policy)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setSchedPolicy after start()");
    }
    _policy = policy;
    (void)_policy;
}

void Thread::setPriority(int posixPriority)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setPriority after start()");
    }
    _posixPriority = posixPriority;
}

void Thread::setAffinity(const CpuAffinity& affinity)
{
    if (_started.load())
    {
        throw std::logic_error("tools::os::thread::Thread::setAffinity after start()");
    }
    _affinity = affinity;
}

void Thread::start()
{
    if (_started.exchange(true))
    {
        throw std::logic_error("tools::os::thread::Thread::start called twice");
    }

    _running.store(true);
    _handle = reinterpret_cast<void*>(CreateThread(
        nullptr,
        0,
        Thread::entryPoint,
        this,
        0,
        reinterpret_cast<DWORD*>(&_threadId)));
    if (_handle == nullptr)
    {
        _started.store(false);
        _running.store(false);
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateThread");
    }

    if (!SetThreadPriority(static_cast<HANDLE>(_handle), mapPosixPriority(_posixPriority)))
    {
        const auto err = GetLastError();
        CloseHandle(static_cast<HANDLE>(_handle));
        _handle = nullptr;
        _started.store(false);
        _running.store(false);
        throw std::system_error(static_cast<int>(err), std::system_category(), "SetThreadPriority");
    }

    const std::uint64_t mask = _affinity.toBitMask();
    if (mask != 0)
    {
        if (SetThreadAffinityMask(static_cast<HANDLE>(_handle), static_cast<DWORD_PTR>(mask)) == 0)
        {
            const auto err = GetLastError();
            CloseHandle(static_cast<HANDLE>(_handle));
            _handle = nullptr;
            _started.store(false);
            _running.store(false);
            throw std::system_error(static_cast<int>(err), std::system_category(), "SetThreadAffinityMask");
        }
    }
}

void Thread::join()
{
    if (!_started.load() || _joined.load())
    {
        return;
    }

    {
        std::unique_lock lock(_mutex);
        _cv.wait(lock, [this]()
                 { return !_running.load(); });
    }

    ensureJoined();
}

bool Thread::join_for(util::chrono::Delay timeout)
{
    if (!_started.load() || _joined.load())
    {
        return true;
    }

    {
        std::unique_lock lock(_mutex);
        const auto deadline = std::chrono::steady_clock::now() + timeout.toNanoseconds();
        if (!_cv.wait_until(lock, deadline, [this]()
                            { return !_running.load(); }))
        {
            return false;
        }
    }

    ensureJoined();
    return true;
}

void Thread::body()
{
    if (_fn)
    {
        _fn();
    }
}

void Thread::runEntry()
{
    if (!_name.empty())
    {
        setCurrentThreadName(_name.c_str());
    }

    body();
    completeRun();
}

void Thread::completeRun()
{
    {
        std::lock_guard lock(_mutex);
        _running.store(false);
    }
    _cv.notify_all();
}

void Thread::ensureJoined()
{
    if (_joined.exchange(true))
    {
        return;
    }

    if (_handle == nullptr)
    {
        return;
    }

    const DWORD waitResult = WaitForSingleObject(static_cast<HANDLE>(_handle), INFINITE);
    if (waitResult != WAIT_OBJECT_0)
    {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "WaitForSingleObject");
    }
}

int Thread::mapPosixPriority(int posixPriority) const
{
    if (posixPriority <= 10)
    {
        return THREAD_PRIORITY_TIME_CRITICAL;
    }
    if (posixPriority <= 25)
    {
        return THREAD_PRIORITY_HIGHEST;
    }
    if (posixPriority <= 40)
    {
        return THREAD_PRIORITY_ABOVE_NORMAL;
    }
    if (posixPriority <= 55)
    {
        return THREAD_PRIORITY_NORMAL;
    }
    if (posixPriority <= 70)
    {
        return THREAD_PRIORITY_BELOW_NORMAL;
    }
    if (posixPriority <= 85)
    {
        return THREAD_PRIORITY_LOWEST;
    }
    return THREAD_PRIORITY_IDLE;
}

} // namespace tools::os::thread
