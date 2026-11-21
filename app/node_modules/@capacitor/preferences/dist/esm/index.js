import { registerPlugin } from '@capacitor/core';
const Preferences = registerPlugin('Preferences', {
    web: () => import('./web').then(m => new m.PreferencesWeb()),
});
export * from './definitions';
export { Preferences };
//# sourceMappingURL=index.js.map