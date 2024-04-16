/* eslint-disable no-undef */
/* eslint-disable no-console */
/* eslint-disable @typescript-eslint/explicit-function-return-type */
/* eslint-disable @typescript-eslint/no-confusing-void-expression */
import child_process from "child_process";
import commonjs from "@rollup/plugin-commonjs";
import fs from "fs";
import replace from "@rollup/plugin-replace";
import resolve from "@rollup/plugin-node-resolve";
import strip from "@rollup/plugin-strip";
import terser from "@rollup/plugin-terser";
import typescript from "rollup-plugin-typescript2";

const minify = false;
const plugins = [
    resolve({
        preferBuiltins: false
    }),
    commonjs(),
    typescript({
        tsconfig: `tsconfig.json`,
        cacheRoot: ".cache",
    }),
    ...(minify
        ? [
            terser({
                output: {
                    ascii_only: true,
                    semicolons: false,
                    comments: false
                },
                compress: true,
                mangle: true
            })
        ]
        : [])
];
const builds = [{
    input: "./src/spurv.ts",
    plugins,
    external: [],
    output: {
        file: "./dist/spurv.js",
        format: "iife",
        name: "spurv",
        exports: "named",
        sourcemap: "inline"
    }
}];

export default builds;
