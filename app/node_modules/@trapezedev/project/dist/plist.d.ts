import plist, { PlistObject } from "plist";
import { VFS, VFSFile, VFSStorable } from "./vfs";
import { MobileProject } from "./project";
export declare class PlistFile extends VFSStorable {
    private path;
    private vfs;
    private project?;
    doc: PlistObject | null;
    constructor(path: string, vfs: VFS, project?: MobileProject | undefined);
    getDocument(): plist.PlistObject | null;
    setDocument(doc: any): void;
    exists(): Promise<boolean>;
    load(): Promise<void>;
    private plistCommitFn;
    plistDiffFn: (file: VFSFile) => Promise<{
        old: string;
        new: string;
    }>;
    setFromXml(xml: string): Promise<void>;
    set(properties: any): Promise<void>;
    merge(properties: any): Promise<void>;
    /**
     * This is confusing but this uses a different set algorithm than the above set and merge.
     * TODO: Get rid of this or make this behavior the default for set or merge
     */
    update(entries: any, replace?: boolean): void;
}
//# sourceMappingURL=plist.d.ts.map