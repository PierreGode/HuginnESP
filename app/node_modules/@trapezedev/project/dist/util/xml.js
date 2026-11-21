"use strict";
/// <reference lib="dom" />
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseXml = parseXml;
exports.parseXmlString = parseXmlString;
exports.parseXmlFragment = parseXmlFragment;
exports.serializeXml = serializeXml;
exports.formatXml = formatXml;
exports.writeXml = writeXml;
const path_1 = require("path");
const utils_fs_1 = require("@ionic/utils-fs");
const xmldom_1 = __importStar(require("@xmldom/xmldom"));
const prettier_1 = __importDefault(require("prettier"));
const plugin_xml_1 = __importDefault(require("@prettier/plugin-xml"));
async function parseXml(filename) {
    let contents = await (0, utils_fs_1.readFile)(filename, { encoding: 'utf-8' });
    if (!contents) {
        contents = '<?xml version="1.0" encoding="utf-8" ?>\n<root />';
    }
    return new xmldom_1.default.DOMParser().parseFromString(contents, 'text/xml');
}
function parseXmlString(contents) {
    return new xmldom_1.default.DOMParser().parseFromString(contents, 'text/xml');
}
/**
 * Parses an XML fragment that may contain multiple root elements
 * by wrapping it in a temporary root element.
 * Accepts optional namespace attributes to support prefixed elements.
 */
function parseXmlFragment(contents, namespaceAttrs) {
    const nsAttrs = namespaceAttrs ? ` ${namespaceAttrs}` : '';
    const wrapped = `<__fragment__${nsAttrs}>${contents}</__fragment__>`;
    const doc = new xmldom_1.default.DOMParser().parseFromString(wrapped, 'text/xml');
    return doc.documentElement.childNodes;
}
function serializeXml(doc) {
    return new xmldom_1.XMLSerializer().serializeToString(doc);
}
async function formatXml(doc) {
    const xml = new xmldom_1.XMLSerializer().serializeToString(doc);
    const pluginDir = (0, path_1.dirname)(require.resolve('@prettier/plugin-xml'));
    return prettier_1.default.format(xml, {
        parser: 'xml',
        printWidth: 120,
        bracketSameLine: true,
        xmlWhitespaceSensitivity: 'ignore',
        tabWidth: 4,
        pluginSearchDirs: [pluginDir],
        plugins: [plugin_xml_1.default]
    });
}
async function writeXml(doc, filename) {
    const formatted = await formatXml(doc);
    return (0, utils_fs_1.writeFile)(filename, formatted);
}
//# sourceMappingURL=xml.js.map