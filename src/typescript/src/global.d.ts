declare function setTimeout(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearTimeout(timeoutId: number): void;
declare function setInterval(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearInterval(intervalId: number): void;
declare function queueMicrotask(callback: () => void): void;
declare function atob(str: string, mode?: "string"): string;
declare function atob(str: string, mode: "ArrayBuffer"): ArrayBuffer;
declare function atob(buf: ArrayBuffer, mode?: "ArrayBuffer"): ArrayBuffer;
declare function atob(buf: ArrayBuffer, mode: "string"): string;
declare function btoa(str: string, mode?: "string"): string;
declare function btoa(str: string, mode: "ArrayBuffer"): ArrayBuffer;
declare function btoa(buf: ArrayBuffer, mode?: "ArrayBuffer"): ArrayBuffer;
declare function btoa(buf: ArrayBuffer, mode: "string"): string;

declare namespace console {
    function error(...args: unknown[]): void;
    function log(...args: unknown[]): void;
    function info(...args: unknown[]): void;
    function warn(...args: unknown[]): void;
}
