#include "ScriptEngine.h"
#include <cassert>
#include <Logger.h>
#include <ThreadPool.h>
#include <EventLoopUv.h>
#include <unistd.h>

namespace spurv {
ScriptValue ScriptEngine::setProcessHandler(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    mProcessHandler = std::move(args[0]);
    return {};
}

// static
inline void ScriptEngine::sendOutputEvent(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf, OutputType type)
{
    ProcessData *pd = static_cast<ProcessData *>(stream->data);
    if (pd->synchronousResponse) {
        if (nread > 0) {
            if (type == OutputType::Stdout) {
                pd->synchronousResponse->stdout.append(buf->base, nread);
            } else {
                pd->synchronousResponse->stderr.append(buf->base, nread);
            }
        }
    } else {
        ScriptEngine *engine = ScriptEngine::scriptEngine();
        ScriptValue object(ScriptValue::Object);
        object.setProperty(engine->mAtoms.type, ScriptValue(type == OutputType::Stdout ? "stdout" : "stderr"));
        object.setProperty(engine->mAtoms.pid, ScriptValue(pd->proc->pid));
        if (nread == UV_EOF) {
            object.setProperty(engine->mAtoms.end, ScriptValue(true));
        } else {
            object.setProperty(engine->mAtoms.end, ScriptValue(false));
            if (pd->flags & ProcessFlag::Strings) {
                object.setProperty(engine->mAtoms.data, ScriptValue(buf->base, nread));
            } else {
                object.setProperty(engine->mAtoms.data, ScriptValue::makeArrayBuffer(buf->base, nread));
            }
        }
        spdlog::error("read callback {} {} {:#04x}", nread, buf->len, pd->flags);
        engine->mProcessHandler.call(std::move(object));
    }
}

std::variant<ScriptValue, ScriptEngine::ProcessParameters> ScriptEngine::prepareProcess(std::vector<ScriptValue> &&args)
{
    if (args.size() < 5 || !args[0].isArray() || !args[4].isNumber()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    ProcessParameters params = {};

    params.flags = static_cast<ProcessFlag>(*args[4].toInt());

    if (!args[1].isNullOrUndefined()) {
        if (!args[1].isObject()) {
            return ScriptValue::makeError("Invalid env argument");
        }
        auto vec = *args[1].toObject();
        std::vector<std::pair<std::string, std::string>> env;
        env.reserve(vec.size());
        for (std::pair<std::string, ScriptValue> &arg : vec) {
            auto str = arg.second.toString();
            if (!str.ok()) {
                return ScriptValue::makeError("Couldn't convert env argument with key " + arg.first);
            }
            env.emplace_back(std::move(arg.first), *std::move(str));
        }
        params.env = std::move(env);
    }

    if (!args[2].isNullOrUndefined()) {
        auto cwdArg = args[2].toString();
        if (cwdArg.ok()) {
            params.cwd = *std::move(cwdArg);
        } else {
            return ScriptValue::makeError("Invalid cwd argument");
        }
    }

    if (args[3].isArrayBuffer() || args[3].isTypedArray()) {
        auto arrayBufferData = *args[2].arrayBufferData(); // ### can this fail?
        std::string pendingStdin(arrayBufferData.second, ' ');
        memcpy(pendingStdin.data(), arrayBufferData.first, arrayBufferData.second);
        params.stdinWrite = std::move(pendingStdin);
    } else if (!args[3].isNullOrUndefined()) {
        auto str = args[3].toString();
        if (!str.ok()) {
            return ScriptValue::makeError("Invalid stdin argument");
        }
        params.stdinWrite = *std::move(str);
    }

    const std::vector<ScriptValue> argv = *args[0].toArray();

    params.arguments.resize(argv.size());
    for (size_t i=0; i<argv.size(); ++i) {
        auto str = argv[i].toString();
        if (!str.ok()) {
            return ScriptValue::makeError("Invalid arguments");
        }
        if (!i) {
            params.executable = findExecutable(*std::move(str));
            params.arguments[i] = params.executable;
        } else {
            params.arguments[i] = std::move(*std::move(str));
        }
    }
    return params;
}

ScriptValue ScriptEngine::startProcess(std::vector<ScriptValue> &&args)
{
    std::variant<ScriptValue, ProcessParameters> params = prepareProcess(std::move(args));
    if (std::holds_alternative<ScriptValue>(params)) {
        return std::get<ScriptValue>(std::move(params));
    }

    uv_loop_t *loop = static_cast<uv_loop_t *>(mEventLoop->handle());
    std::unique_ptr<ProcessData> data = runProcess(loop, std::get<ProcessParameters>(std::move(params)));
    if (!data->spawnError.empty()) {
        return ScriptValue::makeError(data->spawnError);
    }
    const int pid = data->proc->pid;
    {
        std::lock_guard<std::mutex> lock(mProcessesMutex);
        mProcesses.push_back(std::move(data));
    }
    return ScriptValue(pid);
}

// static_cast
std::unique_ptr<ScriptEngine::ProcessData> ScriptEngine::runProcess(uv_loop_t *loop, ProcessParameters &&params)
{
    std::unique_ptr<ProcessData> data = std::make_unique<ProcessData>();
    memset(&data->options, 0, sizeof(data->options));

    data->flags = params.flags;

    data->options.stdio = data->child_stdio;
    data->options.stdio_count = 3;

    if (data->flags & ProcessFlag::Synchronous) {
        data->synchronousResponse = ProcessData::SynchronousResponse();
    }

    if (data->flags & ProcessFlag::StdinClosed) {
        data->child_stdio[0].flags = UV_IGNORE;
    } else {
        // ### flag for blocking reads?
        data->child_stdio[0].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdinPipe = uv_pipe_t();
        uv_pipe_init(loop, &(*data->stdinPipe), 1);
        data->child_stdio[0].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stdinPipe));
        data->child_stdio[0].data.stream->data = data.get();
    }

