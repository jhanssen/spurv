import { type ParseEntry, parse } from "shell-quote";

export function splitCommand(command: string): string[] {
    return parse(command).map((x: ParseEntry) => {
        if (typeof x === "string") {
            return x;
        }
        throw new Error(`Invalid entry: ${JSON.stringify(x)}`);
    });
}
