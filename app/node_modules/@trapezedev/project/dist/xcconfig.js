"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.XCConfigFile = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const logger_1 = require("./logger");
const fs_1 = require("./util/fs");
const vfs_1 = require("./vfs");
/**
 * iOS .strings files
 */
class XCConfigFile extends vfs_1.VFSStorable {
    constructor(path, vfs, project) {
        super();
        this.path = path;
        this.vfs = vfs;
        this.project = project;
        this.doc = "";
        // Match key = value pairs that are terminated
        // by newlines or by the start of comments
        this.keyValueRegex = /^\s*([^ \/]+)\s*=[^\S\r\n]*(([^\n;](?!\/\/))*)/gm;
        this.commitFn = async (file) => {
            const src = file.getData().generate();
            await (0, fs_1.assertParentDirs)(file.getFilename());
            return (0, utils_fs_1.writeFile)(file.getFilename(), src);
        };
        this.diffFn = async (file) => {
            return {
                old: await (0, fs_1.readFileOrEmpty)(file.getFilename()),
                new: file.getData().generate(),
            };
        };
    }
    getDocument() {
        return this.doc;
    }
    getPairs() {
        var _a;
        const found = this.doc.matchAll(this.keyValueRegex);
        const pairs = {};
        for (const group of found) {
            pairs[group[1]] = (_a = group[2].trimEnd()) !== null && _a !== void 0 ? _a : '';
        }
        return pairs;
    }
    async set(values) {
        logger_1.Logger.v('xcconfig', 'update', `${this.path}`);
        const foundKeys = [];
        function replace(match, key, value) {
            var _a;
            // Return the new key/value pair
            if (key in values) {
                foundKeys.push(key);
                const newValue = (_a = values[key]) !== null && _a !== void 0 ? _a : '';
                return `${key} = ${newValue}`;
            }
            return match;
        }
        this.doc = this.doc.replace(this.keyValueRegex, replace);
        const newKeys = Object.keys(values).filter(k => !!!foundKeys.find(fk => fk === k));
        for (const key of newKeys) {
            this.doc += `\n${key} = ${values[key]}`;
        }
        this.vfs.markModified(this.path);
    }
    async load() {
        var _a, _b, _c;
        if (this.vfs.isOpen(this.path)) {
            return;
        }
        if (!await (0, utils_fs_1.pathExists)(this.path)) {
            this.doc = "";
            if (this.project) {
                const rel = (0, path_1.relative)((_b = (_a = this.project.config.ios) === null || _a === void 0 ? void 0 : _a.path) !== null && _b !== void 0 ? _b : '', this.path);
                (_c = this.project.ios) === null || _c === void 0 ? void 0 : _c.addFile(rel);
            }
        }
        else {
            this.doc = await this.parse(this.path);
        }
        logger_1.Logger.v('xcconfig', 'load', `at ${this.path}`);
        this.vfs.open(this.path, this, this.commitFn, this.diffFn);
    }
    generate() {
        return this.doc;
    }
    async parse(path) {
        const contents = await (0, utils_fs_1.readFile)(path, { encoding: 'utf-8' });
        return contents;
    }
}
exports.XCConfigFile = XCConfigFile;
//# sourceMappingURL=xcconfig.js.map