import { WebPlugin } from '@capacitor/core';
import type { PreferencesPlugin, ConfigureOptions, GetOptions, GetResult, SetOptions, RemoveOptions, KeysResult, MigrateResult } from './definitions';
export declare class PreferencesWeb extends WebPlugin implements PreferencesPlugin {
    private group;
    configure({ group }: ConfigureOptions): Promise<void>;
    get(options: GetOptions): Promise<GetResult>;
    set(options: SetOptions): Promise<void>;
    remove(options: RemoveOptions): Promise<void>;
    keys(): Promise<KeysResult>;
    clear(): Promise<void>;
    migrate(): Promise<MigrateResult>;
    removeOld(): Promise<void>;
    private get impl();
    private get prefix();
    private rawKeys;
    private applyPrefix;
}
