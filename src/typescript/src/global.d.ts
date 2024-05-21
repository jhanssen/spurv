/* eslint-disable max-classes-per-file */
declare function setTimeout(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearTimeout(timeoutId: number): void;
declare function setInterval(callback: (...args: unknown[]) => void, ms: number, ...args: unknown[]): number;
declare function clearInterval(intervalId: number): void;
declare function queueMicrotask(callback: () => void): void;
declare function atob(str: string, mode?: "string"): string;
declare function atob(str: string, mode: "ArrayBuffer"): ArrayBuffer;
declare function atob(buf: ArrayBuffer, mode?: "ArrayBuffer"): ArrayBuffer;
declare function atob(buf: ArrayBuffer, mode: "string"): string;
declare function btoa(str: string, mode?: "string"): string;
declare function btoa(str: string, mode: "ArrayBuffer"): ArrayBuffer;
declare function btoa(buf: ArrayBuffer, mode?: "ArrayBuffer"): ArrayBuffer;
declare function btoa(buf: ArrayBuffer, mode: "string"): string;

declare namespace console {
    function error(...args: unknown[]): void;
    function log(...args: unknown[]): void;
    function info(...args: unknown[]): void;
    function warn(...args: unknown[]): void;
}

type QueryParser = (query: string) => Record<string, unknown>;

type StringifyQuery = (query: Record<string, unknown>) => string;

interface URL {
    readonly auth: string;
    readonly hash: string;
    readonly host: string;
    readonly hostname: string;
    readonly href: string;
    readonly origin: string;
    readonly password: string;
    readonly pathname: string;
    readonly port: string;
    readonly protocol: string;
    readonly query: { [key: string]: string | undefined };
    readonly slashes: boolean;
    readonly username: string;
    set(part: URLPart, value: string | Record<string, unknown> | number | undefined, fn?: boolean | QueryParser): URL;
    toString(stringify?: StringifyQuery): string;
}

declare const URL: {
    new (address: string, parser?: boolean | QueryParser): URL;
    new (address: string, location?: string | URL, parser?: boolean | QueryParser): URL;
    (address: string, parser?: boolean | QueryParser): URL;
    (address: string, location?: string | Record<string, unknown>, parser?: boolean | QueryParser): URL;

    extractProtocol(url: string): {
        slashes: boolean;
        protocol: string;
        rest: string;
    };
    location(url: string): Record<string, unknown>;
    qs: {
        parse: QueryParser;
        stringify: StringifyQuery;
    };
};

declare class Request {
    readonly cache: RequestCache;
    readonly credentials: RequestCredentials;
    readonly destination: RequestDestination;
    readonly headers: Headers;
    readonly integrity: string;
    readonly isHistoryNavigation: boolean;
    readonly isReloadNavigation: boolean;
    readonly keepalive: boolean;
    readonly method: RequestMethod;
    readonly mode: RequestMode;
    readonly redirect: RequestRedirect;
    readonly referrer: string;
    readonly referrerPolicy: RequestReferrerPolicy;
    readonly signal: AbortSignal;
    readonly url: string;

    constructor(input: string | Request, init?: RequestInit);

    clone(): Request;
}

declare class Headers implements Iterable<[string, string]> {
    constructor();
    append(key: string, value: string): void;
    delete(key: string): void;
    entries(): IterableIterator<[string, string]>;
    forEach(func: (value: string, key: string) => boolean | void): void;
    get(key: string): string | null;
    has(key: string): boolean;
    keys(): IterableIterator<string>;
    set(key: string, value: string): void;
    values(): IterableIterator<string>;
    [Symbol.iterator](): Iterator<[string, string]>;
}

declare class FetchError extends Error {
    readonly name: "FetchError";
    constructor();
}

declare class AbortController {
    readonly signal: AbortSignal;

    constructor();
    abort(): void;
}

declare class AbortError extends Error {
    constructor();
}

declare class AbortSignal {
    constructor();
    get aborted(): boolean;
}

declare class Response {
    headers: Headers;

    status: number;

    connect: boolean;

    // clone(): Response;
    // error(): Response;
    // redirect(): Response;

    // Body interface
    //
    body: IReadableStream<Uint8Array>;

    constructor();

    // A boolean indicating whether the response was successful (status in the range 200-299)
    get ok(): boolean;
    get redirected(): boolean;
    get urlFinalURL(): boolean;
    get type(): string;
    get bodyUsed(): boolean;
    get url(): string;
    get statusText(): string;

    // blob(res: Response): Promise<Blob>
    json(): Promise<JSON>;
    arrayBuffer(): Promise<ArrayBuffer>;
    text(): Promise<string>;
}

declare interface RequestInit {
    method?: RequestMethod;
    headers?: Record<string, string> | Headers | [[string, string]];
    body?: string | ArrayBuffer | Uint8Array;
    referrer?: string;
    referrerPolicy?: RequestReferrerPolicy;
    mode?: RequestMode;
    credentials?: RequestCredentials;
    cache?: RequestCache;
    redirect?: RequestRedirect;
    integrity?: string;
    keepalive?: boolean;
    signal?: AbortSignal;
    onData?: boolean | number;
}

declare type RequestCache = "default" | "no-store" | "reload" | "no-cache" | "force-cache" | "only-if-cached";
declare type RequestCredentials = "omit" | "same-origin" | "include";
declare type RequestDestination =
    | ""
    | "audio"
    | "audioworklet"
    | "document"
    | "embed"
    | "font"
    | "frame"
    | "iframe"
    | "image"
    | "manifest"
    | "object"
    | "paintworklet"
    | "report"
    | "script"
    | "sharedworker"
    | "style"
    | "track"
    | "video"
    | "worker"
    | "xslt";

declare type RequestMethod = "" | "GET" | "POST" | "PUT" | "HEAD" | "DELETE" | "PATCH";
declare type RequestMode = "navigate" | "same-origin" | "no-cors" | "cors";
declare type RequestRedirect = "follow" | "error" | "manual";
declare type RequestReferrerPolicy =
    | ""
    | "no-referrer"
    | "no-referrer-when-downgrade"
    | "same-origin"
    | "origin"
    | "strict-origin"
    | "origin-when-cross-origin"
    | "strict-origin-when-cross-origin";

declare interface IWritableStreamWriter<T> {
    readonly closed: Promise<void>;
    readonly desiredSize: number | null;
    readonly ready: Promise<void>;

    abort(reason?: unknown): Promise<void>;
    close(): Promise<void>;
    releaseLock(): void;
    write(chunk: T): Promise<void>;
}

declare interface IWritableStream<T> {
    readonly locked: boolean;
    abort(reason?: unknown): Promise<void>;
    getWriter(): IWritableStreamWriter<T>;
}

declare interface IReadableStreamReadResult<T> {
    readonly value?: T;
    readonly done: boolean;
}

declare interface IReadableStreamReader<T> {
    readonly locked: boolean;
    cancel(reason?: unknown): Promise<void>;
    read(): Promise<IReadableStreamReadResult<T>>;
    releaseLock(): void;
}

declare interface IReadableStreamBYOBReader<T> {
    readonly locked: boolean;
    cancel(reason?: unknown): Promise<void>;
    read(view: Uint8Array): Promise<IReadableStreamReadResult<T>>;
    releaseLock(): void;
}

declare interface IReadableStreamGetReaderOptions {
    mode?: "byob";
}

declare interface IReadableStreamPipeOptions {
    preventAbort?: boolean;
    preventCancel?: boolean;
    preventClose?: boolean;
    signal?: AbortSignal;
}

declare interface IReadableStream<T> {
    readonly locked: boolean;
    cancel(reason?: unknown): Promise<void>;
    getReader(options: IReadableStreamGetReaderOptions): IReadableStreamReader<T> | IReadableStreamBYOBReader<T>;
    getReader(): IReadableStreamReader<T>;
    pipeThrough(
        pipes: {
            readable: IReadableStream<T>;
            writable: IWritableStream<T>;
        },
        options?: IReadableStreamPipeOptions
    ): IReadableStream<T>;

    pipeTo(dest: IWritableStream<T>, options?: IReadableStreamPipeOptions): Promise<void>;
    tee(): [IReadableStream<T>, IWritableStream<T>];
}

declare function fetch(req: string | Request | URL, init?: RequestInit): Promise<Response>;
