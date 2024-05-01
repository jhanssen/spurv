import { Process } from "./Process";
import { assert } from "./assert";
import { installConsole } from "./installConsole";
import { test } from "./test";

Object.defineProperty(globalThis, "Process", Process);
installConsole();

spurv.setKeyEventHandler((event: spurv.KeyEvent) => {
    // console.error("got key event", JSON.stringify(event, undefined, 4));
    if (event.key === 81 && event.action === 1) {
        spurv.exit(0);
    }
});

try {
    test();
} catch (err: unknown) {
    assert(err instanceof Error);
    // console.error("Got test failures", err);
}

const foo = spurv.stringtoutf8("ABCDEFG");
const bar = new Uint8Array(foo);
// error("arraybuffer", foo.byteLength);
// error("uint8", bar.byteLength);
// const zot1 = spurv.utf8tostring(foo);
const zot2 = spurv.utf8tostring(bar);
// error("1", zot1);
console.error("2", zot2);

// console.error("testing");
// console.error("testing2");

// setTimeout(() => {
//     error("about to exit");
//     exit(10);
// }, 2000);
