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
    exitCode: number;
}

declare interface ProcessStdOutEvent {
    type: "stdout";
    data: ArrayBuffer;
}

declare interface ProcessStdErrEvent {
    type: "stderr";
    data: ArrayBuffer;
}

declare function setProcessHandler(handler: (event: ProcessFinishedEvent | ProcessStdOutEvent | ProcessStdErrEvent) => void): void;
declare function startProcess(parameters: ProcessParameters): number;
declare function writeToStdin(id: number, data: ArrayBuffer | string): void;
declare function closeStdin(id: number): void;