    if (!(data->flags & ProcessFlag::Stdout)) {
        data->child_stdio[1].flags = UV_IGNORE;
    } else {
        data->child_stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdoutPipe = uv_pipe_t();
        uv_pipe_init(loop, &(*data->stdoutPipe), 1);
        // need to open
        data->child_stdio[1].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stdoutPipe));
        data->child_stdio[1].data.stream->data = data.get();
    }

    if (!(data->flags & ProcessFlag::Stderr)) {
        data->child_stdio[2].flags = UV_IGNORE;
    } else {
        data->child_stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stderrPipe = uv_pipe_t();;
        uv_pipe_init(loop, &(*data->stderrPipe), 1);
        data->child_stdio[2].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stderrPipe));
        data->child_stdio[2].data.stream->data = data.get();
    }

    if (params.env) {
        data->options.env = new char*[params.env->size() + 1];
        data->options.env[params.env->size()] = nullptr;
        size_t i = 0;
        for (std::pair<std::string, std::string> &arg : *params.env) {
            data->options.env[i++] = strdup(fmt::format("{}={}", arg.first, arg.second).c_str());
        }
    }

    if (params.cwd) {
        data->options.cwd = params.cwd->c_str();
    }

    if (params.stdinWrite) {
        data->stdinWrites.emplace_back(std::make_unique<ProcessData::Write>(*std::move(params.stdinWrite)));
        data->stdinWrites.back()->req.data = data.get();
    }
    data->executable = std::move(params.executable);
    data->options.args = new char*[params.arguments.size() + 1];
    data->options.args[params.arguments.size()] = nullptr;
    memset(data->options.args, 0, sizeof(char *) * params.arguments.size() + 1);

    for (size_t i=0; i<params.arguments.size(); ++i) {
        data->options.args[i] = strdup(params.arguments[i].c_str());
    }
    data->options.file = data->executable.c_str();
    data->options.exit_cb = ScriptEngine::onProcessExit;

    data->proc = uv_process_t();
    data->proc->data = data.get();
    const int ret = uv_spawn(loop, &(*data->proc), &data->options);
    if (ret) {
        // failed to launch
        const char *err = uv_strerror(ret);
        assert(err);
        data->spawnError = fmt::format("Failed to launch {}: {}", data->options.file, err);
        return data;
    }
    if (data->flags & ProcessFlag::Stdout) {
        uv_read_start(data->child_stdio[1].data.stream, [](uv_handle_t *stream, size_t suggestedSize, uv_buf_t *buf) {
            ProcessData *data = static_cast<ProcessData *>(stream->data);
            assert(data);
            if (!data->stdoutBuf) {
                data->stdoutBuf = new char[suggestedSize];
                data->stdoutBufSize = suggestedSize;
            }
            buf->base = data->stdoutBuf;
            buf->len = data->stdoutBufSize;
        }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            ScriptEngine::sendOutputEvent(stream, nread, buf, OutputType::Stdout);
        });
    }

    if (data->flags & ProcessFlag::Stderr) {
        uv_read_start(data->child_stdio[2].data.stream, [](uv_handle_t *stream, size_t suggestedSize, uv_buf_t *buf) {
            ProcessData *data = static_cast<ProcessData *>(stream->data);
            assert(data);
            if (!data->stderrBuf) {
                data->stderrBuf = new char[suggestedSize];
                data->stderrBufSize = suggestedSize;
            }
            buf->base = data->stderrBuf;
            buf->len = data->stderrBufSize;
        }, [](uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            ScriptEngine::sendOutputEvent(stream, nread, buf, OutputType::Stderr);
        });
    }

    if (!data->stdinWrites.empty()) {
        data->flags |= ProcessFlag::HadStdinFromOptions;
        auto &first = data->stdinWrites[0];
        const int ret = uv_write(&first->req, data->child_stdio[0].data.stream, &first->buffer, 1, &ScriptEngine::onWriteFinished);
        if (ret) {
            spdlog::error("Failed to queue write to process {} {}", data->executable, ret);
        }
    }

    return data;
}

