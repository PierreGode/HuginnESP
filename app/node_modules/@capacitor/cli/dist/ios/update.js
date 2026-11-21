"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.installCocoaPodsPlugins = exports.updateIOS = void 0;
const tslib_1 = require("tslib");
const fs_extra_1 = require("fs-extra");
const path_1 = require("path");
const semver_1 = require("semver");
const colors_1 = tslib_1.__importDefault(require("../colors"));
const common_1 = require("../common");
const cordova_1 = require("../cordova");
const errors_1 = require("../errors");
const log_1 = require("../log");
const plugin_1 = require("../plugin");
const copy_1 = require("../tasks/copy");
const migrate_1 = require("../tasks/migrate");
const fs_1 = require("../util/fs");
const iosplugin_1 = require("../util/iosplugin");
const node_1 = require("../util/node");
const spm_1 = require("../util/spm");
const subprocess_1 = require("../util/subprocess");
const template_1 = require("../util/template");
const common_2 = require("./common");
const platform = 'ios';
async function updateIOS(config, deployment) {
    const plugins = await getPluginsTask(config);
    const capacitorPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, platform) === 0 /* PluginType.Core */);
    await updatePluginFiles(config, plugins, deployment);
    await (0, common_1.checkPlatformVersions)(config, platform);
    (0, iosplugin_1.generateIOSPackageJSON)(config, plugins);
    (0, plugin_1.printPlugins)(capacitorPlugins, 'ios');
}
exports.updateIOS = updateIOS;
async function updatePluginFiles(config, plugins, deployment) {
    var _a;
    await removePluginsNativeFiles(config);
    const cordovaPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, platform) === 1 /* PluginType.Cordova */);
    if (cordovaPlugins.length > 0) {
        await copyPluginsNativeFiles(config, cordovaPlugins);
    }
    if (!(await (0, fs_extra_1.pathExists)(await config.ios.webDirAbs))) {
        await (0, copy_1.copy)(config, platform);
    }
    await (0, cordova_1.handleCordovaPluginsJS)(cordovaPlugins, config, platform);
    await (0, cordova_1.checkPluginDependencies)(plugins, platform, (_a = config.app.extConfig.cordova) === null || _a === void 0 ? void 0 : _a.failOnUninstalledPlugins);
    if ((await config.ios.packageManager) === 'SPM') {
        await generateCordovaPackageFiles(cordovaPlugins, config);
        const validSPMPackages = await (0, spm_1.checkPluginsForPackageSwift)(config, plugins);
        await Promise.all(validSPMPackages.map(async (plugin) => {
            var _a;
            const iosPlatformVersion = await (0, common_1.getCapacitorPackageVersion)(config, config.ios.name);
            const packageSwiftPath = (0, path_1.join)(plugin.rootPath, 'Package.swift');
            let content = await (0, fs_extra_1.readFile)(packageSwiftPath, { encoding: 'utf-8' });
            const regex = new RegExp('url:\\s*"https://github.com/ionic-team/capacitor-swift-pm\\.git",\\s*from:\\s*"([^"]+)"');
            const version = (_a = content.match(regex)) === null || _a === void 0 ? void 0 : _a[1];
            const majorCapVersion = (0, semver_1.major)(iosPlatformVersion);
            if (version && (0, semver_1.major)(version) != majorCapVersion) {
                const preCapVersion = (0, semver_1.prerelease)(iosPlatformVersion);
                const forceVersion = preCapVersion ? iosPlatformVersion : `${majorCapVersion}.0.0`;
                content = (0, migrate_1.setAllStringIn)(content, `url: "https://github.com/ionic-team/capacitor-swift-pm.git",`, `)`, ` from: "${forceVersion}"`);
                await (0, fs_extra_1.writeFile)(packageSwiftPath, content);
                log_1.logger.warn(`${plugin.id} is built for Capacitor ${(0, semver_1.major)(version)}, it might cause issues`);
            }
        }));
        await (0, spm_1.generatePackageFile)(config, validSPMPackages.concat(cordovaPlugins));
    }
    else {
        await generateCordovaPodspecs(cordovaPlugins, config);
        await installCocoaPodsPlugins(config, plugins, deployment);
    }
    await (0, cordova_1.logCordovaManualSteps)(cordovaPlugins, config, platform);
    const incompatibleCordovaPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, platform) === 2 /* PluginType.Incompatible */);
    (0, plugin_1.printPlugins)(incompatibleCordovaPlugins, platform, 'incompatible');
}
async function generateCordovaPackageFiles(cordovaPlugins, config) {
    cordovaPlugins.map((plugin) => {
        generateCordovaPackageFile(plugin, config);
    });
}
async function generateCordovaPackageFile(p, config) {
    var _a;
    const iosPlatformVersion = await (0, common_1.getCapacitorPackageVersion)(config, config.ios.name);
    const platformTag = (0, plugin_1.getPluginPlatform)(p, platform);
    if ((_a = platformTag.$) === null || _a === void 0 ? void 0 : _a.package) {
        const packageSwiftPath = (0, path_1.join)(p.rootPath, 'Package.swift');
        let content = await (0, fs_extra_1.readFile)(packageSwiftPath, { encoding: 'utf-8' });
        content = content.replace(`apache`, `ionic-team`).replaceAll(`cordova-ios`, `capacitor-swift-pm`);
        content = (0, migrate_1.setAllStringIn)(content, `url: "https://github.com/ionic-team/capacitor-swift-pm.git",`, `)`, ` from: "${iosPlatformVersion}"`);
        await (0, fs_extra_1.writeFile)(packageSwiftPath, content);
    }
    else {
        await writeGeneratedPackageSwift(p, config, iosPlatformVersion);
    }
}
function buildBinaryTargetEntries(p, frameworks) {
    const customXcframeworks = frameworks.filter((f) => f.$.custom === 'true' && f.$.src.endsWith('.xcframework'));
    const customFrameworks = frameworks.filter((f) => f.$.custom === 'true' && f.$.src.endsWith('.framework'));
    if (customFrameworks.length > 0) {
        customFrameworks.forEach((f) => {
            log_1.logger.warn(`${p.id}: custom .framework files are not supported as binaryTarget in SPM (${f.$.src}). Convert to .xcframework for SPM compatibility.`);
        });
    }
    const binaryTargetsText = customXcframeworks
        .map((f) => {
        const name = f.$.src.split('/').pop().replace('.xcframework', '');
        return `,
        .binaryTarget(
            name: "${name}",
            path: "${f.$.src}"
        )`;
    })
        .join('');
    const binaryDepsText = customXcframeworks
        .map((f) => {
        const name = f.$.src.split('/').pop().replace('.xcframework', '');
        return `,\n                .target(name: "${name}")`;
    })
        .join('');
    return { binaryTargetsText, binaryDepsText };
}
function buildResourcesText(resources) {
    const resourceEntry = [];
    for (const resource of resources) {
        resourceEntry.push(`.copy("resources/${resource.$.src.split('/').pop()}")`);
    }
    return resources.length > 0
        ? `,
            resources: [
                ${resourceEntry.join(',\n                ')}
            ]`
        : '';
}
async function buildDependencyTexts(p, config) {
    let allDependencies = (0, plugin_1.getPlatformElement)(p, platform, 'dependency');
    if (p.xml['dependency']) {
        allDependencies = allDependencies.concat(p.xml['dependency']);
    }
    let packageText = '';
    let productText = '';
    await Promise.all(allDependencies.map(async (dep) => {
        let plugin = dep.$.id;
        if (plugin.includes('@') && plugin.indexOf('@') !== 0) {
            [plugin] = plugin.split('@');
        }
        const depPlugin = await (0, plugin_1.resolvePlugin)(config, plugin);
        if (depPlugin) {
            const headerFiles = (0, plugin_1.getPlatformElement)(depPlugin, platform, 'header-file');
            const resources = (0, plugin_1.getPlatformElement)(depPlugin, platform, 'resource-file');
            const sourceFiles = (0, plugin_1.getPlatformElement)(depPlugin, platform, 'source-file');
            if (sourceFiles.length === 0 && headerFiles.length === 0 && resources.length === 0) {
                return;
            }
            packageText += `,\n        .package(name: "${depPlugin.name}", path: "../${depPlugin.name}")`;
            productText += `,\n                .product(name: "${depPlugin.name}", package: "${depPlugin.name}")`;
        }
    }));
    return { packageText, productText };
}
function buildCSettingsText(p, sourceFiles) {
    var _a;
    const pluginId = p.id;
    const allFlags = new Set();
    for (const sourceFile of sourceFiles) {
        const flags = (_a = sourceFile.$) === null || _a === void 0 ? void 0 : _a['compiler-flags'];
        if (flags) {
            flags
                .split(/\s+/)
                .map((f) => f.trim())
                .filter((f) => f.length > 0)
                .forEach((f) => allFlags.add(f));
        }
    }
    if (allFlags.size === 0) {
        return '';
    }
    const entries = [];
    const unsupportedFlags = [];
    for (const flag of allFlags) {
        if (flag.startsWith('-D')) {
            const definition = flag.slice(2);
            const eqIndex = definition.indexOf('=');
            if (eqIndex !== -1) {
                const name = definition.slice(0, eqIndex);
                const value = definition.slice(eqIndex + 1);
                entries.push(`                .define("${name}", to: "${value}")`);
            }
            else {
                entries.push(`                .define("${definition}")`);
            }
        }
        else {
            unsupportedFlags.push(flag);
        }
    }
    if (unsupportedFlags.length > 0) {
        log_1.logger.warn(`${pluginId}: the following compiler flags are not supported in SPM and were ignored: ${unsupportedFlags.join(', ')}. ` +
            `This might cause the plugin to fail to compile.`);
    }
    if (entries.length === 0) {
        return '';
    }
    return `,
            cSettings: [
${entries.join(',\n')}
            ]`;
}
async function writeGeneratedPackageSwift(p, config, iosPlatformVersion) {
    const iosVersion = (0, common_2.getMajoriOSVersion)(config);
    const headerFiles = (0, plugin_1.getPlatformElement)(p, platform, 'header-file');
    const headersText = headerFiles.length > 0
        ? `,
            publicHeadersPath: "."`
        : '';
    const resources = (0, plugin_1.getPlatformElement)(p, platform, 'resource-file');
    const sourceFiles = (0, plugin_1.getPlatformElement)(p, platform, 'source-file');
    if (sourceFiles.length === 0 && headerFiles.length === 0 && resources.length === 0) {
        return;
    }
    const frameworks = (0, plugin_1.getPlatformElement)(p, platform, 'framework');
    const { binaryTargetsText, binaryDepsText } = buildBinaryTargetEntries(p, frameworks);
    const cSettingsText = buildCSettingsText(p, sourceFiles);
    const systemFrameworks = frameworks.filter((f) => !f.$.custom && f.$.src.endsWith('.framework'));
    const hasWeakFrameworks = systemFrameworks.some((f) => f.$.weak === 'true');
    const requiredSystemFrameworks = systemFrameworks.filter((f) => f.$.weak !== 'true');
    const resourcesText = buildResourcesText(resources);
    const libraryTypeText = hasWeakFrameworks ? `\n            type: .dynamic,` : '';
    const linkerSettingsText = requiredSystemFrameworks.length > 0
        ? `,
            linkerSettings: [
${requiredSystemFrameworks.map((f) => `                .linkedFramework("${f.$.src.replace('.framework', '')}")`).join(',\n')}
            ]`
        : '';
    const { packageText, productText } = await buildDependencyTexts(p, config);
    const content = `// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "${p.name}",
    platforms: [.iOS(.v${iosVersion})],
    products: [
        .library(
            name: "${p.name}",${libraryTypeText}
            targets: ["${p.name}"]
        )
    ],
    dependencies: [
        .package(url: "https://github.com/ionic-team/capacitor-swift-pm.git", from: "${iosPlatformVersion}")${packageText}
    ],
    targets: [
        .target(
            name: "${p.name}",
            dependencies: [
                .product(name: "Cordova", package: "capacitor-swift-pm")${binaryDepsText}${productText}
            ],
            path: "."${resourcesText}${headersText}${cSettingsText}${linkerSettingsText}
        )${binaryTargetsText}
    ]
)`;
    await (0, fs_extra_1.writeFile)((0, path_1.join)(config.ios.cordovaPluginsDirAbs, 'sources', p.name, 'Package.swift'), content);
}
async function installCocoaPodsPlugins(config, plugins, deployment) {
    await (0, common_1.runTask)(`Updating iOS native dependencies with ${colors_1.default.input(`${await config.ios.podPath} install`)}`, () => {
        return updatePodfile(config, plugins, deployment);
    });
}
exports.installCocoaPodsPlugins = installCocoaPodsPlugins;
async function updatePodfile(config, plugins, deployment) {
    const dependenciesContent = await generatePodFile(config, plugins);
    const relativeCapacitoriOSPath = await getRelativeCapacitoriOSPath(config);
    const podfilePath = (0, path_1.join)(config.ios.nativeProjectDirAbs, 'Podfile');
    let podfileContent = await (0, fs_extra_1.readFile)(podfilePath, { encoding: 'utf-8' });
    podfileContent = podfileContent.replace(/(def capacitor_pods)[\s\S]+?(\nend)/, `$1${dependenciesContent}$2`);
    podfileContent = podfileContent.replace(/(require_relative)[\s\S]+?(@capacitor\/ios\/scripts\/pods_helpers')/, `require_relative '${relativeCapacitoriOSPath}/scripts/pods_helpers'`);
    await (0, fs_extra_1.writeFile)(podfilePath, podfileContent, { encoding: 'utf-8' });
    const podPath = await config.ios.podPath;
    const useBundler = (await config.ios.packageManager) === 'bundler';
    if (useBundler) {
        await (0, subprocess_1.runCommand)('bundle', ['exec', 'pod', 'install', ...(deployment ? ['--deployment'] : [])], {
            cwd: config.ios.nativeProjectDirAbs,
        });
    }
    else if (await (0, subprocess_1.isInstalled)('pod')) {
        await (0, subprocess_1.runCommand)(podPath, ['install', ...(deployment ? ['--deployment'] : [])], {
            cwd: config.ios.nativeProjectDirAbs,
        });
    }
    else {
        log_1.logger.warn('Skipping pod install because CocoaPods is not installed');
    }
    const isXcodebuildAvailable = await (0, subprocess_1.isInstalled)('xcodebuild');
    if (isXcodebuildAvailable) {
        await (0, subprocess_1.runCommand)('xcodebuild', ['-project', (0, path_1.basename)(`${config.ios.nativeXcodeProjDirAbs}`), 'clean'], {
            cwd: config.ios.nativeProjectDirAbs,
        });
    }
    else {
        log_1.logger.warn('Unable to find "xcodebuild". Skipping xcodebuild clean step...');
    }
}
async function getRelativeCapacitoriOSPath(config) {
    const capacitoriOSPath = (0, node_1.resolveNode)(config.app.rootDir, '@capacitor/ios', 'package.json');
    if (!capacitoriOSPath) {
        (0, errors_1.fatal)(`Unable to find ${colors_1.default.strong('node_modules/@capacitor/ios')}.\n` +
            `Are you sure ${colors_1.default.strong('@capacitor/ios')} is installed?`);
    }
    return (0, fs_1.convertToUnixPath)((0, path_1.relative)(config.ios.nativeProjectDirAbs, await (0, fs_extra_1.realpath)((0, path_1.dirname)(capacitoriOSPath))));
}
async function generatePodFile(config, plugins) {
    const relativeCapacitoriOSPath = await getRelativeCapacitoriOSPath(config);
    const capacitorPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, platform) === 0 /* PluginType.Core */);
    const pods = await Promise.all(capacitorPlugins.map(async (p) => {
        if (!p.ios) {
            return '';
        }
        return `  pod '${p.ios.name}', :path => '${(0, fs_1.convertToUnixPath)((0, path_1.relative)(config.ios.nativeProjectDirAbs, await (0, fs_extra_1.realpath)(p.rootPath)))}'\n`;
    }));
    const cordovaPlugins = plugins.filter((p) => (0, plugin_1.getPluginType)(p, platform) === 1 /* PluginType.Cordova */);
    cordovaPlugins.map(async (p) => {
        const podspecs = (0, plugin_1.getPlatformElement)(p, platform, 'podspec');
        podspecs.map((podspec) => {
            podspec.pods.map((pPods) => {
                pPods.pod.map((pod) => {
                    if (pod.$.git) {
                        let gitRef = '';
                        if (pod.$.tag) {
                            gitRef = `, :tag => '${pod.$.tag}'`;
                        }
                        else if (pod.$.branch) {
                            gitRef = `, :branch => '${pod.$.branch}'`;
                        }
                        else if (pod.$.commit) {
                            gitRef = `, :commit => '${pod.$.commit}'`;
                        }
                        pods.push(`  pod '${pod.$.name}', :git => '${pod.$.git}'${gitRef}\n`);
                    }
                });
            });
        });
    });
    const staticPlugins = cordovaPlugins.filter((p) => (0, cordova_1.needsStaticPod)(p));
    const noStaticPlugins = cordovaPlugins.filter((el) => !staticPlugins.includes(el));
    if (noStaticPlugins.length > 0) {
        pods.push(`  pod 'CordovaPlugins', :path => '../capacitor-cordova-ios-plugins'\n`);
    }
    if (staticPlugins.length > 0) {
        pods.push(`  pod 'CordovaPluginsStatic', :path => '../capacitor-cordova-ios-plugins'\n`);
    }
    const resourcesPlugins = cordovaPlugins.filter(filterResources);
    if (resourcesPlugins.length > 0) {
        pods.push(`  pod 'CordovaPluginsResources', :path => '../capacitor-cordova-ios-plugins'\n`);
    }
    return `
  pod 'Capacitor', :path => '${relativeCapacitoriOSPath}'
  pod 'CapacitorCordova', :path => '${relativeCapacitoriOSPath}'
${pods.join('').trimRight()}`;
}
function getFrameworkName(framework) {
    if (isFramework(framework)) {
        if (framework.$.custom && framework.$.custom === 'true') {
            return framework.$.src;
        }
        return framework.$.src.substr(0, framework.$.src.indexOf('.'));
    }
    return framework.$.src.substr(0, framework.$.src.indexOf('.')).replace('lib', '');
}
function isFramework(framework) {
    return framework.$.src.split('.').pop().includes('framework');
}
async function generateCordovaPodspecs(cordovaPlugins, config) {
    const staticPlugins = cordovaPlugins.filter((p) => (0, cordova_1.needsStaticPod)(p));
    const noStaticPlugins = cordovaPlugins.filter((el) => !staticPlugins.includes(el));
    generateCordovaPodspec(noStaticPlugins, config, false);
    generateCordovaPodspec(staticPlugins, config, true);
}
async function generateCordovaPodspec(cordovaPlugins, config, isStatic) {
    const weakFrameworks = [];
    const linkedFrameworks = [];
    const customFrameworks = [];
    const systemLibraries = [];
    const sourceFrameworks = [];
    const frameworkDeps = [];
    const compilerFlags = [];
    let prefsArray = [];
    let name = 'CordovaPlugins';
    let sourcesFolderName = 'sources';
    if (isStatic) {
        name += 'Static';
        frameworkDeps.push('s.static_framework = true');
        sourcesFolderName += 'static';
    }
    cordovaPlugins.map((plugin) => {
        const frameworks = (0, plugin_1.getPlatformElement)(plugin, platform, 'framework');
        frameworks.map((framework) => {
            if (!framework.$.type) {
                const name = getFrameworkName(framework);
                if (isFramework(framework)) {
                    if (framework.$.weak && framework.$.weak === 'true') {
                        if (!weakFrameworks.includes(name)) {
                            weakFrameworks.push(name);
                        }
                    }
                    else if (framework.$.custom && framework.$.custom === 'true') {
                        const frameworktPath = (0, path_1.join)(sourcesFolderName, plugin.name, name);
                        if (!customFrameworks.includes(frameworktPath)) {
                            customFrameworks.push(frameworktPath);
                        }
                    }
                    else {
                        if (!linkedFrameworks.includes(name)) {
                            linkedFrameworks.push(name);
                        }
                    }
                }
                else {
                    if (!systemLibraries.includes(name)) {
                        systemLibraries.push(name);
                    }
                }
            }
            else if (framework.$.type && framework.$.type === 'podspec') {
                let depString = `s.dependency '${framework.$.src}'`;
                if (framework.$.spec && framework.$.spec !== '') {
                    depString += `, '${framework.$.spec}'`;
                }
                if (!frameworkDeps.includes(depString)) {
                    frameworkDeps.push(depString);
                }
            }
        });
        prefsArray = prefsArray.concat((0, plugin_1.getAllElements)(plugin, platform, 'preference'));
        const podspecs = (0, plugin_1.getPlatformElement)(plugin, platform, 'podspec');
        podspecs.map((podspec) => {
            podspec.pods.map((pods) => {
                pods.pod.map((pod) => {
                    let depString = `s.dependency '${pod.$.name}'`;
                    if (pod.$.spec && pod.$.spec !== '') {
                        depString += `, '${pod.$.spec}'`;
                    }
                    if (!frameworkDeps.includes(depString)) {
                        frameworkDeps.push(depString);
                    }
                });
            });
        });
        const sourceFiles = (0, plugin_1.getPlatformElement)(plugin, platform, 'source-file');
        sourceFiles.map((sourceFile) => {
            if (sourceFile.$.framework && sourceFile.$.framework === 'true') {
                let fileName = sourceFile.$.src.split('/').pop();
                if (!fileName.startsWith('lib')) {
                    fileName = 'lib' + fileName;
                }
                const frameworktPath = (0, path_1.join)(sourcesFolderName, plugin.name, fileName);
                if (!sourceFrameworks.includes(frameworktPath)) {
                    sourceFrameworks.push(frameworktPath);
                }
            }
            else if (sourceFile.$['compiler-flags']) {
                const cFlag = sourceFile.$['compiler-flags'];
                if (!compilerFlags.includes(cFlag)) {
                    compilerFlags.push(cFlag);
                }
            }
        });
    });
    const onlySystemLibraries = systemLibraries.filter((library) => removeNoSystem(library, sourceFrameworks));
    if (weakFrameworks.length > 0) {
        frameworkDeps.push(`s.weak_frameworks = '${weakFrameworks.join(`', '`)}'`);
    }
    if (linkedFrameworks.length > 0) {
        frameworkDeps.push(`s.frameworks = '${linkedFrameworks.join(`', '`)}'`);
    }
    if (onlySystemLibraries.length > 0) {
        frameworkDeps.push(`s.libraries = '${onlySystemLibraries.join(`', '`)}'`);
    }
    if (customFrameworks.length > 0) {
        frameworkDeps.push(`s.vendored_frameworks = '${customFrameworks.join(`', '`)}'`);
        frameworkDeps.push(`s.exclude_files = 'sources/**/*.framework/Headers/*.h', 'sources/**/*.framework/PrivateHeaders/*.h'`);
    }
    if (sourceFrameworks.length > 0) {
        frameworkDeps.push(`s.vendored_libraries = '${sourceFrameworks.join(`', '`)}'`);
    }
    if (compilerFlags.length > 0) {
        frameworkDeps.push(`s.compiler_flags = '${compilerFlags.join(' ')}'`);
    }
    const arcPlugins = cordovaPlugins.filter(filterARCFiles);
    if (arcPlugins.length > 0) {
        frameworkDeps.push(`s.subspec 'noarc' do |sna|
      sna.requires_arc = false
      sna.source_files = 'noarc/**/*.{swift,h,m,c,cc,mm,cpp}'
    end`);
    }
    let frameworksString = frameworkDeps.join('\n    ');
    frameworksString = await replaceFrameworkVariables(config, prefsArray, frameworksString);
    const content = `
  Pod::Spec.new do |s|
    s.name = '${name}'
    s.version = '${config.cli.package.version}'
    s.summary = 'Autogenerated spec'
    s.license = 'Unknown'
    s.homepage = 'https://example.com'
    s.authors = { 'Capacitor Generator' => 'hi@example.com' }
    s.source = { :git => 'https://github.com/ionic-team/does-not-exist.git', :tag => '${config.cli.package.version}' }
    s.source_files = '${sourcesFolderName}/**/*.{swift,h,m,c,cc,mm,cpp}'
    s.ios.deployment_target  = '${config.ios.minVersion}'
    s.xcconfig = {'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) COCOAPODS=1 WK_WEB_VIEW_ONLY=1' }
    s.dependency 'CapacitorCordova'${getLinkerFlags(config)}
    s.swift_version  = '5.1'
    ${frameworksString}
  end`;
    await (0, fs_extra_1.writeFile)((0, path_1.join)(config.ios.cordovaPluginsDirAbs, `${name}.podspec`), content);
}
function getLinkerFlags(config) {
    var _a;
    if ((_a = config.app.extConfig.ios) === null || _a === void 0 ? void 0 : _a.cordovaLinkerFlags) {
        return `\n    s.pod_target_xcconfig = { 'OTHER_LDFLAGS' => '${config.app.extConfig.ios.cordovaLinkerFlags.join(' ')}' }`;
    }
    return '';
}
async function copyPluginsNativeFiles(config, cordovaPlugins) {
    var _a;
    const isSPM = (await config.ios.packageManager) === 'SPM';
    for (const p of cordovaPlugins) {
        const platformTag = (0, plugin_1.getPluginPlatform)(p, platform);
        if (isSPM && ((_a = platformTag.$) === null || _a === void 0 ? void 0 : _a.package)) {
            continue;
        }
        const sourceFiles = (0, plugin_1.getPlatformElement)(p, platform, 'source-file');
        const headerFiles = (0, plugin_1.getPlatformElement)(p, platform, 'header-file');
        const codeFiles = sourceFiles.concat(headerFiles);
        const frameworks = (0, plugin_1.getPlatformElement)(p, platform, 'framework');
        let sourcesFolderName = 'sources';
        if (!isSPM && (0, cordova_1.needsStaticPod)(p)) {
            sourcesFolderName += 'static';
        }
        const sourcesFolder = (0, path_1.join)(config.ios.cordovaPluginsDirAbs, sourcesFolderName, p.name);
        for (const codeFile of codeFiles) {
            let fileName = codeFile.$.src.split('/').pop();
            const fileExt = codeFile.$.src.split('.').pop();
            if (fileExt === 'a' && !fileName.startsWith('lib')) {
                fileName = 'lib' + fileName;
            }
            let destFolder = sourcesFolderName;
            if (!isSPM && codeFile.$['compiler-flags'] && codeFile.$['compiler-flags'] === '-fno-objc-arc') {
                destFolder = 'noarc';
            }
            const filePath = (0, plugin_1.getFilePath)(config, p, codeFile.$.src);
            const fileDest = (0, path_1.join)(config.ios.cordovaPluginsDirAbs, destFolder, p.name, fileName);
            await (0, fs_extra_1.copy)(filePath, fileDest);
            if (!codeFile.$.framework) {
                let fileContent = await (0, fs_extra_1.readFile)(fileDest, { encoding: 'utf-8' });
                if (fileExt === 'swift') {
                    fileContent = 'import Cordova\n' + fileContent;
                    await (0, fs_extra_1.writeFile)(fileDest, fileContent, { encoding: 'utf-8' });
                }
                else {
                    if (fileContent.includes('@import Firebase;')) {
                        fileContent = fileContent.replace('@import Firebase;', '#import <Firebase/Firebase.h>');
                        await (0, fs_extra_1.writeFile)(fileDest, fileContent, { encoding: 'utf-8' });
                    }
                    if (fileContent.includes('[NSBundle bundleForClass:[self class]]') ||
                        fileContent.includes('[NSBundle bundleForClass:[CDVCapture class]]')) {
                        const bundleName = isSPM ? 'SWIFTPM_MODULE_BUNDLE' : '[NSBundle mainBundle]';
                        fileContent = fileContent.replace('[NSBundle bundleForClass:[self class]]', bundleName);
                        fileContent = fileContent.replace('[NSBundle bundleForClass:[CDVCapture class]]', bundleName);
                        await (0, fs_extra_1.writeFile)(fileDest, fileContent, { encoding: 'utf-8' });
                    }
                    if (isSPM && fileContent.includes('[NSBundle mainBundle] URLForResource')) {
                        fileContent = fileContent.replace('[NSBundle mainBundle] URLForResource', 'SWIFTPM_MODULE_BUNDLE URLForResource');
                        await (0, fs_extra_1.writeFile)(fileDest, fileContent, { encoding: 'utf-8' });
                    }
                    if (fileContent.includes('[self.webView superview]') || fileContent.includes('self.webView.superview')) {
                        fileContent = fileContent.replace(/\[self.webView superview\]/g, 'self.viewController.view');
                        fileContent = fileContent.replace(/self.webView.superview/g, 'self.viewController.view');
                        await (0, fs_extra_1.writeFile)(fileDest, fileContent, { encoding: 'utf-8' });
                    }
                }
            }
        }
        const resourceFiles = (0, plugin_1.getPlatformElement)(p, platform, 'resource-file');
        for (const resourceFile of resourceFiles) {
            const fileName = resourceFile.$.src.split('/').pop();
            const rootResFolder = isSPM ? sourcesFolder : config.ios.cordovaPluginsDirAbs;
            await (0, fs_extra_1.copy)((0, plugin_1.getFilePath)(config, p, resourceFile.$.src), (0, path_1.join)(rootResFolder, 'resources', fileName));
        }
        for (const framework of frameworks) {
            if (framework.$.custom && framework.$.custom === 'true') {
                await (0, fs_extra_1.copy)((0, plugin_1.getFilePath)(config, p, framework.$.src), (0, path_1.join)(sourcesFolder, framework.$.src));
            }
        }
    }
}
async function removePluginsNativeFiles(config) {
    await (0, fs_extra_1.remove)(config.ios.cordovaPluginsDirAbs);
    await (0, template_1.extractTemplate)(config.cli.assets.ios.cordovaPluginsTemplateArchiveAbs, config.ios.cordovaPluginsDirAbs);
}
function filterResources(plugin) {
    const resources = (0, plugin_1.getPlatformElement)(plugin, platform, 'resource-file');
    return resources.length > 0;
}
function filterARCFiles(plugin) {
    const sources = (0, plugin_1.getPlatformElement)(plugin, platform, 'source-file');
    const sourcesARC = sources.filter((sourceFile) => sourceFile.$['compiler-flags'] && sourceFile.$['compiler-flags'] === '-fno-objc-arc');
    return sourcesARC.length > 0;
}
function removeNoSystem(library, sourceFrameworks) {
    const libraries = sourceFrameworks.filter((framework) => framework.includes(library));
    return libraries.length === 0;
}
async function getPluginsTask(config) {
    return await (0, common_1.runTask)('Updating iOS plugins', async () => {
        const allPlugins = await (0, plugin_1.getPlugins)(config, 'ios');
        const iosPlugins = await (0, common_2.getIOSPlugins)(allPlugins);
        return iosPlugins;
    });
}
async function replaceFrameworkVariables(config, prefsArray, frameworkString) {
    prefsArray.map((preference) => {
        frameworkString = frameworkString.replace(new RegExp(('$' + preference.$.name).replace('$', '\\$&'), 'g'), preference.$.default);
    });
    return frameworkString;
}
