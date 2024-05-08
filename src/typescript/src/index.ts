import { Process } from "./Process";
import { assert } from "./assert";
import { installConsole } from "./installConsole";
import { test } from "./test";
import type { ProcessStderrEvent } from "./ProcessStderrEvent";
import type { ProcessStdoutEvent } from "./ProcessStdoutEvent";

Object.defineProperty(globalThis, "Process", Process);
installConsole();

queueMicrotask(() => {
    console.log("BALLS");
});

let view: spurv.View | undefined;
spurv.setKeyEventHandler(async (event: spurv.KeyEvent): Promise<void> => {
    try {
        // console.error(spurv.View.prototype);
        // console.error(spurv.View.prototype.constructor);
        // console.error(spurv.View.prototype.constructor === spurv.View);
        console.error("got key event", JSON.stringify(event, undefined, 4));
        if (event.action !== 1) {
            return;
        }
        if (event.key === spurv.Key.Q) {
            // q
            spurv.exit(0);
        } else if (event.key === spurv.Key.Up) {
            // up
            console.log("up", typeof view, typeof spurv.View);
            if (!view) {
                view = new spurv.View();
            }
            console.log(
                "shat",
                spurv.View.prototype === Object.getPrototypeOf(view),
                // Object.keys(Object.getPrototypeOf(view)),
                Object.keys(spurv.View.prototype)
            );

            console.log(
                "Scrolling up",
                typeof view,
                view,
                view.currentLine,
                JSON.stringify(Object.getPrototypeOf(view))
            );
            view.scrollUp();
        } else if (event.key === spurv.Key.Down) {
            // down
            console.log("down", typeof view, typeof spurv.View);
            if (!view) {
                view = new spurv.View();
            }
            console.log("Scrolling down", view.currentLine, typeof spurv.View);
            view.scrollDown();
        } else if (event.key === spurv.Key.P) {
            console.log("trying process");
            const proc = new Process();
            proc.on("stdout", (stdoutEv: ProcessStdoutEvent) => {
                console.log("got stdout", stdoutEv.data, stdoutEv.end);
            });
            proc.on("stderr", (stderrEv: ProcessStderrEvent) => {
                console.error("got stderr", stderrEv.data, stderrEv.end);
            });

            try {
                const ret = await proc.start(["cat"], { strings: true, stdout: true, stderr: true, stdin: "foobar" });
                console.log("got ret", ret);
            } catch (err: unknown) {
                console.error("got err", err);
            }
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