// static
void ScriptEngine::onWriteFinished(uv_write_t *req, int status)
{
    ProcessData *data = static_cast<ProcessData *>(req->data);
    assert(data);
    if (status) {
        spdlog::error("Failed to write data to process {} {}", data->executable, status);
    }

    assert(!data->stdinWrites.empty());
    assert(&data->stdinWrites.front()->req == req);
    data->stdinWrites.erase(data->stdinWrites.begin());

    if (data->stdinWrites.empty()) {
        if (data->flags & (ProcessFlag::HadStdinFromOptions|ProcessFlag::StdinClosed)) {
            uv_close(reinterpret_cast<uv_handle_t*>(&*data->stdinPipe), nullptr);
            data->flags |= ProcessFlag::StdinCloseCalled;
        }
        return;
    }
    auto &first = data->stdinWrites[0];
    const int ret = uv_write(&first->req, data->child_stdio[0].data.stream, &first->buffer, 1, &ScriptEngine::onWriteFinished);
    if (ret) {
        spdlog::error("Failed to queue write to process {} {}", data->executable, ret);
    }
}

ScriptValue ScriptEngine::execProcess(std::vector<ScriptValue> &&args)
{
    std::variant<ScriptValue, ProcessParameters> params = prepareProcess(std::move(args));
    if (std::holds_alternative<ScriptValue>(params)) {
        return std::get<ScriptValue>(std::move(params));
    }

    ProcessParameters parameters = std::get<ProcessParameters>(std::move(params));
    parameters.flags |= ProcessFlag::Synchronous;
    const ProcessFlag flags = parameters.flags;

    ThreadPool *pool = ThreadPool::mainThreadPool();
    std::string spawnError;
    ProcessData::SynchronousResponse syncResponse;
    int pid = 0;
    std::future<int> ret = pool->post([&parameters, &spawnError, &syncResponse, &pid]() {
        EventLoopUv eventLoop;
        eventLoop.install();
        std::unique_ptr<ProcessData> data;
        eventLoop.post([&parameters, &spawnError, &pid, &eventLoop, &data]() {
            uv_loop_t *handle = static_cast<uv_loop_t *>(eventLoop.handle());
            data = runProcess(handle, std::move(parameters));
            if (!data->spawnError.empty()) {
                spawnError = std::move(data->spawnError);
            } else {
                pid = data->proc->pid;
            }
        });
        eventLoop.run();
        syncResponse = std::move(*data->synchronousResponse);
        eventLoop.uninstall();
        return pid;
    });
    ret.wait();
    const int processPid = ret.get();
    ScriptValue object(ScriptValue::Object);
    object.setProperty(mAtoms.exitCode, ScriptValue(syncResponse.exitStatus));
    object.setProperty(mAtoms.pid, ScriptValue(processPid));
    if (!syncResponse.stdout.empty()) {
        if (flags & ProcessFlag::Strings) {
            object.setProperty(mAtoms.stdout, ScriptValue(syncResponse.stdout));
        } else {
            object.setProperty(mAtoms.stdout,
                               ScriptValue::makeArrayBuffer(syncResponse.stdout.c_str(), syncResponse.stdout.size()));

        }
    }

    if (!syncResponse.stderr.empty()) {
        if (flags & ProcessFlag::Strings) {
            object.setProperty(mAtoms.stderr, ScriptValue(syncResponse.stderr));
        } else {
            object.setProperty(mAtoms.stderr,
                               ScriptValue::makeArrayBuffer(syncResponse.stderr.c_str(), syncResponse.stderr.size()));

        }
    }

    return object;
}

