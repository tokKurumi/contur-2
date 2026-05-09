/// @file native_engine.cpp
/// @brief NativeEngine — Windows x86 host-process execution engine.

#include "contur/execution/native_engine.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "contur/core/clock.h"

#include "contur/arch/interrupt.h"
#include "contur/arch/register_file.h"
#include "contur/process/process_image.h"
#include "contur/tracing/i_tracer.h"
#include "contur/tracing/trace_event.h"
#include "contur/tracing/trace_level.h"
#include "contur/tracing/trace_scope.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <ctime>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace contur {

    namespace {

#if defined(_WIN32)
        /// @brief Converts a UTF-8 string to UTF-16 for Win32 API calls.
        ///
        /// Returns an empty wstring on conversion error or on empty input.
        [[nodiscard]] std::wstring toWide(std::string_view utf8)
        {
            if (utf8.empty())
            {
                return {};
            }
            const int needed =
                ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (needed <= 0)
            {
                return {};
            }
            std::wstring out(static_cast<std::size_t>(needed), L'\0');
            const int written =
                ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
            if (written <= 0)
            {
                return {};
            }
            return out;
        }
#endif

    } // namespace

    struct NativeEngine::Impl
    {
#if defined(_WIN32)
        struct Child
        {
            HANDLE process = nullptr;
            HANDLE mainThread = nullptr;
            HANDLE stdoutRead = nullptr;
            DWORD exitCode = 0;
            bool exited = false;
            bool resumedAtLeastOnce = false;
            std::string capturedStdout;
        };
#elif defined(__unix__) || defined(__APPLE__)
        struct Child
        {
            pid_t pid = -1;    // forked child PID, -1 when none
            int stdoutFd = -1; // read end of the stdout/stderr pipe (non-blocking)
            int exitCode = 0;
            bool exited = false;
            bool resumedAtLeastOnce = false;
            std::string capturedStdout;
        };
#else
        struct Child
        {
            // Placeholder for unsupported hosts; engine returns error on those.
            int exitCode = 0;
            bool exited = false;
            std::string capturedStdout;
        };
#endif

        std::reference_wrapper<ITracer> tracer;
        std::uint32_t tickQuantumMs;
        std::unordered_map<ProcessId, Child> children;
        std::unordered_set<ProcessId> haltRequested;
        mutable std::mutex mutex; // guards children + haltRequested

        Impl(ITracer &t, std::uint32_t quantumMs)
            : tracer(t)
            , tickQuantumMs(quantumMs == 0 ? 1u : quantumMs)
        {}

#if defined(_WIN32)
        /// @brief Closes any non-null handle and nulls it.
        static void closeHandleSafe(HANDLE &h) noexcept
        {
            if (h != nullptr && h != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(h);
                h = nullptr;
            }
        }

        /// @brief Releases all OS handles for a child entry.
        static void releaseChild(Child &c) noexcept
        {
            closeHandleSafe(c.mainThread);
            closeHandleSafe(c.process);
            closeHandleSafe(c.stdoutRead);
        }

        /// @brief Drains all bytes currently available on the stdout pipe.
        ///
        /// Non-blocking: only reads as much as `PeekNamedPipe` reports as available.
        /// Appends drained bytes to @p c.capturedStdout.
        static void drainStdout(Child &c)
        {
            if (c.stdoutRead == nullptr)
            {
                return;
            }
            for (;;)
            {
                DWORD available = 0;
                if (!::PeekNamedPipe(c.stdoutRead, nullptr, 0, nullptr, &available, nullptr))
                {
                    return;
                }
                if (available == 0)
                {
                    return;
                }
                char buffer[1024];
                const DWORD chunk = available > sizeof(buffer) ? static_cast<DWORD>(sizeof(buffer)) : available;
                DWORD read = 0;
                if (!::ReadFile(c.stdoutRead, buffer, chunk, &read, nullptr) || read == 0)
                {
                    return;
                }
                c.capturedStdout.append(buffer, buffer + read);
            }
        }

        /// @brief Spawns @p path as a suspended child process with stdout redirected.
        ///
        /// On success populates @p out with the new handles; on failure returns false
        /// and leaves @p out untouched.
        bool spawn(std::string_view path, Child &out)
        {
            std::wstring widePath = toWide(path);
            if (widePath.empty())
            {
                return false;
            }
            // CreateProcessW requires a writable command-line buffer; copy widePath into one.
            // We pass lpApplicationName so lpCommandLine just needs to be the program path
            // (cmd.exe accepts either form).
            std::wstring cmdLine = widePath;

            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = nullptr;

            HANDLE pipeRead = nullptr;
            HANDLE pipeWrite = nullptr;
            if (!::CreatePipe(&pipeRead, &pipeWrite, &sa, 0))
            {
                return false;
            }
            // Make sure the read end stays in the parent.
            ::SetHandleInformation(pipeRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = pipeWrite;
            si.hStdError = pipeWrite;

            PROCESS_INFORMATION pi{};
            const BOOL ok = ::CreateProcessW(
                widePath.c_str(),
                cmdLine.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_SUSPENDED | CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &si,
                &pi
            );
            // The child inherited the write end; we must close it in the parent so the
            // pipe reaches EOF when the child exits.
            ::CloseHandle(pipeWrite);

            if (!ok)
            {
                ::CloseHandle(pipeRead);
                return false;
            }

            out.process = pi.hProcess;
            out.mainThread = pi.hThread;
            out.stdoutRead = pipeRead;
            return true;
        }

        /// @brief Forcibly terminates a still-running child and reaps its handles.
        static void terminateAndReap(Child &c) noexcept
        {
            if (c.process != nullptr && !c.exited)
            {
                ::TerminateProcess(c.process, static_cast<UINT>(STATUS_CONTROL_C_EXIT));
                ::WaitForSingleObject(c.process, 250);
                DWORD code = 0;
                if (::GetExitCodeProcess(c.process, &code))
                {
                    c.exitCode = code;
                }
                c.exited = true;
            }
            drainStdout(c);
            releaseChild(c);
        }
#elif defined(__unix__) || defined(__APPLE__)
        /// @brief Best-effort sleep for the requested wallclock budget.
        ///
        /// Restarts on EINTR so that asynchronous SIGCHLD or other signals don't cut
        /// the slice short.
        static void sleepMs(std::uint32_t ms) noexcept
        {
            if (ms == 0)
            {
                return;
            }
            struct timespec ts;
            ts.tv_sec = static_cast<time_t>(ms / 1000u);
            ts.tv_nsec = static_cast<long>((ms % 1000u) * 1'000'000L);
            while (::nanosleep(&ts, &ts) != 0 && errno == EINTR)
            {
                // continue sleeping the remainder
            }
        }

        /// @brief Drains all currently available bytes from the child's stdout pipe.
        ///
        /// Non-blocking: relies on `O_NONBLOCK` set on the read end at spawn time.
        /// Reads in 1 KiB chunks until `read` returns 0 (EOF) or -1 with EAGAIN.
        static void drainStdout(Child &c)
        {
            if (c.stdoutFd < 0)
            {
                return;
            }
            char buffer[1024];
            for (;;)
            {
                const ssize_t n = ::read(c.stdoutFd, buffer, sizeof(buffer));
                if (n > 0)
                {
                    c.capturedStdout.append(buffer, buffer + static_cast<std::size_t>(n));
                    continue;
                }
                // n == 0 → child closed the write end (likely exited)
                // n  < 0 with EAGAIN/EWOULDBLOCK → no data right now
                return;
            }
        }

        /// @brief Polls the child via `waitpid(WNOHANG)` and updates state on exit.
        /// @return true when the child has exited (either now or previously).
        static bool pollExitNoHang(Child &c)
        {
            if (c.exited)
            {
                return true;
            }
            int status = 0;
            const pid_t r = ::waitpid(c.pid, &status, WNOHANG);
            if (r == c.pid)
            {
                if (WIFEXITED(status))
                {
                    c.exitCode = WEXITSTATUS(status);
                }
                else if (WIFSIGNALED(status))
                {
                    c.exitCode = 128 + WTERMSIG(status);
                }
                else
                {
                    // Stopped/continued — not a terminal state; report not-exited.
                    return false;
                }
                c.exited = true;
                return true;
            }
            if (r < 0 && errno == ECHILD)
            {
                // No such child — already reaped or never existed in this process group.
                c.exited = true;
                return true;
            }
            return false;
        }

        /// @brief Spawns @p path as a child process suspended via SIGSTOP.
        ///
        /// Algorithm:
        /// 1. Create a `pipe()` with the read end set to `O_NONBLOCK | FD_CLOEXEC`.
        /// 2. `fork()`. In the child: `dup2` the write end onto stdout/stderr,
        ///    close both pipe FDs, then `execv` the target path (path-only argv).
        /// 3. In the parent: close the write end and immediately `kill(pid, SIGSTOP)`
        ///    so the child is stopped before the dispatcher hands it any wallclock.
        ///
        /// There is a small race window: the child may start executing user code
        /// between exec and SIGSTOP. For an educational simulator this is acceptable —
        /// `execute()` polls for early exit and surfaces it through the normal
        /// "process exited" path.
        bool spawn(std::string_view path, Child &out)
        {
            // Pre-allocate the NUL-terminated path buffer in the *parent* — heap
            // allocation in the child after fork() is not async-signal-safe.
            std::string pathStr(path);

            int pipeFd[2] = {-1, -1};
            if (::pipe(pipeFd) != 0)
            {
                return false;
            }

            // Configure the read end for non-blocking drains and CLOEXEC.
            const int flags = ::fcntl(pipeFd[0], F_GETFL, 0);
            if (flags >= 0)
            {
                (void)::fcntl(pipeFd[0], F_SETFL, flags | O_NONBLOCK);
            }
            const int fdFlags = ::fcntl(pipeFd[0], F_GETFD, 0);
            if (fdFlags >= 0)
            {
                (void)::fcntl(pipeFd[0], F_SETFD, fdFlags | FD_CLOEXEC);
            }

            const pid_t childPid = ::fork();
            if (childPid < 0)
            {
                ::close(pipeFd[0]);
                ::close(pipeFd[1]);
                return false;
            }

            if (childPid == 0)
            {
                // ---- child ----
                // Only async-signal-safe calls past this point. We read pathStr's
                // buffer (parent-allocated) but never mutate it.
                ::close(pipeFd[0]);
                if (::dup2(pipeFd[1], STDOUT_FILENO) < 0)
                {
                    ::_exit(127);
                }
                if (::dup2(pipeFd[1], STDERR_FILENO) < 0)
                {
                    ::_exit(127);
                }
                ::close(pipeFd[1]);

                char *argv[] = {const_cast<char *>(pathStr.c_str()), nullptr};
                ::execv(argv[0], argv);
                ::_exit(127); // exec failed
            }

            // ---- parent ----
            ::close(pipeFd[1]);

            // Stop the child immediately so the dispatcher fully owns its wallclock.
            // If the child has already exited (very fast binary, ESRCH) the next
            // execute() call will reap it through pollExitNoHang.
            (void)::kill(childPid, SIGSTOP);

            out.pid = childPid;
            out.stdoutFd = pipeFd[0];
            out.exitCode = 0;
            out.exited = false;
            out.resumedAtLeastOnce = false;
            return true;
        }

        /// @brief Forcibly terminates a still-running child and reaps the zombie.
        static void terminateAndReap(Child &c) noexcept
        {
            if (c.pid > 0 && !c.exited)
            {
                (void)::kill(c.pid, SIGKILL);
                int status = 0;
                // Blocking wait so we don't leave a zombie on the system.
                const pid_t r = ::waitpid(c.pid, &status, 0);
                if (r == c.pid)
                {
                    if (WIFEXITED(status))
                    {
                        c.exitCode = WEXITSTATUS(status);
                    }
                    else if (WIFSIGNALED(status))
                    {
                        c.exitCode = 128 + WTERMSIG(status);
                    }
                }
                c.exited = true;
            }
            drainStdout(c);
            if (c.stdoutFd >= 0)
            {
                ::close(c.stdoutFd);
                c.stdoutFd = -1;
            }
        }
#endif // _WIN32 / __unix__
    };

    NativeEngine::NativeEngine(ITracer &tracer, std::uint32_t tickQuantumMs)
        : impl_(std::make_unique<Impl>(tracer, tickQuantumMs))
    {}

    NativeEngine::~NativeEngine()
    {
#if defined(_WIN32) || defined(__unix__) || defined(__APPLE__)
        if (!impl_)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(impl_->mutex);
        for (auto &kv : impl_->children)
        {
            Impl::terminateAndReap(kv.second);
        }
        impl_->children.clear();
#endif
    }

    NativeEngine::NativeEngine(NativeEngine &&) noexcept = default;
    NativeEngine &NativeEngine::operator=(NativeEngine &&) noexcept = default;

#if defined(_WIN32)

    ExecutionResult NativeEngine::execute(ProcessImage &process, std::size_t tickBudget)
    {
        const ProcessId pid = process.id();

        CONTUR_TRACE_SCOPE(impl_->tracer.get(), "NativeEngine", "execute");

        // Honour a previously requested halt before doing any work.
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->haltRequested.count(pid) > 0)
            {
                impl_->haltRequested.erase(pid);
                auto it = impl_->children.find(pid);
                if (it != impl_->children.end())
                {
                    Impl::terminateAndReap(it->second);
                    impl_->children.erase(it);
                }
                CONTUR_TRACE(
                    impl_->tracer.get(), "NativeEngine", "halt.honor", std::string("pid=") + std::to_string(pid)
                );
                return ExecutionResult::halted(pid, 0);
            }
        }

        // First call for this PID — spawn a suspended child.
        Impl::Child *childPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            auto it = impl_->children.find(pid);
            if (it == impl_->children.end())
            {
                if (process.nativePath().empty())
                {
                    CONTUR_TRACE_L(
                        impl_->tracer.get(), TraceLevel::Error, "NativeEngine", "spawn.error", "missing nativePath"
                    );
                    return ExecutionResult::error(pid, 0, Interrupt::Error);
                }
                Impl::Child fresh;
                if (!impl_->spawn(process.nativePath(), fresh))
                {
                    CONTUR_TRACE_L(
                        impl_->tracer.get(),
                        TraceLevel::Error,
                        "NativeEngine",
                        "spawn.error",
                        std::string("CreateProcess failed: ") + std::string(process.nativePath())
                    );
                    return ExecutionResult::error(pid, 0, Interrupt::Error);
                }
                childPtr = &impl_->children.emplace(pid, std::move(fresh)).first->second;
                CONTUR_TRACE(
                    impl_->tracer.get(),
                    "NativeEngine",
                    "spawn.ok",
                    std::string("pid=") + std::to_string(pid) + " path=" + std::string(process.nativePath())
                );
            }
            else
            {
                childPtr = &it->second;
            }
        }

        Impl::Child &child = *childPtr;

        // Already-exited entry: report once and let the kernel reap us.
        if (child.exited)
        {
            // Move exit code into R0 so the kernel/user can observe it.
            process.registers().set(Register::R0, static_cast<RegisterValue>(child.exitCode));
            CONTUR_TRACE(
                impl_->tracer.get(),
                "NativeEngine",
                "exit.report",
                std::string("pid=") + std::to_string(pid) + " code=" + std::to_string(child.exitCode)
            );
            return ExecutionResult::exited(pid, 0);
        }

        // Compute the wallclock slice for this dispatch.
        const DWORD sliceMs = static_cast<DWORD>((tickBudget == 0 ? 1u : tickBudget) * impl_->tickQuantumMs);

        // Resume the primary thread for the slice.
        const DWORD prevSuspendCount = ::ResumeThread(child.mainThread);
        if (prevSuspendCount == static_cast<DWORD>(-1))
        {
            CONTUR_TRACE_L(impl_->tracer.get(), TraceLevel::Error, "NativeEngine", "resume.error", std::to_string(pid));
            return ExecutionResult::error(pid, 0, Interrupt::Error);
        }
        child.resumedAtLeastOnce = true;
        CONTUR_TRACE(
            impl_->tracer.get(),
            "NativeEngine",
            "resume",
            std::string("pid=") + std::to_string(pid) + " slice_ms=" + std::to_string(sliceMs)
        );

        // Wait for the slice or the child to exit, whichever happens first.
        DWORD waitResult = ::WaitForSingleObject(child.process, sliceMs);

        // Whatever the wait result, ensure the child is suspended before we continue
        // (so it doesn't keep running while the dispatcher does other work). This is
        // racy: the thread can terminate between WaitForSingleObject returning a
        // timeout and our SuspendThread call. Failure here is therefore not fatal —
        // we re-check process state below and let exit detection handle it.
        if (waitResult != WAIT_OBJECT_0)
        {
            (void)::SuspendThread(child.mainThread);
            // Re-poll the process: if it exited during the suspend race window,
            // upgrade the wait result so the exit branch handles it.
            if (::WaitForSingleObject(child.process, 0) == WAIT_OBJECT_0)
            {
                waitResult = WAIT_OBJECT_0;
            }
        }

        // Drain any stdout produced during this slice.
        Impl::drainStdout(child);

        if (waitResult == WAIT_OBJECT_0)
        {
            DWORD code = 0;
            if (::GetExitCodeProcess(child.process, &code))
            {
                child.exitCode = code;
            }
            child.exited = true;
            // Do a final drain in case the child wrote at the very end.
            Impl::drainStdout(child);
            process.registers().set(Register::R0, static_cast<RegisterValue>(child.exitCode));
            CONTUR_TRACE(
                impl_->tracer.get(),
                "NativeEngine",
                "exit",
                std::string("pid=") + std::to_string(pid) + " code=" + std::to_string(child.exitCode)
            );
            return ExecutionResult::exited(pid, tickBudget);
        }

        CONTUR_TRACE_L(
            impl_->tracer.get(), TraceLevel::Debug, "NativeEngine", "suspend", std::string("pid=") + std::to_string(pid)
        );
        return ExecutionResult::budgetExhausted(pid, tickBudget);
    }

    void NativeEngine::halt(ProcessId pid)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->haltRequested.insert(pid);
        // If a child is currently tracked, terminate eagerly so callers don't have to
        // run another execute() cycle just to reap.
        auto it = impl_->children.find(pid);
        if (it != impl_->children.end())
        {
            Impl::terminateAndReap(it->second);
            // Keep the entry so a subsequent execute() can still report exit status,
            // but mark it exited.
            it->second.exited = true;
        }
    }

    std::string_view NativeEngine::name() const noexcept
    {
        return "Native";
    }

    std::string NativeEngine::capturedStdout(ProcessId pid) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        auto it = impl_->children.find(pid);
        if (it == impl_->children.end())
        {
            return {};
        }
        return it->second.capturedStdout;
    }

    bool NativeEngine::isTracking(ProcessId pid) const noexcept
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->children.find(pid) != impl_->children.end();
    }

