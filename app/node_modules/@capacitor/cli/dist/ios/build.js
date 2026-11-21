"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.buildiOS = void 0;
const fs_extra_1 = require("fs-extra");
const path_1 = require("path");
const rimraf_1 = require("rimraf");
const common_1 = require("../common");
const definitions_1 = require("../definitions");
const log_1 = require("../log");
const subprocess_1 = require("../util/subprocess");
async function buildiOS(config, buildOptions) {
    var _a, _b, _c, _d;
    const theScheme = (_a = buildOptions.scheme) !== null && _a !== void 0 ? _a : 'App';
    let typeOfBuild;
    let projectName;
    if ((await config.ios.packageManager) !== 'SPM') {
        typeOfBuild = '-workspace';
        projectName = (0, path_1.basename)(await config.ios.nativeXcodeWorkspaceDirAbs);
    }
    else {
        typeOfBuild = '-project';
        projectName = (0, path_1.basename)(await config.ios.nativeXcodeProjDirAbs);
    }
    if (buildOptions.xcodeSigningType == 'manual' &&
        (!buildOptions.xcodeSigningCertificate || !buildOptions.xcodeProvisioningProfile)) {
        throw 'Manually signed Xcode builds require a signing certificate and provisioning profile.';
    }
    const buildArgs = [
        typeOfBuild,
        projectName,
        '-scheme',
        `${theScheme}`,
        '-destination',
        `generic/platform=iOS`,
        '-archivePath',
        `${theScheme}.xcarchive`,
        'archive',
        '-configuration',
        buildOptions.configuration,
    ];
    if (buildOptions.xcodeTeamId) {
        buildArgs.push(`DEVELOPMENT_TEAM=${buildOptions.xcodeTeamId}`);
    }
    if (buildOptions.xcodeSigningType == 'manual') {
        buildArgs.push(`PROVISIONING_PROFILE_SPECIFIER=${buildOptions.xcodeProvisioningProfile}`);
    }
    await (0, common_1.runTask)('Building xArchive', async () => (0, subprocess_1.runCommand)('xcodebuild', buildArgs, {
        cwd: config.ios.nativeProjectDirAbs,
    }));
    const manualSigningContents = `<key>provisioningProfiles</key>
<dict>
<key>${config.app.appId}</key>
<string>${(_b = buildOptions.xcodeProvisioningProfile) !== null && _b !== void 0 ? _b : ''}</string>
</dict>
<key>signingCertificate</key>
<string>${(_c = buildOptions.xcodeSigningCertificate) !== null && _c !== void 0 ? _c : ''}</string>`;
    const archivePlistContents = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>method</key>
<string>${(_d = buildOptions.xcodeExportMethod) !== null && _d !== void 0 ? _d : definitions_1.XcodeExportMethod.AppStoreConnect}</string>
<key>signingStyle</key>
<string>${buildOptions.xcodeSigningType}</string>
${buildOptions.xcodeSigningType == 'manual' ? manualSigningContents : ''}
</dict>
</plist>`;
    const archivePlistPath = (0, path_1.join)(`${config.ios.nativeProjectDirAbs}`, 'archive.plist');
    (0, fs_extra_1.writeFileSync)(archivePlistPath, archivePlistContents);
    const archiveArgs = [
        'archive',
        '-archivePath',
        `${theScheme}.xcarchive`,
        '-exportArchive',
        '-exportOptionsPlist',
        'archive.plist',
        '-exportPath',
        'output',
        '-configuration',
        buildOptions.configuration,
    ];
    if (buildOptions.xcodeSigningType == 'automatic') {
        archiveArgs.push('-allowProvisioningUpdates');
    }
    await (0, common_1.runTask)('Building IPA', async () => (0, subprocess_1.runCommand)('xcodebuild', archiveArgs, {
        cwd: config.ios.nativeProjectDirAbs,
    }));
    await (0, common_1.runTask)('Cleaning up', async () => {
        (0, fs_extra_1.unlinkSync)(archivePlistPath);
        rimraf_1.rimraf.sync((0, path_1.join)(config.ios.nativeProjectDirAbs, `${theScheme}.xcarchive`));
    });
    (0, log_1.logSuccess)(`Successfully generated an IPA at: ${(0, path_1.join)(config.ios.nativeProjectDirAbs, 'output')}`);
}
exports.buildiOS = buildiOS;
