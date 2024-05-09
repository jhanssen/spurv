export function splitCommand(command: string): string[] {
    // from here: https://stackoverflow.com/questions/2817646/javascript-split-string-on-space-or-on-quotes-to-array
    const regexp = /[^\s"]+|"([^"]*)"/gi;
    const ret: string[] = [];

    while (true) {
        // Each call to exec returns the next regex match as an array
        const match = regexp.exec(command);
        if (match) {
            // Index 1 in the array is the captured group if it exists
            // Index 0 is the matched text, which we use if no captured group exists
            ret.push(match[1] ? match[1] : match[0]);
        } else {
            break;
        }
    }
    return ret;
}
