#!/usr/bin/env node
'use strict';

var fs = require('fs');
var parser = require('./lib/parser');
var path = process.argv.slice(2).join('');

if (!path) {
  console.error('No input detected');
  return;
}

if (fs.statSync(path)) {
  parser.parseFile(path).then(function(parsedValue) {
    console.log(JSON.stringify(parsedValue, '', 2));
  });
}
