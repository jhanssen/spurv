import type { Process } from "./Process";

export interface ProcessData {
    name: string;
    process: Process;
    stderr: ArrayBuffer[];
    stdout: ArrayBuffer[];
    finish: (exitCode: number, error?: string) => void;
}
