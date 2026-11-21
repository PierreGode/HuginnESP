const Buffer = require('.')

global.Buffer = Buffer

global.atob = Buffer.atob
global.btoa = Buffer.btoa
