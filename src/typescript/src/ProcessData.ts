import type { Process } from "./Process";
import type { ProcessResult } from "./ProcessResult";

export interface ProcessData {
    name: string;
    process: Process;
    resolve: (result: ProcessResult) => void;
    reject: (error: Error) => void;
}


