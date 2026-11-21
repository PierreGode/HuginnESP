# Synapse

### Example

You have plugins that exist for both Capacitor and Cordova. They share the same API for the most part; as in they both expose a `ping` method that takes a string and returns that string with the word "_pong" appeneded to it. You can use synapse to create a single call in ODC that will use which ever plugin is available to the built app. Adding this to your plugins does not alter their ability to be used in the normal fashion they would be directly in a Capacitor or Cordova app.

#### Capacitor Usage of MyPlugin
```javascript
import { MyPlugin } from 'capacitor-my-plugin';
...
async function buttonClick() {
  try {
    const resp = await MyPlugin.ping({text: "Hello World"});
    console.log(resp); // "Hello World_pong"
  } catch (error) {
    console.error(error);
  }
}
```

#### Cordova Usage of MyPlugin
```javascript
function buttonClick() {
  window.cordova.plugins.MyPlugin.ping(
    {text: "Hello World"},
    function(response) {
      console.log(response); // "Hello World_pong"
    },
    function(error) {
      console.error(error);
    }
  )
}
```

#### Synapse Usage of MyPlugin

```javascript
function buttonClick() {
  window.CapacitorUtils.Synapse.MyPlugin.ping(
    {text: "Hello World"},
    function(response) {
      console.log(response); // "Hello World_pong"
    },
    function(error) {
      console.error(error);
    }
  )
}
```

While similar to the way you call cordova plugins, the synapse call will use the appropriate plugin for the platform it is running on. If the app is built with Capacitor, it will use the Capacitor plugin. If the app is built with Cordova, it will use the Cordova plugin.