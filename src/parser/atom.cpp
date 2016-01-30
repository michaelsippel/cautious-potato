#include <string.h>

#include <logger.h>
#include <context.h>
#include <reader.h>
#include <parser.h>

static Logger* parser_logger = new Logger("parser");
static Logger* atom_logger = new Logger(parser_logger, "atom");

int parse_symbol(Context* context, vword_t addr, SNode* ast)
{
    vword_t res = resolve_symbol(ast->string);
    if(res != 0)
    {
        *((vword_t*) context->base(addr)) = res;
    }
    else
    {
        atom_logger->log(lerror, "couldn't resolve symbol \"%s\"", ast->string);
    }

    return sizeof(vword_t);
}

int parse_string(Context* context, vword_t addr, SNode* ast)
{
    strcpy((char*) context->base(addr), ast->string);
    return strlen(ast->string);
}

int parse_integer(Context* context, vword_t addr, SNode* ast)
{
    *((vword_t*) context->base(addr)) = (vword_t) ast->integer;
    return sizeof(vword_t);
}
