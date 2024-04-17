import { format } from "./format";

export function trace(...args: unknown[]): void {
    log(LogLevel.Trace, format(...args, "\n"));
}

export function debug(...args: unknown[]): void {
    log(LogLevel.Debug, format(...args, "\n"));
}

export function info(...args: unknown[]): void {
    log(LogLevel.Info, format(...args, "\n"));
}

export function warn(...args: unknown[]): void {
    log(LogLevel.Warn, format(...args, "\n"));
}

export function error(...args: unknown[]): void {
    log(LogLevel.Error, format(...args, "\n"));
}

export function critical(...args: unknown[]): void {
    log(LogLevel.Critical, format(...args, "\n"));
}
