/**
 * Expose CapacitorUtils.Synapse.<pluginName>
 *
 * By default it decides to use capacitor if is defined.
 * If you want to instead use a cordova plugin in a capacitor shell, set overrideCapacitorWithCordova to true.
 *
 * Example use:
 *    window.CapacitorUtils.Synapse.DemoPlugin.ping(
 *      {value: "hello"},
 *      (val) => {console.log(val)},
 *      (err) => {console.error(err)}
 *    );
 */
export declare function exposeSynapse(overrideCapacitorWithCordova?: boolean): void;
