export function assert(cond: unknown, message?: string): asserts cond {
    if (!(cond)) {
        if (message) {
            throw new Error(`Assertion failed: ${message}`);
        } else {
            throw new Error("Assertion failed");
        }
    }
}
