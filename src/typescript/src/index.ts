// import { Process } from "./Process";
import { error } from "./log";

// Object.defineProperty(globalThis, "Process", Process);

spurv.setKeyEventHandler((event: spurv.KeyEvent) => {
    error("got key event", JSON.stringify(event, undefined, 4));
    if (event.key === 81 && event.action === 1) {
        exit(0);
    }
});

error("testing");
error("testing2");

// setTimeout(() => {
//     error("about to exit");
//     exit(10);
// }, 2000);
