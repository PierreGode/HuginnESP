"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.assertParentDirs = assertParentDirs;
exports.readFileOrEmpty = readFileOrEmpty;
exports.listFilesRecursive = listFilesRecursive;
const utils_fs_1 = require("@ionic/utils-fs");
const promises_1 = require("fs/promises");
const path_1 = require("path");
async function assertParentDirs(path) {
    const dirs = (0, path_1.dirname)(path);
    await (0, utils_fs_1.mkdirp)(dirs);
}
/**
 * Read a file, treating a file that doesn't exist yet as empty. Diffing a file an
 * operation is about to create has no previous contents to compare against.
 */
async function readFileOrEmpty(path) {
    try {
        return await (0, utils_fs_1.readFile)(path, { encoding: 'utf-8' });
    }
    catch (e) {
        if (e.code === 'ENOENT') {
            return '';
        }
        throw e;
    }
}
/**
 * List every file below the given directory, sub directories included.
 */
async function listFilesRecursive(dir) {
    const entries = await (0, promises_1.readdir)(dir, { withFileTypes: true });
    const files = await Promise.all(entries.map(entry => {
        const path = (0, path_1.join)(dir, entry.name);
        return entry.isDirectory() ? listFilesRecursive(path) : [path];
    }));
    return files.flat();
}
//# sourceMappingURL=fs.js.map