#elif defined(__unix__) || defined(__APPLE__)

    // POSIX implementation: fork/exec the native binary, drive it via SIGCONT/SIGSTOP
    // for each dispatch slice, and reap with waitpid. Mirrors the Windows lifecycle
    // contract so the dispatcher can drive interpreted and native processes through
    // exactly the same `IExecutionEngine` interface.

    ExecutionResult NativeEngine::execute(ProcessImage &process, std::size_t tickBudget)
    {
        const ProcessId pid = process.id();

        CONTUR_TRACE_SCOPE(impl_->tracer.get(), "NativeEngine", "execute");

        // Honour a previously requested halt before doing any work.
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->haltRequested.count(pid) > 0)
            {
                impl_->haltRequested.erase(pid);
                auto it = impl_->children.find(pid);
                if (it != impl_->children.end())
                {
                    Impl::terminateAndReap(it->second);
                    impl_->children.erase(it);
                }
                CONTUR_TRACE(
                    impl_->tracer.get(), "NativeEngine", "halt.honor", std::string("pid=") + std::to_string(pid)
                );
                return ExecutionResult::halted(pid, 0);
            }
        }

        // Get-or-spawn the host child for this PID.
        Impl::Child *childPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            auto it = impl_->children.find(pid);
            if (it == impl_->children.end())
            {
                if (process.nativePath().empty())
                {
                    CONTUR_TRACE_L(
                        impl_->tracer.get(), TraceLevel::Error, "NativeEngine", "spawn.error", "missing nativePath"
                    );
                    return ExecutionResult::error(pid, 0, Interrupt::Error);
                }
                Impl::Child fresh;
                if (!impl_->spawn(process.nativePath(), fresh))
                {
                    CONTUR_TRACE_L(
                        impl_->tracer.get(),
                        TraceLevel::Error,
                        "NativeEngine",
                        "spawn.error",
                        std::string("fork/exec failed: ") + std::string(process.nativePath())
                    );
                    return ExecutionResult::error(pid, 0, Interrupt::Error);
                }
                childPtr = &impl_->children.emplace(pid, std::move(fresh)).first->second;
                CONTUR_TRACE(
                    impl_->tracer.get(),
                    "NativeEngine",
                    "spawn.ok",
                    std::string("pid=") + std::to_string(pid) + " path=" + std::string(process.nativePath())
                );
            }
            else
            {
                childPtr = &it->second;
            }
        }

        Impl::Child &child = *childPtr;

        // Already-exited entry: surface the captured exit code and let the kernel reap.
        if (child.exited)
        {
            process.registers().set(Register::R0, static_cast<RegisterValue>(child.exitCode));
            CONTUR_TRACE(
                impl_->tracer.get(),
                "NativeEngine",
                "exit.report",
                std::string("pid=") + std::to_string(pid) + " code=" + std::to_string(child.exitCode)
            );
            return ExecutionResult::exited(pid, 0);
        }

        // Resume the child for this dispatch slice.
        if (::kill(child.pid, SIGCONT) != 0)
        {
            // Most likely the child already exited between the previous suspend and now.
            if (Impl::pollExitNoHang(child))
            {
                Impl::drainStdout(child);
                process.registers().set(Register::R0, static_cast<RegisterValue>(child.exitCode));
                CONTUR_TRACE(
                    impl_->tracer.get(),
                    "NativeEngine",
                    "exit",
                    std::string("pid=") + std::to_string(pid) + " code=" + std::to_string(child.exitCode)
                );
                return ExecutionResult::exited(pid, tickBudget);
            }
            CONTUR_TRACE_L(impl_->tracer.get(), TraceLevel::Error, "NativeEngine", "resume.error", std::to_string(pid));
            return ExecutionResult::error(pid, 0, Interrupt::Error);
        }
        child.resumedAtLeastOnce = true;

        const std::uint32_t sliceMs =
            static_cast<std::uint32_t>((tickBudget == 0 ? 1u : tickBudget) * impl_->tickQuantumMs);
        CONTUR_TRACE(
            impl_->tracer.get(),
            "NativeEngine",
            "resume",
            std::string("pid=") + std::to_string(pid) + " slice_ms=" + std::to_string(sliceMs)
        );

        // Run the slice. We don't have a wait-on-process-with-timeout primitive on
        // POSIX as cleanly as Win32's WaitForSingleObject, so we sleep the slice and
        // then check for early exit. The cost is at most one slice of "wasted" wait
        // when the child has finished mid-slice — acceptable for tickQuantumMs ~ms.
        Impl::sleepMs(sliceMs);

        // Suspend (best effort — child may have already exited).
        (void)::kill(child.pid, SIGSTOP);

        // Drain any stdout produced during this slice.
        Impl::drainStdout(child);

        // Did the child exit during this slice?
        if (Impl::pollExitNoHang(child))
        {
            Impl::drainStdout(child);
            process.registers().set(Register::R0, static_cast<RegisterValue>(child.exitCode));
            CONTUR_TRACE(
                impl_->tracer.get(),
                "NativeEngine",
                "exit",
                std::string("pid=") + std::to_string(pid) + " code=" + std::to_string(child.exitCode)
            );
            return ExecutionResult::exited(pid, tickBudget);
        }

        CONTUR_TRACE_L(
            impl_->tracer.get(), TraceLevel::Debug, "NativeEngine", "suspend", std::string("pid=") + std::to_string(pid)
        );
        return ExecutionResult::budgetExhausted(pid, tickBudget);
    }

    void NativeEngine::halt(ProcessId pid)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->haltRequested.insert(pid);
        auto it = impl_->children.find(pid);
        if (it != impl_->children.end())
        {
            Impl::terminateAndReap(it->second);
            it->second.exited = true;
        }
    }

    std::string_view NativeEngine::name() const noexcept
    {
        return "Native";
    }

    std::string NativeEngine::capturedStdout(ProcessId pid) const
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        auto it = impl_->children.find(pid);
        if (it == impl_->children.end())
        {
            return {};
        }
        return it->second.capturedStdout;
    }

    bool NativeEngine::isTracking(ProcessId pid) const noexcept
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        return impl_->children.find(pid) != impl_->children.end();
    }

#else // unsupported host (neither _WIN32 nor __unix__)

    // Truly exotic targets: keep the symbol set so the project still links.
    // execute() always reports an error so misconfiguration is loud.

    ExecutionResult NativeEngine::execute(ProcessImage &process, std::size_t /*tickBudget*/)
    {
        return ExecutionResult::error(process.id(), 0, Interrupt::Error);
    }

    void NativeEngine::halt(ProcessId pid)
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->haltRequested.insert(pid);
    }

    std::string_view NativeEngine::name() const noexcept
    {
        return "Native";
    }

    std::string NativeEngine::capturedStdout(ProcessId /*pid*/) const
    {
        return {};
    }

    bool NativeEngine::isTracking(ProcessId /*pid*/) const noexcept
    {
        return false;
    }

#endif // _WIN32 / __unix__ / fallback

} // namespace contur
