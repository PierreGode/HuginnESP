"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.extractTemplate = void 0;
const fs_extra_1 = require("fs-extra");
const tar_1 = require("tar");
async function extractTemplate(src, dir) {
    await (0, fs_extra_1.mkdirp)(dir);
    await (0, tar_1.extract)({ file: src, cwd: dir });
}
exports.extractTemplate = extractTemplate;
