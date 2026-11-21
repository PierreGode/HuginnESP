import { MobileProject } from './project';
import { VFS, VFSStorable } from './vfs';
/**
 * iOS .strings files
 */
export declare class XCConfigFile extends VFSStorable {
    path: string;
    private vfs;
    private project?;
    private doc;
    private keyValueRegex;
    constructor(path: string, vfs: VFS, project?: MobileProject | undefined);
    getDocument(): string;
    getPairs(): any;
    set(values: any): Promise<void>;
    load(): Promise<void>;
    generate(): string;
    private parse;
    private commitFn;
    private diffFn;
}
//# sourceMappingURL=xcconfig.d.ts.map