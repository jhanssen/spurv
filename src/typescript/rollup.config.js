/* eslint-disable no-undef */
/* eslint-disable no-console */
/* eslint-disable @typescript-eslint/explicit-function-return-type */
/* eslint-disable @typescript-eslint/no-confusing-void-expression */
import commonjs from "@rollup/plugin-commonjs";
import resolve from "@rollup/plugin-node-resolve";
import terser from "@rollup/plugin-terser";
import typescript from "rollup-plugin-typescript2";

const minify = false;
const plugins = [
    resolve({
        preferBuiltins: false,
        modulePaths: ["./node_modules/"]
    }),
    commonjs(),
    typescript({
        tsconfig: "./tsconfig.json"
        // cacheRoot: ".cache",
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
const builds = [
    {
        input: "./src/index.ts",
        plugins,
        external: [],
        output: {
            file: "./dist/spurv.js",
            format: "iife",
            name: "spurv",
            exports: "named",
            sourcemap: "inline"
        }
    }
];

export default builds;
