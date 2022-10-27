#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Node.h"

static NodeElement *elements;
static int pos = 0;

#define MAX_NODES_IN_STACK	1024

int NodeStackAllocate()
{
    elements = calloc(MAX_NODES_IN_STACK, sizeof(NodeElement));
    if (elements == NULL) {
	    perror("NodeStackAllocate");
	    return -1;
    }

    return 0;
}

int NodePos()
{
    return pos;
}

NodeElement *NodeNew()
{
    if (pos >= MAX_NODES_IN_STACK)
        return NULL;

    return &elements[pos++];
}

NodeElement *NodeAddr(int n)
{
    return &elements[n];
}

int NodeCollect(enum node_type type, int ppos)
{
    int nelems = pos - ppos;
    NodeElement *prev = &elements[ppos];
 
    switch(type) {
    case NODE_STREAM:
    {
        pNodeStream pNS = calloc(1, sizeof(NodeStream) + sizeof(NodeElement) * nelems);
        pNS->type = NODE_STREAM;
        pNS->nelems = nelems;
        memcpy(pNS->elements, prev, sizeof(NodeElement) * nelems);
        prev->type = NODE_STREAM_POINTER;
        prev->key = NULL;
        prev->data.pvalue = pNS;
        pos = ppos + 1;
    }
    break;
    case NODE_DOCUMENT:
    {
        pNodeDocument pND = calloc(1, sizeof(NodeDocument) + sizeof(NodeElement) * nelems);
        pND->type = NODE_DOCUMENT;
        pND->nelems = nelems;
        memcpy(pND->elements, prev, sizeof(NodeElement) * nelems);
        prev->type = NODE_DOCUMENT_POINTER;
        prev->key = NULL;
        prev->data.pvalue = pND;
        pos = ppos + 1;
        //NodeOutputTree(0, prev);
    }
    break;
    case NODE_MAPPING:
    {
        pNodeMapping pNM = calloc(1, sizeof(NodeMapping) + sizeof(NodeElement) * nelems);
        pNM->type = NODE_MAPPING;
        pNM->nelems = nelems;
        memcpy(pNM->elements, prev, sizeof(NodeElement) * nelems);
        prev->type = NODE_MAPPING_POINTER;
        prev->key = NULL;
        prev->data.pvalue = pNM;
        pos = ppos + 1;
    }
    break;
    case NODE_SEQUENCE:
    {
        pNodeMapping pNS = calloc(1, sizeof(NodeSequence) + sizeof(NodeElement) * nelems);
        pNS->type = NODE_SEQUENCE;
        pNS->nelems = nelems;
        memcpy(pNS->elements, prev, sizeof(NodeElement) * nelems);
        prev->type = NODE_SEQUENCE_POINTER;
        prev->key = NULL;
        prev->data.pvalue = pNS;
        pos = ppos + 1;
    }
    break;
    default:
        printf("Illegal collection code %d\n", type);
        return -1;
    }

    return 0;
}

void NodeKeyValueCollect()
{
    pos--;
    elements[pos - 1].type = elements[pos].type;
    elements[pos - 1].data = elements[pos].data;
}

void NodeOutputTree(int level, NodeElement *NE)
{
    int i;

    switch(NE->type) {
    //case NODE_EMPTY:
    case NODE_NULL:
        break;
    case NODE_SCALAR:
    {
        printf("%*s", level * 2," ");
        if (NE->key)
            printf("%s : ", NE->key);
        printf("%s\n", (char*)NE->data.pvalue);
    }
    break;
    //case NODE_SEQUENCE:
    case NODE_SEQUENCE_POINTER:
    {
        pNodeSequence pNS = NE->data.pvalue;
        printf("%*s", level * 2," ");
        if (NE->key)
            printf("%s : ", NE->key);
        printf("[\n");
        for (i = 0; i < pNS->nelems; i++) {
            NodeOutputTree(level + 1, &pNS->elements[i]);
        }
        printf("%*s]\n", level * 2," ");
    }
    break;
    //case NODE_MAPPING:
    case NODE_MAPPING_POINTER:
    {
        pNodeMapping pNM = NE->data.pvalue;
        printf("%*s", level * 2," ");
        if (NE->key)
            printf("%s : ", NE->key);
        printf("{\n");
        for (i = 0; i < pNM->nelems; i++) {
            NodeOutputTree(level + 1, &pNM->elements[i]);
        }
        printf("%*s}\n", level * 2," ");
    }
    break;
    case NODE_STREAM:
    {
        pNodeStream pNS = (pNodeStream)NE;
        for (i = 0; i < pNS->nelems; i++) {
            NodeOutputTree(0, &pNS->elements[i]);
        }
    }
    break;
    case NODE_STREAM_POINTER:
    {
        pNodeStream pNS = (pNodeStream)NE->data.pvalue;
        for (i = 0; i < pNS->nelems; i++) {
            NodeOutputTree(0, &pNS->elements[i]);
        }
    }
    break;
    case NODE_DOCUMENT:
    {
        pNodeDocument pND = (pNodeDocument)NE;
        printf("---\n");
        for (i = 0; i < pND->nelems; i++) {
            NodeOutputTree(level + 1, &pND->elements[i]);
        }
        printf("...\n");
    }
    break;
    case NODE_DOCUMENT_POINTER:
        NodeOutputTree(0, NE->data.pvalue);
        break;
    default:
        printf("Illegal node type %d\n", NE->type);
    }
}