export function formatArrayBufferView(buffer: ArrayBufferView): string {
    return `${Object.getPrototypeOf(buffer).constructor.name}((${buffer.byteLength})`;
}
