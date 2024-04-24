import { format } from "./format";

export function trace(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Trace, format(...args));
}

export function debug(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Debug, format(...args));
}

export function info(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Info, format(...args));
}

export function warn(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Warn, format(...args));
}

export function error(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Error, format(...args));
}

export function critical(...args: unknown[]): void {
    spurv.log(spurv.LogLevel.Critical, format(...args));
}
