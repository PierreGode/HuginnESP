"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.run = exports.runCommand = void 0;
const tslib_1 = require("tslib");
const utils_process_1 = require("@ionic/utils-process");
const utils_terminal_1 = require("@ionic/utils-terminal");
const run_1 = require("../android/run");
const colors_1 = tslib_1.__importDefault(require("../colors"));
const common_1 = require("../common");
const cordova_1 = require("../cordova");
const errors_1 = require("../errors");
const run_2 = require("../ios/run");
const log_1 = require("../log");
const livereload_1 = require("../util/livereload");
const native_run_1 = require("../util/native-run");
const sync_1 = require("./sync");
async function runCommand(config, selectedPlatformName, options) {
    var _a, _b, _c, _d;
    options.host =
        (_b = (_a = options.host) !== null && _a !== void 0 ? _a : livereload_1.CapLiveReloadHelper.getIpAddress()) !== null && _b !== void 0 ? _b : 'localhost';
    options.port = (_c = options.port) !== null && _c !== void 0 ? _c : '3000';
    if (selectedPlatformName && !(await (0, common_1.isValidPlatform)(selectedPlatformName))) {
        const platformDir = (0, common_1.resolvePlatform)(config, selectedPlatformName);
        if (platformDir) {
            await (0, common_1.runPlatformHook)(config, selectedPlatformName, platformDir, 'capacitor:run');
        }
        else {
            log_1.logger.error(`Platform ${colors_1.default.input(selectedPlatformName)} not found.`);
        }
    }
    else {
        const platforms = await (0, common_1.selectPlatforms)(config, selectedPlatformName);
        let platformName;
        if (platforms.length === 1) {
            platformName = platforms[0];
        }
        else {
            platformName = await (0, common_1.promptForPlatform)(platforms.filter(createRunnablePlatformFilter(config)), `Please choose a platform to run:`);
        }
        if (options.list) {
            const targets = await (0, native_run_1.getPlatformTargets)(platformName);
            const outputTargets = targets.map(t => {
                var _a;
                return ({
                    name: (0, common_1.getPlatformTargetName)(t),
                    api: `${t.platform === 'ios' ? 'iOS' : 'API'} ${t.sdkVersion}`,
                    id: (_a = t.id) !== null && _a !== void 0 ? _a : '?',
                });
            });
            // TODO: make hidden commander option (https://github.com/tj/commander.js/issues/1106)
            if (process.argv.includes('--json')) {
                process.stdout.write(`${JSON.stringify(outputTargets)}\n`);
            }
            else {
                const rows = outputTargets.map(t => [t.name, t.api, t.id]);
                log_1.output.write(`${(0, utils_terminal_1.columnar)(rows, {
                    headers: ['Name', 'API', 'Target ID'],
                    vsep: ' ',
                })}\n`);
            }
            return;
        }
        try {
            if (options.sync) {
                await (0, sync_1.sync)(config, platformName, false, true);
            }
            const cordovaPlugins = await (0, cordova_1.getCordovaPlugins)(config, platformName);
            if (options.liveReload) {
                await livereload_1.CapLiveReloadHelper.editCapConfigForLiveReload(config, platformName, options);
                if (platformName === config.android.name) {
                    await await (0, cordova_1.writeCordovaAndroidManifest)(cordovaPlugins, config, platformName, true);
                }
            }
            await run(config, platformName, options);
            if (options.liveReload) {
                new Promise(resolve => process.on('SIGINT', resolve))
                    .then(async () => {
                    await livereload_1.CapLiveReloadHelper.revertCapConfigForLiveReload();
                    if (platformName === config.android.name) {
                        await (0, cordova_1.writeCordovaAndroidManifest)(cordovaPlugins, config, platformName, false);
                    }
                })
                    .then(() => process.exit());
                log_1.logger.info(`App running with live reload listing for: http://${options.host}:${options.port}. Press Ctrl+C to quit.`);
                await (0, utils_process_1.sleepForever)();
            }
        }
        catch (e) {
            if (!(0, errors_1.isFatal)(e)) {
                (0, errors_1.fatal)((_d = e.stack) !== null && _d !== void 0 ? _d : e);
            }
            throw e;
        }
    }
}
exports.runCommand = runCommand;
async function run(config, platformName, options) {
    if (platformName == config.ios.name) {
        await (0, run_2.runIOS)(config, options);
    }
    else if (platformName === config.android.name) {
        await (0, run_1.runAndroid)(config, options);
    }
    else if (platformName === config.web.name) {
        return;
    }
    else {
        throw `Platform ${platformName} is not valid.`;
    }
}
exports.run = run;
function createRunnablePlatformFilter(config) {
    return platform => platform === config.ios.name || platform === config.android.name;
}
