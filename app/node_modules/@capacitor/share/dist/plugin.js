var capacitorShare = (function (exports, core) {
    'use strict';

    const Share = core.registerPlugin('Share', {
        web: () => Promise.resolve().then(function () { return web; }).then(m => new m.ShareWeb()),
    });

    class ShareWeb extends core.WebPlugin {
        async canShare() {
            if (typeof navigator === 'undefined' || !navigator.share) {
                return { value: false };
            }
            else {
                return { value: true };
            }
        }
        async share(options) {
            if (typeof navigator === 'undefined' || !navigator.share) {
                throw this.unavailable('Share API not available in this browser');
            }
            await navigator.share({
                title: options.title,
                text: options.text,
                url: options.url,
            });
            return {};
        }
    }

    var web = /*#__PURE__*/Object.freeze({
        __proto__: null,
        ShareWeb: ShareWeb
    });

    exports.Share = Share;

    return exports;

})({}, capacitorExports);
//# sourceMappingURL=plugin.js.map
