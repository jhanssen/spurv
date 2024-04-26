// import { Process } from "./Process";
import { error } from "./log";

// Object.defineProperty(globalThis, "Process", Process);

spurv.setKeyEventHandler((event: spurv.KeyEvent) => {
    error("got key event", JSON.stringify(event, undefined, 4));
    if (event.key === 81 && event.action === 1) {
        exit(0);
    }
});

const foo = spurv.stringtoutf8("ABCDEFG");
const bar = new Uint8Array(foo);
error("arraybuffer", foo.byteLength);
error("uint8", bar.byteLength);
// const zot1 = spurv.utf8tostring(foo);
const zot2 = spurv.utf8tostring(bar);
// error("1", zot1);
error("2", zot2);

error("testing");
error("testing2");

// setTimeout(() => {
//     error("about to exit");
//     exit(10);
// }, 2000);
