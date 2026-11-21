"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.readSource = readSource;
const utils_fs_1 = require("@ionic/utils-fs");
// Supporting reading files from either a path or URL
async function readSource(pathOrUrl) {
    if (/^(https?:\/\/)/.test(pathOrUrl)) {
        const res = await fetch(pathOrUrl);
        return res.text();
    }
    return (0, utils_fs_1.readFile)(pathOrUrl);
}
//# sourceMappingURL=read-src.js.map