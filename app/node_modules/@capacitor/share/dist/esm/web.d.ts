import { WebPlugin } from '@capacitor/core';
import type { CanShareResult, ShareOptions, SharePlugin, ShareResult } from './definitions';
export declare class ShareWeb extends WebPlugin implements SharePlugin {
    canShare(): Promise<CanShareResult>;
    share(options: ShareOptions): Promise<ShareResult>;
}
