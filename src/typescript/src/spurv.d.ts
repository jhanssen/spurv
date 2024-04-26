declare namespace spurv {
    // logging
    export const enum LogLevel {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5
    }

    export function log(level: LogLevel, message: string): void;

    // process
    export interface NativeProcessFinishedEvent {
        type: "finished";
        pid: number;
        exitCode: number;
        error?: string;
    }

    export interface NativeProcessStdoutEvent {
        type: "stdout";
        pid: number;
        data?: ArrayBuffer | string;
        end: boolean;
    }

    export interface NativeProcessStderrEvent {
        type: "stderr";
        pid: number;
        data?: ArrayBuffer | string;
        end: boolean;
    }

    export interface NativeSynchronousProcessResult {
        exitCode: number;
        error?: string;
        stdout?: ArrayBuffer | string;
        stderr?: ArrayBuffer | string;
    }

    export const enum ProcessFlags {
        None = 0x0,
        Stdout = 0x1,
        Stderr = 0x2,
        Strings = 0x4
    }

    export function setProcessHandler(
        handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void
    ): void;
    export function startProcess(arguments: string[], stdin: ArrayBuffer | string | undefined, flags: number): number;
    export function execProcess(
        arguments: string[],
        stdin: ArrayBuffer | string | undefined,
        flags: number
    ): NativeSynchronousProcessResult;
    export function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
    export function closeProcessStdin(id: number): void;

    // unicode/buffers

    export function utf8tostring(buffer: ArrayBuffer): string;

    export function utf16tostring(buffer: ArrayBuffer): string;
    export function utf16letostring(buffer: ArrayBuffer): string;
    export function utf16betostring(buffer: ArrayBuffer): string;

    export function utf32tostring(buffer: ArrayBuffer): string;

    export function stringtoutf8(str: string): ArrayBuffer;

    export function stringtoutf16(str: string): ArrayBuffer;
    export function stringtoutf16le(str: string): ArrayBuffer;
    export function stringtoutf16be(str: string): ArrayBuffer;

    export function stringtoutf32(str: string): ArrayBuffer;

    // events

    export interface KeyEvent {
        key: number;
        scancode: number;
        action: number;
        mods: number;
    }
    export function setKeyEventHandler(handler: (event: KeyEvent) => void): void;

    export function exit(number?: number): void;
}
