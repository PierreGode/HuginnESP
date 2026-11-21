"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.runCommand = runCommand;
exports.spawnCommand = spawnCommand;
const utils_subprocess_1 = require("@ionic/utils-subprocess");
const cross_spawn_1 = require("cross-spawn");
async function runCommand(command, args, options = {}) {
    // console.log(chalk`> {bold ${command} ${args.join(" ")}}`);
    const p = new utils_subprocess_1.Subprocess(command, args, options);
    try {
        // return await p.output();
        await p.run();
        return p.output();
    }
    catch (e) {
        if (e instanceof utils_subprocess_1.SubprocessError) {
            // old behavior of just throwing the stdout/stderr strings
            throw e.output
                ? e.output
                : e.code
                    ? e.code
                    : e.error
                        ? e.error.message
                        : "Unknown error";
        }
        throw e;
    }
}
async function spawnCommand(command, args, options = {}) {
    return new Promise((resolve, reject) => {
        var _a, _b;
        const child = (0, cross_spawn_1.spawn)(command, args, options);
        const stderr = [];
        const stdout = [];
        (_a = child.stdout) === null || _a === void 0 ? void 0 : _a.addListener('data', e => {
            stdout.push(e.toString());
        });
        (_b = child.stderr) === null || _b === void 0 ? void 0 : _b.addListener('data', e => {
            if (options.combineStreams) {
                stdout.push(e.toString());
            }
            else {
                stderr.push(e.toString());
            }
        });
        child.on('exit', e => {
        });
        child.on('error', e => {
            reject(e);
        });
        child.on('close', e => {
            if (e) {
                reject(stderr.join());
            }
            else {
                resolve(stdout.join(''));
            }
        });
    });
}
//# sourceMappingURL=subprocess.js.map