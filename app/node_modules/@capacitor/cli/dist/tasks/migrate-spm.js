"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.migrateToSPM = void 0;
const common_1 = require("../common");
const errors_1 = require("../errors");
const common_2 = require("../ios/common");
const log_1 = require("../log");
const spm_1 = require("../util/spm");
const update_1 = require("./update");
async function migrateToSPM(config) {
    if ((await config.ios.packageManager) == 'SPM') {
        (0, errors_1.fatal)('Capacitor project is already using SPM, exiting.');
    }
    await (0, common_1.check)(await (0, common_2.getCommonChecks)(config));
    await (0, spm_1.extractSPMPackageDirectory)(config);
    await (0, spm_1.runCocoapodsDeintegrate)(config);
    await (0, spm_1.removeCocoapodsFiles)(config);
    await (0, spm_1.addInfoPlistDebugIfNeeded)(config);
    const configWritable = config;
    configWritable.ios.packageManager = Promise.resolve('SPM');
    await (0, update_1.update)(configWritable, 'ios', false);
    log_1.logger.info('To complete migration follow the manual steps at https://capacitorjs.com/docs/ios/spm#using-our-migration-tool');
}
exports.migrateToSPM = migrateToSPM;
