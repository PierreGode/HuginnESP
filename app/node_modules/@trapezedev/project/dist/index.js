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
var __exportStar = (this && this.__exportStar) || function(m, exports) {
    for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(exports, p)) __createBinding(exports, m, p);
};
Object.defineProperty(exports, "__esModule", { value: true });
__exportStar(require("./definitions"), exports);
__exportStar(require("./frameworks"), exports);
__exportStar(require("./config"), exports);
__exportStar(require("./logger"), exports);
__exportStar(require("./project"), exports);
__exportStar(require("./xml"), exports);
__exportStar(require("./json"), exports);
__exportStar(require("./plist"), exports);
__exportStar(require("./properties"), exports);
__exportStar(require("./strings"), exports);
__exportStar(require("./xcconfig"), exports);
__exportStar(require("./vfs"), exports);
__exportStar(require("./util/plist"), exports);
//# sourceMappingURL=index.js.map