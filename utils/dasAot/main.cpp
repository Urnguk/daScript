#include "daScript/daScript.h"
#include "../examples/common/fileAccess.h"

using namespace das;

TextPrinter tout;

bool saveToFile ( const string & fname, const string & str ) {
    printf("saving to %s\n", fname.c_str());
    FILE * f = fopen ( fname.c_str(), "w" );
    if ( !f ) {
        tout << "can't open " << fname << "\n";
        return false;
    }
    fwrite ( str.c_str(), str.length(), 1, f );
    return true;
}

bool compile ( const string & fn, const string & cppFn ) {
    auto access = make_shared<FsFileAccess>();
    ModuleGroup dummyGroup;
    if ( auto program = compileDaScript(fn,access,tout,dummyGroup) ) {
        if ( program->failed() ) {
            for ( auto & err : program->errors ) {
                tout << reportError(err.at, err.what, err.cerr );
            }
            return false;
        } else {
            Context ctx;
            if ( !program->simulate(ctx, tout) ) {
                tout << "failed to simulate\n";
                for ( auto & err : program->errors ) {
                    tout << reportError(err.at, err.what, err.cerr );
                }
                return false;
            }
            // AOT time
            TextWriter tw;
            // header
            tw << "#include \"daScript/misc/platform.h\"\n\n";

            tw << "#include \"daScript/simulate/simulate.h\"\n";
            tw << "#include \"daScript/simulate/aot.h\"\n";
            tw << "#include \"daScript/simulate/aot_library.h\"\n";
            tw << "\n";
            // lets comment on required modules
            program->library.foreach([&](Module * mod){
                if ( mod->name=="" ) {
                    // nothing, its main program module. i.e ::
                } else {
                    if ( mod->name=="$" ) {
                        tw << " // require builtin\n";
                    } else {
                        tw << " // require " << mod->name << "\n";
                    }
                    mod->aotRequire(tw);
                }
                return true;
            },"*");
            tw << "\n";
            tw << "#if defined(_MSC_VER)\n";
            tw << "#pragma warning(push)\n";
            tw << "#pragma warning(disable:4100)   // unreferenced formal parameter\n";
            tw << "#pragma warning(disable:4189)   // local variable is initialized but not referenced\n";
            tw << "#pragma warning(disable:4244)   // conversion from 'int32_t' to 'float', possible loss of data\n";
            tw << "#elif defined(__clang__)\n";
            tw << "#pragma clang diagnostic push\n";
            tw << "#pragma clang diagnostic ignored \"-Wunused-parameter\"\n";
            tw << "#pragma clang diagnostic ignored \"-Wwritable-strings\"\n";
            tw << "#pragma clang diagnostic ignored \"-Wunused-variable\"\n";
            tw << "#pragma clang diagnostic ignored \"-Wunsequenced\"\n";
            tw << "#pragma clang diagnostic ignored \"-Wunused-function\"\n";
            tw << "#endif\n";
            tw << "\n";
            tw << "namespace das {\n";
            tw << "namespace {\n"; // anonymous
            // AOT actual
            program->aotCpp(ctx, tw);
            // list STUFF
            tw << "struct AotList_impl : AotListBase {\n";
            tw << "\tvirtual void registerAotFunctions ( AotLibrary & aotLib ) override {\n";
            program->registerAotCpp(tw, ctx, false);
            tw << "\t};\n";
            tw << "};\n";
            tw << "AotList_impl impl;\n";
            tw << "}\n";
            tw << "}\n";
            tw << "#if defined(_MSC_VER)\n";
            tw << "#pragma warning(pop)\n";
            tw << "#elif defined(__clang__)\n";
            tw << "#pragma clang diagnostic pop\n";
            tw << "#endif\n";
            // and save
            return saveToFile(cppFn, tw.str());
        }
    } else {
        return false;
    }
}

int main(int argc, const char * argv[]) {
    if ( argc!=3 ) {
        tout << "dasAot [script.das] [script.das.src]\n";
        return -1;
    }
    NEED_MODULE(Module_BuiltIn);
    NEED_MODULE(Module_Math);
    NEED_MODULE(Module_Random);
    NEED_MODULE(Module_PathTracerHelper);
    NEED_MODULE(Module_TestProfile);
    NEED_MODULE(Module_UnitTest);
    bool compiled = compile(argv[1], argv[2]);
    Module::Shutdown();
    return compiled ? 0 : -1;
}

