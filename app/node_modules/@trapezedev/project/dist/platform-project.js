"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.PlatformProject = void 0;
class PlatformProject {
    constructor(project) {
        this.project = project;
        this.error = null;
    }
    getError() {
        return this.error;
    }
    setError(error) {
        this.error = error;
    }
}
exports.PlatformProject = PlatformProject;
//# sourceMappingURL=platform-project.js.map