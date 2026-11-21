"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.AndroidProject = void 0;
const path_1 = require("path");
const utils_fs_1 = require("@ionic/utils-fs");
const promises_1 = require("fs/promises");
const gradle_file_1 = require("./gradle-file");
const xml_1 = require("../xml");
const properties_1 = require("../properties");
const platform_project_1 = require("../platform-project");
const read_src_1 = require("../read-src");
const logger_1 = require("../logger");
const fs_1 = require("../util/fs");
const LAUNCHER_ACTIVITY = 'manifest/application/activity[intent-filter/action/@android:name="android.intent.action.MAIN"]';
// The Capacitor string resources that hold a copy of the package name
const PACKAGE_NAME_STRINGS = ['package_name', 'custom_url_scheme'];
// Used when the manifest declares no activity to take the name from
const DEFAULT_MAIN_ACTIVITY_NAME = 'MainActivity';
class AndroidProject extends platform_project_1.PlatformProject {
    constructor(project) {
        super(project);
        this.buildGradle = null;
        this.appBuildGradle = null;
        const manifestPath = this.getAndroidManifestPath();
        this.manifest = new xml_1.XmlFile(manifestPath, project.vfs);
    }
    async load() {
        try {
            await this.manifest.load();
            this.buildGradle = await this.loadGradle('build.gradle');
            this.appBuildGradle = await this.loadGradle('app/build.gradle');
        }
        catch (e) {
            this.setError(e);
        }
    }
    getBuildGradle() {
        return this.buildGradle;
    }
    getAppBuildGradle() {
        return this.appBuildGradle;
    }
    getAndroidManifest() {
        return this.manifest;
    }
    /**
     * Get a project file container for the given path in the project root.
     * This will return an existing file container or create a new one.
     */
    getProjectFile(path, create) {
        var _a;
        const root = (_a = this.project.config.android) === null || _a === void 0 ? void 0 : _a.path;
        if (!root) {
            return null;
        }
        const filename = (0, path_1.join)(root, path);
        const existing = this.project.vfs.get(filename);
        if (existing) {
            return existing.getData();
        }
        return create(filename);
    }
    getResourceXmlFile(resourcePath) {
        return this.getXmlFile((0, path_1.join)(this.getResourcesPath(), resourcePath));
    }
    getXmlFile(path) {
        return this.getProjectFile(path, (filename) => new xml_1.XmlFile(filename, this.project.vfs));
    }
    getPropertiesFile(path) {
        return this.getProjectFile(path, (filename) => new properties_1.PropertiesFile(filename, this.project.vfs));
    }
    async getGradleFile(path) {
        if (path === 'build.gradle') {
            return this.buildGradle;
        }
        else if (path === 'app/build.gradle') {
            return this.appBuildGradle;
        }
        return this.loadGradle(path);
    }
    async setAppName(appName) {
        const application = this.manifest.find('manifest/application');
        if (!application) {
            logger_1.Logger.v('android', 'setAppName', `No <application> node found in <manifest>`);
            return;
        }
        const label = application[0].getAttribute('android:label');
        logger_1.Logger.v('android', 'setAppName', `current app label is ${label}`);
        if (label) {
            if (label.indexOf('@string') === 0) {
                logger_1.Logger.v('android', 'setAppName', 'android:label pointing to strings.xml resource file. Reading values/strings.xml');
                const stringsFile = await this.getResourceXmlFile('values/strings.xml');
                if (!stringsFile) {
                    logger_1.Logger.v('android', 'setAppName', 'Unable to load values/strings.xml resource file');
                    return;
                }
                await stringsFile.load();
                const attr = label.replace('@string/', '');
                // TODO: use the value specified in the @strings attribute
                logger_1.Logger.v('android', 'setAppName', `Updated values/strings.xml <string name="${attr}"> to <string name="${attr}">${appName}</string>`);
                stringsFile.replaceFragment(`resources/string[@name="${attr}"]`, `<string name="${attr}">${appName}</string>`);
            }
        }
        else {
            logger_1.Logger.v('android', 'setAppName', `No android:label on <application> node, setting value directly`);
            this.manifest.setAttrs('manifest/application', { 'android:label': appName });
        }
    }
    /**
     * Update the Android package name. This renames the package in `AndroidManifest.xml`,
     * the `applicationId` and `namespace` in `app/build.gradle`, and moves the sources of
     * the old package to the new one.
     *
     * This action will mutate the project on disk!
     */
    async setPackageName(packageName) {
        var _a, _b, _c, _d;
        const manifestPackage = (_a = this.manifest.getDocumentElement()) === null || _a === void 0 ? void 0 : _a.getAttribute('package');
        const oldPackageName = manifestPackage || (await this.getPackageName()) || (await ((_b = this.appBuildGradle) === null || _b === void 0 ? void 0 : _b.getApplicationId()));
        if (!oldPackageName) {
            throw new Error('Unable to detect the current package name. Set the package attribute in AndroidManifest.xml or the namespace or applicationId in app/build.gradle before modifying the project package name');
        }
        logger_1.Logger.v('android', 'setPackageName', 'setting Android package name to', packageName, 'from', oldPackageName);
        if (packageName === oldPackageName) {
            return;
        }
        const sourceDir = (0, path_1.join)(this.getAppRoot(), 'src', 'main', 'java');
        const oldPackageParts = oldPackageName.split('.');
        const oldPackageDir = (0, path_1.join)(sourceDir, ...oldPackageParts);
        if (!(await (0, utils_fs_1.pathExists)(oldPackageDir))) {
            throw new Error('Current Java package name and directory structure do not match the <manifest> package attribute. Ensure these match before modifying the project package name');
        }
        if (manifestPackage) {
            this.manifest.setAttrs('manifest', { package: packageName });
        }
        this.setLauncherActivityPackage(packageName);
        await ((_c = this.appBuildGradle) === null || _c === void 0 ? void 0 : _c.setApplicationId(packageName));
        await ((_d = this.appBuildGradle) === null || _d === void 0 ? void 0 : _d.setNamespace(packageName));
        logger_1.Logger.v('android', 'setPackageName', `set applicationId and namespace to ${packageName}`);
        const movedFiles = await this.movePackageSources(oldPackageDir, (0, path_1.join)(sourceDir, ...packageName.split('.')));
        await this.renamePackageInSources(movedFiles, oldPackageName, packageName);
        await this.removeOldPackageDirs(sourceDir, oldPackageParts);
        await this.renamePackageInStrings(oldPackageName, packageName);
    }
    /**
     * Point the launcher activity at the new package. Activities of other packages, and
     * activity names that are relative to the package (`.MainActivity`), are left alone.
     */
    setLauncherActivityPackage(packageName) {
        var _a, _b;
        const activityName = (_b = (_a = this.manifest.find(LAUNCHER_ACTIVITY)) === null || _a === void 0 ? void 0 : _a[0]) === null || _b === void 0 ? void 0 : _b.getAttribute('android:name');
        if (!activityName || activityName.startsWith('.')) {
            return;
        }
        const newActivityName = `${packageName}.${activityName.split('.').pop()}`;
        logger_1.Logger.v('android', 'setPackageName', `set launcher <activity android:name="${newActivityName}">`);
        this.manifest.setAttrs(LAUNCHER_ACTIVITY, {
            'android:name': newActivityName
        });
    }
    /**
     * Move every file of the old package, sub packages included, to the new package
     * directory and return the new file paths.
     *
     * Every destination is checked before the first file is moved, so a file that is in
     * the way cannot leave the sources split across the old and the new package.
     */
    async movePackageSources(oldPackageDir, newPackageDir) {
        const files = (await (0, fs_1.listFilesRecursive)(oldPackageDir)).map(file => ({
            file,
            dest: (0, path_1.join)(newPackageDir, (0, path_1.relative)(oldPackageDir, file)),
        }));
        for (const { dest } of files) {
            if (await (0, utils_fs_1.pathExists)(dest)) {
                throw new Error(`Unable to move the sources to the new package: a file already exists at ${dest}. Remove it before modifying the project package name`);
            }
        }
        for (const { file, dest } of files) {
            logger_1.Logger.v('android', 'setPackageName', `moving ${file} to ${dest}`);
            await (0, utils_fs_1.mkdirp)((0, path_1.dirname)(dest));
            await (0, utils_fs_1.move)(file, dest);
        }
        return files.map(({ dest }) => dest);
    }
    /**
     * Rename the package declaration and the imports of the old package in the given
     * source files. Sources that declare a package outside of the old one are left alone.
     */
    async renamePackageInSources(files, oldPackageName, packageName) {
        var _a;
        // Anchored to the start of a line so that the word package in a header comment is
        // not mistaken for the declaration
        const packageDeclaration = /^([ \t]*package\s+)([\w.]+)/m;
        const oldPackageImport = new RegExp(`(import\\s+(static\\s+)?)${oldPackageName.replace(/\./g, '\\.')}\\.`, 'g');
        for (const file of files.filter(f => /\.(java|kt)$/.test(f))) {
            const source = await (0, utils_fs_1.readFile)(file, { encoding: 'utf-8' });
            const declaredPackage = (_a = source.match(packageDeclaration)) === null || _a === void 0 ? void 0 : _a[2];
            if (!declaredPackage) {
                continue;
            }
            if (declaredPackage !== oldPackageName && !declaredPackage.startsWith(`${oldPackageName}.`)) {
                continue;
            }
            logger_1.Logger.v('android', 'setPackageName', `renaming package in source file ${file}`);
            const subPackage = declaredPackage.slice(oldPackageName.length);
            const newSource = source
                .replace(packageDeclaration, (_, keyword) => `${keyword}${packageName}${subPackage}`)
                .replace(oldPackageImport, (_, keyword) => `${keyword}${packageName}.`);
            await (0, utils_fs_1.writeFile)(file, newSource);
        }
    }
    /**
     * Remove the directories the old package left behind, deepest first. Directories that
     * still have contents are kept.
     */
    async removeOldPackageDirs(sourceDir, oldPackageParts) {
        logger_1.Logger.v('android', 'setPackageName', `removing old source dirs for old package (${oldPackageParts.join('.')})`);
        await this.removeDirTreeIfEmpty((0, path_1.join)(sourceDir, ...oldPackageParts));
        for (let depth = oldPackageParts.length - 1; depth > 0; depth--) {
            await this.removeDirIfEmpty((0, path_1.join)(sourceDir, ...oldPackageParts.slice(0, depth)));
        }
    }
    async removeDirTreeIfEmpty(dir) {
        for (const entry of await (0, promises_1.readdir)(dir, { withFileTypes: true })) {
            if (entry.isDirectory()) {
                await this.removeDirTreeIfEmpty((0, path_1.join)(dir, entry.name));
            }
        }
        await this.removeDirIfEmpty(dir);
    }
    async removeDirIfEmpty(dir) {
        try {
            await (0, utils_fs_1.rmdir)(dir);
        }
        catch (e) {
            // rmdir only fails when the directory still has contents, which is when we want to keep it
        }
    }
    /**
     * Keep the Capacitor string resources that mirror the package name in sync. Values that
     * hold something else, because the project isn't a Capacitor one or they were edited by
     * hand, are left alone.
     */
    async renamePackageInStrings(oldPackageName, packageName) {
        var _a, _b, _c;
        const stringsPath = (0, path_1.join)((_a = this.getResourcesRoot()) !== null && _a !== void 0 ? _a : '', 'values', 'strings.xml');
        if (!(await (0, utils_fs_1.pathExists)(stringsPath))) {
            return;
        }
        // Opening a file adds it to the files to commit, so keep it closed when it doesn't
        // mention the old package name at all
        if (!(await (0, utils_fs_1.readFile)(stringsPath, { encoding: 'utf-8' })).includes(oldPackageName)) {
            return;
        }
        const stringsFile = this.getResourceXmlFile('values/strings.xml');
        if (!stringsFile) {
            return;
        }
        await stringsFile.load();
        for (const name of PACKAGE_NAME_STRINGS) {
            const target = `resources/string[@name="${name}"]`;
            if (((_c = (_b = stringsFile.find(target)) === null || _b === void 0 ? void 0 : _b[0]) === null || _c === void 0 ? void 0 : _c.textContent) !== oldPackageName) {
                continue;
            }
            logger_1.Logger.v('android', 'setPackageName', `set <string name="${name}"> to ${packageName}`);
            stringsFile.replaceFragment(target, `<string name="${name}">${packageName}</string>`);
        }
    }
    /**
     * Get the Java file name of the main activity. Use `getMainActivityPath()` to get the
     * name of the source file the project actually has, which can be a Kotlin one.
     */
    getMainActivityFilename() {
        return `${this.getMainActivityName()}.java`;
    }
    async getMainActivityPath() {
        var _a, _b;
        const packageParts = (_b = (_a = (await this.getPackageName())) === null || _a === void 0 ? void 0 : _a.split('.')) !== null && _b !== void 0 ? _b : [];
        return (0, path_1.join)('app', 'src', 'main', 'java', ...packageParts, await this.resolveMainActivityFilename());
    }
    /**
     * Get the file name of the main activity, with the source file extension the
     * project actually uses.
     */
    async resolveMainActivityFilename() {
        var _a, _b, _c;
        const activityName = this.getMainActivityName();
        const packageParts = (_b = (_a = (await this.getPackageName())) === null || _a === void 0 ? void 0 : _a.split('.')) !== null && _b !== void 0 ? _b : [];
        const kotlinActivity = `${activityName}.kt`;
        if (await (0, utils_fs_1.pathExists)((0, path_1.join)((_c = this.getAppRoot()) !== null && _c !== void 0 ? _c : '', 'src', 'main', 'java', ...packageParts, kotlinActivity))) {
            return kotlinActivity;
        }
        return `${activityName}.java`;
    }
    getMainActivityName() {
        var _a, _b, _c, _d;
        const activity = (_b = (_a = this.manifest.find(LAUNCHER_ACTIVITY)) === null || _a === void 0 ? void 0 : _a[0]) !== null && _b !== void 0 ? _b : (_c = this.manifest.find('manifest/application/activity')) === null || _c === void 0 ? void 0 : _c[0];
        const activityName = activity === null || activity === void 0 ? void 0 : activity.getAttribute('android:name');
        return (_d = activityName === null || activityName === void 0 ? void 0 : activityName.split('.').pop()) !== null && _d !== void 0 ? _d : DEFAULT_MAIN_ACTIVITY_NAME;
    }
    async getGradlePluginVersion() {
        var _a, _b, _c, _d;
        await ((_a = this.buildGradle) === null || _a === void 0 ? void 0 : _a.parse());
        const found = (_b = this.buildGradle) === null || _b === void 0 ? void 0 : _b.find({
            buildscript: {
                dependencies: {
                    classpath: {}
                }
            }
        });
        const sources = (found !== null && found !== void 0 ? found : []).map(f => { var _a, _b; return (_b = (_a = this.buildGradle) === null || _a === void 0 ? void 0 : _a.getSource(f.node)) !== null && _b !== void 0 ? _b : ''; });
        const gradleLine = sources.find(s => s.indexOf('com.android.tools.build:gradle:'));
        return (_d = (_c = gradleLine === null || gradleLine === void 0 ? void 0 : gradleLine.match(/:([\d.]+)/)) === null || _c === void 0 ? void 0 : _c[1]) !== null && _d !== void 0 ? _d : null;
    }
    async getPackageName() {
        var _a, _b;
        const namespace = await ((_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.getNamespace());
        if (namespace) {
            return namespace;
        }
        return (_b = this.manifest.getDocumentElement()) === null || _b === void 0 ? void 0 : _b.getAttribute('package');
    }
    setVersionCode(versionCode) {
        var _a;
        if (versionCode === '') {
            versionCode = 1;
        }
        return (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.setVersionCode(typeof versionCode === 'number' ? versionCode : parseInt(versionCode, 10));
    }
    async getVersionCode() {
        var _a, _b;
        return (_b = (await ((_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.getVersionCode()))) !== null && _b !== void 0 ? _b : null;
    }
    incrementVersionCode() {
        var _a, _b;
        return (_b = (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.incrementVersionCode()) !== null && _b !== void 0 ? _b : Promise.resolve();
    }
    setVersionName(versionName) {
        var _a;
        return (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.setVersionName(versionName);
    }
    getVersionName() {
        var _a, _b;
        return (_b = (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.getVersionName()) !== null && _b !== void 0 ? _b : Promise.resolve(null);
    }
    setVersionNameSuffix(versionNameSuffix) {
        var _a;
        return (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.setVersionNameSuffix(versionNameSuffix);
    }
    getVersionNameSuffix() {
        var _a, _b;
        return (_b = (_a = this.appBuildGradle) === null || _a === void 0 ? void 0 : _a.getVersionNameSuffix()) !== null && _b !== void 0 ? _b : Promise.resolve(null);
    }
    /**
     * Add a new file to the given resources directory with the given contents and
     * given file name
     **/
    getResource(resDir, file, options = { encoding: 'utf-8' }) {
        const root = this.getResourcesRoot();
        if (!root) {
            return;
        }
        const dir = (0, path_1.join)(root, resDir);
        if (!options) {
            return (0, utils_fs_1.readFile)((0, path_1.join)(dir, file));
        }
        return (0, utils_fs_1.readFile)((0, path_1.join)(dir, file), options);
    }
    /**
     * Add a new file to the given resources directory with the given contents and
     * given file name
     **/
    async addResource(resDir, file, contents) {
        const root = this.getResourcesRoot();
        if (!root) {
            return;
        }
        const dir = (0, path_1.join)(root, resDir);
        logger_1.Logger.v(`android`, 'addResource', `add res file ${file} to ${resDir}`);
        if (!(await (0, utils_fs_1.pathExists)(dir))) {
            await (0, utils_fs_1.mkdir)(dir);
        }
        return (0, utils_fs_1.writeFile)((0, path_1.join)(dir, file), contents);
    }
    async copyFile(src, dest) {
        var _a, _b, _c;
        if (!((_c = (_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config) === null || _b === void 0 ? void 0 : _b.android) === null || _c === void 0 ? void 0 : _c.path)) {
            return Promise.reject();
        }
        const destPath = (0, path_1.join)(this.project.config.android.path, dest);
        logger_1.Logger.v(`android`, `copyFile`, `copying ${src} to ${destPath}`);
        if (/^(https?:\/\/)/.test(src)) {
            const res = await fetch(src);
            return (0, utils_fs_1.writeFile)(destPath, Buffer.from(await res.arrayBuffer()));
        }
        const srcPath = (0, path_1.join)(this.project.config.android.path, src);
        return (0, utils_fs_1.copy)(srcPath, destPath);
    }
    /**
     * Copy the given source into the given resources directory with the
     * given file name
     **/
    async copyToResources(resDir, file, source) {
        const root = this.getResourcesRoot();
        if (!root) {
            return;
        }
        const dir = (0, path_1.join)(root, resDir);
        if (!(await (0, utils_fs_1.pathExists)(dir))) {
            await (0, utils_fs_1.mkdir)(dir);
        }
        logger_1.Logger.v(`android`, `copyToResources`, `copying ${file} to Android resources at ${(0, path_1.join)(dir, file)}`);
        const sourceData = await (0, read_src_1.readSource)(source);
        return (0, utils_fs_1.writeFile)((0, path_1.join)(dir, file), sourceData);
    }
    getAndroidManifestPath() {
        var _a, _b;
        if (!((_a = this.project.config.android) === null || _a === void 0 ? void 0 : _a.path)) {
            return null;
        }
        return (0, path_1.join)((_b = this.project.config.android) === null || _b === void 0 ? void 0 : _b.path, 'app', 'src', 'main', 'AndroidManifest.xml');
    }
    getResourcesPath() {
        return (0, path_1.join)('app', 'src', 'main', 'res');
    }
    getResourcesRoot() {
        var _a, _b;
        if (!((_a = this.project.config.android) === null || _a === void 0 ? void 0 : _a.path)) {
            return null;
        }
        return (0, path_1.join)((_b = this.project.config.android) === null || _b === void 0 ? void 0 : _b.path, this.getResourcesPath());
    }
    getAppRoot() {
        var _a, _b;
        if (!((_a = this.project.config.android) === null || _a === void 0 ? void 0 : _a.path)) {
            return null;
        }
        // TODO: Don't hard-code app
        return (0, path_1.join)((_b = this.project.config.android) === null || _b === void 0 ? void 0 : _b.path, 'app');
    }
    async loadGradle(path) {
        var _a, _b;
        if (!((_a = this.project.config.android) === null || _a === void 0 ? void 0 : _a.path)) {
            return null;
        }
        const filename = (0, path_1.join)((_b = this.project.config.android) === null || _b === void 0 ? void 0 : _b.path, path);
        return new gradle_file_1.GradleFile(filename, this.project.vfs);
    }
}
exports.AndroidProject = AndroidProject;
//# sourceMappingURL=project.js.map