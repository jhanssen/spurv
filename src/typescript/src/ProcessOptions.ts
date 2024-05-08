export interface ProcessOptions {
    stdin?: ArrayBuffer | string;
    env?: Record<string, string>;
    cwd?: string;
    stdout?: boolean;
    stderr?: boolean;
    strings?: boolean;
}
