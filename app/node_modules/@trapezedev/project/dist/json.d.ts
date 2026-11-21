import { VFS, VFSStorable } from './vfs';
export declare class JsonFile extends VFSStorable {
    private path;
    private vfs;
    private json;
    constructor(path: string, vfs: VFS);
    getDocument(): Record<string, any> | null;
    exists(): Promise<boolean>;
    load(): Promise<void>;
    set(properties: any): Promise<void>;
    merge(properties: any): Promise<void>;
    private commitFn;
    private diffFn;
}
//# sourceMappingURL=json.d.ts.map