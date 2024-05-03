import { Process } from "./Process";
import { assert } from "./assert";
import { installConsole } from "./installConsole";
import { test } from "./test";

Object.defineProperty(globalThis, "Process", Process);
installConsole();

let view: spurv.View | undefined;
spurv.setKeyEventHandler((event: spurv.KeyEvent) => {
    try {
        // console.error(spurv.View.prototype);
        // console.error(spurv.View.prototype.constructor);
        // console.error(spurv.View.prototype.constructor === spurv.View);
        console.error("got key event", JSON.stringify(event, undefined, 4));
        if (event.action !== 1) {
            return;
        }
        if (event.key === 81) { // q
            spurv.exit(0);
        } else if (event.key === 265) { // up
            console.log("up", typeof view, typeof spurv.View);
            if (!view) {
                view = new spurv.View();
            }
            console.log("shat", spurv.View.prototype === Object.getPrototypeOf(view),
                // Object.keys(Object.getPrototypeOf(view)),
                Object.keys(spurv.View.prototype));

            console.log("Scrolling up", typeof view, view, view.currentLine, JSON.stringify(Object.getPrototypeOf(view)));
            view.scrollUp();
        } else if (event.key === 264) { // down
            console.log("down", typeof view, typeof spurv.View);
            if (!view) {
                view = new spurv.View();
            }
            console.log("Scrolling down", view.currentLine, typeof spurv.View);
            view.scrollDown();
        }
    } catch (err: unknown) {
        console.error("Got some error here", err);
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
