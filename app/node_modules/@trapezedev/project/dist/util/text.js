"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.indent = indent;
exports.getIndentation = getIndentation;
function indent(s, char, amount) {
    if (amount < 0) {
        return '';
    }
    const lines = s.split(/\r?\n/);
    const indentChars = new Array(amount).fill(char).join('');
    const indentedLines = lines.map((l, i) => {
        // Don't indent empty lines that are first/last as those are just filler newlines
        if (l === '') {
            // If this is the first line, return the indented line
            if (i === 0) {
                return indentChars;
            }
            else if (i === lines.length - 1) {
                return '';
            }
        }
        return indentChars + l;
    });
    return indentedLines.join('\n');
}
;
function getIndentation(line) {
    var _a;
    return (_a = line.match(/(^\s+)/)) === null || _a === void 0 ? void 0 : _a[0];
}
//# sourceMappingURL=text.js.map