"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CapacitorFramework = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const _1 = require(".");
class CapacitorFramework extends _1.Framework {
    constructor() {
        super();
    }
    static async getFramework(project) {
        if (!project.config.projectRoot) {
            return null;
        }
        const paths = ['capacitor.config.ts', 'capacitor.config.js', 'capacitor.config.json'];
        if ((await Promise.all(paths.map(p => (0, utils_fs_1.pathExists)((0, path_1.join)(project.config.projectRoot, p))))).some(s => s)) {
            return new CapacitorFramework();
        }
        return null;
    }
}
exports.CapacitorFramework = CapacitorFramework;
//# sourceMappingURL=capacitor.js.map