export function formatError(error: Error): string {
    return JSON.stringify({ message: error.message, stack: error.stack });
}