ScriptValue ScriptEngine::writeToProcessStdin(std::vector<ScriptValue> &&args)
{
    bool ok = false;
    std::string str;
    if (args.size() >= 2 && args[0].isInt()) {
        if (args[1].isArrayBuffer() || args[1].isTypedArray()) {
            auto data = args[1].arrayBufferData();
            if (data.ok()) {
                ok = true;
                str.append(reinterpret_cast<const char *>(data->first), data->second);
            }
        } else {
            auto string = args[1].toString();
            if (string.ok()) {
                ok = true;
                str = *std::move(string);
            }
        }
    }

    if (!ok) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto it = findProcessByPid(*args[0].toInt());
    if (it == mProcesses.end()) {
        return ScriptValue::makeError(fmt::format("Can't find process with pid {}", *args[0].toInt()));
    }

    const auto &data = *it;

    if (!data->stdinPipe || data->flags & ProcessFlag::StdinClosed) {
        return ScriptValue::makeError(fmt::format("stdin has been closed for {} {}",
                                                  data->executable, data->proc->pid));
    }

    if (!data->stdinWrites.empty()) {
        data->stdinWrites.push_back(std::make_unique<ProcessData::Write>(std::move(str)));
    } else {
        auto &first = data->stdinWrites[0];
        const int ret = uv_write(&first->req, data->child_stdio[0].data.stream, &first->buffer, 1, &ScriptEngine::onWriteFinished);
        if (ret) {
            spdlog::error("Failed to queue write to process {} {}", data->executable, ret);
        }
    }

    return {};
}

ScriptValue ScriptEngine::closeProcessStdin(std::vector<ScriptValue> &&args)
{
    bool ok = false;
    std::string str;
    if (args.size() >= 2 && args[0].isInt()) {
        if (args[1].isArrayBuffer() || args[1].isTypedArray()) {
            auto data = args[1].arrayBufferData();
            if (data.ok()) {
                ok = true;
                str.append(reinterpret_cast<const char *>(data->first), data->second);
            }
        } else {
            auto string = args[1].toString();
            if (string.ok()) {
                ok = true;
                str = *std::move(string);
            }
        }
    }

    if (!ok) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto it = findProcessByPid(*args[0].toInt());
    if (it == mProcesses.end()) {
        return ScriptValue::makeError(fmt::format("Can't find process with pid {}", *args[0].toInt()));
    }

    const auto &data = *it;
    data->flags |= ProcessFlag::StdinClosed;

    if (data->stdinPipe) {
        if (data->stdinWrites.empty()) {
            // can I just close it before the callbacks have happened?
            uv_close(reinterpret_cast<uv_handle_t*>(&*data->stdinPipe), nullptr);
            data->flags |= ProcessFlag::StdinCloseCalled;
        }
    }

    return {};
}

