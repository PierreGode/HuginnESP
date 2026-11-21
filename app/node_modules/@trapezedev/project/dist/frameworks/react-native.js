"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ReactNativeFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class ReactNativeFramework extends _1.Framework {
    constructor(isExpo) {
        super();
        this.isExpo = isExpo;
    }
    static async getFramework(project) {
        var _a;
        if (!project.config.projectRoot) {
            return null;
        }
        if (!(await (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, 'app.json')))) {
            return null;
        }
        const packageJson = (_a = (await (0, utils_fs_1.readJSON)((0, path_1.join)(project.config.projectRoot, 'package.json')))) !== null && _a !== void 0 ? _a : {};
        const deps = {
            ...packageJson.dependencies,
            ...packageJson.devDependencies
        };
        if (!('react-native' in deps)) {
            return null;
        }
        if ('expo' in deps) {
            return new ReactNativeFramework(true);
        }
        return new ReactNativeFramework(false);
    }
}
exports.ReactNativeFramework = ReactNativeFramework;
//# sourceMappingURL=react-native.js.map