enum node_type {NODE_EMPTY, NODE_NULL, NODE_SCALAR, NODE_SEQUENCE, NODE_SEQUENCE_POINTER,
                NODE_MAPPING, NODE_MAPPING_POINTER, NODE_STREAM, NODE_STREAM_POINTER,
                NODE_DOCUMENT, NODE_DOCUMENT_POINTER};

typedef struct NodeElement {
    enum node_type type;
    char *key;
    union {
	    int ivalue;
	    void *pvalue;
    } data;
} NodeElement, *pNodeElement;

typedef struct NodeSequence {
    enum node_type type;
    int nelems;
    NodeElement elements[0];
} NodeSequence, *pNodeSequence;

typedef struct NodePair {
    char *key;
    NodeElement value;
} NodePair, *pNodePair;

typedef struct NodeMapping {
    enum node_type type;
    int nelems;
    NodeElement elements[0];
} NodeMapping, *pNodeMapping;

typedef struct NodeDocument {
    enum node_type type;
    int nelems;
    NodeElement elements[0];
} NodeDocument, *pNodeDocument;

typedef struct NodeStream {
    enum node_type type;
    int nelems;
    NodeElement elements[0];
} NodeStream, *pNodeStream;

typedef union Node {
    enum node_type type;
    NodeElement element;
    NodeSequence sequence;
    NodeMapping mapping;
    NodeDocument document;
    NodeStream stream;
} Node, *pNode;

typedef struct NodePosition {
    Node *node;
    char *name;
    int pos;
} NodePosition, *pNodePosition;

int NodeStackAllocate();
int NodePos();
NodeElement *NodeNew();
NodeElement *NodeAddr(int n);
int NodeCollect(enum node_type type, int ppos);
void NodeKeyValueCollect();
void NodeOutputTree(int level, NodeElement *NE);

Node *NodeFind(void *vnode, char *name);
int NodeGetBoolean(void *vnode, char *name, int value);
int NodeGetInteger(void *vnode, char *name, int value);
void NodeFindStart(void *vnode, char *name, pNodePosition pNS);
Node *NodeFindNext(pNodePosition pNS);
char *NodeGetString(void *vnode, char *name, char *value);

Node *ReadConfiguration(const char *fname);