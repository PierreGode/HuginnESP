"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.JsonFile = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const lodash_1 = require("lodash");
const logger_1 = require("./logger");
const fs_1 = require("./util/fs");
const vfs_1 = require("./vfs");
class JsonFile extends vfs_1.VFSStorable {
    constructor(path, vfs) {
        super();
        this.path = path;
        this.vfs = vfs;
        this.json = null;
        this.commitFn = async (file) => {
            var _a;
            await (0, fs_1.assertParentDirs)(file.getFilename());
            return (0, utils_fs_1.writeJson)(file.getFilename(), (_a = file.getData()) === null || _a === void 0 ? void 0 : _a.getDocument(), {
                spaces: 2
            });
        };
        this.diffFn = async (file) => {
            var _a;
            const oldJson = await (0, fs_1.readFileOrEmpty)(file.getFilename());
            const newJson = JSON.stringify((_a = file.getData()) === null || _a === void 0 ? void 0 : _a.getDocument(), null, 2);
            return {
                old: oldJson,
                new: newJson
            };
        };
    }
    getDocument() {
        return this.json;
    }
    async exists() {
        return (0, utils_fs_1.pathExists)(this.path);
    }
    async load() {
        if (this.vfs.isOpen(this.path)) {
            return;
        }
        if (await this.exists()) {
            this.json = await (0, utils_fs_1.readJson)(this.path);
        }
        else {
            this.json = {};
        }
        logger_1.Logger.v('json', 'read', this.json);
        this.vfs.open(this.path, this, this.commitFn, this.diffFn);
    }
    async set(properties) {
        if (!this.json) {
            return;
        }
        const merged = (0, lodash_1.mergeWith)(this.json, properties, (objValue, srcValue) => {
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
        Object.assign(this.json, merged);
        this.vfs.markModified(this.path);
    }
    async merge(properties) {
        if (!this.json) {
            return;
        }
        const merged = (0, lodash_1.mergeWith)(this.json, properties, (objValue, srcValue) => {
            if (Array.isArray(objValue)) {
                return (0, lodash_1.union)(objValue, srcValue);
            }
        });
        Object.assign(this.json, merged);
        this.vfs.markModified(this.path);
    }
}
exports.JsonFile = JsonFile;
//# sourceMappingURL=json.js.map