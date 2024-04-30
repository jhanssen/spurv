// import { error } from "./log";

// function compareArrayBuffers(a: ArrayBuffer, b: ArrayBuffer): void {
//     if (b.byteLength !== b.byteLength) {
//         throw new Error(`Different lengths: ${a.byteLength} ${b.byteLength}`);
//     }

//     const aa = new Uint8Array(a);
//     const bb = new Uint8Array(b);
//     const errors: string[] = [];
//     for (let i=0; i<aa.byteLength; ++i) {
//         if (aa[i] !== bb[i]) {
//             errors.push(`Different byte at index: ${i} ${aa[i]} vs ${bb[i]}`);
//         }
//     }
//     if (errors.length) {
//         throw new Error(errors.join("\n"));
//     }
// }

function compareStrings(a: string, b: string): void {
    if (a !== b) {
        throw new Error(`Strings are different:\n${a}\nvs\n${b}`);
    }
}

export function test(): void {
    const a = "0123456789";
    const b = spurv.stringtoutf8(a);
    const c = spurv.utf8tostring(b);
    compareStrings(a, c);
    const d = spurv.stringtoutf16(a);
    const e = spurv.utf16tostring(d);
    compareStrings(a, e);
    const f = spurv.stringtoutf32(a);
    const g = spurv.utf32tostring(f);
    compareStrings(a, g);
    const h = btoa(a);
    const i = atob(h);
    compareStrings(a, i);

    const j = btoa(a, "ArrayBuffer");
    const k = atob(j, "string");
    compareStrings(a, k);
    if (!(j instanceof ArrayBuffer)) {
        throw new Error("Should have been ArrayBuffer");
    }
}
