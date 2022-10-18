#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#include "Node.h"

static Node *NodeFixed(NodeElement *pNE)
{
    switch(pNE->type) {
        case NODE_SEQUENCE_POINTER:
        case NODE_MAPPING_POINTER:
        case NODE_DOCUMENT_POINTER:
            return pNE->data.pvalue;
    }
    return (Node *)pNE;
}

Node *NodeFind(void *vnode, char *name)
{
    int i;
    Node *node = vnode;

    if (vnode == NULL)
        return NULL;

    switch (node->type)
    {
    case NODE_MAPPING:
    {
        pNodeMapping pNM = &node->mapping;
        for (i = 0; i < pNM->nelems; i++) {
            NodeElement *pNE = &pNM->elements[i];
            if (pNE->key != NULL & !strcmp(name, pNE->key))
                return NodeFixed(pNE);
        }
        return NULL;
    }

    case NODE_MAPPING_POINTER:
    case NODE_DOCUMENT_POINTER:
    case NODE_STREAM_POINTER:
        return NodeFind(node->element.data.pvalue, name);

    case NODE_DOCUMENT:
        if (node->document.nelems < 1)
            return NULL;
        return NodeFind(node->document.elements, name);

    case NODE_STREAM:
        if (node->stream.nelems < 1)
            return NULL;
        return NodeFind(node->stream.elements, name);

    default:
        break;
    }
    return NULL;
}

int NodeGetBoolean(void *vnode, char *name, int value)
{
    Node *node = vnode;
    char *str;

    if (vnode == NULL)
        return value;

    if (name != NULL)
        node = NodeFind(vnode, name);

    if (node == NULL)
        return value;

    if (node->type != NODE_SCALAR)
        return value;

    str = node->element.data.pvalue;
    if (str == NULL)
        return value;

    if (!strcasecmp(str, "on") || !strcasecmp(str, "true") || !strcmp(str, "1"))
        return 1;

    if (!strcasecmp(str, "off") || !strcasecmp(str, "false") || !strcmp(str, "0"))
        return 0;

    return value;
}

int NodeGetInteger(void *vnode, char *name, int value)
{
    Node *node = vnode;
    char *str;
    int result;

    if (vnode == NULL)
        return value;

    if (name != NULL)
        node = NodeFind(vnode, name);

    if (node == NULL)
        return value;

    if (node->type != NODE_SCALAR)
        return value;

    str = node->element.data.pvalue;
    if (str == NULL)
        return value;

    if (sscanf(str, "%i", &result) < 1)
        return value;

    return result;
}

void NodeFindStart(void *vnode, char *name, pNodePosition pNS)
{
    pNS->node = NodeFixed(vnode);
    pNS->name = name;
    pNS->pos = 0;
}

Node *NodeFindNext(pNodePosition pNS)
{
    pNodeMapping pNM = (pNodeMapping)pNS->node;
    pNodeSequence pS = (pNodeSequence)pNS->node;
    int i;

    switch(pNS->node->type) {
    case NODE_MAPPING:
        if (pNM->nelems <= pNS->pos)
            return NULL;
        for (i = pNS->pos; i < pNM->nelems; i++) {
            char *key = pNM->elements[i].key;
            if (pNS->name != NULL && (key == NULL || strcmp(key, pNS->name)))
                continue;
            pNS->pos = i + 1;
            return NodeFixed(&pNM->elements[i]);
        }
        pNS->pos = pNM->nelems;
        return NULL;
    case NODE_SEQUENCE:
        if (pS->nelems <= pNS->pos)
            return NULL;
        i = pNS->pos++;
        return NodeFixed(&pS->elements[i]); 
    default:
        return NULL;
    }
}

char *NodeGetString(void *vnode, char *name, char *value)
{
    Node *node = vnode;
    char *str;

    if (vnode == NULL)
        return value;

    if (name != NULL)
        node = NodeFind(vnode, name);

    if (node == NULL)
        return value;

    if (node->type != NODE_SCALAR)
        return value;

    str = node->element.data.pvalue;
    if (str == NULL)
        return value;

    return str;
}