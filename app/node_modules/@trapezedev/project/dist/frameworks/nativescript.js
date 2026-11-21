"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.NativeScriptFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class NativeScriptFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return false;
        }
        if (!(await (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, 'nativescript.config.ts')))) {
            return null;
        }
        return new NativeScriptFramework();
    }
}
exports.NativeScriptFramework = NativeScriptFramework;
//# sourceMappingURL=nativescript.js.map