
#pragma once

#include <string>
#include <functional>
#include <unordered_map>

#include <giecs/memory/context.hpp>
#include <giecs/memory/accessors/stack.hpp>
#include <giecs/core.hpp>
#include <giecs/ll/arithmetic.hpp>
#include <giecs/ll/io.hpp>
#include <giecs/ll/system.hpp>

#include <lisp/ast_write.hpp>

namespace lisp
{

/**
 * Contains symbol table and stack access
 */
template <int page_size, typename align_t, typename addr_t, typename val_t>
class Context
{
    private:
        static void ll_printi(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
        {
            giecs::ll::ConsoleIO<int>::print(stack);
            std::cout << std::endl;
        }

        static void ll_nop(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
        {
        }

        void eval_param(int l, giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
        {
            int i = 0;
            val_t buffer[l];

            while(i < l)
            {
                auto root = read_ast<page_size, align_t, addr_t, val_t>(stack);

                int start = this->def_stack.pos;
                parse(root, *this);
                int end = this->def_stack.pos;

                int sb = stack.pos;

                size_t len = end - start;
                size_t nl = 0;
                if(root->getType() == ast::NodeType::list)
                {
                    val_t buf[len];
                    this->def_stack.read(addr_t(start), len, buf);

                    stack.push(len, buf);
                    stack.pop();
                    this->core.eval(stack);

                    nl = stack.pos - sb;
                    stack.read(addr_t(sb), nl, &buffer[i]);
                }
                else
                {
                    this->def_stack.read(addr_t(start), len, &buffer[i]);
                    nl = len;
                }

                stack.pos = sb;
                this->def_stack.pos = start;
                i += nl;
            }

            stack.push(l, buffer);
        }

        static std::shared_ptr<ast::Node> expand_macro(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
        {
            auto plist = read_ast<page_size, align_t, addr_t, val_t>(stack);
            if(plist->getType() != ast::NodeType::list)
            {
                std::cout << "macro: first parameter must be a list!" << std::endl;
                return nullptr;
            }

            auto def = read_ast<page_size, align_t, addr_t, val_t>(stack);
            if(def->getType() == ast::NodeType::list)
            {
                auto list = std::static_pointer_cast<ast::List>(def);
                for(auto p : *std::static_pointer_cast<ast::List>(plist))
                {
                    auto node = read_ast<page_size, align_t, addr_t, val_t>(stack);
                    list->replace_symbol((*std::static_pointer_cast<ast::Atom<std::string>>(p))(), node);
                }
            }

            return def;
        };

    public:
        Context(giecs::memory::Context<page_size, align_t> const& mem_context_, giecs::Core<page_size, align_t, addr_t>& core_, int limit_)
            : mem_context(mem_context_), core(core_),
              limit(limit_), def_limit(0),
              stack(this->mem_context.template createStack<addr_t, val_t>()),
              def_stack(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>(this->mem_context,
        {
            0,0
        }, this->limit))
        {
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> eval =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                this->core.eval(stack);
            };
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> deval =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                int llen = stack.template pop<val_t>();
                addr_t laddr = stack.template pop<addr_t>();

                addr_t list_index[llen];

                for(int i = 0; i < llen; i++)
                {
                    list_index[i] = addr_t(laddr + i);
                    val_t attr = this->stack[addr_t(laddr + i)];
                    laddr = laddr + ((attr == val_t(-1)) ? val_t(1) : attr);
                }

                for(int i = llen-1; i >= 0; i--)
                {
                    addr_t list_addr = list_index[i];

                    val_t attr = this->stack[list_addr];
                    if(attr == val_t(-1))
                    {
                        // execute
                        addr_t w = this->stack[++list_addr];
                        stack.push(w);

                        this->core.eval(stack);
                    }
                    else
                    {
                        val_t buf[int(attr)];
                        this->stack.read(++list_addr, attr, buf);
                        stack.push(attr, buf);
                    }
                }

                this->core.eval(stack);
            };
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> quote =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                auto root = read_ast<page_size, align_t, addr_t, val_t>(stack);

                int start = this->def_stack.pos;
                parse(root, *this);
                int end = this->def_stack.pos;

                if(root->getType() != ast::NodeType::list)
                {
                    int len = end - start;
                    val_t buf[len];
                    this->def_stack.read(addr_t(start), len, buf);
                    stack.push(len, buf);
                    this->def_stack.pos = start;
                }
                else
                    stack.push(addr_t(start));
            };
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> define =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                auto name = read_ast<page_size, align_t, addr_t, val_t>(stack);
                if(name->getType() != ast::NodeType::symbol)
                {
                    std::cout << "define: first parameter must be a symbol!" << std::endl;
                    return;
                }

                auto def = read_ast<page_size, align_t, addr_t, val_t>(stack);

                int start = this->def_stack.pos;
                parse(def, *this);
                stack.push(addr_t(start));

                this->reset();
                this->core.eval(stack);

                this->save_symbol((*std::static_pointer_cast<ast::Atom<std::string>>(name))());
            };
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> peval =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                addr_t fn = stack.pop();
                int len = stack.template pop<val_t>();

                this->eval_param(len, stack);
                stack.push(fn);
                this->core.eval(stack);
            };
            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> call =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                auto fn = read_ast<page_size, align_t, addr_t, val_t>(stack);
                auto len = read_ast<page_size, align_t, addr_t, val_t>(stack);

