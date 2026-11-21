"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DotNetMauiFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class DotNetMauiFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return null;
        }
        if (!(await (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, 'App.xaml')))) {
            return null;
        }
        return new DotNetMauiFramework();
    }
}
exports.DotNetMauiFramework = DotNetMauiFramework;
//# sourceMappingURL=dotnet-maui.js.map