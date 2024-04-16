// logging
declare const enum LogLevel {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    err = 4,
    critical = 5
}

declare function log(level: LogLevel, message: string): void;

// process
declare interface ProcessParameters {
    arguments: string[];
    stdin?: ArrayBuffer | string | boolean;
    stdout?: boolean;
    stderr?: boolean;
}

declare interface ProcessFinishedEvent {
    type: "finished";
    pid: number;
    exitCode: number;
    error?: string;
}

declare interface ProcessStdOutEvent {
    type: "stdout";
    pid: number;
    data: ArrayBuffer;
}

declare interface ProcessStdErrEvent {
    type: "stderr";
    pid: number;
    data: ArrayBuffer;
}

declare interface SynchronousProcessResult {
    exitCode: number;
    error?: string;
    stdout?: ArrayBuffer;
    stderr?: ArrayBuffer;
}

declare function setProcessHandler(handler: (event: ProcessFinishedEvent | ProcessStdOutEvent | ProcessStdErrEvent) => void): void;
declare function startProcess(parameters: ProcessParameters): number;
declare function execProcess(parameters: ProcessParameters): SynchronousProcessResult;
declare function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
declare function closeProcessStdin(id: number): void;
