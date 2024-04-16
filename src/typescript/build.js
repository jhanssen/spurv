#!/usr/bin/env node

const path = require("path");
const fs = require("fs");
const child_process = require("node:child_process");

const src_package_json = path.join(__dirname, "package.json");
const src_package_json_lock = path.join(__dirname, "package-lock.json");
const build_package_json = path.join(process.cwd(), "package.json");
const build_package_json_lock = path.join(process.cwd(), "package-lock.json");

function stat(file) {
    try {
        return fs.statSync(file).mtimeMs;
    } catch (err) {
        return 0;
    }
}

function spawn(command, args) {
    const proc = child_process.spawn(command, args, { stdio: "inherit" });
    proc.on("exit", async (code) => {
        if (!code) {
            resolve();
        } else {
            await removeBuildPackageJson();
            reject(new Error(`${command} ${args} failed with code ${code}`));
        }
    });
}

async function removeBuildPackageJson() {
    try {
        await fs.promises.unlink(build_package_json);
    } catch (err) {
    }
}

async function npm_install() {
    try {
        await fs.promises.copyFile(src_package_json, build_package_json);
        await fs.promises.copyFile(src_package_json_lock, build_package_json_lock);
        await spawn("npm", ["install"], { stdio: "inherit" });
    } catch (err) {
        await removeBuildPackageJson();
        throw err;
    }
}

async function copyTsConfig() {
    const tsconfigSrc = path.join(__dirname, "tsconfig.json");
    const tsconfigBuild = path.join(process.cwd(), "tsconfig.json");
    await fs.promises.copyFile(tsconfigSrc, tsconfigBuild);

}

async function eslint() {
    const eslintPath = path.join(process.cwd(), "node_modules", ".bin", "eslint");
    const eslintConfigSrc = path.join(__dirname, ".eslintrc");
    const eslintConfigBuild = path.join(process.cwd(), ".eslintrc");
    await fs.promises.copyFile(eslintConfigSrc, eslintConfigBuild);
    const srcDir = path.join(__dirname, "src");
    await spawn(eslintPath, [ "--config", eslintConfigBuild, srcDir ]);
}

async function rollup() {
    // const eslintPath = path.join(process.cwd(), "node_modules", ".bin", "rollup");
    // const eslintConfig = path.join(__dirname, ".eslintrc");
    // const srcDir = path.join(__dirname, "src");
    // await spawn(eslintPath, [ "--config", eslintConfig, srcDir ]);
}

(async function () {
    const src_package_json_stat = stat(src_package_json);
    const build_package_json_stat = stat(build_package_json);
    // console.log(src_package_json_stat, build_package_json_stat);
    if (!src_package_json_stat) {
        throw new Error(`Can't stat ${src_package_json}`);
    }

    if (src_package_json_stat > build_package_json_stat) {
        await npm_install();
    }

    await copyTsConfig();

    await Promise.all([rollup(), eslint()]);
})();
