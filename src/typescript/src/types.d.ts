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
    data?: ArrayBuffer | string;
    end: boolean;
}

declare interface NativeProcessStderrEvent {
    type: "stderr";
    pid: number;
    data?: ArrayBuffer | string;
    end: boolean;
}

declare interface NativeSynchronousProcessResult {
    exitCode: number;
    error?: string;
    stdout?: ArrayBuffer | string;
    stderr?: ArrayBuffer | string;
}

declare const enum ProcessFlags {
    None = 0x0,
    Stdout = 0x1,
    Stderr = 0x2,
    Strings = 0x4
}

declare function utf8tostring(buffer: ArrayBuffer): string;

declare function utf16tostring(buffer: ArrayBuffer): string;
declare function utf16letostring(buffer: ArrayBuffer): string;
declare function utf16betostring(buffer: ArrayBuffer): string;

declare function utf32tostring(buffer: ArrayBuffer): string;

declare function stringtoutf8(str: string): ArrayBuffer;

declare function stringtoutf16(str: string): ArrayBuffer;
declare function stringtoutf16le(str: string): ArrayBuffer;
declare function stringtoutf16be(str: string): ArrayBuffer;

declare function stringtoutf32(str: string): ArrayBuffer;

declare function setProcessHandler(handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void): void;
declare function startProcess(arguments: string[], stdin: ArrayBuffer | string | undefined, flags: number): number;
declare function execProcess(arguments: string[], stdin: ArrayBuffer | string | undefined, flags: number): NativeSynchronousProcessResult;
declare function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
declare function closeProcessStdin(id: number): void;
