export interface ProcessOptions {
    stdin?: ArrayBuffer;
    env?: Record<string, string>;
    cwd?: string;
    stdout?: boolean;
    stderr?: boolean;
    strings?: boolean;
}
