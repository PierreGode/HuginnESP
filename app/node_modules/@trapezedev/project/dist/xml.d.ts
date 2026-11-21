import { VFS, VFSStorable } from './vfs';
export declare class XmlFile extends VFSStorable {
    private path;
    private vfs;
    private doc;
    private namespaces;
    constructor(path: string, vfs: VFS);
    load(): Promise<void>;
    getDocumentElement(): HTMLElement | undefined;
    private getNamespaceAttrs;
    /**
     * Selects the nodes matching an XPath node-set expression.
     *
     * `allowAnyNamespaceForNoPrefix` lets an unprefixed name test match elements in any
     * namespace, not just the document's default one. That is more permissive than plain
     * XPath 1.0, but acceptable here: prefixed name tests still resolve through the
     * namespaces collected from the root element, and without it an unprefixed target
     * matches nothing at all in a document that declares a default namespace.
     *
     * An expression that evaluates to a string, a number or a boolean selects nothing.
     */
    private select;
    private selectTargetNodes;
    find(target: string): Element[] | null;
    /**
     * Modifies every node matching the target and flags the file as modified.
     * A target that matches no node leaves the file untouched, so it isn't
     * needlessly rewritten on commit.
     */
    private modifyTargetNodes;
    deleteNodes(target: string): void;
    deleteAttributes(target: string, attributes: string[]): void;
    /**
     * Injects a fragment of XML as a child of the given target.
     * Note: If the target resolves to a node list, each node will
     * have the fragment appended.
     */
    injectFragment(target: string, fragment: string): void;
    /**
     * Merges a fragment of XML into the given target.
     */
    mergeFragment(target: string, fragment: string): void;
    mergeJsonTree(target: any, fragment: any): any;
    _mergeJson(target: any, fragment: any): void;
    /**
     * Replaces a given target with the given fragment
     */
    replaceFragment(target: string, fragment: string): void;
    /**
     * Set the key/value attributes on the target.
     * Note: if the target resolves to a node list, each node will
     * have its attributes modified
     */
    setAttrs(target: string, attrs: any): void;
    private xmlCommitFn;
    private xmlDiffFn;
}
//# sourceMappingURL=xml.d.ts.map