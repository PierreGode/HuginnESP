"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.addInfoPlistDebugIfNeeded = exports.runCocoapodsDeintegrate = exports.generatePackageText = exports.removeCocoapodsFiles = exports.extractSPMPackageDirectory = exports.checkPluginsForPackageSwift = exports.generatePackageFile = exports.findPackageSwiftFile = exports.checkPackageManager = void 0;
const fs_extra_1 = require("fs-extra");
const os_1 = require("os");
const path_1 = require("path");
const plist_1 = require("plist");
const tar_1 = require("tar");
const common_1 = require("../common");
const errors_1 = require("../errors");
const common_2 = require("../ios/common");
const log_1 = require("../log");
const plugin_1 = require("../plugin");
const subprocess_1 = require("../util/subprocess");
/**
 * @deprecated use config.ios.packageManager
 * @param config
 * @returns 'Cocoapods' | 'SPM'
 */
async function checkPackageManager(config) {
    const iosDirectory = config.ios.nativeProjectDirAbs;
    if ((0, fs_extra_1.existsSync)((0, path_1.resolve)(iosDirectory, 'CapApp-SPM'))) {
        return 'SPM';
    }
    return 'Cocoapods';
}
exports.checkPackageManager = checkPackageManager;
async function findPackageSwiftFile(config) {
    const packageDirectory = (0, path_1.resolve)(config.ios.nativeProjectDirAbs, 'CapApp-SPM');
    return (0, path_1.resolve)(packageDirectory, 'Package.swift');
}
exports.findPackageSwiftFile = findPackageSwiftFile;
async function generatePackageFile(config, plugins) {
    const packageSwiftFile = await findPackageSwiftFile(config);
    try {
        log_1.logger.info('Writing Package.swift');
        const textToWrite = await generatePackageText(config, plugins);
        (0, fs_extra_1.writeFileSync)(packageSwiftFile, textToWrite);
    }
    catch (err) {
        log_1.logger.error(`Unable to write to ${packageSwiftFile}. Verify it is not already open. \n Error: ${err}`);
    }
}
exports.generatePackageFile = generatePackageFile;
async function checkPluginsForPackageSwift(config, plugins) {
    const iOSCapacitorPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, 'ios') === 0 /* PluginType.Core */);
    const packageSwiftPluginList = await pluginsWithPackageSwift(iOSCapacitorPlugins);
    if (iOSCapacitorPlugins.length == packageSwiftPluginList.length) {
        log_1.logger.debug(`Found ${iOSCapacitorPlugins.length} Capacitor iOS plugins, ${packageSwiftPluginList.length} have a Package.swift file`);
        log_1.logger.info('All Capacitor plugins have a Package.swift file and will be included in Package.swift');
    }
    else {
        log_1.logger.warn('Some installed Capacitor plugins are not compatible with SPM');
    }
    return packageSwiftPluginList;
}
exports.checkPluginsForPackageSwift = checkPluginsForPackageSwift;
async function extractSPMPackageDirectory(config) {
    const spmDirectory = (0, path_1.join)(config.ios.nativeProjectDirAbs, 'CapApp-SPM');
    const spmTemplate = (0, path_1.join)(config.cli.assetsDirAbs, 'ios-spm-template.tar.gz');
    const debugConfig = (0, path_1.join)(config.ios.platformDirAbs, 'debug.xcconfig');
    log_1.logger.info('Extracting ' + spmTemplate + ' to ' + spmDirectory);
    try {
        const tempCapDir = await (0, fs_extra_1.mkdtemp)((0, path_1.join)((0, os_1.tmpdir)(), 'cap-'));
        const tempCapSPM = (0, path_1.join)(tempCapDir, 'App', 'CapApp-SPM');
        const tempDebugXCConfig = (0, path_1.join)(tempCapDir, 'debug.xcconfig');
        await (0, tar_1.extract)({ file: spmTemplate, cwd: tempCapDir });
        await (0, fs_extra_1.move)(tempCapSPM, spmDirectory);
        await (0, fs_extra_1.move)(tempDebugXCConfig, debugConfig);
    }
    catch (err) {
        (0, errors_1.fatal)('Failed to create ' + spmDirectory + ' with error: ' + err);
    }
}
exports.extractSPMPackageDirectory = extractSPMPackageDirectory;
async function removeCocoapodsFiles(config) {
    const iosDirectory = config.ios.nativeProjectDirAbs;
    const podFile = (0, path_1.resolve)(iosDirectory, 'Podfile');
    const podlockFile = (0, path_1.resolve)(iosDirectory, 'Podfile.lock');
    const xcworkspaceFile = (0, path_1.resolve)(iosDirectory, 'App.xcworkspace');
    await (0, fs_extra_1.remove)(podFile);
    await (0, fs_extra_1.remove)(podlockFile);
    await (0, fs_extra_1.remove)(xcworkspaceFile);
}
exports.removeCocoapodsFiles = removeCocoapodsFiles;
async function generatePackageText(config, plugins) {
    var _a, _b, _c, _d, _e;
    const iosPlatformVersion = await (0, common_1.getCapacitorPackageVersion)(config, config.ios.name);
    const iosVersion = (0, common_2.getMajoriOSVersion)(config);
    let packageSwiftText = `// swift-tools-version: 5.9
import PackageDescription

// DO NOT MODIFY THIS FILE - managed by Capacitor CLI commands
let package = Package(
    name: "CapApp-SPM",
    platforms: [.iOS(.v${iosVersion})],
    products: [
        .library(
            name: "CapApp-SPM",
            targets: ["CapApp-SPM"])
    ],
    dependencies: [
        .package(url: "https://github.com/ionic-team/capacitor-swift-pm.git", exact: "${iosPlatformVersion}")`;
    for (const plugin of plugins) {
        if ((0, plugin_1.getPluginType)(plugin, config.ios.name) === 1 /* PluginType.Cordova */) {
            const platformTag = (0, plugin_1.getPluginPlatform)(plugin, config.ios.name);
            if ((_a = platformTag.$) === null || _a === void 0 ? void 0 : _a.package) {
                const relPath = (0, path_1.relative)(config.ios.nativeXcodeProjDirAbs, plugin.rootPath);
                packageSwiftText += `,\n        .package(name: "${plugin.id}", path: "${relPath}")`;
            }
            else {
                const sourceFiles = (0, plugin_1.getPlatformElement)(plugin, config.ios.name, 'source-file');
                const headerFiles = (0, plugin_1.getPlatformElement)(plugin, config.ios.name, 'header-file');
                if (sourceFiles.length === 0 && headerFiles.length === 0) {
                    continue;
                }
                packageSwiftText += `,\n        .package(name: "${plugin.name}", path: "../../capacitor-cordova-ios-plugins/sources/${plugin.name}")`;
            }
        }
        else {
            const relPath = (0, path_1.relative)(config.ios.nativeXcodeProjDirAbs, plugin.rootPath);
            packageSwiftText += `,\n        .package(name: "${(_b = plugin.ios) === null || _b === void 0 ? void 0 : _b.name}", path: "${relPath}")`;
        }
    }
    packageSwiftText += `
    ],
    targets: [
        .target(
            name: "CapApp-SPM",
            dependencies: [
                .product(name: "Capacitor", package: "capacitor-swift-pm"),
                .product(name: "Cordova", package: "capacitor-swift-pm")`;
    for (const plugin of plugins) {
        let pluginText = `,\n                .product(name: "${(_c = plugin.ios) === null || _c === void 0 ? void 0 : _c.name}", package: "${(_d = plugin.ios) === null || _d === void 0 ? void 0 : _d.name}")`;
        if ((0, plugin_1.getPluginType)(plugin, config.ios.name) === 1 /* PluginType.Cordova */) {
            const platformTag = (0, plugin_1.getPluginPlatform)(plugin, config.ios.name);
            if ((_e = platformTag.$) === null || _e === void 0 ? void 0 : _e.package) {
                pluginText = `,\n                .product(name: "${plugin.id}", package: "${plugin.id}")`;
            }
            else {
                const sourceFiles = (0, plugin_1.getPlatformElement)(plugin, config.ios.name, 'source-file');
                const headerFiles = (0, plugin_1.getPlatformElement)(plugin, config.ios.name, 'header-file');
                if (sourceFiles.length === 0 && headerFiles.length === 0) {
                    pluginText = '';
                }
            }
        }
        packageSwiftText += pluginText;
    }
    packageSwiftText += `
            ]
        )
    ]
)
`;
    return packageSwiftText;
}
exports.generatePackageText = generatePackageText;
async function runCocoapodsDeintegrate(config) {
    const podPath = await config.ios.podPath;
    const projectFileName = config.ios.nativeXcodeProjDirAbs;
    const useBundler = (await config.ios.packageManager) === 'bundler';
    log_1.logger.info('Running pod deintegrate on project ' + projectFileName);
    if (useBundler) {
        log_1.logger.info('Found bundler, using it to run CocoaPods.');
        await (0, subprocess_1.runCommand)('bundle', ['exec', 'pod', 'deintegrate', projectFileName], {
            cwd: config.ios.nativeProjectDirAbs,
        });
    }
    else {
        await (0, subprocess_1.runCommand)(podPath, ['deintegrate', projectFileName], {
            cwd: config.ios.nativeProjectDirAbs,
        });
    }
}
exports.runCocoapodsDeintegrate = runCocoapodsDeintegrate;
async function addInfoPlistDebugIfNeeded(config) {
    const infoPlist = (0, path_1.resolve)(config.ios.nativeTargetDirAbs, 'Info.plist');
    log_1.logger.info('Checking ' + infoPlist + ' for CAPACITOR_DEBUG');
    if ((0, fs_extra_1.existsSync)(infoPlist)) {
        const infoPlistContents = (0, fs_extra_1.readFileSync)(infoPlist, 'utf-8');
        const plistEntries = (0, plist_1.parse)(infoPlistContents);
        if (plistEntries['CAPACITOR_DEBUG'] === undefined) {
            log_1.logger.info('Writing CAPACITOR_DEBUG to ' + infoPlist);
            plistEntries['CAPACITOR_DEBUG'] = '$(CAPACITOR_DEBUG)';
            const plistToWrite = (0, plist_1.build)(plistEntries);
            (0, fs_extra_1.writeFileSync)(infoPlist, plistToWrite);
        }
        else {
            log_1.logger.warn('Found CAPACITOR_DEBUG set to ' + plistEntries['CAPACITOR_DEBUG'] + ', skipping.');
        }
    }
    else {
        log_1.logger.warn(infoPlist + ' not found.');
    }
}
exports.addInfoPlistDebugIfNeeded = addInfoPlistDebugIfNeeded;
// Private Functions
async function pluginsWithPackageSwift(plugins) {
    const pluginList = [];
    for (const plugin of plugins) {
        const packageSwiftFound = await (0, fs_extra_1.pathExists)((0, path_1.join)(plugin.rootPath, 'Package.swift'));
        if (packageSwiftFound) {
            pluginList.push(plugin);
        }
        else {
            log_1.logger.warn(plugin.id + ' does not have a Package.swift');
        }
    }
    return pluginList;
}
