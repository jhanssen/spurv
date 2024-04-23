import { assert } from "./assert";

export function concatArrayBuffers(buffers: ArrayBuffer[]): ArrayBuffer {
    switch (buffers.length) {
        case 0:
            return new ArrayBuffer(0);
        case 1: {
            const ret = buffers[0];
            assert(ret, "Must have ret");
            return ret;
        }
        default: {
            const len = buffers.reduce<number>((cur: number, value: ArrayBuffer | Uint8Array) => {
                return cur + value.byteLength;
            }, 0);

            const ret = new Uint8Array(len);
            let idx = 0;
            buffers.forEach((b: ArrayBuffer) => {
                ret.set(new Uint8Array(b), idx);
                idx += b.byteLength;
            });
            return ret.buffer;
        }
    }
}
