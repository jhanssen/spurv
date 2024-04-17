import { error } from "./log";
import { splitCommand } from "./splitCommand";
import type { ProcessData } from "./ProcessData";
import type { ProcessFinishedEvent } from "./ProcessFinishedEvent";
import type { ProcessResult } from "./ProcessResult";
import type { ProcessStderrEvent } from "./ProcessStderrEvent";
import type { ProcessStdoutEvent } from "./ProcessStdoutEvent";

const processes: Map<number, ProcessData> = new Map();

type Events = "finished" | "stdout" | "stderr";
type ProcessFinishedEventHandler = (event: ProcessFinishedEvent) => void;
type ProcessStdoutEventHandler = (event: ProcessStdoutEvent) => void;
type ProcessStderrEventHandler = (event: ProcessStderrEvent) => void;
type EventHandlers = ProcessFinishedEventHandler | ProcessStdoutEventHandler | ProcessStderrEventHandler;

export class Process {
    private finishedHandlers: Array<(event: ProcessFinishedEvent) => void>;
    private stdoutHandlers: Array<(event: ProcessStdoutEvent) => void>;
    private stderrHandlers: Array<(event: ProcessStderrEvent) => void>;
    private processId?: number;

    constructor() {
        this.finishedHandlers = [];
        this.stdoutHandlers = [];
        this.stderrHandlers = [];
    }

    on(type: Events & "finished", handler: ProcessFinishedEventHandler): void;
    on(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    on(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    on(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    // off(type: Events & "finished", handler: ProcessFinishedEventHandler): void;
    // off(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    // off(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    // off(type: Events, handler: EventHandlers): void {
    //     switch (type) {
    //         case "finished": {
    //             const idx = this.finishedHandlers.indexOf(handler as ProcessFinishedEventHandler);
    //             if (idx === -1) {
    //                 break;
    //             }
    //             this.finishedHandlers.splice(idx, 1);
    //             break; }

    //         case "stdout": {
    //             // const idx = this.stderr
    //             this.stdoutHandlers.push(handler as ProcessStdoutEventHandler);
    //             break; }
    //         case "stderr":
    //             this.stderrHandlers.push(handler as ProcessStderrEventHandler);
    //             break;
    //         default:
    //             throw new Error(`Invalid event: ${type}`);
    //     }
    // }

    addEventListener(type: Events & "finished", handler: ProcessFinishedEventHandler): void;
    addEventListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    addEventListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    addEventListener(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    addListener(type: Events & "finished", handler: ProcessFinishedEventHandler): void;
    addListener(type: Events & "stdout", handler: ProcessStdoutEventHandler): void;
    addListener(type: Events & "stderr", handler: ProcessStderrEventHandler): void;
    addListener(type: Events, handler: EventHandlers): void {
        this.onImpl(type, handler);
    }

    start(command: string): Promise<ProcessResult>;
    start(args: string[]): Promise<ProcessResult>;
    start(commandOrArgs: string | string[]): Promise<ProcessResult> {
        return new Promise((resolve, reject) => {
            if (!this.processId !== undefined) {
                reject(new Error("Process is already running"));
                return;
            }
            if (typeof commandOrArgs === "string") {
                commandOrArgs = splitCommand(commandOrArgs);
            }
        });
    }

    private onImpl(type: Events, handler: EventHandlers): void {
        switch (type) {
            case "finished":
                this.finishedHandlers.push(handler as ProcessFinishedEventHandler);
                break;
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
            if (event.exitCode) {
                data.reject(new Error(`Process ${data.name} exited with exit code: ${event.exitCode}${event.error ? "\n" + event.error : ""}`));
            } else {
                // event
            }
            break;
        case "stdout":
        case "stderr":
            break;
    }
});
