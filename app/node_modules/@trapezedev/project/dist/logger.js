"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.Logger = void 0;
const kleur_1 = __importDefault(require("kleur"));
class Logger {
    static debug(...args) {
        if (process.env.VERBOSE !== 'false') {
            console.log(kleur_1.default.bold().grey('[log]'), ...args);
        }
    }
    static v(platform, op, ...args) {
        this.debug(`${kleur_1.default.yellow(platform)}(${kleur_1.default.cyan(op)})`, ...args);
    }
    static log(...args) {
        console.log(...args);
    }
    static warn(...args) {
        console.warn(...args);
    }
    static error(...args) {
        console.warn(...args);
    }
}
exports.Logger = Logger;
//# sourceMappingURL=logger.js.map