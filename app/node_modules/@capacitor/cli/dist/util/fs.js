"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.readdirp = exports.deleteFolderRecursive = exports.convertToUnixPath = void 0;
const fs_1 = require("fs");
const promises_1 = require("fs/promises");
const fs_extra_1 = require("fs-extra");
const path_1 = require("path");
const convertToUnixPath = (path) => {
    return path.replace(/\\/g, '/');
};
exports.convertToUnixPath = convertToUnixPath;
const deleteFolderRecursive = (directoryPath) => {
    if ((0, fs_extra_1.existsSync)(directoryPath)) {
        (0, fs_extra_1.readdirSync)(directoryPath).forEach((file) => {
            const curPath = (0, path_1.join)(directoryPath, file);
            if ((0, fs_extra_1.lstatSync)(curPath).isDirectory()) {
                (0, exports.deleteFolderRecursive)(curPath);
            }
            else {
                (0, fs_extra_1.unlinkSync)(curPath);
            }
        });
        (0, fs_extra_1.rmdirSync)(directoryPath);
    }
};
exports.deleteFolderRecursive = deleteFolderRecursive;
async function readdirp(dir, { filter }) {
    const dirContent = await (0, promises_1.readdir)(dir, { recursive: true });
    const dirContentWalker = [];
    const filteredContent = [];
    dirContent.forEach((element) => {
        const path = (0, path_1.join)(dir, element);
        const stats = (0, fs_1.statSync)(path);
        dirContentWalker.push({ path, stats });
    });
    dirContentWalker.forEach((element) => {
        if (filter(element)) {
            filteredContent.push(element.path);
        }
    });
    return filteredContent;
}
exports.readdirp = readdirp;
