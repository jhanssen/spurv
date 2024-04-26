// import { assert } from "./assert";

export function format(...args: unknown[]): string {
    let ret = "";
    args.forEach((arg: unknown) => {
        let str: string;
        switch (typeof arg) {
            case "undefined":
                str = "undefined";
                break;

            case "function":
                str = `function(${arg.name})`;
                break;

            case "object":
                if (!arg) {
                    str = "null";
                    break;
                }
                // ### need to format buffer sources etc, better stringifying too
                try {
                    str = JSON.stringify(arg);
                } catch (err: unknown) {
                    // ### not working for some reason
                    // assert(err instanceof Error, "Must be error");
                    // str = JSON.stringify({ error: err.message });
                    str = "Won't stringify it won't";
                }
                break;

            case "string":
            case "symbol":
            case "boolean":
            case "number":
            case "bigint":
                str = String(arg);
                break;
        }

        const last = ret[ret.length - 1];
        if (ret && last !== " " && last !== "\n") {
            ret += " ";
        }
        ret += str!;
    });
    return ret;
}
