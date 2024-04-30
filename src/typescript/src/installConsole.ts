import { format } from "./format";

export function installConsole(): void {
    const console = {
        error: (...args: unknown[]): void => {
            spurv.log(spurv.LogLevel.Error, format(...args));
        },
        log: (...args: unknown[]): void => {
            spurv.log(spurv.LogLevel.Info, format(...args));
        },
        warn: (...args: unknown[]): void => {
            spurv.log(spurv.LogLevel.Warn, format(...args));
        },
        info: (...args: unknown[]): void => {
            spurv.log(spurv.LogLevel.Info, format(...args));
        }
    };
    globalThis.console = console;
}
