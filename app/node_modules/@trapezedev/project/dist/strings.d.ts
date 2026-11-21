import { MobileProject } from './project';
import { StringsEntries } from './util/strings';
import { VFS, VFSStorable } from './vfs';
/**
 * iOS .strings files
 */
export declare class StringsFile extends VFSStorable {
    path: string;
    private vfs;
    private project?;
    private doc;
    constructor(path: string, vfs: VFS, project?: MobileProject | undefined);
    getDocument(): StringsEntries;
    setFromJson(jsonFile: any): Promise<void>;
    set(values: any): Promise<void>;
    load(): Promise<void>;
    generate(): string;
    private parse;
    private commitFn;
    private diffFn;
}
//# sourceMappingURL=strings.d.ts.map