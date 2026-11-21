export declare function assertParentDirs(path: string): Promise<void>;
/**
 * Read a file, treating a file that doesn't exist yet as empty. Diffing a file an
 * operation is about to create has no previous contents to compare against.
 */
export declare function readFileOrEmpty(path: string): Promise<string>;
/**
 * List every file below the given directory, sub directories included.
 */
export declare function listFilesRecursive(dir: string): Promise<string[]>;
//# sourceMappingURL=fs.d.ts.map