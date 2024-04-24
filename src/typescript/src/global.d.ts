declare function setTimeout(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearTimeout(timeoutId: number): void;
declare function setInterval(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearInterval(intervalId: number): void;
declare function queueMicrotask(callback: () => void): void;
