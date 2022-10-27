#undef NDEBUG /* workaround */

#include <stdio.h>
#include <yaml.h>

#include "Node.h"

#define RSTRING(name) case name: return #name;

static int parse_value(yaml_parser_t *parser, yaml_token_t *token);
static int parse_mapping(yaml_parser_t *parser, yaml_token_t *token);

const char *NodeTokenString(yaml_token_t *token)
{
    switch(token->type) {
    RSTRING(YAML_NO_TOKEN)
    RSTRING(YAML_STREAM_START_TOKEN)
    RSTRING(YAML_STREAM_END_TOKEN)
    RSTRING(YAML_VERSION_DIRECTIVE_TOKEN)
    RSTRING(YAML_TAG_DIRECTIVE_TOKEN)
    RSTRING(YAML_DOCUMENT_START_TOKEN)
    RSTRING(YAML_DOCUMENT_END_TOKEN)
    RSTRING(YAML_BLOCK_SEQUENCE_START_TOKEN)
    RSTRING(YAML_BLOCK_MAPPING_START_TOKEN)
    RSTRING(YAML_BLOCK_END_TOKEN)
    RSTRING(YAML_FLOW_SEQUENCE_START_TOKEN)
    RSTRING(YAML_FLOW_SEQUENCE_END_TOKEN)
    RSTRING(YAML_FLOW_MAPPING_START_TOKEN)
    RSTRING(YAML_FLOW_MAPPING_END_TOKEN)
    RSTRING(YAML_BLOCK_ENTRY_TOKEN)
    RSTRING(YAML_FLOW_ENTRY_TOKEN)
    RSTRING(YAML_KEY_TOKEN)
    RSTRING(YAML_VALUE_TOKEN)
    RSTRING(YAML_ALIAS_TOKEN)
    RSTRING(YAML_ANCHOR_TOKEN)
    RSTRING(YAML_TAG_TOKEN)
    RSTRING(YAML_SCALAR_TOKEN)
    default:
	    fprintf(stderr, "Strange token %d\n", token->type);
    }
}

static int parse_key(yaml_parser_t *parser, yaml_token_t *token)
{
    NodeElement *NE = NodeNew();

    if (NE == NULL) {
        fprintf(stderr, "No space in YAML stack\n");
        return -1;
    }

    //printf("parse_key\n");
    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    switch(token->type) {
    case YAML_SCALAR_TOKEN:
        NE->key = strdup(token->data.scalar.value);
        yaml_token_delete(token);
        yaml_parser_scan(parser, token);
        break;
    default:
        fprintf(stderr, "parse_key error %s\n", NodeTokenString(token));
        return -1;
    }

    return 0;
}

static int parse_flow_mapping(yaml_parser_t *parser, yaml_token_t *token)
{
    int pos = NodePos();
    int done = 0;

    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    while(!done) {
        switch(token->type) {
	    case YAML_KEY_TOKEN:
            if (parse_key(parser, token))
                return -1;
            break;
        case YAML_FLOW_MAPPING_END_TOKEN:
            break;
        case YAML_FLOW_ENTRY_TOKEN:
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            continue;
        default:
	        fprintf(stderr, "parse_mapping error %s\n", NodeTokenString(token));
            return -1;
        }
        switch(token->type) {
	    case YAML_VALUE_TOKEN:
            if (parse_value(parser, token))
                return -1;
            NodeKeyValueCollect();
            break;
        case YAML_FLOW_MAPPING_END_TOKEN:
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            if (NodeCollect(NODE_MAPPING, pos))
                return -1;
            done = 1;
            break;
        default:
	        fprintf(stderr, "parse_flow_mapping error %s\n", NodeTokenString(token));
            return -1;
        }
    }

    return 0;
}

static int parse_flow_sequence(yaml_parser_t *parser, yaml_token_t *token)
{
    int pos = NodePos();
    int done = 0;
    NodeElement *NE;

    //printf("parse_flow_sequence\n");
    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    while(!done) {
        switch(token->type) {
        case YAML_SCALAR_TOKEN:
            NE = NodeNew();
            if (NE == NULL) {
                fprintf(stderr, "No space in YAML stack\n");
                return -1;
            }
            NE->type = NODE_SCALAR;
            NE->data.pvalue = strdup(token->data.scalar.value);
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            break;
        case YAML_FLOW_ENTRY_TOKEN:
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            break;
        case YAML_FLOW_SEQUENCE_END_TOKEN:
            if (NodeCollect(NODE_SEQUENCE, pos))
                return -1;
            done = 1;
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            break;
        case YAML_FLOW_MAPPING_START_TOKEN:
            if (parse_flow_mapping(parser, token))
                return -1;
            break;
        case YAML_FLOW_SEQUENCE_START_TOKEN:
            if (parse_flow_sequence(parser, token))
                return -1;
            break;
        default:
	        fprintf(stderr, "parse_flow_sequence error %s\n", NodeTokenString(token));
            return -1;
        }
    }

    return 0;
}

