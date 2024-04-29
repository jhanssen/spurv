export function formatError(error: Error): string {
    return `${error.message}\n${error.stack}`;
}
