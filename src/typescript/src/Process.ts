// import { error } from "./log";
import { splitCommand } from "./splitCommand";
import EventEmitter from "events";
// import type { ProcessData } from "./ProcessData";
import type { ProcessResult } from "./ProcessResult";

// const processes: Map<number, ProcessData> = new Map();

export class Process extends EventEmitter {
    private processId?: number;

    constructor() {
        super();
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

    static exec(command: string): ProcessResult;
    static exec(args: string[]): ProcessResult;
    static exec(commandOrArgs: string | string[]): ProcessResult {
        if (typeof commandOrArgs === "string") {
            commandOrArgs = splitCommand(commandOrArgs);
        }
    }
}

// setProcessHandler((event: ProcessFinishedEvent | ProcessStdOutEvent | ProcessStdErrEvent) => {
//     const data = processes.get(event.pid);
//     if (!data) {
//         error("Got event for unknown pid", event);
//         return;
//     }

//     switch (event.type) {
//         case "finished":
//             if (event.exitCode) {
//                 data.reject(new Error(`Process ${data.name} exited with exit code: ${event.exitCode}${event.error ? "\n" + event.error : ""}`));
//             } else {
//                 // event
//             }
//             break;
//         case "stdout":
//         case "stderr":
//             break;
//     }
// });



