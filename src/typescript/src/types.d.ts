// logging
declare const enum LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5
}

declare function log(level: LogLevel, message: string): void;

// process
declare interface NativeProcessFinishedEvent {
    type: "finished";
    pid: number;
    exitCode: number;
    error?: string;
}

declare interface NativeProcessStdoutEvent {
    type: "stdout";
    pid: number;
    data?: ArrayBuffer;
    end: boolean;
}

declare interface NativeProcessStderrEvent {
    type: "stderr";
    pid: number;
    data?: ArrayBuffer;
    end: boolean;
}

declare interface NativeSynchronousProcessResult {
    exitCode: number;
    error?: string;
    stdout?: ArrayBuffer;
    stderr?: ArrayBuffer;
}

declare function setProcessHandler(handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void): void;
declare function startProcess(arguments: string[], stdin: ArrayBuffer | boolean, stdout: boolean, stderr: boolean): number;
declare function execProcess(arguments: string[], stdin: ArrayBuffer | boolean, stdout: boolean, stderr: boolean): NativeSynchronousProcessResult;
declare function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
declare function closeProcessStdin(id: number): void;
