"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.parsePbxProject = parsePbxProject;
exports.pbxSerializeString = pbxSerializeString;
exports.pbxReadString = pbxReadString;
const xcode_1 = __importDefault(require("xcode"));
const utils_fs_1 = require("@ionic/utils-fs");
async function parsePbxProject(filename) {
    if (!(await (0, utils_fs_1.pathExists)(filename))) {
        throw new Error(`pbxproj file does not exist at ${filename}`);
    }
    const proj = xcode_1.default.project(filename);
    return proj.parseSync();
}
// Xcode leaves a value unquoted only when it consists solely of these characters,
// see http://danwright.info/blog/2010/10/xcode-pbxproject-files/
const PBX_UNQUOTED_VALUE = /^[A-Za-z0-9_$./]+$/;
/**
 * PBX files are esoteric. Quote every value that Xcode itself would quote,
 * otherwise the pbxproj we write can no longer be parsed (a `,` terminates a
 * value, and non-ASCII characters are not valid in an unquoted value).
 * Inside a quoted value, a backslash and a double quote have to be escaped.
 */
function pbxSerializeString(value) {
    if (!PBX_UNQUOTED_VALUE.test(value)) {
        return `"${value.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`;
    }
    return value;
}
/**
 * The pbx parser hands quoted values back verbatim, quotes and escapes
 * included, so undo what `pbxSerializeString` wrote.
 */
function pbxReadString(value) {
    if (typeof value !== 'string') {
        return value;
    }
    if (value.length >= 2 && value.startsWith('"') && value.endsWith('"')) {
        return value.slice(1, -1).replace(/\\([\\"])/g, '$1');
    }
    return value;
}
//# sourceMappingURL=pbx.js.map