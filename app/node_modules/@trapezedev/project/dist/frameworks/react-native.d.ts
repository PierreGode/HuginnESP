import { Framework } from ".";
import { MobileProject } from "../project";
export declare class ReactNativeFramework extends Framework {
    isExpo: boolean;
    constructor(isExpo: boolean);
    static getFramework(project: MobileProject): Promise<ReactNativeFramework | null>;
}
//# sourceMappingURL=react-native.d.ts.map