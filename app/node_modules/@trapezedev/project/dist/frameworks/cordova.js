"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CordovaFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class CordovaFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return null;
        }
        const paths = ['config.xml'];
        if ((await Promise.all(paths.map(p => (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, p))))).some(s => s)) {
            return new CordovaFramework();
        }
        return null;
    }
}
exports.CordovaFramework = CordovaFramework;
//# sourceMappingURL=cordova.js.map