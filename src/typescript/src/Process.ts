import { ProcessData } from "./ProcessData";
import { assert } from "./assert";
import { error } from "./log";
import { splitCommand } from "./splitCommand";
import type { ProcessFinishedEvent } from "./ProcessFinishedEvent";
import type { ProcessOptions } from "./ProcessOptions";
import type { ProcessResult } from "./ProcessResult";
import type { ProcessStderrEvent } from "./ProcessStderrEvent";
import type { ProcessStdoutEvent } from "./ProcessStdoutEvent";

const processes: Map<number, ProcessData> = new Map();

type Events = "stdout" | "stderr";
type ProcessStdoutEventHandler = (event: ProcessStdoutEvent) => void;
type ProcessStderrEventHandler = (event: ProcessStderrEvent) => void;
type EventHandlers = ProcessStdoutEventHandler | ProcessStderrEventHandler;

export class Process {
    private stdoutHandlers: Array<(event: ProcessStdoutEvent) => void>;
    private stderrHandlers: Array<(event: ProcessStderrEvent) => void>;
    private processId?: number;
    private stdinClosed?: boolean;

    constructor() {
        this.stdoutHandlers = [];
        this.stderrHandlers = [];
    }

    get pid(): number | undefined {
        return this.processId;
    }

    on(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    on(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    on(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    off(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    off(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    off(type: Events, handler: EventHandlers): void {
        this.offImpl(type, handler);
    }

    addEventListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    addEventListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    addEventListener(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    removeEventListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    removeEventListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    removeEventListener(type: Events, handler: EventHandlers): void {
        this.offImpl(type, handler);
    }

    addListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    addListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    addListener(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    removeListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    removeListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    removeListener(type: Events, handler: EventHandlers): void {
        this.offImpl(type, handler);
    }

    emit(type: Events & "stdout", event: ProcessStdoutEvent): void;
    emit(type: Events & "stderr", event: ProcessStderrEvent): void;
    emit(type: Events, event: ProcessFinishedEvent | ProcessStdoutEvent | ProcessStderrEvent): boolean {
        let ret = false;
        switch (type) {
            case "stdout":
                this.stdoutHandlers.forEach((x: ProcessStdoutEventHandler) => {
                    assert(event.type === "stdout");
                    ret = true;
                    try {
                        x(event);
                    } catch (err: unknown) {
                        error("Error in event handler", err);
                    }
                });
                break;
            case "stderr":
                this.stderrHandlers.forEach((x: ProcessStderrEventHandler) => {
                    assert(event.type === "stderr");
                    ret = true;
                    try {
                        x(event);
                    } catch (err: unknown) {
                        error("Error in event handler", err);
                    }
                });
                break;
        }
        return ret;
    }

    writeToStdin(data: ArrayBuffer | string): void { // expose buffering innards?
        if (this.processId === undefined) {
            throw new Error("Process is not active");
        }

        if (this.stdinClosed) {
            throw new Error("Stdin is closed");
        }

        writeToProcessStdin(this.processId, data);
    }

    closeStdin(): void {
        if (this.processId === undefined) {
            throw new Error("Process is not active");
        }

        if (this.stdinClosed) {
            throw new Error("Stdin is already closed");
        }

        this.stdinClosed = true;
        closeProcessStdin(this.processId);
    }

    start(command: string, options?: ProcessOptions): Promise<ProcessResult>;
    start(args: string[], options?: ProcessOptions): Promise<ProcessResult>;
    start(commandOrArgs: string | string[], options?: ProcessOptions): Promise<ProcessResult> {
        return new Promise((resolve: (result: ProcessResult) => void, reject: (err: Error) => void) => {
            if (!this.processId !== undefined) {
                reject(new Error("Process is already running"));
                return;
            }
            if (typeof commandOrArgs === "string") {
                commandOrArgs = splitCommand(commandOrArgs);
            }

            const data = new ProcessData(commandOrArgs[0] || "", this, resolve, reject);

            let flags: ProcessFlags = ProcessFlags.None;
            if (options?.stderr) {
                flags |= ProcessFlags.Stderr;
            }
            if (options?.stdout) {
                flags |= ProcessFlags.Stdout;
            }
            if (options?.strings) {
                flags |= ProcessFlags.Strings;
            }

            this.processId = startProcess(commandOrArgs, options?.stdin, flags);
            processes.set(this.processId, data);
        });
    }

    private onImpl(type: Events, handler: EventHandlers): void {
        switch (type) {
            case "stdout":
                this.stdoutHandlers.push(handler as ProcessStdoutEventHandler);
                break;
            case "stderr":
                this.stderrHandlers.push(handler as ProcessStderrEventHandler);
                break;
            default:
                throw new Error(`Invalid event: ${type}`);
        }
    }

    private offImpl(type: Events, handler: EventHandlers): void {
        switch (type) {
            case "stdout": {
                // const idx = this.stderr
                this.stdoutHandlers.push(handler as ProcessStdoutEventHandler);
                break; }
            case "stderr":
                this.stderrHandlers.push(handler as ProcessStderrEventHandler);
                break;
            default:
                throw new Error(`Invalid event: ${type}`);
        }
    }

    // static exec(command: string): ProcessResult;
    // static exec(args: string[]): ProcessResult;
    // static exec(commandOrArgs: string | string[]): ProcessResult {
    //     if (typeof commandOrArgs === "string") {
    //         commandOrArgs = splitCommand(commandOrArgs);
    //     }
    // }
}

setProcessHandler((event: NativeProcessFinishedEvent | NativeProcessStdoutEvent | NativeProcessStderrEvent) => {
    const data = processes.get(event.pid);
    if (!data) {
        error("Got event for unknown pid", event);
        return;
    }

    switch (event.type) {
        case "finished":
            data.finish(event.exitCode, event.error);
            break;
        case "stdout":
            data.process.emit("stdout", event);
            // ### maybe not add it if there's listeners? This API is getting a little weird
            if (event.data) {
                data.add(event.type, event.data);
            }
            break;
        case "stderr":
            data.process.emit("stderr", event);
            if (event.data) {
                data.add(event.type, event.data);
            }
            break;
    }
});


