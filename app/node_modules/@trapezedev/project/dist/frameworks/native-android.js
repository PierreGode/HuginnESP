"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.NativeAndroidFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class NativeAndroidFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return false;
        }
        if (!(await (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, 'build.gradle')))) {
            return null;
        }
        return new NativeAndroidFramework();
    }
}
exports.NativeAndroidFramework = NativeAndroidFramework;
//# sourceMappingURL=native-android.js.map