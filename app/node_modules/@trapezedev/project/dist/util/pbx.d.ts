export declare function parsePbxProject(filename: string): Promise<any>;
/**
 * PBX files are esoteric. Quote every value that Xcode itself would quote,
 * otherwise the pbxproj we write can no longer be parsed (a `,` terminates a
 * value, and non-ASCII characters are not valid in an unquoted value).
 * Inside a quoted value, a backslash and a double quote have to be escaped.
 */
export declare function pbxSerializeString(value: string): string;
/**
 * The pbx parser hands quoted values back verbatim, quotes and escapes
 * included, so undo what `pbxSerializeString` wrote.
 */
export declare function pbxReadString(value: string): string;
//# sourceMappingURL=pbx.d.ts.map