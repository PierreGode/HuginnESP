"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.GradleFile = exports.GradleTargetNotFound = exports.WINDOWS_GRADLE_PARSE_CLASSPATH = void 0;
const path_1 = require("path");
const fs_1 = require("fs");
const os_1 = __importDefault(require("os"));
const utils_fs_1 = require("@ionic/utils-fs");
const subprocess_1 = require("../util/subprocess");
const text_1 = require("../util/text");
const vfs_1 = require("../vfs");
const detect_indent_1 = __importDefault(require("../util/detect-indent"));
const definitions_1 = require("../definitions");
const logger_1 = require("../logger");
const fs_2 = require("../util/fs");
// Windows does not expand the lib/* wildcard the POSIX classpath uses, so every jar
// vendored by @trapezedev/gradle-parse has to be listed here explicitly
exports.WINDOWS_GRADLE_PARSE_CLASSPATH = 'lib/groovy-3.0.9.jar;lib/json-20260814.jar;capacitor-gradle-parse.jar;.';
/**
 * Thrown when neither the property to replace nor the block to insert it into exists,
 * so callers can tell a missing target apart from a failed modification.
 */
class GradleTargetNotFound extends Error {
}
exports.GradleTargetNotFound = GradleTargetNotFound;
class GradleFile extends vfs_1.VFSStorable {
    constructor(filename, vfs) {
        super();
        this.filename = filename;
        this.vfs = vfs;
        this.source = null;
        this.parsed = null;
        this.gradleCommitFn = async (file) => {
            await (0, fs_2.assertParentDirs)(file.getFilename());
            return (0, utils_fs_1.writeFile)(file.getFilename(), file.getData().getDocument());
        };
        this.gradleDiffFn = async (file) => {
            var _a;
            let old = '';
            try {
                old = await (0, utils_fs_1.readFile)(file.getFilename(), { encoding: 'utf-8' });
            }
            catch (e) { }
            return {
                old,
                new: (_a = this.source) !== null && _a !== void 0 ? _a : '',
            };
        };
    }
    getDocument() {
        return this.source;
    }
    setSource(source) {
        this.source = source;
        this.vfs.markModified(this.filename);
    }
    /**
     * Replace the given properties at the specified point in the Gradle file or insert
     * if the replacement doesn't exist
     *
     * Every property of toReplace is resolved and replaced on its own, so the target can
     * point either at a single property or at the block containing the properties
     *
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    async replaceProperties(pathObject, toReplace, exact = false, type = definitions_1.AndroidGradleInjectType.Infer) {
        const keys = Object.keys(toReplace);
        for (const key of keys) {
            await this.replaceProperty(this._makePropertyPathObject(pathObject, key, keys), { [key]: toReplace[key] }, exact, type);
        }
    }
    async replaceProperty(pathObject, toReplace, exact, type) {
        await this.parse();
        if (!this.parsed) {
            throw new Error('Call parse() first to load Gradle file');
        }
        const found = this.find(pathObject, exact);
        if (!found.length) {
            // Create a parent selector object since we're going to insert instead
            const parent = this._makeReplacePathObject(pathObject, Object.keys(toReplace)[0]);
            const foundParent = this.find(parent, exact);
            if (foundParent.length) {
                return this.insertIntoGradleFile([toReplace], foundParent[0], type);
            }
            else {
                throw new GradleTargetNotFound('Unable to find target in Gradle file to replace or insert');
            }
        }
        const target = found[0];
        return this.replaceInGradleFile(toReplace, target, type);
    }
    // Build the path to a single property of the replacement object: the given path either
    // already ends at one of the replaced property names, which is then swapped for propertyKey,
    // or at the block holding the properties, where propertyKey is appended
    _makePropertyPathObject(pathObject, propertyKey, replaceKeys) {
        const path = [];
        let node = pathObject;
        while (node && Object.keys(node).length) {
            const key = Object.keys(node)[0];
            path.push(key);
            node = node[key];
        }
        const leaf = path.pop();
        if (typeof leaf !== 'undefined' && !replaceKeys.includes(leaf)) {
            path.push(leaf);
        }
        path.push(propertyKey);
        return path.reverse().reduce((child, key) => ({ [key]: child }), {});
    }
    // Build a new pathObject that is the path to the parent rather than
    // the path in pathObject
    _makeReplacePathObject(pathObject, injectKey) {
        let x = {};
        let y = x;
        let a = pathObject;
        while (a) {
            const keys = Object.keys(a);
            if (keys[0] === injectKey || !keys.length) {
                return y;
            }
            const o = {};
            x[keys[0]] = o;
            x = o;
            a = a[keys[0]];
        }
        return y;
    }
    /**
     * Replace an entry in the gradle file.
     */
    // This is a beast, sorry. Hey, at least there's tests
    // In the future, this could be moved to the Java `gradle-parse` package provided in this monorepo
    // along with modifying the AST to inject our script but this works fine for now
    async replaceInGradleFile(toInject, targetNode, type) {
        var _a;
        // These values are 1-indexed not 0-indexed
        //let { line, column, lastLine, lastColumn } = targetNode.node.source;
        let { line, column, lastLine, lastColumn } = targetNode.node.source;
        const source = (_a = (await this.getGradleSource())) !== null && _a !== void 0 ? _a : '';
        const sourceLines = source.split(/\r?\n/);
        if (line == -1) {
            // Set to first line (remember, 1-indexed)
            line = 1;
        }
        if (lastLine === -1) {
            // Set to last line (remember, 1-indexed)
            lastLine = sourceLines.length + 1;
        }
        const detectedIndent = (0, detect_indent_1.default)(source);
        let lines = [];
        this.createGradleSource([toInject], lines /* out */, detectedIndent.indent, undefined, targetNode.node, type);
        const resolvedLastLine = lastLine < 0 ? sourceLines.length : lastLine;
        const formatted = lines.join('\n');
        const indentAmount = targetNode.depth;
        const indented = (0, text_1.indent)(formatted, detectedIndent.indent, indentAmount - 1);
        // Replace the target lines with our new source line
        const newSource = sourceLines.slice(0, Math.max(0, line - 1)).join('\n') +
            '\n' +
            indented +
            '\n' +
            sourceLines
                .slice(Math.max(0, resolvedLastLine), sourceLines.length)
                .join('\n');
        this.setSource(newSource);
    }
    /**
     * Insert the given properties at the specified point in the Gradle file.
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    async insertProperties(pathObject, toInject, type = definitions_1.AndroidGradleInjectType.Method, exact = false) {
        await this.parse();
        if (!this.parsed) {
            throw new Error('Call parse() first to load Gradle file');
        }
        return this.insertIntoGradleFile(toInject, this.findInsertTarget(pathObject, exact), type);
    }
    /**
     * Inject the given properties at the specified point in the Gradle file.
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    async insertFragment(pathObject, toInject, exact = false) {
        await this.parse();
        if (!this.parsed) {
            throw new Error('Call parse() first to load Gradle file');
        }
        return this.insertIntoGradleFile(toInject, this.findInsertTarget(pathObject, exact), definitions_1.AndroidGradleInjectType.Infer);
    }
    /**
     * Statements are injected into the body of the target node, so prefer a method block
     * over an assignment that happens to share its name (`def dependencies = [...]` next
     * to a `dependencies { }` block).
     */
    findInsertTarget(pathObject, exact) {
        var _a;
        const found = this.find(pathObject, exact);
        const target = (_a = found.find(f => f.node.type === 'method')) !== null && _a !== void 0 ? _a : found[0];
        if (!target) {
            throw new Error('Unable to find method in Gradle file to inject');
        }
        return target;
    }
    /**
     * Parse the underlying Gradle file and build the AST. Note: this calls out to
     * a Java process which incurs some overhead and requires java to be installed
     * This is because Gradle is actually a DSL for the Groovy language, which is
     * a JVM language. Additionally, the Groovy parser is based on a modified version
     * of the Antlr project that is tightly bound to the JVM. Ultimatley, this means
     * the only safe, accurate way to feasibly build a Gradle AST is to use the Groovy
     * parser API which this uses under the hood.
     */
    async parse() {
        if (!(await (0, utils_fs_1.pathExists)(this.filename))) {
            throw new Error(`Unable to locate file at ${this.filename}`);
        }
        // The parser reads from a temp file rather than the file on disk so it operates on
        // the current, possibly modified, source and we can handle multiple modifications
        // to the same file in sequence
        const tempDir = (0, fs_1.mkdtempSync)((0, path_1.join)(os_1.default.tmpdir(), 'trapeze-'));
        const tempFile = (0, path_1.join)(tempDir, 'temp.gradle');
        await (0, utils_fs_1.writeFile)(tempFile, await this.getGradleSource());
        try {
            return await this.runParser(tempFile);
        }
        finally {
            try {
                await (0, utils_fs_1.remove)(tempDir);
            }
            catch (e) {
                logger_1.Logger.v('gradle', 'parse', `unable to remove temp dir ${tempDir}: ${e.message}`);
            }
        }
    }
    async runParser(tempFile) {
        const parserRoot = this.getGradleParserPath();
        const java = await this.getJava();
        if (!java) {
            throw new Error(this.gradleParseError());
        }
        logger_1.Logger.v('gradle', 'parse', `running Gradle parse with Java path ${java}`);
        logger_1.Logger.v('gradle', 'parse', `read gradle file at ${this.filename}`);
        try {
            let json = null;
            if (os_1.default.platform() === 'win32') {
                json = await (0, subprocess_1.spawnCommand)(java, [
                    '-cp',
                    exports.WINDOWS_GRADLE_PARSE_CLASSPATH,
                    'com.capacitorjs.gradle.Parse',
                    tempFile,
                ], {
                    cwd: parserRoot,
                    stdio: 'pipe',
                });
            }
            else {
                json = await (0, subprocess_1.spawnCommand)(java, [
                    '-cp',
                    'lib/*:capacitor-gradle-parse.jar:.',
                    'com.capacitorjs.gradle.Parse',
                    tempFile,
                ], {
                    cwd: parserRoot,
                    stdio: 'pipe',
                });
            }
            this.parsed = JSON.parse(json || '{}');
            return this.parsed;
        }
        catch (e) {
            throw new Error(`Unable to load or parse gradle file: ${e}`);
        }
    }
    /**
     * Inject a modification into the gradle file.
     */
    // This is a beast, sorry. Hey, at least there's tests
    // In the future, this could be moved to the Java `gradle-parse` package provided in this monorepo
    // along with modifying the AST to inject our script but this works fine forn ow
    async insertIntoGradleFile(toInject, targetNode, type) {
        var _a;
        // These values are 1-indexed not 0-indexed
        let { line, column, lastLine, lastColumn } = targetNode.node.source;
        const source = (_a = (await this.getGradleSource())) !== null && _a !== void 0 ? _a : '';
        const sourceLines = source.split(/\r?\n/);
        if (line == -1) {
            // Set to first line (remember, 1-indexed)
            line = 1;
        }
        if (lastLine === -1) {
            // Set to last line (remember, 1-indexed)
            lastLine = sourceLines.length + 1;
        }
        const detectedIndent = (0, detect_indent_1.default)(source);
        let lines = [];
        if (Array.isArray(toInject)) {
            this.createGradleSource(toInject, lines /* out */, detectedIndent.indent, undefined, targetNode.node, type);
        }
        else {
            lines = toInject.split(/\r?\n/);
        }
        const resolvedLastLine = lastLine < 0 ? sourceLines.length : lastLine;
        const formatted = lines.join('\n');
        const indentAmount = targetNode.depth;
        let newSource = null;
        if (line === lastLine) {
            // Block is empty, like dependencies {}
            const indented = (0, text_1.indent)(formatted, detectedIndent.indent, indentAmount);
            const sourceLine = sourceLines[line - 1];
            // The new line is the slice from the start of the line to one character before the end (remember,
            // the lines and columns are 1-indexed so lastColumn - 2 is one character before the end
            const newLine = sourceLine.slice(0, Math.max(0, lastColumn - 2)) +
                '\n' +
                indented +
                '\n' +
                (0, text_1.indent)(sourceLine.slice(Math.max(0, lastColumn - 2)).trim(), detectedIndent.indent, Math.max(0, indentAmount - 1));
            newSource =
                sourceLines.slice(0, Math.max(0, resolvedLastLine - 1)).join('\n') +
                    '\n' +
                    newLine +
                    '\n' +
                    sourceLines
                        .slice(Math.max(0, resolvedLastLine), sourceLines.length)
                        .join('\n');
        }
        else {
            const indented = (0, text_1.indent)(formatted, detectedIndent.indent, indentAmount);
            newSource =
                sourceLines.slice(0, Math.max(0, resolvedLastLine - 1)).join('\n') +
                    '\n' +
                    indented +
                    '\n' +
                    sourceLines
                        .slice(Math.max(0, resolvedLastLine - 1), sourceLines.length)
                        .join('\n');
        }
        this.setSource(newSource);
    }
    find(pathObject, exact = false) {
        var _a;
        if (!this.parsed) {
            throw new Error('Call parse() first to load Gradle file');
        }
        // Null or empty object means the root node
        if (!pathObject || !Object.keys(pathObject).length) {
            const firstChild = (_a = this.parsed.children) === null || _a === void 0 ? void 0 : _a[0];
            if (firstChild) {
                return [{ node: firstChild, depth: 0 }];
            }
            return [];
        }
        const found = [];
        this._find(pathObject, this.parsed, pathObject, exact, [], found);
        return found;
    }
    _find(pathObject, node, pathNode, exact, pathToNode, found, depth = 0) {
        var _a, _b;
        if (!pathNode) {
            return;
        }
        const targetKey = (_a = Object.keys(pathNode)) === null || _a === void 0 ? void 0 : _a[0];
        if (!targetKey) {
            return;
        }
        for (const c of ((_b = node.children) !== null && _b !== void 0 ? _b : [])) {
            // Descend into a copy of the cursor so siblings that share the target name are
            // each matched against the full remaining path
            let childPathNode = pathNode;
            if (this.isTargetNode(c) && c.name === targetKey) {
                childPathNode = pathNode[targetKey];
                if (!childPathNode || Object.keys(childPathNode).length == 0) {
                    // We've run out of path nodes to match
                    if (!exact) {
                        found.push({ node: c, depth });
                    }
                    else if (exact && this.matchesExact(pathObject, c, [...pathToNode, c])) {
                        found.push({ node: c, depth });
                    }
                }
            }
            const newPathToNode = this.isTargetNode(node) ? [...pathToNode, node] : pathToNode;
            this._find(pathObject, c, childPathNode, exact, newPathToNode, found, c.type === 'block' ? depth + 1 : depth);
        }
    }
    getSource(node) {
        if (!this.parsed || !this.source) {
            throw new Error('Call parse() first to load Gradle file');
        }
        const lines = this.source.split(/\r?\n/);
        const sourceLines = lines.slice(node.source.line - 1, node.source.lastLine);
        const firstLine = sourceLines[0].slice(Math.max(0, node.source.column - 1));
        const lastLine = sourceLines[sourceLines.length - 1].slice(0, node.source.lastColumn);
        if (sourceLines.length > 2) {
            return [firstLine, ...sourceLines.slice(1, sourceLines.length - 1), lastLine].join('\n');
        }
        else if (sourceLines.length == 2) {
            return [firstLine, lastLine].join('\n');
        }
        else {
            return firstLine;
        }
    }
    getDepth(pathObject) {
        let depth = 0;
        let n = pathObject;
        while (n) {
            const keys = Object.keys(n);
            if (keys.length > 0) {
                depth++;
                n = n[keys[0]];
            }
            else {
                break;
            }
        }
        return depth;
    }
    // When doing an exact match, need to check the path to the node
    // and verify the hierarchy matches
    matchesExact(pathObject, node, pathToNode) {
        var _a;
        const targetDepth = this.getDepth(pathObject);
        const currentDepth = pathToNode.length;
        if (currentDepth != targetDepth) {
            return false;
        }
        let n = pathObject;
        let m = pathToNode;
        while (n && m) {
            const key = Object.keys(n)[0];
            if (key && key !== ((_a = m[0]) === null || _a === void 0 ? void 0 : _a.name)) {
                return false;
            }
            n = n[key];
            m = m.slice(1);
        }
        return true;
    }
    isTargetNode(node) {
        return node.type === 'method' || node.type === 'variable';
    }
    async getJava() {
        try {
            if (process.env.JAVA_HOME) {
                return (0, path_1.join)(process.env.JAVA_HOME, 'bin', 'java');
            }
            const v = await (0, subprocess_1.spawnCommand)('java', ['-version'], {
                stdio: 'pipe',
                combineStreams: true
            });
            if (!v) {
                throw new Error('Unable to find java on PATH');
            }
            return 'java';
        }
        catch (e) {
        }
        return null;
    }
    getGradleParserPath() {
        return (0, path_1.dirname)(require.resolve('@trapezedev/gradle-parse'));
    }
    async setApplicationId(applicationId) {
        const source = await this.getGradleSource();
        if (source) {
            logger_1.Logger.v('gradle', 'setApplicationId', `to ${applicationId} in ${this.filename}`);
            try {
                await this.replaceProperties({
                    android: {
                        defaultConfig: {
                            applicationId: {}
                        }
                    }
                }, {
                    applicationId: `"${applicationId}"`
                });
            }
            catch (e) {
                // An app build.gradle without an android.defaultConfig block is valid, the
                // applicationId then defaults to the namespace and there is nothing to set
                if (!(e instanceof GradleTargetNotFound)) {
                    throw e;
                }
                logger_1.Logger.warn(`Unable to set the applicationId in ${this.filename}: no android.defaultConfig block found`);
            }
        }
    }
    async getApplicationId() {
        const source = await this.getGradleSource();
        if (source) {
            const applicationId = source.match(/applicationId\s*=?\s*["']([^"']+)["']/);
            if (!applicationId) {
                return null;
            }
            return applicationId[1];
        }
        return null;
    }
    async setVersionCode(versionCode) {
        const source = await this.getGradleSource();
        if (source) {
            logger_1.Logger.v('gradle', 'setVersionCode', `to ${versionCode} in ${this.filename}`);
            return this.replaceProperties({
                android: {
                    defaultConfig: {
                        versionCode: {}
                    }
                }
            }, {
                versionCode
            });
        }
    }
    async getVersionCode() {
        const source = await this.getGradleSource();
        if (source) {
            // The word boundary keeps properties that start with versionCode, such as the
            // versionCodeOverride of an APK split, from being read as the version code
            const versionCode = source.match(/versionCode\b\s*=?\s*(\w+)/);
            if (!versionCode) {
                return null;
            }
            return parseInt(versionCode[1]);
        }
        return null;
    }
    async incrementVersionCode() {
        const versionCode = await this.getVersionCode();
        if (versionCode === null || isNaN(versionCode)) {
            return;
        }
        logger_1.Logger.v('gradle', 'incrementVersionCode', `to ${versionCode + 1} in ${this.filename}`);
        return this.setVersionCode(versionCode + 1);
    }
    async setVersionName(versionName) {
        const source = await this.getGradleSource();
        if (source) {
            logger_1.Logger.v('gradle', 'setVersionName', `to ${versionName} in ${this.filename}`);
            return this.replaceProperties({
                android: {
                    defaultConfig: {
                        versionName: {}
                    }
                }
            }, {
                versionName: `"${versionName}"`
            });
        }
    }
    async getVersionName() {
        const source = await this.getGradleSource();
        if (source) {
            const versionName = source.match(/versionName\s*=?\s*["']([^"']+)["']/) || null;
            if (!versionName) {
                return null;
            }
            return versionName[1];
        }
        return null;
    }
    async setVersionNameSuffix(versionNameSuffix) {
        const source = await this.getGradleSource();
        if (source) {
            logger_1.Logger.v('gradle', 'setVersionNameSuffix', `to ${versionNameSuffix} in ${this.filename}`);
            return this.replaceProperties({
                android: {
                    defaultConfig: {
                        versionNameSuffix: {}
                    }
                }
            }, {
                versionNameSuffix: `"${versionNameSuffix}"`
            });
        }
    }
    async getVersionNameSuffix() {
        const source = await this.getGradleSource();
        if (source) {
            const versionName = source.match(/versionNameSuffix\s*=?\s*["']([^"']+)["']/) || null;
            if (!versionName) {
                return null;
            }
            return versionName[1];
        }
        return null;
    }
    async getNamespace() {
        const source = await this.getGradleSource();
        if (source) {
            const namespace = source.match(/namespace\s*=?\s*["']([^"']+)["']/);
            if (!namespace) {
                return null;
            }
            return namespace[1];
        }
        return null;
    }
    async setNamespace(namespace) {
        const source = await this.getGradleSource();
        if (source) {
            logger_1.Logger.v('gradle', 'setNamespace', `to ${namespace} in ${this.filename}`);
            return this.replaceProperties({
                android: {
                    namespace: {}
                }
            }, {
                namespace: `"${namespace}"`
            });
        }
    }
    /*
    Generate a fragment of Gradle/Groovy code given the inject object
  
    A gradle edit will be of the form:
  
    [
      {
        maven: [{
          url: 'https://pkgs.dev.azure.com/MicrosoftDeviceSDK/DuoSDK-Public/_packaging/Duo-SDK-Feed/maven/v1',
          name: 'Duo-SDK-Feed'
        }]
      }
    ]
    */
    createGradleSource(injectObj, lines, indentation, depth = 0, targetNode, type) {
        for (const entry of injectObj) {
            const keys = Object.keys(entry);
            for (const key of keys) {
                const editEntry = entry[key];
                if (Array.isArray(editEntry)) {
                    if (typeof editEntry[0] === 'object') {
                        lines.push(`${key} {`);
                        this.createGradleSource(editEntry, lines, indentation, depth + 1, targetNode, type);
                        lines.push('}');
                    }
                    else {
                        if (this.isVariableEntry(targetNode, type)) {
                            lines.push(`${key} = ${JSON.stringify(editEntry)}`);
                        }
                        else {
                            lines.push(`${key} ${editEntry}`);
                        }
                    }
                }
                else if (typeof editEntry === 'string' ||
                    typeof editEntry === 'number' ||
                    typeof editEntry === 'boolean') {
                    if (this.isVariableEntry(targetNode, type)) {
                        lines.push((0, text_1.indent)(`${key} = ${editEntry}`, indentation, depth));
                    }
                    else {
                        lines.push((0, text_1.indent)(`${key} ${editEntry}`, indentation, depth));
                    }
                }
                else {
                    const fields = Object.keys(editEntry);
                    for (const fieldKey of fields) {
                        const fieldEntry = editEntry[fieldKey];
                        if (typeof fieldEntry === 'string') {
                            if (this.isVariableEntry(targetNode, type)) {
                                lines.push((0, text_1.indent)(`${fieldKey} = ${fieldEntry}`, indentation, depth));
                            }
                            else {
                                lines.push((0, text_1.indent)(`${fieldKey} ${fieldEntry}`, indentation, depth));
                            }
                        }
                        else if (Array.isArray(fieldEntry)) {
                            lines.push('{');
                            this.createGradleSource(fieldEntry, lines, indentation, depth + 1, targetNode, type);
                            lines.push('}');
                        }
                    }
                }
            }
        }
    }
    // Generate a variable assignment (`key = value`) instead of a method call (`key value`)
    // if the target node is a variable or the provided type is a variable
    isVariableEntry(targetNode, type) {
        return targetNode.type === 'variable' || type === definitions_1.AndroidGradleInjectType.Variable;
    }
    async getGradleSource() {
        var _a, _b;
        const ref = this.vfs.get(this.filename);
        if (ref) {
            return (_b = (_a = ref.getData()) === null || _a === void 0 ? void 0 : _a.getDocument()) !== null && _b !== void 0 ? _b : '';
        }
        const contents = await (0, utils_fs_1.readFile)(this.filename, { encoding: 'utf-8' });
        this.source = contents;
        this.vfs.open(this.filename, this, this.gradleCommitFn, this.gradleDiffFn);
        return contents;
    }
    gradleParseError() {
        return `java not found on path and JAVA_HOME not set. Please set JAVA_HOME to the root of your Java installation.\n\nGradle parse functionality depends on a local Java install for accurate Gradle file modification.`;
    }
}
exports.GradleFile = GradleFile;
//# sourceMappingURL=gradle-file.js.map