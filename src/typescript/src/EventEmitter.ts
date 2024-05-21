import { assert } from "./assert";

type EventEmitterCallback = (...args: unknown[]) => void;

interface EventConnection {
    listener?: EventEmitterCallback;
    once?: boolean;
}

interface EventConnectionData {
    connections: EventConnection[];
    emitDepth: number;
    removed: boolean;
}

function addHelper(
    listenerMap: Map<string | number | symbol, EventConnectionData>,
    eventListenerAdded: ((event: string) => void) | undefined,
    event: string | number | symbol,
    conn: EventConnection,
    prepend: boolean
): void {
    let data = listenerMap.get(event);
    if (!data) {
        data = { connections: [], emitDepth: 0, removed: false };
        listenerMap.set(event, data);
    }
    if (prepend) {
        data.connections.unshift(conn);
    } else {
        data.connections.push(conn);
    }
    if (eventListenerAdded) {
        eventListenerAdded(String(event));
    }
}

type Arguments<T> = [T] extends [(...args: infer U) => unknown] ? U : [T] extends [void] ? [] : [T];
// @internal
export class EventEmitter<Events> {
    private listenerMap: Map<string | number | symbol, EventConnectionData>;

    eventListenerAdded?: (event: string) => void;

    constructor() {
        this.listenerMap = new Map();
    }

    addEventListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            { listener: listener as unknown as EventEmitterCallback },
            false
        );
    }

    addListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            { listener: listener as unknown as EventEmitterCallback },
            false
        );
    }

    addOnceListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            {
                listener: listener as unknown as EventEmitterCallback,
                once: true
            },
            false
        );
    }

    dumpEventListeners(): string {
        const entryArray: Array<[string | number | symbol, EventConnectionData]> = Array.from(this.listenerMap);
        return entryArray
            .map(
                (item: [string | number | symbol, EventConnectionData]) =>
                    `key: ${String(item[0])} connections: ${
                        item[1].connections.length
                    } removed: ${item[1].removed} emitDepth: ${item[1].emitDepth}`
            )
            .join("\n");
    }

    on<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            { listener: listener as unknown as EventEmitterCallback },
            false
        );
    }

    once<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            {
                listener: listener as unknown as EventEmitterCallback,
                once: true
            },
            false
        );
    }

    prependListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            { listener: listener as unknown as EventEmitterCallback },
            true
        );
    }

    prependOnceListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        addHelper(
            this.listenerMap,
            this.eventListenerAdded,
            event,
            {
                listener: listener as unknown as EventEmitterCallback,
                once: true
            },
            true
        );
    }

    off<E extends keyof Events>(event: E, listener: Events[E]): void {
        if (!listener) {
            throw new Error("No listener");
        }

        const data = this.listenerMap.get(event);
        if (!data) {
            return;
        }

        const idx = data.connections.findIndex((conn: EventConnection) => conn.listener === listener);
        if (idx !== -1) {
            if (data.emitDepth) {
                data.connections[idx]!.listener = undefined;
                data.removed = true;
            } else {
                data.connections.splice(idx, 1);
                if (!data.connections.length) {
                    this.listenerMap.delete(event);
                }
            }
        }
    }

    removeAllListeners<E extends keyof Events>(event?: E): void {
        if (event) {
            this.listenerMap.delete(event);
        } else {
            this.listenerMap = new Map();
        }
    }

    removeListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        this.off(event, listener);
    }

    removeEventListener<E extends keyof Events>(event: E, listener: Events[E]): void {
        this.off(event, listener);
    }

    emit<E extends keyof Events>(event: E, ...args: Arguments<Events[E]>): boolean {
        const data = this.listenerMap.get(event);
        if (!data) {
            return false;
        }

        ++data.emitDepth;
        let err: Error | undefined;
        for (let i = 0; i < data.connections.length; ++i) {
            const conn = data.connections[i];
            let listener: undefined | EventEmitterCallback;
            assert(conn, "Must have conn");
            if (!conn.listener) {
                data.removed = true;
            } else {
                listener = conn.listener;
                if (conn.once) {
                    conn.listener = undefined;
                    data.removed = true;
                }
            }
            if (listener) {
                try {
                    listener.apply(this, args);
                } catch (error: unknown) {
                    err = error as Error;
                    break;
                }
            }
        }
        --data.emitDepth;
        if (data.removed && !data.emitDepth) {
            data.removed = false;
            data.connections = data.connections.filter((x: EventConnection) => x.listener);
        }
        if (!data.connections.length) {
            this.listenerMap.delete(event);
        }
        if (err) {
            throw err;
        }
        return true;
    }

    eventNames(): Array<string | number | symbol> {
        return Array.from(this.listenerMap.keys());
    }

    listeners<E extends keyof Events>(event: E): Array<Events[E]> {
        const data = this.listenerMap.get(event);
        if (!data) {
            return [];
        }

        return data.connections.map((x: EventConnection) => x.listener as Events[E]).filter((x: Events[E]) => x);
    }

    _getEventListeners<E extends keyof Events>(event: E): Array<Events[E]> {
        return this.listeners(event);
    }

    listenerCount<E extends keyof Events>(event: E): number {
        return this.listeners(event).length;
    }

    // milo-specific
    hasListeners(): boolean {
        return this.listenerMap.size > 0;
    }

    hasListener<E extends keyof Events>(event: E): boolean {
        return this.listenerMap.has(event);
    }
}
