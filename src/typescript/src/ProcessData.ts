import { assert } from "./assert";
import { concatArrayBuffers } from "./concatArrayBuffers";
import type { Process } from "./Process";
import type { ProcessResult } from "./ProcessResult";

export class ProcessData {
    private stderrStrings: string[];
    private stderrArrayBuffers: ArrayBuffer[];
    private stdoutStrings: string[];
    private stdoutArrayBuffers: ArrayBuffer[];

    constructor(
        readonly name: string,
        readonly process: Process,
        readonly resolve: (result: ProcessResult) => void,
        readonly reject: (error: Error) => void
    ) {
        this.stdoutStrings = [];
        this.stdoutArrayBuffers = [];
        this.stderrStrings = [];
        this.stderrArrayBuffers = [];
    }

    finish(exitCode: number, error?: string): void {
        let stderr: undefined | ArrayBuffer | string;
        let stdout: undefined | ArrayBuffer | string;
        if (this.stderrStrings.length) {
            stderr = this.stderrStrings.join("");
        } else if (this.stderrArrayBuffers.length) {
            stderr = concatArrayBuffers(this.stderrArrayBuffers);
        }

        if (this.stdoutStrings.length) {
            stdout = this.stdoutStrings.join("");
        } else if (this.stdoutArrayBuffers.length) {
            stdout = concatArrayBuffers(this.stdoutArrayBuffers);
        }

        if (exitCode === 0) {
            this.resolve({ exitCode, stdout, stderr });
        } else {
            this.reject(
                new Error(`Process ${this.name} exited with exit code: ${exitCode}${error ? "\n" + error : ""}`)
            );
        }
    }

    add(type: "stdout" | "stderr", data: ArrayBuffer | string): void {
        if (type === "stdout") {
            if (typeof data === "string") {
                assert(this.stdoutArrayBuffers.length === 0);
                this.stdoutStrings.push(data);
            } else {
                assert(this.stdoutStrings.length === 0);
                this.stdoutArrayBuffers.push(data);
            }
        } else {
            assert(type === "stderr");
            if (typeof data === "string") {
                assert(this.stderrArrayBuffers.length === 0);
                this.stderrStrings.push(data);
            } else {
                assert(this.stderrStrings.length === 0);
                this.stderrArrayBuffers.push(data);
            }
        }
    }
}
