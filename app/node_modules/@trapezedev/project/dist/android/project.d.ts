import { MobileProject } from '../project';
import { AndroidResDir } from '../definitions';
import { GradleFile } from './gradle-file';
import { XmlFile } from '../xml';
import { PropertiesFile } from '../properties';
import { PlatformProject } from '../platform-project';
export declare class AndroidProject extends PlatformProject {
    private manifest;
    private buildGradle;
    private appBuildGradle;
    constructor(project: MobileProject);
    load(): Promise<void>;
    getBuildGradle(): GradleFile | null;
    getAppBuildGradle(): GradleFile | null;
    getAndroidManifest(): XmlFile;
    /**
     * Get a project file container for the given path in the project root.
     * This will return an existing file container or create a new one.
     */
    getProjectFile<T>(path: string, create: (filename: string) => T): T | null;
    getResourceXmlFile(resourcePath: string): XmlFile | null;
    getXmlFile(path: string): XmlFile | null;
    getPropertiesFile(path: string): PropertiesFile | null;
    getGradleFile(path: string): Promise<GradleFile | null>;
    setAppName(appName: string): Promise<void>;
    /**
     * Update the Android package name. This renames the package in `AndroidManifest.xml`,
     * the `applicationId` and `namespace` in `app/build.gradle`, and moves the sources of
     * the old package to the new one.
     *
     * This action will mutate the project on disk!
     */
    setPackageName(packageName: string): Promise<void>;
    /**
     * Point the launcher activity at the new package. Activities of other packages, and
     * activity names that are relative to the package (`.MainActivity`), are left alone.
     */
    private setLauncherActivityPackage;
    /**
     * Move every file of the old package, sub packages included, to the new package
     * directory and return the new file paths.
     *
     * Every destination is checked before the first file is moved, so a file that is in
     * the way cannot leave the sources split across the old and the new package.
     */
    private movePackageSources;
    /**
     * Rename the package declaration and the imports of the old package in the given
     * source files. Sources that declare a package outside of the old one are left alone.
     */
    private renamePackageInSources;
    /**
     * Remove the directories the old package left behind, deepest first. Directories that
     * still have contents are kept.
     */
    private removeOldPackageDirs;
    private removeDirTreeIfEmpty;
    private removeDirIfEmpty;
    /**
     * Keep the Capacitor string resources that mirror the package name in sync. Values that
     * hold something else, because the project isn't a Capacitor one or they were edited by
     * hand, are left alone.
     */
    private renamePackageInStrings;
    /**
     * Get the Java file name of the main activity. Use `getMainActivityPath()` to get the
     * name of the source file the project actually has, which can be a Kotlin one.
     */
    getMainActivityFilename(): string;
    getMainActivityPath(): Promise<string>;
    /**
     * Get the file name of the main activity, with the source file extension the
     * project actually uses.
     */
    private resolveMainActivityFilename;
    private getMainActivityName;
    getGradlePluginVersion(): Promise<string | null>;
    getPackageName(): Promise<string | null | undefined>;
    setVersionCode(versionCode: number): Promise<void> | undefined;
    getVersionCode(): Promise<number | null>;
    incrementVersionCode(): Promise<void>;
    setVersionName(versionName: string): Promise<void> | undefined;
    getVersionName(): Promise<string | null>;
    setVersionNameSuffix(versionNameSuffix: string): Promise<void> | undefined;
    getVersionNameSuffix(): Promise<string | null>;
    /**
     * Add a new file to the given resources directory with the given contents and
     * given file name
     **/
    getResource(resDir: AndroidResDir, file: string, options?: {
        encoding: 'utf-8' | string;
    } | null): Promise<string> | Promise<Buffer<ArrayBufferLike>> | undefined;
    /**
     * Add a new file to the given resources directory with the given contents and
     * given file name
     **/
    addResource(resDir: AndroidResDir, file: string, contents: string): Promise<void>;
    copyFile(src: string, dest: string): Promise<void>;
    /**
     * Copy the given source into the given resources directory with the
     * given file name
     **/
    copyToResources(resDir: AndroidResDir, file: string, source: string): Promise<void>;
    private getAndroidManifestPath;
    getResourcesPath(): string;
    getResourcesRoot(): string | null;
    private getAppRoot;
    private loadGradle;
}
//# sourceMappingURL=project.d.ts.map