import { MobileProject } from "./project";
export declare class PlatformProject {
    protected project: MobileProject;
    private error;
    constructor(project: MobileProject);
    getError(): Error | null;
    setError(error: Error): void;
}
//# sourceMappingURL=platform-project.d.ts.map