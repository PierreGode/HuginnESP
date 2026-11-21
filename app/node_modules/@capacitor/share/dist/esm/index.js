import { registerPlugin } from '@capacitor/core';
const Share = registerPlugin('Share', {
    web: () => import('./web').then(m => new m.ShareWeb()),
});
export * from './definitions';
export { Share };
//# sourceMappingURL=index.js.map