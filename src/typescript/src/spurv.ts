import { Process } from "./Process";
import { error } from "./log";

Object.defineProperty(globalThis, "Process", Process);

error("testing");


