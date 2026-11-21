import plist from 'plist';
import { MobileProject } from "../project";
import { IosPbxProject, IosEntitlements, IosFramework, IosBuildName, IosTarget, IosTargetName, IosTargetBuildConfiguration, IosFrameworkOpts } from '../definitions';
import { XmlFile } from '../xml';
import { PlistFile } from '../plist';
import { PlatformProject } from '../platform-project';
/**
 * An instance of an IosProject in a mobile project
 */
export declare class IosProject extends PlatformProject {
    private pbxProject;
    private pbxFile;
    constructor(project: MobileProject);
    private markPbxModified;
    private log;
    load(): Promise<void>;
    /**
     * Get a project file container for the given path in the project root.
     * This will return an existing file container or create a new one.
     */
    getProjectFile<T>(path: string, create: (filename: string) => T): T | null;
    getXmlFile(path: string): Promise<XmlFile | null>;
    getPlistFile(path: string): Promise<PlistFile | null>;
    getPbxProject(): IosPbxProject | null;
    /**
     * Get all targets in the project
     */
    getTargets(): IosTarget[] | null;
    /**
     * Get the target with the given name
     */
    getTarget(name: string): IosTarget | null;
    /**
     * Get the main app target in the project.
     */
    getAppTarget(): IosTarget | null;
    /**
     * Get the name of the main app target in the project
     */
    getAppTargetName(): string | null;
    /**
     * Get the bundle id (aka the PRODUCT_BUNDLE_IDENTIFIER) for the given target and build. If build is null
     * the value for all build targets (Debug and Release) will be set to the same value. If target is null
     * the default app target will be used.
     */
    getBundleId(targetName: IosTargetName | null, buildName?: string): string | null;
    /**
     * Set the bundle id (aka the PRODUCT_BUNDLE_IDENTIFIER) for the given target and build. If build is null
     * the value for all build targets (Debug and Release) will be set to the same value. If target is null
     * the default app target will be used.
     */
    setBundleId(targetName: IosTargetName | null, buildName: IosBuildName | null, bundleId: string): void;
    /**
     * Get the build configurations for a given target.
     */
    getBuildConfigurations(targetName: IosTargetName | null): IosTargetBuildConfiguration[];
    /**
     * Get the build configuration names (ex: Debug, Release) for a given target.
     */
    getBuildConfigurationNames(targetName: IosTargetName | null): string[];
    /**
     * Set the product name for the given target. If the `targetName` is null the main app target is used.
     */
    setProductName(targetName: IosTargetName | null, productName: string): void;
    /**
     * Get the product name for the given target. If the `targetName` is null the main app target is used.
     */
    getProductName(targetName?: IosTargetName | undefined): string | null;
    /**
     * Set the build number (aka the `CURRENT_PROJECT_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    setBuild(targetName: IosTargetName | null, buildName: IosBuildName | null, buildNumber: number | string | null): Promise<void>;
    /**
     * Get the build number (aka the `CURRENT_PROJECT_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    getBuild(targetName: IosTargetName | null, buildName?: IosBuildName | null | undefined): Promise<any>;
    /**
     * Increment the build number for the given build name. If the build
     * name is not specified, both Debug and Release builds are incremented.
     */
    incrementBuild(targetName?: IosTargetName | undefined | null, buildName?: IosBuildName | null | undefined): Promise<void>;
    /**
     * Set the version (aka `MARKETING_VERSION`) for the given build (Debug/Release/etc)
     */
    setVersion(targetName: IosTargetName | null, buildName: IosBuildName | null, version: string): Promise<void>;
    /**
     * Get the version (aka the `MARKETING_VERSION`) for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    getVersion(targetName: IosTargetName | null, buildName: IosBuildName | null): string;
    /**
     * Set the build property for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    setBuildProperty(targetName: IosTargetName | null, buildName: IosBuildName | null, key: string, value: string): void;
    /**
     * Get the build property for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the value is set for both builds (Debug/Release);
     */
    getBuildProperty(targetName: IosTargetName | null, buildName: IosBuildName | null, key: string): string;
    /**
     * Add a framework for the given target.
     * If the `targetName` is null the main app target is used.
     */
    addFramework(targetName: IosTargetName | null, framework: IosFramework, opts?: IosFrameworkOpts): void;
    /**
     * Get the frameworks for the given target
     * If the `targetName` is null the main app target is used.
     */
    getFrameworks(targetName: IosTargetName | null): IosFramework[];
    /**
     * Get the path to the entitlements file for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    getEntitlementsFile(targetName: IosTargetName | null, buildName?: IosBuildName | undefined): string;
    /**
     * Add entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    addEntitlements(targetName: IosTargetName | null, buildName: IosBuildName | null, entitlements: IosEntitlements): Promise<void>;
    /**
     * Set entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    setEntitlements(targetName: IosTargetName | null, buildName: IosBuildName | null, entitlements: IosEntitlements): Promise<void>;
    /**
     * Get the parsed plist of the entitlements for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    getEntitlements(targetName: IosTargetName | null, buildName?: IosBuildName | undefined): Promise<plist.PlistObject | null | undefined>;
    /**
     * Gets the relative Info plist file from the build settings.
     */
    getInfoPlist(targetName: IosTargetName | null, buildName?: IosBuildName | null | undefined): Promise<string>;
    /**
     * Gets the full relative path to the Info plist after getting the relative path
     * from the build settings and resolving it with the app path
     */
    getInfoPlistFilename(targetName: IosTargetName, buildName?: IosBuildName | null | undefined): Promise<string | null>;
    /**
     * Set the display name for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    setDisplayName(targetName: IosTargetName | null, buildName: IosBuildName | null, displayName: string): Promise<void>;
    /**
     * Get the display name for the given target and build.
     * If the `targetName` is null the main app target is used. If the `buildName` is null the first
     * build name is used.
     */
    getDisplayName(targetName: IosTargetName | null, buildName?: IosBuildName | null | undefined): Promise<string | null>;
    /**
     * Update the Info plist for the given target and build. The entries will be merged
     * into the existing plist file.
     *
     * Pass null as the `targetName` to use the main app target
     */
    updateInfoPlist(targetName: IosTargetName | null, buildName: IosBuildName | null, entries: any, mergeMode?: {
        replace: boolean;
    }): Promise<void>;
    copyFile(src: string, dest: string): Promise<void>;
    /**
     * Add a file to the project. This attempts to add the file
     * to the main "app" target, or adds it to the empty group (i.e. the root of
     * the project tree) if the app target can't be found.
     */
    addFile(filePath: string): Promise<void>;
    /**
     * Register the file in the build phase Xcode expects for its type: resources are copied
     * into the app bundle, xcconfig files are read by the build settings and belong to no
     * build phase at all, and everything else is compiled.
     */
    private addFileToBuildPhase;
    private addResourceFile;
    private assertEntitlementsFile;
    private assertTargetName;
    private makeTargets;
    private makeBuildConfigurations;
    private plist;
    private iosProjectRoot;
    private pbxFilename;
    xcodeprojName(): Promise<string>;
    pbxprojName(): Promise<string>;
    private pbx;
    private pbxCommitFn;
    private pbxDiffFn;
}
//# sourceMappingURL=project.d.ts.map