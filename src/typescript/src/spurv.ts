import EventEmitter from "events";

class Process extends EventEmitter {
    constructor() {
        super();
    }

    start(command: string): Promise<ProcessResult>;
    start(args: string[]): Promise<ProcessResult>;
    start(commandOrArgs: string | string[]): Promise<ProcessResult> {


    }

    exec(command: string): ProcessResult;
    exec(args: string[]): ProcessResult;
    exec(commandOrArgs: string | string[]): ProcessResult {

    }
}

Object.defineProperty(globalThis, "Process", Process);
