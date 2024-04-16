export interface ProcessResult {
    statusCode: number;
    error?: string;
    stdout?: ArrayBuffer; // string?
    stderr?: ArrayBuffer; // string?
}
