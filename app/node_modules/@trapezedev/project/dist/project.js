"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MobileProject = void 0;
const utils_fs_1 = require("@ionic/utils-fs");
const path_1 = require("path");
const project_1 = require("./android/project");
const capacitor_1 = require("./frameworks/capacitor");
const cordova_1 = require("./frameworks/cordova");
const flutter_1 = require("./frameworks/flutter");
const dotnet_maui_1 = require("./frameworks/dotnet-maui");
const react_native_1 = require("./frameworks/react-native");
const project_2 = require("./ios/project");
const vfs_1 = require("./vfs");
const native_ios_1 = require("./frameworks/native-ios");
const native_android_1 = require("./frameworks/native-android");
const nativescript_1 = require("./frameworks/nativescript");
const logger_1 = require("./logger");
class MobileProject {
    constructor(projectRoot, config = {}) {
        var _a, _b, _c, _d;
        this.projectRoot = projectRoot;
        this.framework = null;
        this.ios = null;
        this.android = null;
        this.vfs = new vfs_1.VFS();
        // Resolve the platform paths into a copy: mutating the caller's config would make
        // the paths grow another projectRoot every time the config is reused
        this.config = {
            ...config,
            projectRoot,
            enableAndroid: (_a = config.enableAndroid) !== null && _a !== void 0 ? _a : true,
            enableIos: (_b = config.enableIos) !== null && _b !== void 0 ? _b : true,
            ...(config.ios && {
                ios: { ...config.ios, path: (0, path_1.join)(projectRoot, (_c = config.ios.path) !== null && _c !== void 0 ? _c : '') },
            }),
            ...(config.android && {
                android: { ...config.android, path: (0, path_1.join)(projectRoot, (_d = config.android.path) !== null && _d !== void 0 ? _d : '') },
            }),
        };
    }
    async detectFramework() {
        var _a;
        const frameworks = [
            flutter_1.FlutterFramework,
            react_native_1.ReactNativeFramework,
            capacitor_1.CapacitorFramework,
            cordova_1.CordovaFramework,
            dotnet_maui_1.DotNetMauiFramework,
            nativescript_1.NativeScriptFramework,
            native_ios_1.NativeIosFramework,
            native_android_1.NativeAndroidFramework,
        ];
        const results = await Promise.all(frameworks.map(f => f.getFramework(this)));
        return (_a = results.filter(f => f).find(f => !!f)) !== null && _a !== void 0 ? _a : null;
    }
    async load() {
        var _a, _b, _c, _d, _e, _f, _g, _h, _j, _k;
        if (((_a = this.config) === null || _a === void 0 ? void 0 : _a.enableAndroid) &&
            ((_c = (_b = this.config) === null || _b === void 0 ? void 0 : _b.android) === null || _c === void 0 ? void 0 : _c.path) &&
            (await (0, utils_fs_1.pathExists)((_d = this.config.android) === null || _d === void 0 ? void 0 : _d.path))) {
            this.android = new project_1.AndroidProject(this);
            await ((_e = this.android) === null || _e === void 0 ? void 0 : _e.load());
        }
        if (((_f = this.config) === null || _f === void 0 ? void 0 : _f.enableIos) &&
            ((_h = (_g = this.config) === null || _g === void 0 ? void 0 : _g.ios) === null || _h === void 0 ? void 0 : _h.path) &&
            (await (0, utils_fs_1.pathExists)((_j = this.config.ios) === null || _j === void 0 ? void 0 : _j.path))) {
            this.ios = new project_2.IosProject(this);
            await ((_k = this.ios) === null || _k === void 0 ? void 0 : _k.load());
        }
        this.framework = await this.detectFramework();
    }
    commit() {
        return this.vfs.commitAll(this);
    }
    async copyFile(src, dest) {
        const destPath = (0, path_1.join)(this.projectRoot, dest);
        logger_1.Logger.v(`project`, `copyFile`, `copying ${src} to ${destPath}`);
        if (/^(https?:\/\/)/.test(src)) {
            const res = await fetch(src);
            return (0, utils_fs_1.writeFile)(destPath, Buffer.from(await res.arrayBuffer()));
        }
        const srcPath = (0, path_1.join)(this.projectRoot, src);
        return (0, utils_fs_1.copy)(srcPath, destPath);
    }
}
exports.MobileProject = MobileProject;
//# sourceMappingURL=project.js.map