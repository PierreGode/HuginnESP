import { AndroidProject } from './android/project';
import { MobileProjectConfig } from './config';
import { Framework } from './frameworks';
import { IosProject } from './ios/project';
import { VFS } from './vfs';
export declare class MobileProject {
    projectRoot: string;
    framework: Framework | null;
    ios: IosProject | null;
    android: AndroidProject | null;
    config: MobileProjectConfig;
    vfs: VFS;
    constructor(projectRoot: string, config?: MobileProjectConfig);
    detectFramework(): Promise<Framework | null>;
    load(): Promise<void>;
    commit(): Promise<void>;
    copyFile(src: string, dest: string): Promise<void>;
}
//# sourceMappingURL=project.d.ts.map