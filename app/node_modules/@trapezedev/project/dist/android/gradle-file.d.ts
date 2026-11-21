import { VFS, VFSStorable } from '../vfs';
import { AndroidGradleInjectType } from '../definitions';
export declare const WINDOWS_GRADLE_PARSE_CLASSPATH = "lib/groovy-3.0.9.jar;lib/json-20260814.jar;capacitor-gradle-parse.jar;.";
/**
 * Thrown when neither the property to replace nor the block to insert it into exists,
 * so callers can tell a missing target apart from a failed modification.
 */
export declare class GradleTargetNotFound extends Error {
}
export type GradleAST = any;
export interface GradleASTNode {
    type: string;
    name: string;
    children?: GradleASTNode[];
    source: {
        line: number;
        column: number;
        lastLine: number;
        lastColumn: number;
    };
}
export declare class GradleFile extends VFSStorable {
    filename: string;
    private vfs;
    private source;
    private parsed;
    constructor(filename: string, vfs: VFS);
    getDocument(): string | null;
    private setSource;
    /**
     * Replace the given properties at the specified point in the Gradle file or insert
     * if the replacement doesn't exist
     *
     * Every property of toReplace is resolved and replaced on its own, so the target can
     * point either at a single property or at the block containing the properties
     *
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    replaceProperties(pathObject: any, toReplace: any, exact?: boolean, type?: AndroidGradleInjectType): Promise<void>;
    private replaceProperty;
    private _makePropertyPathObject;
    _makeReplacePathObject(pathObject: any, injectKey: string): any;
    /**
     * Replace an entry in the gradle file.
     */
    private replaceInGradleFile;
    /**
     * Insert the given properties at the specified point in the Gradle file.
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    insertProperties(pathObject: any, toInject: any[], type?: AndroidGradleInjectType, exact?: boolean): Promise<void>;
    /**
     * Inject the given properties at the specified point in the Gradle file.
     * exact specifies whether the pathObject should be exact from the root of the document or
     * if it can match on a sub-object
     **/
    insertFragment(pathObject: any, toInject: string, exact?: boolean): Promise<void>;
    /**
     * Statements are injected into the body of the target node, so prefer a method block
     * over an assignment that happens to share its name (`def dependencies = [...]` next
     * to a `dependencies { }` block).
     */
    private findInsertTarget;
    /**
     * Parse the underlying Gradle file and build the AST. Note: this calls out to
     * a Java process which incurs some overhead and requires java to be installed
     * This is because Gradle is actually a DSL for the Groovy language, which is
     * a JVM language. Additionally, the Groovy parser is based on a modified version
     * of the Antlr project that is tightly bound to the JVM. Ultimatley, this means
     * the only safe, accurate way to feasibly build a Gradle AST is to use the Groovy
     * parser API which this uses under the hood.
     */
    parse(): Promise<any>;
    private runParser;
    /**
     * Inject a modification into the gradle file.
     */
    private insertIntoGradleFile;
    find(pathObject: any | null, exact?: boolean): {
        node: GradleASTNode;
        depth: number;
    }[];
    private _find;
    getSource(node: GradleASTNode): string;
    private getDepth;
    private matchesExact;
    private isTargetNode;
    getJava(): Promise<string | null>;
    getGradleParserPath(): string;
    setApplicationId(applicationId: string): Promise<void>;
    getApplicationId(): Promise<string | null>;
    setVersionCode(versionCode: number): Promise<void>;
    getVersionCode(): Promise<number | null>;
    incrementVersionCode(): Promise<void>;
    setVersionName(versionName: string): Promise<void>;
    getVersionName(): Promise<string | null>;
    setVersionNameSuffix(versionNameSuffix: string): Promise<void>;
    getVersionNameSuffix(): Promise<string | null>;
    getNamespace(): Promise<string | null>;
    setNamespace(namespace: string): Promise<void>;
    private createGradleSource;
    private isVariableEntry;
    private getGradleSource;
    private gradleParseError;
    private gradleCommitFn;
    private gradleDiffFn;
}
//# sourceMappingURL=gradle-file.d.ts.map