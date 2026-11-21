"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.PlistFile = void 0;
const plist_1 = __importDefault(require("plist"));
const path_1 = require("path");
const utils_fs_1 = require("@ionic/utils-fs");
const lodash_1 = require("lodash");
const plist_2 = require("./util/plist");
const vfs_1 = require("./vfs");
const logger_1 = require("./logger");
const fs_1 = require("./util/fs");
class PlistFile extends vfs_1.VFSStorable {
    constructor(path, vfs, project) {
        super();
        this.path = path;
        this.vfs = vfs;
        this.project = project;
        this.doc = null;
        this.plistCommitFn = async (file) => {
            var _a;
            const data = file.getData();
            const xml = plist_1.default.build((_a = data.getDocument()) !== null && _a !== void 0 ? _a : {}, {
                indent: '	', // Tab character
                offset: -1,
                newline: '\n'
            });
            await (0, fs_1.assertParentDirs)(file.getFilename());
            return (0, utils_fs_1.writeFile)(file.getFilename(), xml);
        };
        this.plistDiffFn = async (file) => {
            var _a;
            let old = '';
            try {
                old = await (0, utils_fs_1.readFile)(file.getFilename(), { encoding: 'utf-8' });
            }
            catch (e) { }
            const data = file.getData();
            const xml = plist_1.default.build((_a = data.getDocument()) !== null && _a !== void 0 ? _a : {}, {
                indent: '	', // Tab character
                offset: -1,
                newline: '\n'
            });
            return {
                old,
                new: xml
            };
        };
    }
    getDocument() {
        return this.doc;
    }
    setDocument(doc) {
        this.doc = doc;
        this.vfs.markModified(this.path);
    }
    async exists() {
        return (0, utils_fs_1.pathExists)(this.path);
    }
    async load() {
        var _a, _b, _c;
        if (this.vfs.isOpen(this.path)) {
            return;
        }
        if (await this.exists()) {
            this.doc = await (0, plist_2.parsePlist)(this.path);
        }
        else {
            this.doc = {};
            // Add the file to the project
            if (this.project) {
                const rel = (0, path_1.relative)((_b = (_a = this.project.config.ios) === null || _a === void 0 ? void 0 : _a.path) !== null && _b !== void 0 ? _b : '', this.path);
                (_c = this.project.ios) === null || _c === void 0 ? void 0 : _c.addFile(rel);
            }
        }
        logger_1.Logger.v('plist', 'read', `Loaded plist file at ${this.path}`, this.doc);
        this.vfs.open(this.path, this, this.plistCommitFn, this.plistDiffFn);
    }
    async setFromXml(xml) {
        const parsed = (0, plist_2.parsePlistString)(xml);
        this.setDocument(parsed);
    }
    async set(properties) {
        if (!this.doc) {
            return;
        }
        const merged = (0, lodash_1.mergeWith)(this.doc, properties, (objValue, srcValue) => {
            // Override the default merge behavior for arrays of objects that have the
            // same sub-key. Otherwise lodash merge doesn't work how we need it to
            if (Array.isArray(objValue)) {
                //if (replace) {
                return srcValue;
                //}
                /*
                const firstObjValue = objValue[0];
                const firstSrcValue = srcValue[0];
        
                // https://github.com/ionic-team/capacitor-configure/issues/32
                // When merging an array of dicts, like when modifying
                // CFBundleURLTypes, we don't want to union the two arrays because that
                // would result in duplicated array of dicts. Instead, we want to merge as-is.
                // This check makes sure we're not trying to union an array of dicts
                if (typeof firstObjValue !== 'object' && typeof firstSrcValue !== 'object') {
                  return union(objValue, srcValue);
                }
                */
            }
            else if (typeof objValue === 'object' && objValue !== null) {
                //if (replace) {
                return srcValue;
                //}
            }
        });
        Object.assign(this.doc, merged);
        this.vfs.markModified(this.path);
    }
    async merge(properties) {
        if (!this.doc) {
            return;
        }
        const merged = (0, lodash_1.mergeWith)(this.doc, properties, (objValue, srcValue) => {
            if (Array.isArray(objValue)) {
                return (0, lodash_1.union)(objValue, srcValue);
            }
        });
        Object.assign(this.doc, merged);
        this.vfs.markModified(this.path);
    }
    /**
     * This is confusing but this uses a different set algorithm than the above set and merge.
     * TODO: Get rid of this or make this behavior the default for set or merge
     */
    update(entries, replace = false) {
        const merged = (0, lodash_1.mergeWith)(this.doc, entries, (objValue, srcValue) => {
            // Override the default merge behavior for arrays of objects that have the
            // same sub-key. Otherwise lodash merge doesn't work how we need it to
            if (Array.isArray(objValue)) {
                if (replace) {
                    return srcValue;
                }
                const firstObjValue = objValue[0];
                const firstSrcValue = srcValue[0];
                // https://github.com/ionic-team/capacitor-configure/issues/32
                // When merging an array of dicts, like when modifying
                // CFBundleURLTypes, we don't want to union the two arrays because that
                // would result in duplicated array of dicts. Instead, we want to merge as-is.
                // This check makes sure we're not trying to union an array of dicts
                if (typeof firstObjValue !== 'object' && typeof firstSrcValue !== 'object') {
                    return (0, lodash_1.union)(objValue, srcValue);
                }
            }
            else if (typeof objValue === 'object' && objValue !== null) {
                if (replace) {
                    return srcValue;
                }
            }
        });
        this.setDocument(merged);
    }
}
exports.PlistFile = PlistFile;
//# sourceMappingURL=plist.js.map