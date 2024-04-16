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
                try {
                    str = JSON.stringify(arg);
                } catch (err: unknown) {
                    str = arg.toString();
                }
                break;

            case "string":
            case "symbol":
            case "boolean":
            case "number":
                str = String(arg);
                break;
        }

        const last = ret[ret.length - 1];
        if (last !== " " && last !== "\n") {
            ret += " ";
        }
        ret += str!;
    });
    return ret;
}
