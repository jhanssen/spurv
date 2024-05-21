import { Process } from "./Process";
import { assert } from "./assert";
import { installConsole } from "./installConsole";
import { splitCommand } from "./splitCommand";
import { test } from "./test";
import type { ProcessStderrEvent } from "./ProcessStderrEvent";
import type { ProcessStdoutEvent } from "./ProcessStdoutEvent";
// import URL from "url-parse";

Object.defineProperty(globalThis, "Process", Process);
installConsole();

queueMicrotask(() => {
    console.log("BALLS", splitCommand("foo"));
    console.log("BALLS", splitCommand('"foo"'));
    console.log("BALLS", splitCommand('"foo bar" bar\'foo "bar\'foo"            "foo    bar"'));
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
        } else if (event.key === spurv.Key.V) {
            console.log(spurv.env);
            console.log(spurv.argv);
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
                if (stdoutEv.end) {
                    console.log("got stdout end", stdoutEv.end);
                } else {
                    console.log("got stdout", stdoutEv.data);
                }
            });
            proc.on("stderr", (stderrEv: ProcessStderrEvent) => {
                if (stderrEv.end) {
                    console.log("got stderr end", stderrEv.end);
                } else {
                    console.log("got stderr", stderrEv.data);
                }
            });

            try {
                const ret = await proc.start(["cat"], { strings: true, stdout: true, stderr: true, stdin: "foobar" });
                console.log("got ret", ret);
            } catch (err: unknown) {
                console.error("got err", err);
            }
        } else if (event.key === spurv.Key.O) {
            console.log("trying process 2");
            const proc = new Process();
            proc.on("stdout", (stdoutEv: ProcessStdoutEvent) => {
                if (stdoutEv.end) {
                    console.log("got stdout end", stdoutEv.end);
                } else {
                    console.log("got stdout", stdoutEv.data);
                }
            });
            proc.on("stderr", (stderrEv: ProcessStderrEvent) => {
                if (stderrEv.end) {
                    console.log("got stderr end", stderrEv.end);
                } else {
                    console.log("got stderr", stderrEv.data);
                }
            });

            try {
                const ret = await proc.start("/tmp/foobar", {
                    strings: true,
                    stdout: true,
                    stderr: true,
                    stdin: "foobar"
                });
                console.log("got ret", ret);
            } catch (err: unknown) {
                console.error("got err", err);
            }
        } else if (event.key === spurv.Key.Num_0) {
            const ret = await Process.exec("cat", { strings: true, stdout: true, stderr: true, stdin: "foobar" });
            console.log("sync result", ret);
        } else if (event.key === spurv.Key.Num_1) {
            const ret = await Process.start("cat", { strings: true, stdout: true, stderr: true, stdin: "foobar" });
            console.log(ret);
        } else if (event.key === spurv.Key.Num_2) {
            console.log(await Process.start("touch /tmp/'foo bar'"));
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

(async (): Promise<void> => {
    if (spurv.argv.length > 1) {
        const doc = new spurv.Document();
        await doc.loadFile(spurv.argv[1]!);
        const v = new spurv.View();
        v.document = doc;
    }
})();
