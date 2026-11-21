export declare function parseXml(filename: string): Promise<Document>;
export declare function parseXmlString(contents: string): Document;
/**
 * Parses an XML fragment that may contain multiple root elements
 * by wrapping it in a temporary root element.
 * Accepts optional namespace attributes to support prefixed elements.
 */
export declare function parseXmlFragment(contents: string, namespaceAttrs?: string): NodeList;
export declare function serializeXml(doc: any): string;
export declare function formatXml(doc: any): Promise<string>;
export declare function writeXml(doc: any, filename: string): Promise<void>;
//# sourceMappingURL=xml.d.ts.map