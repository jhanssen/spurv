/* eslint-disable @typescript-eslint/naming-convention */
/* eslint-disable max-classes-per-file */
// import { EventEmitter } from "./EventEmitter";

declare namespace spurv {
    type IArguments<T> = [T] extends [(...args: infer U) => unknown] ? U : [T] extends [void] ? [] : [T];
    class IEventEmitter<Events> {
        eventListenerAdded?: (event: string) => void;
        addEventListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        addListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        addOnceListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        dumpEventListeners(): string;
        on<E extends keyof Events>(event: E, listener: Events[E]): void;
        once<E extends keyof Events>(event: E, listener: Events[E]): void;
        prependListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        prependOnceListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        off<E extends keyof Events>(event: E, listener: Events[E]): void;
        removeAllListeners<E extends keyof Events>(event?: E): void;
        removeListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        removeEventListener<E extends keyof Events>(event: E, listener: Events[E]): void;
        emit<E extends keyof Events>(event: E, ...args: IArguments<Events[E]>): boolean;
        eventNames(): Array<string | number | symbol>;
        listeners<E extends keyof Events>(event: E): Array<Events[E]>;
        _getEventListeners<E extends keyof Events>(event: E): Array<Events[E]>;
        listenerCount<E extends keyof Events>(event: E): number;
        hasListeners(): boolean;
        hasListener<E extends keyof Events>(event: E): boolean;
    }

    export const enum Key {
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Num_0 = 48,
        Num_1 = 49,
        Num_2 = 50,
        Num_3 = 51,
        Num_4 = 52,
        Num_5 = 53,
        Num_6 = 54,
        Num_7 = 55,
        Num_8 = 56,
        Num_9 = 57,
        Semicolon = 59,
        Equal = 61,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        Left_Bracket = 91,
        Backslash = 92,
        Right_Bracket = 93,
        Grave_Accent = 96,
        World_1 = 161,
        World_2 = 162,
        Escape = 256,
        Enter = 257,
        Tab = 258,
        Backspace = 259,
        Insert = 260,
        Delete = 261,
        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,
        Page_Up = 266,
        Page_Down = 267,
        Home = 268,
        End = 269,
        Caps_Lock = 280,
        Scroll_Lock = 281,
        Num_Lock = 282,
        Print_Screen = 283,
        Pause = 284,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
        F13 = 302,
        F14 = 303,
        F15 = 304,
        F16 = 305,
        F17 = 306,
        F18 = 307,
        F19 = 308,
        F20 = 309,
        F21 = 310,
        F22 = 311,
        F23 = 312,
        F24 = 313,
        F25 = 314,
        Keypad_0 = 320,
        Keypad_1 = 321,
        Keypad_2 = 322,
        Keypad_3 = 323,
        Keypad_4 = 324,
        Keypad_5 = 325,
        Keypad_6 = 326,
        Keypad_7 = 327,
        Keypad_8 = 328,
        Keypad_9 = 329,
        Keypad_Decimal = 330,
        Keypad_Divide = 331,
        Keypad_Multiply = 332,
        Keypad_Subtract = 333,
        Keypad_Add = 334,
        Keypad_Enter = 335,
        Keypad_Equal = 336,
        Left_Shift = 340,
        Left_Control = 341,
        Left_Alt = 342,
        Left_Super = 343,
        Right_Shift = 344,
        Right_Control = 345,
        Right_Alt = 346,
        Right_Super = 347,
        Menu = 348,
        Last = Menu
    }

    export const enum Action {
        Release = 0,
        Press = 1,
        Repeat = 2
    }

    export const enum Modifier {
        Shift = 0x0001,
        Control = 0x0002,
        Alt = 0x0004,
        Super = 0x0008,
        CapsLock = 0x0010,
        NumLock = 0x0020
    }

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
        pid: number;
        stdout?: ArrayBuffer | string;
        stderr?: ArrayBuffer | string;
    }

    export const enum ProcessFlags {
        None = 0x00,
        StdinClosed = 0x01,
        Stdout = 0x02,
        Stderr = 0x04,
        Strings = 0x08,
        ForceOutputInResult = 0x10
    }

    export function setProcessHandler(
        handler: (event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => void
    ): void;
    export function startProcess(
        arguments: string[],
        env: Record<string, string> | undefined,
        cwd: string | undefined,
        stdin: ArrayBuffer | string | undefined,
        flags: number
    ): number | string;
    export function execProcess(
        arguments: string[],
        env: Record<string, string> | undefined,
        cwd: string | undefined,
        stdin: ArrayBuffer | string | undefined,
        flags: number
    ): NativeSynchronousProcessResult;
    export function writeToProcessStdin(id: number, data: ArrayBuffer | string): void;
    export function closeProcessStdin(id: number): void;
    export function killProcess(pid: number, signal: number): void;
    export function environ(): Record<string, string>;

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
        key: Key;
        scancode: number;
        action: Action;
        mods: Modifier;
        keyName?: string;
    }
    export function setKeyEventHandler(handler: (event: KeyEvent) => void): void;

    export function exit(number?: number): void;

    export interface FetchNativeHeaders {
        type: "headers";
        statusCode: number;
        headers: Record<string, string>;
    }

    export interface FetchNativeError {
        type: "error";
        error: string;
    }

    export interface FetchNativeData {
        type: "data";
        data: ArrayBuffer;
        end?: boolean;
    }

    export interface FetchNativeOptions {
        url: string;
        headers?: Record<string, string>;
        method?: "GET" | "POST" | "PUT" | "OPTIONS" | "DELETE" | "PATCH";
        body: string | ArrayBuffer | undefined;
    }

    export function fetchNative(options: FetchNativeOptions): number;
    export function abortFetchNative(id: number): void;
    export function setFetchNativeHandler(
        handler: (event: FetchNativeHeaders | FetchNativeData | FetchNativeError) => void
    ): void;

    const argv: string[];
    const env: Record<string, string>;

    interface ViewEvents {
        documentChanged: () => void;
    }

    class View extends IEventEmitter<ViewEvents> {
        readonly currentLine: number;

        document?: Document;

        constructor();

        scrollUp(): void;
        scrollDown(): void;
    }

    interface DocumentEvents {
        ready: () => void;
        propertiesChanged: (start: number, end: number) => void;
    }

    class Document extends IEventEmitter<DocumentEvents> {
        font: string;
        readonly ready: boolean;
        readonly lines: number;

        constructor();

        loadFile(path: string): Promise<void>;
        setContents(contents: string): Promise<void>;
    }
}
