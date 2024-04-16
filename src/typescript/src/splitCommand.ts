export function splitCommand(command: string): string[] {
    // ### this should support quoting
    return command.split(/ +/).filter((x: string) => x);
}
