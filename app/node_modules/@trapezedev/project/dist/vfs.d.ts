import { MobileProject } from './project';
export interface VFSDiff {
    file?: VFSFile;
    old?: string;
    new?: string;
    patch?: string;
}
export declare class VFSStorable {
}
/**
 * Reference to a file and its data (which can be of any type) in the VFS
 */
export declare class VFSRef<T extends VFSStorable> {
    private filename;
    private data;
    private commitFn;
    private diffFn?;
    buffer: Buffer | null;
    modified: boolean;
    constructor(filename: string, data: T | null, commitFn: (file: VFSFile) => Promise<void>, diffFn?: ((file: VFSFile) => Promise<VFSDiff>) | undefined);
    getFilename(): string;
    getData(): T | null;
    isModified(): boolean;
    markModified(): void;
    setData(data: T): void;
    commit(): Promise<void>;
    diff(): Promise<VFSDiff>;
}
export type VFSFile = VFSRef<string | VFSStorable>;
/**
 * Simple virtual filesystem to share files across operations and
 * keep track of modifications over time
 */
export declare class VFS {
    private openFiles;
    constructor();
    open<T extends VFSStorable>(filename: string, data: T, commitFn: (file: VFSFile) => Promise<void>, diffFn?: (file: VFSFile) => Promise<VFSDiff>): VFSRef<T>;
    get<T extends VFSStorable>(filename: string): VFSRef<T> | null;
    isOpen(filename: string): boolean;
    all(): {
        [key: string]: VFSFile;
    };
    modifiedFiles(): VFSFile[];
    commitAll(project: MobileProject): Promise<void>;
    diffAll(): Promise<VFSDiff[]>;
    set(filename: string, data: string | VFSStorable): void;
    markModified(filename: string): void;
    close(ref: VFSFile): void;
}
//# sourceMappingURL=vfs.d.ts.map