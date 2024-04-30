import { format } from "./format";

export namespace console {
    function error(...args: unknown[]): void {
        spurv.log(spurv.LogLevel.Error, format(...args));
    }

    function log(...args: unknown[]): void {
        spurv.log(spurv.LogLevel.Info, format(...args));
    }

    function warn(...args: unknown[]): void {
        spurv.log(spurv.LogLevel.Warn, format(...args));
    }

    function info(...args: unknown[]): void {
        spurv.log(spurv.LogLevel.Info, format(...args));
    }

}

