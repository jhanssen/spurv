#include "ScriptEngine.h"
#include <cassert>
#include <Logger.h>

namespace spurv {
ScriptValue ScriptEngine::setProcessHandler(std::vector<ScriptValue> &&args)
{
    if (args.empty() || !args[0].isFunction()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    mProcessHandler = std::move(args[0]);
    return {};
}

namespace {
// Duplicated in typescript
enum class ProcessFlag {
    None = 0x0,
    StdinClosed = 0x1,
    Stdout = 0x2,
    Stderr = 0x4,
    Strings = 0x8
};
} // anonymous namespace

template <>
struct IsEnumBitmask<ProcessFlag> {
    static constexpr bool enable = true;
};

// static
inline void ScriptEngine::sendOutputEvent(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf, const char *type)
{
    ScriptEngine *engine = ScriptEngine::scriptEngine();
    ProcessData *pd = static_cast<ProcessData *>(stream->data);
    ScriptValue object(ScriptValue::Object);
    object.setProperty(engine->mAtoms.type, ScriptValue(type));
    object.setProperty(engine->mAtoms.pid, ScriptValue(pd->proc->pid));
    if (nread == UV_EOF) {
        object.setProperty(engine->mAtoms.end, ScriptValue(true));
    } else {
        object.setProperty(engine->mAtoms.end, ScriptValue(false));
        if (pd->stringReturnValues) {
            object.setProperty(engine->mAtoms.data, ScriptValue(buf->base, nread));
        } else {
            object.setProperty(engine->mAtoms.data, ScriptValue::makeArrayBuffer(buf->base, nread));
        }
    }
    engine->mProcessHandler.call(std::move(object));
    spdlog::error("read callback {} {}", nread, buf->len);
}

ScriptValue ScriptEngine::startProcess(std::vector<ScriptValue> &&args)
{
    if (args.size() < 5 || !args[0].isArray() || !args[4].isNumber()) {
        return ScriptValue::makeError("Invalid arguments");
    }

    uv_loop_t *loop = static_cast<uv_loop_t *>(mEventLoop->handle());

    std::unique_ptr<ProcessData> data = std::make_unique<ProcessData>();
    memset(&data->options, 0, sizeof(data->options));

    ProcessFlag flags = static_cast<ProcessFlag>(*args[4].toInt());
    if (flags & ProcessFlag::Strings) {
        data->stringReturnValues = true;
    }

    data->options.stdio = data->child_stdio;
    data->options.stdio_count = 3;

    if (flags & ProcessFlag::StdinClosed) {
        data->child_stdio[0].flags = UV_IGNORE;
    } else {
        // ### flag for blocking reads?
        data->child_stdio[0].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdinPipe = {};
        uv_pipe_init(loop, &(*data->stdinPipe), 1);
        data->child_stdio[0].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stdinPipe));
        data->child_stdio[0].data.stream->data = data.get();
    }

