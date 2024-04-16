import { format } from "./format";

export function trace(...args: unknown[]): void
{
    log(LogLevel.trace, format(...args, "\n"));
}

export function debug(...args: unknown[]): void
{
    log(LogLevel.debug, format(...args, "\n"));
}

export function info(...args: unknown[]): void
{
    log(LogLevel.info, format(...args, "\n"));
}

export function warn(...args: unknown[]): void
{
    log(LogLevel.warn, format(...args, "\n"));
}

export function error(...args: unknown[]): void
{
    log(LogLevel.err, format(...args, "\n"));
}

export function critical(...args: unknown[]): void
{
    log(LogLevel.critical, format(...args, "\n"));
}
