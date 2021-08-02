#include "daScript/misc/platform.h"

#include "module_builtin.h"

#include "daScript/ast/ast_interop.h"
#include "daScript/ast/ast_handle.h"
#include "daScript/simulate/aot_builtin.h"
#include "daScript/simulate/runtime_profile.h"
#include "daScript/simulate/hash.h"
#include "daScript/simulate/bin_serializer.h"
#include "daScript/simulate/runtime_array.h"
#include "daScript/simulate/runtime_range.h"
#include "daScript/simulate/runtime_string_delete.h"
#include "daScript/simulate/simulate_nodes.h"
#include "daScript/simulate/aot.h"
#include "daScript/misc/sysos.h"

namespace das
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif

    struct MarkFunctionAnnotation : FunctionAnnotation {
        MarkFunctionAnnotation(const string & na) : FunctionAnnotation(na) { }
        virtual bool apply(ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string & err) override {
            err = "not supported for block";
            return false;
        }
        virtual bool finalize(ExprBlock *, ModuleGroup &,const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
        virtual bool finalize(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
    };

    struct MarkFunctionOrBlockAnnotation : FunctionAnnotation {
        MarkFunctionOrBlockAnnotation() : FunctionAnnotation("marker") { }
        virtual bool apply ( const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string & ) {
            return true;
        }
        virtual bool apply(ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            return true;
        }
        virtual bool finalize(ExprBlock *, ModuleGroup &,const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
        virtual bool finalize(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string &) override {
            return true;
        }
    };

    extern ProgramPtr g_Program;

    struct MacroFunctionAnnotation : MarkFunctionAnnotation {
        MacroFunctionAnnotation() : MarkFunctionAnnotation("_macro") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->init = true;
            g_Program->needMacroModule = true;
            return true;
        };
    };

    struct UnsafeDerefFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeDerefFunctionAnnotation() : MarkFunctionAnnotation("unsafe_deref") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeDeref = true;
            return true;
        };
    };

    struct GenericFunctionAnnotation : MarkFunctionAnnotation {
        GenericFunctionAnnotation() : MarkFunctionAnnotation("generic") { }
        virtual bool isGeneric() const override {
            return true;
        }
        virtual bool apply(const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            return true;
        };
    };

    struct ExportFunctionAnnotation : MarkFunctionAnnotation {
        ExportFunctionAnnotation() : MarkFunctionAnnotation("export") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->exports = true;
            return true;
        };
    };

    struct SideEffectsFunctionAnnotation : MarkFunctionAnnotation {
        SideEffectsFunctionAnnotation() : MarkFunctionAnnotation("sideeffects") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->sideEffectFlags |= uint32_t(SideEffects::userScenario);
            return true;
        };
    };

    struct RunAtCompileTimeFunctionAnnotation : MarkFunctionAnnotation {
        RunAtCompileTimeFunctionAnnotation() : MarkFunctionAnnotation("run") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->hasToRunAtCompileTime = true;
            return true;
        };
    };

    struct UnsafeOpFunctionAnnotation : MarkFunctionAnnotation {
        UnsafeOpFunctionAnnotation() : MarkFunctionAnnotation("unsafe_operation") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->unsafeOperation = true;
            return true;
        };
    };

    struct NoAotFunctionAnnotation : MarkFunctionAnnotation {
        NoAotFunctionAnnotation() : MarkFunctionAnnotation("no_aot") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->noAot = true;
            return true;
        };
    };

    struct InitFunctionAnnotation : MarkFunctionAnnotation {
        InitFunctionAnnotation() : MarkFunctionAnnotation("init") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->init = true;
            return true;
        };
        virtual bool finalize(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & errors) override {
            if ( func->arguments.size() ) {
                errors += "[init] function can't have any arguments";
                return false;
            }
            if ( !func->result->isVoid() ) {
                errors += "[init] function can't return value";
                return false;
            }
            return true;
        }
    };

    struct FinalizeFunctionAnnotation : MarkFunctionAnnotation {
        FinalizeFunctionAnnotation() : MarkFunctionAnnotation("finalize") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->shutdown = true;
            return true;
        };
        virtual bool finalize(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, const AnnotationArgumentList &, string & errors) override {
            if ( func->arguments.size() ) {
                errors += "[finalize] function can't have any arguments";
                return false;
            }
            if ( !func->result->isVoid() ) {
                errors += "[finalize] function can't return value";
                return false;
            }
            return true;
        }
    };

    struct MarkUsedFunctionAnnotation : MarkFunctionAnnotation {
        MarkUsedFunctionAnnotation() : MarkFunctionAnnotation("unused_argument") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList & args, string &) override {
            for ( auto & fnArg : func->arguments ) {
                if ( auto optArg = args.find(fnArg->name, Type::tBool) ) {
                    fnArg->marked_used = optArg->bValue;
                }
            }
            return true;
        };
        virtual bool apply(ExprBlock * block, ModuleGroup &, const AnnotationArgumentList & args, string &) override {
            for ( auto & bArg : block->arguments ) {
                if ( auto optArg = args.find(bArg->name, Type::tBool) ) {
                    bArg->marked_used = optArg->bValue;
                }
            }
            return true;
        };
    };

    struct IsYetAnotherVectorTemplateAnnotation : MarkFunctionAnnotation {
        IsYetAnotherVectorTemplateAnnotation() : MarkFunctionAnnotation("expect_any_vector") {}
        virtual bool isSpecialized() const override { return true; }
        virtual bool isCompatible ( const FunctionPtr & fn, const vector<TypeDeclPtr> & types,
                const AnnotationDeclaration & decl, string & err  ) const override {
            size_t maxIndex = min(types.size(), fn->arguments.size());
            for ( size_t ai=0; ai!=maxIndex; ++ ai) {
                if ( decl.arguments.find(fn->arguments[ai]->name, Type::tBool) ) {
                    const auto & argT = types[ai];
                    if ( !(argT->isHandle() && argT->annotation && argT->annotation->isYetAnotherVectorTemplate()) ) {
                        err = "argument " + fn->arguments[ai]->name + " is expected to be a vector template";
                        return false;
                    }
                }
            }
            return true;
        }
        virtual bool apply(const FunctionPtr & fn, ModuleGroup &, const AnnotationArgumentList & decl, string & err) override {
            for ( const auto & arg : decl ) {
                if ( arg.type!=Type::tBool ) {
                    err = "expecting names only";
                    return false;
                }
                if ( !fn->findArgument(arg.name) ) {
                    err = "unknown argument " + arg.name;
                    return false;
                }
            }
            return true;
        }
    };

    // totally dummy annotation, needed for comments
    struct CommentAnnotation : StructureAnnotation {
        CommentAnnotation() : StructureAnnotation("comment") {}
        virtual bool touch(const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

    struct HybridFunctionAnnotation : MarkFunctionAnnotation {
        HybridFunctionAnnotation() : MarkFunctionAnnotation("hybrid") { }
        virtual bool apply(const FunctionPtr & func, ModuleGroup &, const AnnotationArgumentList &, string &) override {
            func->aotHybrid = true;
            return true;
        };
    };

    struct CppAlignmentAnnotation : StructureAnnotation {
        CppAlignmentAnnotation() : StructureAnnotation("cpp_layout") {}
        virtual bool touch(const StructurePtr & ps, ModuleGroup &,
                           const AnnotationArgumentList & args, string & ) override {
            ps->cppLayout = true;
            ps->cppLayoutNotPod = !args.getBoolOption("pod", true);
            return true;
        }
        virtual bool look ( const StructurePtr &, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            return true;
        }
    };

    struct LocalOnlyFunctionAnnotation : FunctionAnnotation {
        LocalOnlyFunctionAnnotation() : FunctionAnnotation("local_only") { }
        virtual bool apply ( ExprBlock *, ModuleGroup &, const AnnotationArgumentList &, string & err ) override {
            err = "not a block annotation";
            return false;
        }
        virtual bool finalize ( ExprBlock *, ModuleGroup &, const AnnotationArgumentList &,
                               const AnnotationArgumentList &, string & err ) override {
            err = "not a block annotation";
            return false;
        }
        virtual bool apply ( const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &, string & ) override {
            return true;
        };
        virtual bool finalize ( const FunctionPtr &, ModuleGroup &, const AnnotationArgumentList &,
                               const AnnotationArgumentList &, string & ) override {
            return true;
        }
        // [local_only ()]
        virtual bool verifyCall ( ExprCallFunc * call, const AnnotationArgumentList & args, string & err ) override {
            if ( !call->func ) {
                err = "unknown function";
                return false;
            }
            for ( size_t i=0; i!=call->func->arguments.size(); ++i ) {
                auto & farg = call->func->arguments[i];
                if ( auto it = args.find(farg->name, Type::tBool) ) {
                    auto & carg = call->arguments[i];
                    bool isLocalArg = carg->rtti_isMakeLocal() || carg->rtti_isMakeTuple();
                    bool isLocalFArg = it->bValue;
                    if ( isLocalArg != isLocalFArg ) {
                        err = isLocalFArg ? "expecting [[...]]" : "not expecting [[...]]";
                        return false;
                    }
                }
            }
            return true;
        }
    };

    struct PersistentStructureAnnotation : StructureAnnotation {
        PersistentStructureAnnotation() : StructureAnnotation("persistent") {}
        virtual bool touch(const StructurePtr & ps, ModuleGroup &,
                           const AnnotationArgumentList &, string & ) override {
            ps->persistent = true;
            return true;
        }
        virtual bool look ( const StructurePtr & st, ModuleGroup &,
                           const AnnotationArgumentList & args, string & errors ) override {
            bool allPod = true;
            if ( !args.getBoolOption("mixed_heap", false) ) {
                for ( const auto & field : st->fields ) {
                    if ( !field.type->isPod() ) {
                        allPod = false;
                        errors += "\t" + field.name + " : " + field.type->describe() + " is not a pod\n";
                    }
                }
            }
            return allPod;
        }
    };


#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    template <typename intT>
    struct EnumIterator : Iterator {
        EnumIterator ( EnumInfo * ei ) : info(ei) {}
        virtual bool first ( Context &, char * _value ) override {
            count = 0;
            range_to = int32_t(info->count);
            if ( range_to ) {
                intT * value = (intT *) _value;
                *value      = intT(info->fields[count++]->value);
                return true;
            } else {
                return false;
            }
        }
        virtual bool next  ( Context &, char * _value ) override {
            if ( range_to != count ) {
                intT * value = (intT *) _value;
                *value = intT(info->fields[count++]->value);
                return true;
            } else {
                return false;
            }
        }
        virtual void close ( Context & context, char * ) override {
            context.heap->free((char *)this, sizeof(RangeIterator));
        }
        EnumInfo *  info = nullptr;
        int32_t     count = 0;
        int32_t     range_to = 0;
    };

    // core functions

    void builtin_throw ( char * text, Context * context ) {
        context->throw_error(text);
    }

    void builtin_print ( char * text, Context * context ) {
        context->to_out(text);
    }

    vec4f builtin_breakpoint ( Context & context, SimNode_CallBase * call, vec4f * ) {
        context.breakPoint(call->debugInfo);
        return v_zero();
    }

    void builtin_stackwalk ( bool args, bool vars, Context * context, LineInfoArg * lineInfo ) {
        context->stackWalk(lineInfo, args, vars);
    }

    void builtin_terminate ( Context * context ) {
        context->throw_error("terminate");
    }

    int builtin_table_size ( const Table & arr ) {
        return arr.size;
    }

    int builtin_table_capacity ( const Table & arr ) {
        return arr.capacity;
    }

    void builtin_table_clear ( Table & arr, Context * context ) {
        table_clear(*context, arr);
    }

    vec4f _builtin_hash ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        auto uhash = hash_value(context, args[0], call->types[0]);
        return cast<uint32_t>::from(uhash);
    }

    uint64_t heap_bytes_allocated ( Context * context ) {
        return context->heap->bytesAllocated();
    }

    int32_t heap_depth ( Context * context ) {
        return (int32_t) context->heap->depth();
    }

    uint64_t string_heap_bytes_allocated ( Context * context ) {
        return context->stringHeap->bytesAllocated();
    }

    int32_t string_heap_depth ( Context * context ) {
        return (int32_t) context->stringHeap->depth();
    }

    void string_heap_collect ( Context * context, LineInfoArg * info ) {
        context->collectStringHeap(info);
    }

    void string_heap_report ( Context * context ) {
        context->stringHeap->report();
    }

    void heap_report ( Context * context ) {
        context->heap->report();
    }

    void builtin_table_lock ( const Table & arr, Context * context ) {
        table_lock(*context, const_cast<Table&>(arr));
    }

    void builtin_table_unlock ( const Table & arr, Context * context ) {
        table_unlock(*context, const_cast<Table&>(arr));
    }

    void builtin_table_clear_lock ( const Table & arr, Context * ) {
        const_cast<Table&>(arr).hopeless = 0;
    }

    bool builtin_iterator_first ( const Sequence & it, void * data, Context * context ) {
        if ( !it.iter ) context->throw_error("calling first on empty iterator");
        else if ( it.iter->isOpen ) context->throw_error("calling first on already open iterator");
        it.iter->isOpen = true;
        return it.iter->first(*context, (char *)data);
    }

    bool builtin_iterator_next ( const Sequence & it, void * data, Context * context ) {
        if ( !it.iter ) context->throw_error("calling next on empty iterator");
        else if ( !it.iter->isOpen ) context->throw_error("calling next on a non-open iterator");
        return it.iter->next(*context, (char *)data);
    }

    void builtin_iterator_close ( const Sequence & it, void * data, Context * context ) {
        if ( it.iter ) {
            it.iter->close(*context, (char *)&data);
            ((Sequence&)it).iter = nullptr;
        }
    }

    void builtin_iterator_delete ( const Sequence & it, Context * context ) {
        if ( it.iter ) it.iter->close(*context, nullptr);
        ((Sequence&)it).iter = nullptr;
    }

    bool builtin_iterator_iterate ( const Sequence & it, void * value, Context * context ) {
        if ( !it.iter ) {
            return false;
        } else if ( !it.iter->isOpen) {
            if ( !it.iter->first(*context, (char *)value) ) {
                it.iter->close(*context, (char *)value);
                ((Sequence&)it).iter = nullptr;
                return false;
            } else {
                it.iter->isOpen = true;
                return true;
            }
        } else {
            if ( !it.iter->next(*context, (char *)value) ) {
                it.iter->close(*context, (char *)value);
                ((Sequence&)it).iter = nullptr;
                return false;
            } else {
                return true;
            }
        }
    }

    void builtin_make_good_array_iterator ( Sequence & result, const Array & arr, int stride, Context * context ) {
        char * iter = context->heap->allocate(sizeof(GoodArrayIterator));
        context->heap->mark_comment(iter, "array<> iterator");
        new (iter) GoodArrayIterator((Array *)&arr, stride);
        result = { (Iterator *) iter };
    }

    void builtin_make_fixed_array_iterator ( Sequence & result, void * data, int size, int stride, Context * context ) {
        char * iter = context->heap->allocate(sizeof(FixedArrayIterator));
        context->heap->mark_comment(iter, "fixed array iterator");
        new (iter) FixedArrayIterator((char *)data, size, stride);
        result = { (Iterator *) iter };
    }

    void builtin_make_range_iterator ( Sequence & result, range rng, Context * context ) {
        char * iter = context->heap->allocate(sizeof(RangeIterator));
        context->heap->mark_comment(iter, "range iterator");
        new (iter) RangeIterator(rng, true);
        result = { (Iterator *) iter };
    }

    vec4f builtin_make_enum_iterator ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        if ( !call->types ) context.throw_error("missing type info");
        auto itinfo = call->types[0];
        if ( itinfo->type != Type::tIterator ) context.throw_error("not an iterator");
        auto tinfo = itinfo->firstType;
        if ( !tinfo ) context.throw_error("missing iterator type info");
        if ( tinfo->type!=Type::tEnumeration && tinfo->type!=Type::tEnumeration8
            && tinfo->type!=Type::tEnumeration16 ) {
            context.throw_error("not an iterator of enumeration");
        }
        auto einfo = tinfo->enumType;
        if ( !einfo ) context.throw_error("missing enumeraiton type info");
        char * iter = nullptr;
        switch ( tinfo->type ) {
        case Type::tEnumeration:
            iter = context.heap->allocate(sizeof(EnumIterator<int32_t>));
            context.heap->mark_comment(iter, "enum iterator");
            new (iter) EnumIterator<int32_t>(einfo);
            break;
        case Type::tEnumeration8:
            iter = context.heap->allocate(sizeof(EnumIterator<int8_t>));
            context.heap->mark_comment(iter, "enum8 iterator");
            new (iter) EnumIterator<int8_t>(einfo);
            break;
        case Type::tEnumeration16:
            iter = context.heap->allocate(sizeof(EnumIterator<int16_t>));
            context.heap->mark_comment(iter, "enum16 iterator");
            new (iter) EnumIterator<int16_t>(einfo);
            break;
        default:
            DAS_ASSERT(0 && "how???");
        }
        Sequence * seq = cast<Sequence *>::to(args[0]);
        seq->iter = (Iterator *) iter;
        return v_zero();
    }

    void builtin_make_string_iterator ( Sequence & result, char * str, Context * context ) {
        char * iter = context->heap->allocate(sizeof(StringIterator));
        context->heap->mark_comment(iter, "string iterator");
        new (iter) StringIterator(str);
        result = { (Iterator *) iter };
    }

    struct NilIterator : Iterator {
        virtual bool first ( Context &, char * ) override { return false; }
        virtual bool next  ( Context &, char * ) override { return false; }
        virtual void close ( Context & context, char * ) override {
            context.heap->free((char *)this, sizeof(NilIterator));
        }
    };

    void builtin_make_nil_iterator ( Sequence & result, Context * context ) {
        char * iter = context->heap->allocate(sizeof(NilIterator));
        context->heap->mark_comment(iter, "nil iterator");
        new (iter) NilIterator();
        result = { (Iterator *) iter };
    }

    struct LambdaIterator : Iterator {
        using lambdaFunc = bool (*) (Context *,void*, char*);
        LambdaIterator ( Context & context, const Lambda & ll ) : lambda(ll) {
            int32_t * fnIndex = (int32_t *) lambda.capture;
            if (!fnIndex) context.throw_error("invoke null lambda");
            simFunc = context.getFunction(*fnIndex-1);
            if (!simFunc) context.throw_error("invoke null function");
            aotFunc = (lambdaFunc) simFunc->aotFunction;
        }
        __forceinline bool InvokeLambda ( Context & context, char * ptr ) {
            if ( aotFunc ) {
                return (*aotFunc) ( &context, lambda.capture, ptr );
            } else {
                vec4f argValues[4] = {
                    cast<Lambda>::from(lambda),
                    cast<char *>::from(ptr)
                };
                auto res = context.call(simFunc, argValues, 0);
                return cast<bool>::to(res);
            }
        }
        virtual bool first ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual bool next  ( Context & context, char * ptr ) override {
            return InvokeLambda(context, ptr);
        }
        virtual void close ( Context & context, char * ) override {
            int32_t * fnIndex = (int32_t *) lambda.capture;
            SimFunction * finFunc = context.getFunction(fnIndex[1]-1);
            if (!finFunc) context.throw_error("generator finalizer is a null function");
            vec4f argValues[1] = {
                cast<void *>::from(lambda.capture)
            };
            auto flags = context.stopFlags; // need to save stop flags, we can be in the middle of some return or something
            context.call(finFunc, argValues, 0);
            context.heap->free((char *)this, sizeof(LambdaIterator));
            context.stopFlags = flags;
        }
        virtual void walk ( DataWalker & walker ) override {
            walker.beforeLambda(&lambda, lambda.getTypeInfo());
            walker.walk(lambda.capture, lambda.getTypeInfo());
            walker.afterLambda(&lambda, lambda.getTypeInfo());
        }
        Lambda          lambda;
        SimFunction *   simFunc = nullptr;
        lambdaFunc      aotFunc = nullptr;
    };

    void builtin_make_lambda_iterator ( Sequence & result, const Lambda lambda, Context * context ) {
        char * iter = context->heap->allocate(sizeof(LambdaIterator));
        context->heap->mark_comment(iter, "lambda iterator");
        new (iter) LambdaIterator(*context, lambda);
        result = { (Iterator *) iter };
    }

    void resetProfiler( Context * context ) {
        context->resetProfiler();
    }

    void dumpProfileInfo( Context * context ) {
        TextPrinter tout;
        context->collectProfileInfo(tout);
    }

    char * collectProfileInfo( Context * context ) {
        TextWriter tout;
        context->collectProfileInfo(tout);
        return context->stringHeap->allocateString(tout.str());
    }

    void builtin_array_free ( Array & dim, int szt, Context * __context__ ) {
        if ( dim.data ) {
            if ( !dim.lock || dim.hopeless ) {
                uint32_t oldSize = dim.capacity*szt;
                __context__->heap->free(dim.data, oldSize);
            } else {
                __context__->throw_error("can't delete locked array");
            }
            if ( dim.hopeless ) {
                memset ( &dim, 0, sizeof(Array) );
                dim.hopeless = true;
            } else {
                memset ( &dim, 0, sizeof(Array) );
            }
        }
    }

    void builtin_table_free ( Table & tab, int szk, int szv, Context * __context__ ) {
        if ( tab.data ) {
            if ( !tab.lock || tab.hopeless ) {
                uint32_t oldSize = tab.capacity*(szk+szv+sizeof(uint32_t));
                __context__->heap->free(tab.data, oldSize);
            } else {
                __context__->throw_error("can't delete locked table");
            }
            if ( tab.hopeless ) {
                memset ( &tab, 0, sizeof(Table) );
                tab.hopeless = true;
            } else {
                memset ( &tab, 0, sizeof(Table) );
            }
        }
    }

    void builtin_smart_ptr_clone_ptr ( smart_ptr_raw<void> & dest, const void * src ) {
        ptr_ref_count * t = (ptr_ref_count *) dest.ptr;
        dest.ptr = (void *) src;
        if ( src ) ((ptr_ref_count *) src)->addRef();
        if ( t ) t->delRef();
    }

    void builtin_smart_ptr_clone ( smart_ptr_raw<void> & dest, const smart_ptr_raw<void> src ) {
        ptr_ref_count * t = (ptr_ref_count *) dest.ptr;
        dest.ptr = src.ptr;
        if ( src.ptr ) ((ptr_ref_count *) src.ptr)->addRef();
        if ( t ) t->delRef();
    }

    uint32_t builtin_smart_ptr_use_count ( const smart_ptr_raw<void> src ) {
        ptr_ref_count * psrc = (ptr_ref_count *) src.ptr;
        return psrc ? psrc->use_count() : 0;
    }

    struct ClassInfoMacro : TypeInfoMacro {
        ClassInfoMacro() : TypeInfoMacro("rtti_classinfo") {}
        virtual TypeDeclPtr getAstType ( ModuleLibrary & lib, const ExpressionPtr &, string & ) override {
            return typeFactory<void *>::make(lib);
        }
        virtual SimNode * simluate ( Context * context, const ExpressionPtr & expr, string & )  override {
            auto exprTypeInfo = static_pointer_cast<ExprTypeInfo>(expr);
            TypeInfo * typeInfo = context->thisHelper->makeTypeInfo(nullptr, exprTypeInfo->typeexpr);
            return context->code->makeNode<SimNode_TypeInfo>(expr->at, typeInfo);
        }
        virtual void aotPrefix ( TextWriter & ss, const ExpressionPtr & ) override {
            ss << "(void *)(&";
        }
        virtual void aotSuffix ( TextWriter & ss, const ExpressionPtr & ) override {
            ss << ")";
        }
        virtual bool aotNeedTypeInfo ( const ExpressionPtr & ) const override {
            return true;
        }
    };

    bool is_compiling ( Context * ctx ) {
        if ( !ctx->thisProgram ) return false;
        return ctx->thisProgram->isCompiling || ctx->thisProgram->isSimulating;
    }

    bool is_compiling_macros ( Context * ctx ) {
        if ( !ctx->thisProgram ) return false;
        return ctx->thisProgram->isCompilingMacros;
    }

    bool is_compiling_macros_in_module ( char * name, Context * ctx ) {
        if ( !ctx->thisProgram ) return false;
        if ( !ctx->thisProgram->isCompilingMacros ) return false;
        if ( ctx->thisProgram->thisModule->name != to_rts(name) ) return false;
        return true;
    }

    // static storage

    das_hash_map<uint32_t, void*> g_static_storage;

    void gc0_save_ptr ( char * name, void * data, Context * context, LineInfoArg * line ) {
        uint32_t hash = hash_function ( *context, name );
        if ( g_static_storage.find(hash)!=g_static_storage.end() ) {
            context->throw_error_at(*line, "gc0 already there %s (or hash collision)", name);
        }
        g_static_storage[hash] = data;
    }

    void gc0_save_smart_ptr ( char * name, smart_ptr_raw<void> data, Context * context, LineInfoArg * line ) {
        gc0_save_ptr(name, data.get(), context, line);
    }

    void * gc0_restore_ptr ( char * name, Context * context ) {
        uint32_t hash = hash_function ( *context, name );
        auto it = g_static_storage.find(hash);
        if ( it!=g_static_storage.end() ) {
            void * res = it->second;
            g_static_storage.erase(it);
            return res;
        } else {
            return nullptr;
        }
    }

    smart_ptr_raw<void> gc0_restore_smart_ptr ( char * name, Context * context ) {
        return gc0_restore_ptr(name,context);
    }

    void gc0_reset() {
        das_hash_map<uint32_t, void*> dummy;
        swap ( g_static_storage, dummy );
    }

    __forceinline void i_das_ptr_inc ( void * & ptr, int stride ) {
        ptr = (char*) ptr + stride;
    }

    __forceinline void i_das_ptr_dec ( void * & ptr, int stride ) {
        ptr = (char*) ptr - stride;
    }

    __forceinline void * i_das_ptr_add ( void * ptr, int value, int stride ) {
        return (char*) ptr + value * stride;
    }

    __forceinline void * i_das_ptr_sub ( void * & ptr, int value, int stride ) {
        return (char*) ptr - value * stride;
    }

    __forceinline void i_das_ptr_set_add ( void * & ptr, int value, int stride ) {
        ptr = (char*) ptr + value * stride;
    }

    __forceinline void i_das_ptr_set_sub ( void * & ptr, int value, int stride ) {
        ptr = (char*) ptr + value * stride;
    }

    Module_BuiltIn::~Module_BuiltIn() {
        gc0_reset();
    }

    struct UnescapedStringMacro : public ReaderMacro {
        UnescapedStringMacro ( ) : ReaderMacro("_esc") {}
        virtual ExpressionPtr visit ( Program *, Module *, ExprReader * expr ) override {
            return make_smart<ExprConstString>(expr->at,expr->sequence);
        }
        virtual bool accept ( Program *, Module *, ExprReader * re, int Ch, const LineInfo & ) override {
            if ( Ch==-1 ) return false;
            re->sequence.push_back(char(Ch));
            auto sz = re->sequence.size();
            if ( sz>5 ) {
                auto tail = re->sequence.c_str() + sz - 5;
                if ( strcmp(tail,"%_esc")==0 ) {
                    re->sequence.resize(sz - 5);
                    return false;
                }
            }
            return true;
        }
    };

    TypeDeclPtr makePrintFlags() {
        auto ft = make_smart<TypeDecl>(Type::tBitfield);
        ft->alias = "print_flags";
        ft->argNames = { "escapeString", "namesAndDimensions", "typeQualifiers", "refAddresses", "humanReadable" };
        return ft;
    }

    template <>
    struct typeFactory<PrintFlags> {
        static TypeDeclPtr make(const ModuleLibrary &) {
            return makePrintFlags();
        }
    };

    vec4f builtin_sprint ( Context & context, SimNode_CallBase * call, vec4f * args ) {
        TextWriter ssw;
        auto typeInfo = call->types[0];
        auto res = args[0];
        auto flags = cast<uint32_t>::to(args[1]);
        ssw << debug_type(typeInfo) << " = " << debug_value(res, typeInfo, PrintFlags(flags));
        auto sres = context.stringHeap->allocateString(ssw.str());
        return cast<char *>::from(sres);
    }

    Array  g_CommandLineArguments;

    void setCommandLineArguments ( int argc, char * argv[] ) {
        g_CommandLineArguments.data = (char *) argv;
        g_CommandLineArguments.capacity = argc;
        g_CommandLineArguments.size = argc;
        g_CommandLineArguments.lock = 1;
        g_CommandLineArguments.flags = 0;
    }

    void getCommandLineArguments( Array & arr ) {
        arr = g_CommandLineArguments;
    }

    char * builtin_das_root ( Context * context ) {
        return context->stringHeap->allocateString(getDasRoot());
    }

    char * to_das_string(const string & str, Context * ctx) {
        return ctx->stringHeap->allocateString(str);
    }

    void set_das_string(string & str, const char * bs) {
        str = bs ? bs : "";
    }

    void peek_das_string(const string & str, const TBlock<void,TTemporary<const char *>> & block, Context * context) {
        vec4f args[1];
        args[0] = cast<const char *>::from(str.c_str());
        context->invoke(block, args, nullptr);
    }

    char * builtin_string_clone ( const char *str, Context * context ) {
        const uint32_t strLen = stringLengthSafe ( *context, str );
        if (!strLen)
            return nullptr;
        return context->stringHeap->allocateString(str, strLen);
    }

    void builtin_temp_array ( void * data, int size, const Block & block, Context * context ) {
        Array arr;
        arr.data = (char *) data;
        arr.size = arr.capacity = size;
        arr.lock = 1;
        arr.flags = 0;
        vec4f args[1];
        args[0] = cast<Array &>::from(arr);
        context->invoke(block, args, nullptr);
    }

    void builtin_make_temp_array ( Array & arr, void * data, int size ) {
        arr.data = (char *) data;
        arr.size = arr.capacity = size;
        arr.lock = 1;
        arr.flags = 0;
    }

    bool g_isInAot = false;
    bool is_in_aot ( ) {
        return g_isInAot;
    }

    void Module_BuiltIn::addRuntime(ModuleLibrary & lib) {
        // printer flags
        addAlias(makePrintFlags());
        // unescape macro
        addReaderMacro(make_smart<UnescapedStringMacro>());
        // function annotations
        addAnnotation(make_smart<CommentAnnotation>());
        addAnnotation(make_smart<MarkFunctionOrBlockAnnotation>());
        addAnnotation(make_smart<CppAlignmentAnnotation>());
        addAnnotation(make_smart<GenericFunctionAnnotation>());
        addAnnotation(make_smart<MacroFunctionAnnotation>());
        addAnnotation(make_smart<ExportFunctionAnnotation>());
        addAnnotation(make_smart<SideEffectsFunctionAnnotation>());
        addAnnotation(make_smart<RunAtCompileTimeFunctionAnnotation>());
        addAnnotation(make_smart<UnsafeOpFunctionAnnotation>());
        addAnnotation(make_smart<NoAotFunctionAnnotation>());
        addAnnotation(make_smart<InitFunctionAnnotation>());
        addAnnotation(make_smart<FinalizeFunctionAnnotation>());
        addAnnotation(make_smart<HybridFunctionAnnotation>());
        addAnnotation(make_smart<UnsafeDerefFunctionAnnotation>());
        addAnnotation(make_smart<MarkUsedFunctionAnnotation>());
        addAnnotation(make_smart<LocalOnlyFunctionAnnotation>());
        addAnnotation(make_smart<PersistentStructureAnnotation>());
        addAnnotation(make_smart<IsYetAnotherVectorTemplateAnnotation>());
        // typeinfo macros
        addTypeInfoMacro(make_smart<ClassInfoMacro>());
        // command line arguments
        addExtern<DAS_BIND_FUN(builtin_das_root)>(*this, lib, "get_das_root",
            SideEffects::accessExternal,"builtin_das_root");
        addExtern<DAS_BIND_FUN(getCommandLineArguments)>(*this, lib, "builtin_get_command_line_arguments",
                SideEffects::accessExternal,"getCommandLineArguments");
        // compile-time functions
        addExtern<DAS_BIND_FUN(is_compiling)>(*this, lib, "is_compiling",
            SideEffects::accessExternal, "is_compiling");
        addExtern<DAS_BIND_FUN(is_compiling_macros)>(*this, lib, "is_compiling_macros",
            SideEffects::accessExternal, "is_compiling_macros");
        addExtern<DAS_BIND_FUN(is_compiling_macros_in_module)>(*this, lib, "is_compiling_macros_in_module",
            SideEffects::accessExternal, "is_compiling_macros_in_module");
        // iterator functions
        addExtern<DAS_BIND_FUN(builtin_iterator_first)>(*this, lib, "_builtin_iterator_first",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_first");
        addExtern<DAS_BIND_FUN(builtin_iterator_next)>(*this, lib,  "_builtin_iterator_next",
                                                       SideEffects::modifyArgumentAndExternal, "builtin_iterator_next");
        addExtern<DAS_BIND_FUN(builtin_iterator_close)>(*this, lib, "_builtin_iterator_close",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_close");
        addExtern<DAS_BIND_FUN(builtin_iterator_delete)>(*this, lib, "_builtin_iterator_delete",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_delete");
        addExtern<DAS_BIND_FUN(builtin_iterator_iterate)>(*this, lib, "_builtin_iterator_iterate",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_iterate");
        addExtern<DAS_BIND_FUN(builtin_iterator_empty)>(*this, lib, "empty",
                                                        SideEffects::modifyArgumentAndExternal, "builtin_iterator_empty");
        // make-iterator functions
        addExtern<DAS_BIND_FUN(builtin_make_good_array_iterator)>(*this, lib,  "_builtin_make_good_array_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_good_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_fixed_array_iterator)>(*this, lib,  "_builtin_make_fixed_array_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_fixed_array_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_range_iterator)>(*this, lib,  "_builtin_make_range_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_range_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_string_iterator)>(*this, lib,  "_builtin_make_string_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_string_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_nil_iterator)>(*this, lib,  "_builtin_make_nil_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_nil_iterator");
        addExtern<DAS_BIND_FUN(builtin_make_lambda_iterator)>(*this, lib,  "_builtin_make_lambda_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_lambda_iterator");
        addInterop<builtin_make_enum_iterator,void,vec4f>(*this, lib, "_builtin_make_enum_iterator",
            SideEffects::modifyArgumentAndExternal, "builtin_make_enum_iterator");
        // functions
        addExtern<DAS_BIND_FUN(builtin_throw)>(*this, lib, "panic", SideEffects::modifyExternal, "builtin_throw");
        addExtern<DAS_BIND_FUN(builtin_print)>(*this, lib, "print", SideEffects::modifyExternal, "builtin_print");
        addInterop<builtin_sprint,char *,vec4f,PrintFlags>(*this, lib, "sprint", SideEffects::modifyExternal, "builtin_sprint");
        addExtern<DAS_BIND_FUN(builtin_terminate)>(*this, lib, "terminate", SideEffects::modifyExternal, "terminate");
        addInterop<builtin_breakpoint,void>(*this, lib, "breakpoint", SideEffects::modifyExternal, "breakpoint");
        // stackwalk
        auto fnsw = addExtern<DAS_BIND_FUN(builtin_stackwalk)>(*this, lib, "stackwalk", SideEffects::modifyExternal, "builtin_stackwalk");
        fnsw->arguments[0]->init = make_smart<ExprConstBool>(true);
        fnsw->arguments[1]->init = make_smart<ExprConstBool>(true);
        // profiler
        addExtern<DAS_BIND_FUN(resetProfiler)>(*this, lib, "reset_profiler", SideEffects::modifyExternal, "resetProfiler");
        addExtern<DAS_BIND_FUN(dumpProfileInfo)>(*this, lib, "dump_profile_info", SideEffects::modifyExternal, "dumpProfileInfo");
        addExtern<DAS_BIND_FUN(collectProfileInfo)>(*this, lib, "collect_profile_info", SideEffects::modifyExternal, "collectProfileInfo");
        // variant
        addExtern<DAS_BIND_FUN(variant_index)>(*this, lib, "variant_index", SideEffects::none, "variant_index");
        auto svi = addExtern<DAS_BIND_FUN(set_variant_index)>(*this, lib, "set_variant_index",
            SideEffects::modifyArgument, "set_variant_index");
        svi->unsafeOperation = true;
        // heap
        addExtern<DAS_BIND_FUN(heap_bytes_allocated)>(*this, lib, "heap_bytes_allocated",
                SideEffects::modifyExternal, "heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(heap_depth)>(*this, lib, "heap_depth",
                SideEffects::modifyExternal, "heap_depth");
        addExtern<DAS_BIND_FUN(string_heap_bytes_allocated)>(*this, lib, "string_heap_bytes_allocated",
                SideEffects::modifyExternal, "string_heap_bytes_allocated");
        addExtern<DAS_BIND_FUN(string_heap_depth)>(*this, lib, "string_heap_depth",
                SideEffects::modifyExternal, "string_heap_depth");
        auto shc = addExtern<DAS_BIND_FUN(string_heap_collect)>(*this, lib, "string_heap_collect",
                SideEffects::modifyExternal, "string_heap_collect");
        shc->unsafeOperation = true;
        addExtern<DAS_BIND_FUN(string_heap_report)>(*this, lib, "string_heap_report",
                SideEffects::modifyExternal, "string_heap_report");
       addExtern<DAS_BIND_FUN(heap_report)>(*this, lib, "heap_report",
                SideEffects::modifyExternal, "heap_report");
        // binary serializer
        addInterop<_builtin_binary_load,void,vec4f,const Array &>(*this,lib,"_builtin_binary_load",
            SideEffects::modifyArgumentAndExternal, "_builtin_binary_load");
        addInterop<_builtin_binary_save,void,const vec4f,const Block &>(*this, lib, "_builtin_binary_save",
            SideEffects::modifyExternal, "_builtin_binary_save");
        // function-like expresions
        addCall<ExprAssert>         ("assert",false);
        addCall<ExprAssert>         ("verify",true);
        addCall<ExprStaticAssert>   ("static_assert");
        addCall<ExprStaticAssert>   ("concept_assert");
        addCall<ExprDebug>          ("debug");
        addCall<ExprMemZero>        ("memzero");
        // hash
        addInterop<_builtin_hash,uint32_t,vec4f>(*this, lib, "hash", SideEffects::none, "_builtin_hash");
        // table functions
        addExtern<DAS_BIND_FUN(builtin_table_clear)>(*this, lib, "clear", SideEffects::modifyArgument, "builtin_table_clear");
        addExtern<DAS_BIND_FUN(builtin_table_size)>(*this, lib, "length", SideEffects::none, "builtin_table_size");
        addExtern<DAS_BIND_FUN(builtin_table_capacity)>(*this, lib, "capacity", SideEffects::none, "builtin_table_capacity");
        addExtern<DAS_BIND_FUN(builtin_table_lock)>(*this, lib, "__builtin_table_lock",
                                                    SideEffects::modifyArgumentAndExternal, "builtin_table_lock");
        addExtern<DAS_BIND_FUN(builtin_table_unlock)>(*this, lib, "__builtin_table_unlock",
                                                      SideEffects::modifyArgumentAndExternal, "builtin_table_unlock");
        addExtern<DAS_BIND_FUN(builtin_table_clear_lock)>(*this, lib, "__builtin_table_clear_lock",
                                                      SideEffects::modifyArgumentAndExternal, "builtin_table_clear_lock");
        addExtern<DAS_BIND_FUN(builtin_table_keys)>(*this, lib, "__builtin_table_keys",
                                                    SideEffects::modifyArgumentAndExternal, "builtin_table_keys");
        addExtern<DAS_BIND_FUN(builtin_table_values)>(*this, lib, "__builtin_table_values",
                                                      SideEffects::modifyArgumentAndExternal, "builtin_table_values");
        // array and table free
        addExtern<DAS_BIND_FUN(builtin_array_free)>(*this, lib, "__builtin_array_free", SideEffects::modifyArgumentAndExternal, "builtin_array_free");
        addExtern<DAS_BIND_FUN(builtin_table_free)>(*this, lib, "__builtin_table_free", SideEffects::modifyArgumentAndExternal, "builtin_table_free");
        // table expressions
        addCall<ExprErase>("__builtin_table_erase");
        addCall<ExprFind>("__builtin_table_find");
        addCall<ExprKeyExists>("__builtin_table_key_exists");
        // blocks
        addCall<ExprInvoke>("invoke");
        // smart ptr stuff
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_clone_ptr)>(*this, lib, "smart_ptr_clone", SideEffects::modifyExternal, "builtin_smart_ptr_clone_ptr");
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_clone)>(*this, lib, "smart_ptr_clone", SideEffects::modifyExternal, "builtin_smart_ptr_clone");
        addExtern<DAS_BIND_FUN(builtin_smart_ptr_use_count)>(*this, lib, "smart_ptr_use_count", SideEffects::none, "builtin_smart_ptr_use_count");
        addExtern<DAS_BIND_FUN(equ_sptr_sptr)>(*this, lib, "==", SideEffects::none, "equ_sptr_sptr");
        addExtern<DAS_BIND_FUN(nequ_sptr_sptr)>(*this, lib, "!=", SideEffects::none, "nequ_sptr_sptr");
        addExtern<DAS_BIND_FUN(equ_ptr_sptr)>(*this, lib, "==", SideEffects::none, "equ_ptr_sptr");
        addExtern<DAS_BIND_FUN(nequ_ptr_sptr)>(*this, lib, "!=", SideEffects::none, "nequ_ptr_sptr");
        addExtern<DAS_BIND_FUN(equ_sptr_ptr)>(*this, lib, "==", SideEffects::none, "equ_sptr_ptr");
        addExtern<DAS_BIND_FUN(nequ_sptr_ptr)>(*this, lib, "!=", SideEffects::none, "nequ_sptr_ptr");
        // gc0
        addExtern<DAS_BIND_FUN(gc0_save_ptr)>(*this, lib, "gc0_save_ptr", SideEffects::modifyExternal, "gc0_save_ptr");
        addExtern<DAS_BIND_FUN(gc0_save_smart_ptr)>(*this, lib, "gc0_save_smart_ptr", SideEffects::modifyExternal, "gc0_save_smart_ptr");
        addExtern<DAS_BIND_FUN(gc0_restore_ptr)>(*this, lib, "gc0_restore_ptr", SideEffects::accessExternal, "gc0_restore_ptr");
        addExtern<DAS_BIND_FUN(gc0_restore_smart_ptr)>(*this, lib, "gc0_restore_smart_ptr", SideEffects::accessExternal, "gc0_restore_smart_ptr");
        addExtern<DAS_BIND_FUN(gc0_reset)>(*this, lib, "gc0_reset", SideEffects::modifyExternal, "gc0_reset");
        // pointer ari
        addExtern<DAS_BIND_FUN(das_memcpy)>(*this, lib, "memcpy", SideEffects::modifyArgumentAndExternal, "das_memcpy")->unsafeOperation = true;
        addExtern<DAS_BIND_FUN(das_memcmp)>(*this, lib, "memcmp", SideEffects::none, "das_memcmp")->unsafeOperation = true;
        auto idpi = addExtern<DAS_BIND_FUN(i_das_ptr_inc)>(*this, lib, "i_das_ptr_inc", SideEffects::modifyArgument, "das_ptr_inc");
        idpi->unsafeOperation = true;
        idpi->firstArgReturnType = true;
        idpi->noPointerCast = true;
        auto idpd = addExtern<DAS_BIND_FUN(i_das_ptr_dec)>(*this, lib, "i_das_ptr_dec", SideEffects::modifyArgument, "das_ptr_dec");
        idpd->unsafeOperation = true;
        idpd->firstArgReturnType = true;
        idpd->noPointerCast = true;
        auto idpa = addExtern<DAS_BIND_FUN(i_das_ptr_add)>(*this, lib, "i_das_ptr_add", SideEffects::none, "das_ptr_add");
        idpa->unsafeOperation = true;
        idpa->firstArgReturnType = true;
        idpa->noPointerCast = true;
        auto idps = addExtern<DAS_BIND_FUN(i_das_ptr_sub)>(*this, lib, "i_das_ptr_sub", SideEffects::none, "das_ptr_sub");
        idps->unsafeOperation = true;
        idps->firstArgReturnType = true;
        idps->noPointerCast = true;
        auto idpsa = addExtern<DAS_BIND_FUN(i_das_ptr_set_add)>(*this, lib, "i_das_ptr_set_add", SideEffects::modifyArgument, "das_ptr_set_add");
        idpsa->unsafeOperation = true;
        idpsa->firstArgReturnType = true;
        idpsa->noPointerCast = true;
        auto idpss = addExtern<DAS_BIND_FUN(i_das_ptr_set_sub)>(*this, lib, "i_das_ptr_set_sub", SideEffects::modifyArgument, "das_ptr_set_sub");
        idpss->unsafeOperation = true;
        idpss->firstArgReturnType = true;
        idpss->noPointerCast = true;
        addExtern<DAS_BIND_FUN(i_das_ptr_diff)>(*this, lib, "i_das_ptr_diff", SideEffects::none, "i_das_ptr_diff");
        // profile
        addExtern<DAS_BIND_FUN(builtin_profile)>(*this,lib,"profile", SideEffects::modifyExternal, "builtin_profile");
        // das string binding
        addAnnotation(make_smart<DasStringTypeAnnotation>());
        addExtern<DAS_BIND_FUN(to_das_string)>(*this, lib, "string", SideEffects::none, "to_das_string");
        addExtern<DAS_BIND_FUN(set_das_string)>(*this, lib, "clone", SideEffects::modifyArgument,"set_das_string");
        addExtern<DAS_BIND_FUN(peek_das_string)>(*this, lib, "peek",
            SideEffects::modifyExternal,"peek_das_string_T")->setAotTemplate();
        addExtern<DAS_BIND_FUN(builtin_string_clone)>(*this, lib, "clone_string", SideEffects::none, "builtin_string_clone");
        addExtern<DAS_BIND_FUN(das_str_equ)>(*this, lib, "==", SideEffects::none, "das_str_equ");
        addExtern<DAS_BIND_FUN(das_str_nequ)>(*this, lib, "!=", SideEffects::none, "das_str_nequ");
        // temp array out of mem
        auto bta = addExtern<DAS_BIND_FUN(builtin_temp_array)>(*this, lib, "_builtin_temp_array",
            SideEffects::invoke, "builtin_temp_array");
        bta->unsafeOperation = true;
        bta->privateFunction = true;
        auto bmta = addExtern<DAS_BIND_FUN(builtin_make_temp_array)>(*this, lib, "_builtin_make_temp_array",
            SideEffects::modifyArgument, "builtin_make_temp_array");
        bmta->unsafeOperation = true;
                // migrate data
        addExtern<DAS_BIND_FUN(is_in_aot)>(*this, lib, "is_in_aot", SideEffects::worstDefault, "is_in_aot");
    }
}
