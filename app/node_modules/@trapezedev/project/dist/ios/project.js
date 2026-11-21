"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.IosProject = void 0;
const path_1 = __importStar(require("path"));
const utils_fs_1 = require("@ionic/utils-fs");
const pbx_1 = require("../util/pbx");
const xml_1 = require("../xml");
const plist_1 = require("../plist");
const platform_project_1 = require("../platform-project");
const logger_1 = require("../logger");
const fs_1 = require("../util/fs");
const defaultEntitlementsPlist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
</dict>
</plist>
`;
const defaultInfoPlist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
</dict>
</plist>
`;
// File types trapeze creates that Xcode has to copy into the app bundle
const RESOURCE_FILE_EXTENSIONS = ['.plist', '.strings'];
/* Some of the types are unwieldy in this file but the
  pbxProject methods are sensitive to null vs undefined
  so I've tried to accurately map what it expects. */
/**
 * An instance of an IosProject in a mobile project
 */
class IosProject extends platform_project_1.PlatformProject {
    constructor(project) {
        super(project);
        this.pbxProject = null;
        this.pbxFile = null;
        this.pbxCommitFn = async (file) => {
            if (this.pbxProject) {
                await (0, utils_fs_1.writeFile)(file.getFilename(), this.pbxProject.writeSync());
            }
        };
        this.pbxDiffFn = async (file) => {
            var _a, _b;
            return {
                old: await (0, fs_1.readFileOrEmpty)(file.getFilename()),
                new: (_b = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.writeSync()) !== null && _b !== void 0 ? _b : '',
            };
        };
    }
    // The pbx project is mutated in place, so every write has to flag its VFS
    // entry as modified for it to be committed
    markPbxModified() {
        var _a;
        (_a = this.pbxFile) === null || _a === void 0 ? void 0 : _a.markModified();
    }
    log(op, targetName, buildName, msg) {
        logger_1.Logger.v(`ios`, op, `(target: ${targetName}, build: ${buildName}): ${msg}`);
    }
    async load() {
        try {
            const proj = await this.pbx();
            this.pbxProject = proj;
        }
        catch (e) {
            this.setError(e);
        }
    }
    /**
     * Get a project file container for the given path in the project root.
     * This will return an existing file container or create a new one.
     */
    getProjectFile(path, create) {
        var _a;
        const root = (_a = this.project.config.ios) === null || _a === void 0 ? void 0 : _a.path;
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
    async getXmlFile(path) {
        return this.getProjectFile(path, (filename) => new xml_1.XmlFile(filename, this.project.vfs));
    }
    async getPlistFile(path) {
        return this.getProjectFile(path, (filename) => new plist_1.PlistFile(filename, this.project.vfs, this.project));
    }
    getPbxProject() {
        return this.pbxProject;
    }
    /**
     * Get all targets in the project
     */
    getTargets() {
        var _a;
        if (!this.pbxProject) {
            return null;
        }
        const pbxNative = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.pbxNativeTargetSection();
        return this.makeTargets(this.pbxProject, pbxNative);
    }
    /**
     * Get the target with the given name
     */
    getTarget(name) {
        var _a;
        return ((_a = this.getTargets()) === null || _a === void 0 ? void 0 : _a.find(t => t.name === name || t.name === `\"${name}\"`)) || null;
    }
    /**
     * Get the main app target in the project.
     */
    getAppTarget() {
        var _a;
        return ((_a = this.getTargets()) === null || _a === void 0 ? void 0 : _a.find(t => t.productType === '"com.apple.product-type.application"')) || null;
    }
    /**
     * Get the name of the main app target in the project
     */
    getAppTargetName() {
        var _a, _b;
        return ((_b = (_a = this.getTargets()) === null || _a === void 0 ? void 0 : _a.find(t => t.productType === '"com.apple.product-type.application"')) === null || _b === void 0 ? void 0 : _b.name) || null;
    }
    /**
     * Get the bundle id (aka the PRODUCT_BUNDLE_IDENTIFIER) for the given target and build. If build is null
     * the value for all build targets (Debug and Release) will be set to the same value. If target is null
     * the default app target will be used.
     */
    getBundleId(targetName, buildName) {
        var _a, _b, _c, _d, _e, _f;
        targetName = this.assertTargetName(targetName);
        if (buildName) {
            return (0, pbx_1.pbxReadString)((_c = (_b = (_a = this.getTarget(targetName)) === null || _a === void 0 ? void 0 : _a.buildConfigurations.find(c => c.name === buildName)) === null || _b === void 0 ? void 0 : _b.buildSettings) === null || _c === void 0 ? void 0 : _c['PRODUCT_BUNDLE_IDENTIFIER']);
        }
        return (0, pbx_1.pbxReadString)((_f = (_e = (_d = this.getTarget(targetName)) === null || _d === void 0 ? void 0 : _d.buildConfigurations[0]) === null || _e === void 0 ? void 0 : _e.buildSettings) === null || _f === void 0 ? void 0 : _f['PRODUCT_BUNDLE_IDENTIFIER']);
    }
    /**
     * Set the bundle id (aka the PRODUCT_BUNDLE_IDENTIFIER) for the given target and build. If build is null
     * the value for all build targets (Debug and Release) will be set to the same value. If target is null
     * the default app target will be used.
     */
    setBundleId(targetName, buildName, bundleId) {
        var _a;
        targetName = this.assertTargetName(targetName);
        this.log('setBundleId', targetName, buildName, `to ${bundleId}`);
        (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.updateBuildProperty('PRODUCT_BUNDLE_IDENTIFIER', (0, pbx_1.pbxSerializeString)(bundleId), buildName, targetName);
        this.markPbxModified();
    }
    /**
     * Get the build configurations for a given target.
     */
    getBuildConfigurations(targetName) {
        var _a, _b;
        targetName = this.assertTargetName(targetName);
        return (_b = (_a = this.getTarget(targetName)) === null || _a === void 0 ? void 0 : _a.buildConfigurations) !== null && _b !== void 0 ? _b : [];
    }
    /**
     * Get the build configuration names (ex: Debug, Release) for a given target.
     */
    getBuildConfigurationNames(targetName) {
        return this.getBuildConfigurations(targetName).map(c => c.name);
    }
    /**
     * Set the product name for the given target. If the `targetName` is null the main app target is used.
     */
    setProductName(targetName, productName) {
        var _a;
        targetName = this.assertTargetName(targetName);
        this.log(`setProductName`, targetName, null, `PRODUCT_NAME to ${productName}`);
        (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.updateBuildProperty('PRODUCT_NAME', (0, pbx_1.pbxSerializeString)(productName), null, targetName);
        this.markPbxModified();
    }
    /**
     * Get the product name for the given target. If the `targetName` is null the main app target is used.
     */
    getProductName(targetName) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        return ((_a = this.getTarget(targetName)) === null || _a === void 0 ? void 0 : _a.productName) || null;
    }
    /**
     * Set the build number (aka the `CURRENT_PROJECT_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    async setBuild(targetName, buildName, buildNumber) {
        var _a, _b, _c, _d;
        if (buildNumber === '') {
            // This shouldn't happen but can
            buildNumber = 1;
            (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.updateBuildProperty('CURRENT_PROJECT_VERSION', 1, buildName, targetName);
        }
        else if (typeof buildNumber === 'string') {
            // Don't parse version strings
            if (buildNumber.indexOf('.') < 0) {
                buildNumber = parseInt(buildNumber, 10);
            }
        }
        (_b = this.pbxProject) === null || _b === void 0 ? void 0 : _b.updateBuildProperty('CURRENT_PROJECT_VERSION', buildNumber !== null && buildNumber !== void 0 ? buildNumber : 1, buildName, targetName);
        this.markPbxModified();
        this.log(`setBuild`, targetName, buildName, `to ${buildNumber !== null && buildNumber !== void 0 ? buildNumber : 1}`);
        const file = await this.getInfoPlist(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!file || !((_d = (_c = this.project) === null || _c === void 0 ? void 0 : _c.config.ios) === null || _d === void 0 ? void 0 : _d.path)) {
            throw new Error('Unable to load plist file');
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        parsed.set({ 'CFBundleVersion': '$(CURRENT_PROJECT_VERSION)' });
        this.log(`setBuild`, targetName, buildName, `CFBundleVersion to $(CURRENT_PROJECT_VERSION)`);
        this.project.vfs.set(filename, parsed);
    }
    /**
     * Get the build number (aka the `CURRENT_PROJECT_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    async getBuild(targetName, buildName) {
        var _a, _b, _c, _d;
        const currentProjectVersion = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.getBuildProperty('CURRENT_PROJECT_VERSION', buildName ? buildName : undefined /* must use undefined if null */, targetName);
        if (currentProjectVersion) {
            return currentProjectVersion;
        }
        const file = await this.getInfoPlist(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!file || !((_c = (_b = this.project) === null || _b === void 0 ? void 0 : _b.config.ios) === null || _c === void 0 ? void 0 : _c.path)) {
            throw new Error('Unable to load plist file');
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        const doc = (_d = parsed.getDocument()) !== null && _d !== void 0 ? _d : {};
        return doc['CFBundleVersion'];
    }
    /**
     * Increment the build number for the given build name. If the build
     * name is not specified, both Debug and Release builds are incremented.
     */
    async incrementBuild(targetName, buildName) {
        var _a, _b;
        targetName = this.assertTargetName(targetName || null);
        const num = await this.getBuild(targetName !== null && targetName !== void 0 ? targetName : null, buildName);
        if (!isNaN(num)) {
            if (typeof num === 'number') {
                // If the value is a number, increment it
                return this.setBuild(targetName !== null && targetName !== void 0 ? targetName : null, buildName !== null && buildName !== void 0 ? buildName : null, num + 1);
            }
            else if (typeof num === 'string') {
                if (num.indexOf('.') < 0) {
                    // If the value is a string and doesn't contain a period, increment it
                    return this.setBuild(targetName !== null && targetName !== void 0 ? targetName : null, buildName !== null && buildName !== void 0 ? buildName : null, parseInt(num, 10) + 1);
                }
            }
        }
        else {
            // Otherwise, we need to check if there's a build property set for CURRENT_PROJECT_VERSION and create it if not
            let currentProjectVersion = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.getBuildProperty('CURRENT_PROJECT_VERSION', buildName ? buildName : undefined /* must use undefined if null */, targetName);
            if (!currentProjectVersion) {
                this.log(`incrementBuild`, targetName, buildName, `Setting initial value for CURRENT_PROJECT_VERSION to ensure incremented build number works`);
                // Set an initial value for CURRENT_PROJECT_VERSION
                (_b = this.pbxProject) === null || _b === void 0 ? void 0 : _b.updateBuildProperty('CURRENT_PROJECT_VERSION', 1, buildName, targetName);
                this.markPbxModified();
            }
            else {
                // There's already a CURRENT_PROJECT_VERSION set, which shouldn't happen, so do nothing
            }
        }
    }
    /**
     * Set the version (aka `MARKETING_VERSION`) for the given build (Debug/Release/etc)
     */
    async setVersion(targetName, buildName, version) {
        var _a, _b, _c;
        targetName = this.assertTargetName(targetName || null);
        (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.updateBuildProperty('MARKETING_VERSION', (0, pbx_1.pbxSerializeString)(version), buildName, targetName);
        this.markPbxModified();
        this.log(`setVersion`, targetName, buildName, `to ${(0, pbx_1.pbxSerializeString)(version)}`);
        const file = await this.getInfoPlist(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!file || !((_c = (_b = this.project) === null || _b === void 0 ? void 0 : _b.config.ios) === null || _c === void 0 ? void 0 : _c.path)) {
            throw new Error('Unable to load plist file');
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        this.log(`setVersion`, targetName, buildName, `Updated CFBundleShortVersionString to $(MARKETING_VERSION) to ensure updated version works`);
        parsed.set({ 'CFBundleShortVersionString': '$(MARKETING_VERSION)' });
        this.project.vfs.set(filename, parsed);
    }
    /**
     * Get the version (aka the `MARKETING_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    getVersion(targetName, buildName) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        return (0, pbx_1.pbxReadString)((_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.getBuildProperty('MARKETING_VERSION', buildName ? buildName : undefined /* must use undefined if null */, targetName));
    }
    /**
     * Set the build property for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    setBuildProperty(targetName, buildName, key, value) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        this.log(`setBuildProperty`, targetName, buildName, `Setting iOS build property ${key} = ${value}`);
        (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.updateBuildProperty(key, (0, pbx_1.pbxSerializeString)(value), buildName ? buildName : undefined /* must use undefined if null */, targetName);
        this.markPbxModified();
    }
    /**
     * Get the build property for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    getBuildProperty(targetName, buildName, key) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        return (0, pbx_1.pbxReadString)((_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.getBuildProperty(key, buildName ? buildName : undefined /* must use undefined if null */, targetName));
    }
    /**
     * Add a framework for the given target.
     * If the `targetName` is null the main app target is used.
     */
    addFramework(targetName, framework, opts = {}) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        const target = this.getTarget(targetName);
        (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.addFramework(framework, {
            target: target === null || target === void 0 ? void 0 : target.id,
            ...opts
        });
        this.markPbxModified();
    }
    /**
     * Get the frameworks for the given target
     * If the `targetName` is null the main app target is used.
     */
    getFrameworks(targetName) {
        var _a, _b, _c;
        targetName = this.assertTargetName(targetName || null);
        const target = this.getTarget(targetName);
        if (!target) {
            return [];
        }
        return (_c = (_b = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.pbxFrameworksBuildPhaseObj(target.id)) === null || _b === void 0 ? void 0 : _b.files) === null || _c === void 0 ? void 0 : _c.map((f) => f.comment.split(' ')[0]);
    }
    /**
     * Get the path to the entitlements file for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    getEntitlementsFile(targetName, buildName) {
        targetName = this.assertTargetName(targetName || null);
        return this.getBuildProperty(targetName, buildName !== null && buildName !== void 0 ? buildName : null, 'CODE_SIGN_ENTITLEMENTS');
    }
    /**
     * Add entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    async addEntitlements(targetName, buildName, entitlements) {
        targetName = this.assertTargetName(targetName || null);
        let file = await this.assertEntitlementsFile(targetName, buildName);
        if (!file) {
            return;
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        parsed.update(entitlements);
        this.project.vfs.set(filename, parsed);
    }
    /**
     * Set entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    async setEntitlements(targetName, buildName, entitlements) {
        targetName = this.assertTargetName(targetName || null);
        let file = await this.assertEntitlementsFile(targetName, buildName);
        if (!file) {
            return;
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        parsed.update(entitlements, true);
        this.project.vfs.set(filename, parsed);
    }
    /**
     * Get the parsed plist of the entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    async getEntitlements(targetName, buildName) {
        var _a, _b, _c;
        targetName = this.assertTargetName(targetName || null);
        const file = this.getEntitlementsFile(targetName, buildName);
        if (!file || !((_c = (_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config) === null || _b === void 0 ? void 0 : _b.ios) === null || _c === void 0 ? void 0 : _c.path)) {
            return;
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const plistFile = await this.plist(filename);
        return plistFile.getDocument();
    }
    /**
     * Gets the relative Info plist file from the build settings.
     */
    async getInfoPlist(targetName, buildName) {
        targetName = this.assertTargetName(targetName || null);
        return this.getBuildProperty(targetName, buildName !== null && buildName !== void 0 ? buildName : null, 'INFOPLIST_FILE');
    }
    /**
     * Gets the full relative path to the Info plist after getting the relative path
     * from the build settings and resolving it with the app path
     */
    async getInfoPlistFilename(targetName, buildName) {
        var _a, _b;
        const file = await this.getInfoPlist(targetName, buildName);
        if (!((_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config.ios) === null || _b === void 0 ? void 0 : _b.path)) {
            return null;
        }
        return (0, path_1.join)(this.project.config.ios.path, file);
    }
    /**
     * Set the display name for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    async setDisplayName(targetName, buildName, displayName) {
        var _a, _b;
        targetName = this.assertTargetName(targetName || null);
        const file = await this.getInfoPlist(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!file || !((_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config.ios) === null || _b === void 0 ? void 0 : _b.path)) {
            throw new Error('Unable to load plist file');
        }
        const filename = (0, path_1.join)(this.project.config.ios.path, file);
        const parsed = await this.plist(filename);
        parsed.set({ 'CFBundleDisplayName': displayName });
        this.log(`setDisplayName`, targetName, buildName, `Setting CFBundleDisplayName to ${displayName}`);
        this.project.vfs.set(filename, parsed);
    }
    /**
     * Get the display name for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    async getDisplayName(targetName, buildName) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        const filename = await this.getInfoPlistFilename(targetName, buildName);
        if (!filename) {
            return null;
        }
        const parsed = await this.plist(filename);
        const doc = (_a = parsed.getDocument()) !== null && _a !== void 0 ? _a : {};
        return doc['CFBundleDisplayName'];
    }
    /**
     * Update the Info plist for the given target and build. The entries will be merged
     * into the existing plist file.
     *
     * Pass null as the `targetName` to use the main app target
     */
    async updateInfoPlist(targetName, buildName, entries, mergeMode) {
        var _a;
        targetName = this.assertTargetName(targetName || null);
        const filename = await this.getInfoPlistFilename(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!filename) {
            throw new Error('Unable to get plist filename to update');
        }
        if (!await (0, utils_fs_1.pathExists)(filename)) {
            await (0, utils_fs_1.writeFile)(filename, defaultInfoPlist);
        }
        const parsed = await this.plist(filename);
        parsed.update(entries, (_a = mergeMode === null || mergeMode === void 0 ? void 0 : mergeMode.replace) !== null && _a !== void 0 ? _a : false);
        this.project.vfs.set(filename, parsed);
    }
    async copyFile(src, dest) {
        var _a, _b, _c;
        if (!((_c = (_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config) === null || _b === void 0 ? void 0 : _b.ios) === null || _c === void 0 ? void 0 : _c.path)) {
            return Promise.reject();
        }
        const destPath = (0, path_1.join)(this.project.config.ios.path, dest);
        if (/^(https?:\/\/)/.test(src)) {
            const res = await fetch(src);
            return (0, utils_fs_1.writeFile)(destPath, Buffer.from(await res.arrayBuffer()));
        }
        const srcPath = (0, path_1.join)(this.project.config.ios.path, src);
        return (0, utils_fs_1.copy)(srcPath, destPath);
    }
    /**
     * Add a file to the project. This attempts to add the file
     * to the main "app" target, or adds it to the empty group (i.e. the root of
     * the project tree) if the app target can't be found.
     */
    async addFile(filePath) {
        var _a, _b;
        const groups = (_b = (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.hash.project.objects['PBXGroup']) !== null && _b !== void 0 ? _b : [];
        const emptyGroup = Object.entries(groups).find(([key, value]) => {
            return value.isa === 'PBXGroup' && typeof value.name === 'undefined';
        });
        const appTarget = this.getAppTargetName();
        const appGroup = Object.entries(groups).find(([key, value]) => {
            return value.isa === 'PBXGroup' && (value.name === appTarget || value.path === appTarget);
        });
        const pathSplit = filePath.split(path_1.sep);
        const inAppGroup = pathSplit[0] === appTarget && !!appGroup;
        this.addFileToBuildPhase(inAppGroup ? pathSplit.slice(1).join(path_1.sep) : filePath, inAppGroup ? appGroup === null || appGroup === void 0 ? void 0 : appGroup[0] : emptyGroup === null || emptyGroup === void 0 ? void 0 : emptyGroup[0]);
        this.markPbxModified();
    }
    /**
     * Register the file in the build phase Xcode expects for its type: resources are copied
     * into the app bundle, xcconfig files are read by the build settings and belong to no
     * build phase at all, and everything else is compiled.
     */
    addFileToBuildPhase(filePath, group) {
        var _a, _b;
        const extension = (0, path_1.extname)(filePath);
        // Xcode copies a target's Info.plist into the bundle by itself, so copying it from the
        // resources phase too fails the build with "Multiple commands produce Info.plist"
        const belongsToNoBuildPhase = extension === '.xcconfig' || (0, path_1.basename)(filePath) === 'Info.plist';
        if (belongsToNoBuildPhase) {
            (_a = this.pbxProject) === null || _a === void 0 ? void 0 : _a.addFile(filePath, group);
        }
        else if (RESOURCE_FILE_EXTENSIONS.includes(extension)) {
            this.addResourceFile(filePath, group);
        }
        else {
            (_b = this.pbxProject) === null || _b === void 0 ? void 0 : _b.addSourceFile(filePath, {}, group);
        }
    }
    // xcode's own addResourceFile() resolves the group by name and throws on the many
    // projects that have no `Resources` group, so add the file to the resolved group here
    addResourceFile(filePath, group) {
        var _a;
        const pbxProject = this.pbxProject;
        if (!pbxProject) {
            return;
        }
        const file = pbxProject.addFile(filePath, group);
        // Without a target the pbx lib resolves whichever resources build phase comes first
        // in the objects hash, which in a multi-target project can belong to an app extension
        const appTargetId = (_a = this.getAppTarget()) === null || _a === void 0 ? void 0 : _a.id;
        // A project without a resources build phase has nowhere to copy the file from, but
        // the file reference added above is still worth keeping
        if (!file || !pbxProject.pbxResourcesBuildPhaseObj(appTargetId)) {
            return;
        }
        file.uuid = pbxProject.generateUuid();
        file.target = appTargetId;
        pbxProject.addToPbxBuildFileSection(file);
        pbxProject.addToPbxResourcesBuildPhase(file);
    }
    async assertEntitlementsFile(targetName, buildName) {
        var _a, _b, _c, _d, _e, _f;
        let file = this.getEntitlementsFile(targetName, buildName !== null && buildName !== void 0 ? buildName : undefined);
        if (!file) {
            if ((_c = (_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config) === null || _b === void 0 ? void 0 : _b.ios) === null || _c === void 0 ? void 0 : _c.path) {
                const targetDir = targetName || 'App';
                const fname = `${(targetName || 'App').split(/\s+/).join('_')}.entitlements`;
                // Create the default entitlements file
                const target = (0, path_1.join)(this.project.config.ios.path, targetDir, fname);
                await (0, utils_fs_1.writeFile)(target, defaultEntitlementsPlist);
                // Always use posix paths
                file = (0, path_1.join)(targetDir, fname).split(path_1.default.sep).join(path_1.default.posix.sep);
                this.setBuildProperty(targetName, buildName, 'CODE_SIGN_ENTITLEMENTS', file);
            }
            else {
                return null;
            }
        }
        if (!file || !((_f = (_e = (_d = this.project) === null || _d === void 0 ? void 0 : _d.config) === null || _e === void 0 ? void 0 : _e.ios) === null || _f === void 0 ? void 0 : _f.path)) {
            return null;
        }
        return file;
    }
    // Used to get the target name for operations, defaulting to the main app target
    // if no targetName was provided
    assertTargetName(targetName) {
        if (!targetName) {
            const appTargetName = this.getAppTargetName();
            if (!appTargetName) {
                throw new Error('No target supplied and unable to find the main app target');
            }
            return appTargetName;
        }
        if (!this.getTarget(targetName)) {
            throw new Error(`Target '${targetName}' not found in project`);
        }
        return targetName;
    }
    makeTargets(proj, pbxNativeSection) {
        return Object.keys(pbxNativeSection).filter(k => k.indexOf('_comment') < 0).map(k => {
            const n = pbxNativeSection[k];
            return {
                id: k,
                name: n.name.replace(/"/g, ''),
                productName: n.productName,
                productType: n.productType,
                buildConfigurations: this.makeBuildConfigurations(proj, n)
            };
        });
    }
    makeBuildConfigurations(proj, pbxNativeEntry) {
        // const config = proj.pbxXCBuildConfigurationSection();
        const config = proj.pbxXCConfigurationList();
        const buildConfigs = proj.pbxXCBuildConfigurationSection();
        const buildConfiguration = config[pbxNativeEntry.buildConfigurationList];
        return buildConfiguration.buildConfigurations.map((bc) => {
            const c = buildConfigs[bc.value];
            return {
                name: c.name.replace(/"/g, ''),
                buildSettings: c.buildSettings,
            };
        });
    }
    async plist(filename) {
        const open = this.project.vfs.get(filename);
        if (open) {
            return open.getData();
        }
        const plistFile = new plist_1.PlistFile(filename, this.project.vfs, this.project);
        await plistFile.load();
        return plistFile;
    }
    iosProjectRoot() {
        var _a, _b, _c;
        return (_c = (_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config.ios) === null || _b === void 0 ? void 0 : _b.path) !== null && _c !== void 0 ? _c : '';
    }
    // Get the filename of the pbxproj
    async pbxFilename() {
        var _a, _b;
        if (!((_b = (_a = this.project) === null || _a === void 0 ? void 0 : _a.config.ios) === null || _b === void 0 ? void 0 : _b.path)) {
            return null;
        }
        const xcodeprojName = await this.xcodeprojName();
        const pbxprojName = await this.pbxprojName();
        return (0, path_1.join)(this.project.config.ios.path, xcodeprojName, pbxprojName);
    }
    async xcodeprojName() {
        var _a;
        const files = await (0, utils_fs_1.readdir)(this.iosProjectRoot());
        return (_a = files.find(f => f.indexOf('.xcodeproj') >= 0)) !== null && _a !== void 0 ? _a : '';
    }
    async pbxprojName() {
        var _a;
        const xcodeprojName = await this.xcodeprojName();
        const xcodeprojDir = (0, path_1.join)(this.iosProjectRoot(), xcodeprojName);
        const files = await (0, utils_fs_1.readdir)(xcodeprojDir);
        return (_a = files.find(f => f.indexOf('.pbxproj') >= 0)) !== null && _a !== void 0 ? _a : '';
    }
    // Parse and return a pbx project
    async pbx() {
        const filename = await this.pbxFilename();
        if (!filename) {
            throw new Error('Unable to load pbxproj');
        }
        const pbxParsed = await (0, pbx_1.parsePbxProject)(filename);
        this.pbxFile = this.project.vfs.open(filename, pbxParsed, this.pbxCommitFn, this.pbxDiffFn);
        return pbxParsed;
    }
}
exports.IosProject = IosProject;
//# sourceMappingURL=project.js.map