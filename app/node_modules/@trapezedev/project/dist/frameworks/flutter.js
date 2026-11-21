"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FlutterFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class FlutterFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return false;
        }
        if (!(await (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, 'pubspec.yaml')))) {
            return null;
        }
        return new FlutterFramework();
    }
}
exports.FlutterFramework = FlutterFramework;
//# sourceMappingURL=flutter.js.map