static int parse_value(yaml_parser_t *parser, yaml_token_t *token)
{
    NodeElement *NE;

    //printf("parse_value\n");
    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    switch(token->type) {
    case YAML_KEY_TOKEN:
        NE = NodeNew();
        if (NE == NULL) {
            fprintf(stderr, "No space in YAML stack\n");
            return -1;
        }
        NE->type = NODE_NULL;
        NE->data.pvalue = NULL;
        break;
    case YAML_SCALAR_TOKEN:
        NE = NodeNew();
        if (NE == NULL) {
            fprintf(stderr, "No space in YAML stack\n");
            return -1;
        }
        NE->type = NODE_SCALAR;
        NE->data.pvalue = strdup(token->data.scalar.value);
        yaml_token_delete(token);
        yaml_parser_scan(parser, token);
        break;
    case YAML_FLOW_SEQUENCE_START_TOKEN:
        if (parse_flow_sequence(parser, token))
            return -1;
        break;
    case YAML_BLOCK_MAPPING_START_TOKEN:
        parse_mapping(parser, token);
        break;
    case YAML_FLOW_MAPPING_START_TOKEN:
        if (parse_flow_mapping(parser, token))
            return -1;

        break;
    default:
        fprintf(stderr, "parse_value error %s\n", NodeTokenString(token));
        return -1;
    }

    return 0;
}

static int parse_mapping(yaml_parser_t *parser, yaml_token_t *token)
{
    int pos = NodePos();
    int done = 0;
    NodeElement *NE;

    //printf("parse_mapping\n");
    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    while(!done) {
        switch(token->type) {
	    case YAML_KEY_TOKEN:
            parse_key(parser, token);
            break;
        case YAML_BLOCK_END_TOKEN:
            break;
        default:
	        fprintf(stderr, "parse_mapping error %s\n", NodeTokenString(token));
            return -1;
        }
        switch(token->type) {
	    case YAML_VALUE_TOKEN:
            if (parse_value(parser, token))
                return -1;

            NodeKeyValueCollect();
            break;
        case YAML_BLOCK_END_TOKEN:
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            if (NodeCollect(NODE_MAPPING, pos))
                return -1;

            done = 1;
            break;
        default:
	        fprintf(stderr, "parse_mapping error %s\n", NodeTokenString(token));
            return -1;
        }
    }

    return 0;
}

static int parse_document(yaml_parser_t *parser, yaml_token_t *token)
{
    int pos = NodePos();
    int done = 0;
    int was_start = 0;

    //printf("parse_document\n");
    while(!done) {
        switch(token->type) {
        case YAML_DOCUMENT_START_TOKEN:
            if (was_start || pos != NodePos()) {
                if (NodeCollect(NODE_DOCUMENT, pos))
                    return -1;
                pos = NodePos();
            }
            was_start = 1;
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            break;
        case YAML_DOCUMENT_END_TOKEN:
            if (was_start || pos != NodePos()) {
                if (NodeCollect(NODE_DOCUMENT, pos))
                    return -1;
                pos = NodePos();
            }
            was_start = 0;
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            break;
#if 0
        case YAML_NO_TOKEN:
            if (was_start) {
                NodeCollect(NODE_DOCUMENT, pos);
                pos = NodePos();
            }
            was_start = 1;
            yaml_token_delete(token);
            yaml_parser_scan(parser, token);
            done = 1;
            break;
#endif
        case YAML_BLOCK_MAPPING_START_TOKEN:
            parse_mapping(parser, token);
            break;
        case YAML_STREAM_END_TOKEN:
            if (was_start || pos != NodePos()) {
                if (NodeCollect(NODE_DOCUMENT, pos))
                    return -1;;
                pos = NodePos();
            }
            was_start = 0;
            done = 1;
            break;
        default:
            //yaml_token_delete(token);
            //yaml_parser_scan(parser, token);
	        fprintf(stderr, "parse_document error %s\n", NodeTokenString(token));
            return -1;
        }
    }

    return 0;
}

static int parse_stream(yaml_parser_t *parser, yaml_token_t *token)
{
    int was_start = 0;
    int done = 0;
    int pos = NodePos();
    NodeDocument *document;

    //printf("parse_stream\n");
    yaml_parser_scan(parser, token);
    if (token->type != YAML_STREAM_START_TOKEN) {
	    fprintf(stderr, "parse_stream error %s\n", NodeTokenString(token));
	    return -1;
    }
    //printf("yaml encoding %d\n", token->data.stream_start.encoding);
    yaml_token_delete(token);
    yaml_parser_scan(parser, token);
    /* Parsing documents */
    while (!done) {
        switch(token->type) {
        case YAML_STREAM_END_TOKEN:
            yaml_token_delete(token);
            done = 1;
            break;
        default:
            if (parse_document(parser, token))
                return -1;
            break;
        }
    }

    /* Collect elements to stream */
    if (NodeCollect(NODE_STREAM, pos))
        return -1;

    return 0;
}

Node *ReadConfiguration(const char *name)
{
    FILE *file;
    yaml_parser_t parser = {0};
    yaml_token_t  token;
    int ret;

    if(NodeStackAllocate())
        return NULL;

    file = fopen(name, "rb");
    if (file == NULL) {
        perror("fopen");
        return NULL;
    }

    yaml_parser_initialize(&parser);

    //printf("parser.encoding %d\n", parser.encoding);
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    yaml_parser_set_input_file(&parser, file);
    ret = parse_stream(&parser, &token);
    yaml_parser_delete(&parser);

    fclose(file);

    return ret ? NULL : (Node *)NodeAddr(0);
}