                int start = this->def_stack.pos;
                parse(fn, *this);
                parse(len, *this);
                int end = this->def_stack.pos;
                int pl = end - start;
                this->def_stack.pos = start;

                val_t buf[pl];
                this->def_stack.read(start, pl, buf);
                stack.push(pl, buf);

                stack.push(this->symbols["peval"]);
                this->core.eval(stack);
            };


            std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> macro =
                [this](giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)
            {
                auto ast = expand_macro(stack);
                if(ast == nullptr)
                    return;

                int start = this->def_stack.pos;
                parse(ast, *this);
                stack.push(addr_t(start));

                this->reset();
                this->core.eval(stack);
            };

            this->add_macro("nop", ll_nop);
            this->add_macro("quote", quote);
            this->add_macro("define", define);
            this->add_macro("peval", peval);
            this->add_macro("call", call);
            this->add_macro("macro", macro);

            this->add_function("eval", 1, eval);
            this->add_function("deval", 2, deval);
            this->add_function("syscall", 6, giecs::ll::System::syscall);
            this->add_function("printi", 1, ll_printi);
            this->add_function("+", 2, giecs::ll::Arithmetic<int>::add);
            this->add_function("-", 2, giecs::ll::Arithmetic<int>::sub);
            this->add_function("*", 2, giecs::ll::Arithmetic<int>::mul);
            this->add_function("/", 2, giecs::ll::Arithmetic<int>::div);
            this->add_function("=", 2, giecs::ll::Arithmetic<int>::eq);
            this->add_function("!=", 2, giecs::ll::Arithmetic<int>::neq);
            this->add_function(">", 2, giecs::ll::Arithmetic<int>::gt);
            this->add_function(">=", 2, giecs::ll::Arithmetic<int>::get);
            this->add_function("<", 2, giecs::ll::Arithmetic<int>::lt);
            this->add_function("<=", 2, giecs::ll::Arithmetic<int>::let);
            this->add_function("vif", 3, giecs::ll::cond);
        }

        void reset(void)
        {
            this->def_stack.pos = this->def_limit;
        }

        void eval(void)
        {
            this->stack.push(addr_t(this->def_limit)); // startig address of last definition
            this->core.eval(this->stack);
        }

        void push(val_t v)
        {
            this->def_stack.push(v);
        }

        addr_t resolve_symbol(std::string name)
        {
            auto it = this->symbols.find(name);
            if(it != this->symbols.end())
                return it->second;
            else
                std::cout << "lisp: undefined symbol \"" << name << "\"." << std::endl;

            return addr_t();
        }
        void push(std::string name)
        {
            this->def_stack.push(resolve_symbol(name));
        }

        addr_t def_ptr(void) const
        {
            return addr_t(this->def_stack.pos);
        }

        void write_def(addr_t addr, val_t val)
        {
            this->def_stack[addr] = val;
        }

        struct List : public std::vector<val_t>
        {
            addr_t start;
        };

        List create_list(int len)
        {
            List l;
            l.start = addr_t(this->def_stack.pos);
            this->def_stack.pos += len;
            return l;
        }

        void push(List& l)
        {
            for(addr_t const& a : l)
            {
                this->def_stack[l.start] = a;
                ++l.start;
            }
        }

        template <typename Node>
        size_t push_ast(std::shared_ptr<Node> node)
        {
            return write_ast<page_size, align_t, addr_t, val_t>(node, this->def_stack);
        }

    private:
        std::unordered_map<std::string, addr_t> symbols;
        giecs::memory::Context<page_size, align_t> const& mem_context;
        giecs::Core<page_size, align_t, addr_t>& core;
        addr_t limit;
        addr_t def_limit;
        giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t> stack;
        giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t> def_stack;

        void save_symbol(std::string name)
        {
            this->symbols[name] = this->def_limit;
            this->def_limit = this->def_stack.pos;

            std::cout << "defined " << name << " as " << int(this->symbols[name]) << std::endl;
        }

        void add_macro(std::string name, void (*fn)(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack))
        {
            this->add_macro(name, std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)>(fn));
        }

        void add_macro(std::string name, std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> fn)
        {
            this->core.template addOperation<val_t>(this->def_ptr(), fn);
            this->def_stack.move(1);
            this->save_symbol(name);
        }

        void add_function(std::string name, int l, void (*fn)(giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack))
        {
            this->add_function(name, l, std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)>(fn));
        }

        void add_function(std::string name, int l, std::function<void (giecs::memory::accessors::Stack<page_size, align_t, addr_t, val_t>& stack)> fn)
        {
            this->core.template addOperation<val_t>(this->def_ptr(), fn);
            addr_t addr = this->def_ptr();
            this->def_stack.move(1);
            this->def_limit = this->def_stack.pos;

            this->push(3);
            this->push(std::string("peval"));
            this->push(addr);
            this->push(l);

            this->save_symbol(name);
        }

}; // class Context

} // namespace lisp