ScriptValue ScriptEngine::killProcess(std::vector<ScriptValue> &&args)
{
    if (args.size() < 2 || !args[0].isInt() || !args[1].isInt()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    auto it = findProcessByPid(*args[0].toInt());
    if (it == mProcesses.end()) {
        return ScriptValue::makeError("Can't find process");
    }


    uv_process_kill(&*(*it)->proc, *args[1].toInt());
    return {};
}

// static
std::filesystem::path ScriptEngine::findExecutable(std::filesystem::path exec)
{
    if (exec.is_absolute()) {
        return exec;
    }

    if (exec.string().find('/') != std::string::npos) {
        return std::filesystem::absolute(exec);
    }

    const char *path = getenv("PATH");
    if (path) {
        const char *last = path;
        while (true) {
            const char *colon = strchr(last, ':');
            int len = colon ? colon - last : strlen(last);
            char buf[PATH_MAX];
            int w;
            if (colon && colon[-1] == '/') {
                w = snprintf(buf, sizeof(buf), "%.*s%s", len, last, exec.string().c_str());
            } else {
                w = snprintf(buf, sizeof(buf), "%.*s/%s", len, last, exec.string().c_str());
            }
            if (!access(buf, S_IXUSR)) {
                return std::string(buf, w);
            }
            if (colon) {
                last = colon + 1;
            } else {
                break;
            }
        }
    }
    return exec;
}

std::vector<std::unique_ptr<ScriptEngine::ProcessData>>::iterator ScriptEngine::findProcessByPid(int pid)
{
    for (auto it = mProcesses.begin(); it != mProcesses.end(); ++it) {
        if ((*it)->proc->pid == pid) {
            return it;
        }
    }
    return mProcesses.end();
}

// static
void ScriptEngine::onProcessExit(uv_process_t *proc, int64_t exit_status, int term_signal)
{
    ProcessData *data = static_cast<ProcessData *>(proc->data);
    if (data->synchronousResponse) {
        data->synchronousResponse->exitStatus = static_cast<int>(exit_status);
        EventLoop::eventLoop()->stop(0);
        return;
    }

    ScriptEngine *that = ScriptEngine::scriptEngine();
    assert(that);
    for (auto it = that->mProcesses.begin(); it != that->mProcesses.end(); ++it) {
        if (&*(*it)->proc == proc) {
            if (that->mProcessHandler.isFunction()) {
                ScriptValue object(ScriptValue::Object);
                object.setProperty(that->mAtoms.type, ScriptValue("finished"));
                object.setProperty(that->mAtoms.pid, ScriptValue(proc->pid));
                assert(exit_status <= std::numeric_limits<int>::max() && exit_status >= std::numeric_limits<int>::min());
                // ### term_signal ?
                object.setProperty(that->mAtoms.exitCode, ScriptValue(static_cast<int>(exit_status)));
                that->mProcessHandler.call(object);
            }
            that->mProcesses.erase(it);
            return;
        }
    }
    // ### if this happens we've probably invalid'read'ed libuv already
    spdlog::error("Couldn't find process with pid {} for onProcessExit wiht exit_status {} and term_signal {}",
                  proc->pid, exit_status, term_signal);
    uv_close(reinterpret_cast<uv_handle_t *>(proc), nullptr);
}

ScriptEngine::ProcessData::~ProcessData()
{
    if (stdinPipe && !(flags & ProcessFlag::StdinCloseCalled)) { // ### could probably check uv_handle_t's flags for this
        uv_close(reinterpret_cast<uv_handle_t*>(&*stdinPipe), nullptr);
    }

    if (stdoutPipe) {
        uv_read_stop(reinterpret_cast<uv_stream_t *>(&*stdoutPipe));
        uv_close(reinterpret_cast<uv_handle_t*>(&*stdoutPipe), nullptr);
    }

    if (stderrPipe) {
        uv_read_stop(reinterpret_cast<uv_stream_t *>(&*stderrPipe));
        uv_close(reinterpret_cast<uv_handle_t*>(&*stderrPipe), nullptr);
    }

    if (proc) {
        uv_close(reinterpret_cast<uv_handle_t *>(&*proc), nullptr);
    }

    if (options.env) {
        for (size_t i=0; options.env[i]; ++i) {
            free(options.env[i]);
        }
        delete[] options.env;
    }

    if (options.args) {
        for (size_t i=0; options.args[i]; ++i) {
            free(options.args[i]);
        }
        delete[] options.args;
    }

    if (stdoutBuf) {
        delete[] stdoutBuf;
    }

    if (stderrBuf) {
        delete[] stderrBuf;
    }
}

ScriptEngine::ProcessData::Write::Write(std::string &&bytes)
    : data(std::move(bytes))
{
    memset(&req, 0, sizeof(req));
    buffer = uv_buf_init(data.data(), data.size());
}

} // namespace spurv

