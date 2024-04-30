#!/usr/bin/env node

const path = require("node:path");
const fs = require("node:fs/promises");
const child_process = require("node:child_process");

const src_package_json = path.join(__dirname, "package.json");
const src_package_json_lock = path.join(__dirname, "package-lock.json");
const build_package_json = path.join(process.cwd(), "package.json");
const build_package_json_lock = path.join(process.cwd(), "package-lock.json");

async function stat(file) {
    try {
        const ret = await fs.stat(file);
        return ret.mtimeMs;
    } catch (err) {
        return 0;
    }
}

let stderr = "";
function onStderr(data) {
    stderr += data.toString();
    const lines = stderr.split("\n");
    stderr = lines[lines.length - 1];
    for (let i=0; i<lines.length - 1; ++i) {
        let line = lines[i];
        if (line.startsWith("[!] (plugin rpt2) Error: ")) {
            line = line.substring(25);
        }

        const idx = line.indexOf("../spurv/src/typescript/");
        if (idx !== -1) {
            const start = line.lastIndexOf(" ", idx);
            line = `${line.substring(0, start)} ${__dirname}${line.substring(idx + 23)}`;
        }

        console.error(line);
    }
}

function spawn(command, args, options) {
    return new Promise((resolve, reject) => {
        const proc = child_process.spawn(command, args, options);
        proc.on("exit", async (code) => {
            if (!code) {
                resolve();
            } else {
                await removeBuildPackageJson();
                reject(new Error(`${command} ${args.join(" ")} failed with code ${code}`));
            }
        });
        proc.stderr.on("data", onStderr);
    });
}

async function removeBuildPackageJson() {
    try {
        await fs.unlink(build_package_json);
    } catch (err) {
    }
}

async function npm_install() {
    try {
        await fs.copyFile(src_package_json, build_package_json);
        await fs.copyFile(src_package_json_lock, build_package_json_lock);
        await spawn("npm", ["install"]);
    } catch (err) {
        await removeBuildPackageJson();
        throw err;
    }
}

async function copyTsConfig() {
    const tsconfigSrc = path.join(__dirname, "tsconfig.json");
    const srcStat = await stat(tsconfigSrc);
    const tsconfigBuild = path.join(process.cwd(), "tsconfig.json");
    const buildStat = await stat(tsconfigBuild);
    if (buildStat >= srcStat) {
        return;
    }
    const contents = (await fs.readFile(tsconfigSrc, "utf8")).replaceAll("./", path.join(__dirname, "/"));
    await fs.writeFile(tsconfigBuild, contents);
}

async function copyRollupConfig() {
    const rollupConfigSrc = path.join(__dirname, "rollup.config.js");
    const srcStat = await stat(rollupConfigSrc);
    const rollupConfigBuild = path.join(process.cwd(), "rollup.config.js");
    const buildStat = await stat(rollupConfigBuild);
    if (buildStat >= srcStat) {
        return;
    }
    const contents = (await fs.readFile(rollupConfigSrc, "utf8")).
          replaceAll("./dist/", path.join(process.cwd(), "dist", "/")).
          replaceAll("./src/index.ts", path.join(__dirname, "src", "index.ts"));

    await fs.writeFile(rollupConfigBuild, contents);
}

async function eslint() {
    const eslintPath = path.join(process.cwd(), "node_modules", ".bin", "eslint");
    const eslintConfigSrc = path.join(__dirname, ".eslintrc");
    const eslintConfigBuild = path.join(process.cwd(), ".eslintrc");
    await fs.copyFile(eslintConfigSrc, eslintConfigBuild);
    const srcDir = path.join(__dirname, "src");
    const env = { ...process.env };
    env.NODE_PATH = path.join(process.cwd(), "node_modules");
    env.FORCE_COLOR = "0"
    await spawn(eslintPath, [ "--config", eslintConfigBuild, srcDir, "--format", "unix" ], { env });
}

async function rollup() {
    const rollupPath = path.join(process.cwd(), "node_modules", ".bin", "rollup");
    const rollupConfig = path.join(process.cwd(), "rollup.config.js");
    const env = { ...process.env };
    env.NODE_PATH = path.join(process.cwd(), "node_modules");
    env.FORCE_COLOR = "0"
    await spawn(rollupPath, [ "--config", rollupConfig ], { env });
}

(async function () {
    const src_package_json_stat = await stat(src_package_json);
    const build_package_json_stat = await stat(build_package_json);
    // console.log(src_package_json_stat, build_package_json_stat);
    if (!src_package_json_stat) {
        throw new Error(`Can't stat ${src_package_json}`);
    }

    if (src_package_json_stat > build_package_json_stat) {
        await npm_install();
    }

    await Promise.all([copyTsConfig(), copyRollupConfig()]);

    await Promise.all([rollup(), eslint()]);
})();