    if (!(flags & ProcessFlag::Stdout)) {
        data->child_stdio[1].flags = UV_IGNORE;
    } else {
        data->child_stdio[1].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stdoutPipe = {};
        uv_pipe_init(loop, &(*data->stdoutPipe), 1);
        // need to open
        data->child_stdio[1].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stdoutPipe));
        data->child_stdio[1].data.stream->data = data.get();
    }

    if (!(flags & ProcessFlag::Stderr)) {
        data->child_stdio[2].flags = UV_IGNORE;
    } else {
        data->child_stdio[2].flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE | UV_WRITABLE_PIPE | UV_NONBLOCK_PIPE);
        data->stderrPipe = {};
        uv_pipe_init(loop, &(*data->stderrPipe), 1);
        data->child_stdio[2].data.stream = reinterpret_cast<uv_stream_t *>(&(*data->stderrPipe));
        data->child_stdio[2].data.stream->data = data.get();
    }

    if (!args[1].isNullOrUndefined()) {
        if (!args[1].isObject()) {
            return ScriptValue::makeError("Invalid env argument");
        }
        auto vec = *args[1].toObject();
        data->options.env = new char*[vec.size() + 1];
        data->options.env[vec.size()] = nullptr;
        size_t i = 0;
        for (std::pair<std::string, ScriptValue> &arg : vec) {
            auto str = arg.second.toString();
            if (!str.ok()) {
                return ScriptValue::makeError("Couldn't convert env argument with key " + arg.first);
            }
            data->options.env[i++] = strdup(fmt::format("{}={}", arg.first, str->c_str()).c_str());
        }
    }

    std::string cwd;
    if (!args[2].isNullOrUndefined()) {
        auto cwdArg = args[2].toString();
        if (cwdArg.ok()) {
            cwd = *cwdArg;
            data->options.cwd = cwd.c_str();
        } else {
            return ScriptValue::makeError("Invalid cwd argument");
        }
    }

    if (args[2].isArrayBuffer() || args[3].isTypedArray()) {
        auto arrayBufferData = *args[2].arrayBufferData(); // ### can this fail?
        std::string pendingStdin(arrayBufferData.second, ' ');
        memcpy(pendingStdin.data(), arrayBufferData.first, arrayBufferData.second);
        data->stdinWrites.emplace_back(std::make_unique<ProcessData::Write>(std::move(pendingStdin)));
        data->stdinWrites.back()->req.data = data.get();
    } else if (!args[2].isNullOrUndefined()) {
        auto str = args[2].toString();
        if (!str.ok()) {
            return ScriptValue::makeError("Invalid stdin argument");
        }
        auto string = *str;
        data->stdinWrites.push_back(std::make_unique<ProcessData::Write>(std::move(string)));
        data->stdinWrites.back()->req.data = data.get();
    }

    const std::vector<ScriptValue> argv = *args[0].toArray();

    data->options.args = new char*[argv.size() + 1];
    data->options.args[argv.size()] = nullptr;
    memset(data->options.args, 0, sizeof(char *) * argv.size() + 1);

    for (size_t i=0; i<argv.size(); ++i) {
        auto str = argv[i].toString();
        if (!str.ok()) {
            return ScriptValue::makeError("Invalid arguments");
        }
        if (!i) {
            data->executable = findExecutable(*str);
            str = data->executable;
        }
        data->options.args[i] = strdup(str->c_str());
    }
    data->options.file = data->executable.c_str();
    data->options.exit_cb = ScriptEngine::onProcessExit;

    data->proc = {};
    const int ret = uv_spawn(loop, &(*data->proc), &data->options);
    if (ret) {
        // failed to launch
        const char *err = uv_strerror(ret);
        assert(err);
        std::string error = fmt::format("Failed to launch {}: {}", data->options.file, err);
        return ScriptValue(error);
    }
    const int pid = data->proc->pid;
    if (flags & ProcessFlag::Stdout) {
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
            ScriptEngine::sendOutputEvent(stream, nread, buf, "stdout");
        });
    }

    if (flags & ProcessFlag::Stderr) {
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
            ScriptEngine::sendOutputEvent(stream, nread, buf, "stderr");
        });
    }

    if (!data->stdinWrites.empty()) {
        data->hadStdinFromOptions = true;
        auto &first = data->stdinWrites[0];
        const int ret = uv_write(&first->req, data->child_stdio[0].data.stream, &first->buffer, 1, &ScriptEngine::onWriteFinished);
        if (ret) {
            spdlog::error("Failed to queue write to process {} {}", data->executable, ret);
        }
    }

    mProcesses.push_back(std::move(data));
    return ScriptValue(pid);
}

// static
void ScriptEngine::onWriteFinished(uv_write_t *req, int status)
{
    ProcessData *data = static_cast<ProcessData *>(req->data);
    assert(data);
    if (status) {
        spdlog::error("Failed to write data to process {} {}", data->executable, status);
    }
    for (auto it = data->stdinWrites.begin(); it != data->stdinWrites.end(); ++it) {
        if (&(*it)->req == req) {
            data->stdinWrites.erase(it);
            if (data->stdinWrites.empty() && data->hadStdinFromOptions) {
                // ### need to do this
                // closeProcessStdin(data);
            }
        }
    }
}

ScriptValue ScriptEngine::execProcess(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
    return {};
}

ScriptValue ScriptEngine::writeToProcessStdin(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
    return {};
}

ScriptValue ScriptEngine::closeProcessStdin(std::vector<ScriptValue> &&args)
{
    static_cast<void>(args);
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
    if (stdinPipe) {
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

