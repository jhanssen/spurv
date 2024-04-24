// import { Process } from "./Process";
import { error } from "./log";

// Object.defineProperty(globalThis, "Process", Process);

spurv.setKeyEventHandler((event: spurv.KeyEvent) => {
    error("got key event", JSON.stringify(event, undefined, 4), "fuck");
});

error("testing");
error("testing2");
