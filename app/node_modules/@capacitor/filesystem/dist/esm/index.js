import { registerPlugin } from '@capacitor/core';
import { exposeSynapse } from '@capacitor/synapse';
const Filesystem = registerPlugin('Filesystem', {
    web: () => import('./web').then((m) => new m.FilesystemWeb()),
});
exposeSynapse();
export * from './definitions';
export { Filesystem };
//# sourceMappingURL=index.js.map