export interface ProcessResult {
    exitCode: number;
    error?: string;
    stdout?: ArrayBuffer; // string?
    stderr?: ArrayBuffer; // string?
}
