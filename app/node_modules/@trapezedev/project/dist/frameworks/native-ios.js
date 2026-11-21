"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.NativeIosFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const _1 = require(".");
class NativeIosFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return false;
        }
        const files = await (0, utils_fs_1.readdir)(project.config.projectRoot);
        if (!(files.some(f => f.indexOf('.xcodeproj') >= 0))) {
            return null;
        }
        return new NativeIosFramework();
    }
}
exports.NativeIosFramework = NativeIosFramework;
//# sourceMappingURL=native-ios.js.map