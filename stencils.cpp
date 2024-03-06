#include <cstdio>
#include <cstdint>
#include <chrono>
#include <thread>
#include <iostream>
#include <memory>
#include <map>
#include <mutex>

#include <jsapi.h>
#include <js/CompilationAndEvaluation.h>
#include <js/Conversions.h>
#include <js/Initialization.h>
#include <js/SourceText.h>
#include <js/experimental/JSStencil.h>
#include <js/experimental/CompileScript.h>

#include "boilerplate.h"

// This example illustrates how to use a cache of Stencils to
// optimise performance by avoiding repetitive compilations.
// It does no error handling and simply exits if something goes wrong.
//
// The 'boilerplate.cpp' is taken from SpiderMonkey embedding examples.
//
// To reuse Stencils in multiple threads, you must create a JS::FrontendContext in
// each thread that compiles Javascript. Spidermonkey 115 ESR requires some modifications.
// It doesn't expose API to retrieve error messages from FrontendContext objects nor
// allows cleaning up errors in the context. Subsequent releases of Spidermonkey will
// have a richer API for that.
//
// This example will be in SpiderMonkey embedding examples

// Helper output
std::ostream &labeled_cout() {
    return std::cout<<"Thread: "<<std::hex<<std::this_thread::get_id()<<" ";
}

// Helper FrontendContext creator
JS::FrontendContext *frontend_context()
{
    static constexpr size_t kCompileStackQuota = 128 * sizeof(size_t) * 1024;
    auto *fc = JS::NewFrontendContext();
    if (fc)
        JS::SetNativeStackQuota(fc, kCompileStackQuota);
    return fc;
}

// Cache of compiled scripts
class JSCache
{
public:
    ~JSCache() {
        labeled_cout()<<"Destructing cache holding "<<_cache.size()<<" scripts\n";
    }

    RefPtr<JS::Stencil> find(std::string const &key) {
        std::lock_guard<std::mutex> lk(_mx);
        auto it = _cache.find(key);
        return it == _cache.end() ? nullptr : it->second;
    }

    void insert(std::string const &key, RefPtr<JS::Stencil>const &val) {
        std::lock_guard<std::mutex> lk(_mx);
        _cache.insert(std::pair{key, val});
    }

private:
    std::mutex _mx;
    std::map<std::string, RefPtr<JS::Stencil>> _cache;
};

// Scripts compiler and executer
class Job
{
public:
    Job(JSCache &cache) : _cache(cache) {};

    void ExecuteScript(JSContext *cx, std::string const &script,
        const char* filename, unsigned long linenumber);

private:
    RefPtr<JS::Stencil> compile_script(std::string const &script,
        const char* filename, unsigned long linenumber);

private:
    JSCache &_cache;
};

void Job::ExecuteScript(JSContext *cx, std::string const &script,
        const char* filename, unsigned long linenumber)
{
    JS::RootedScript rscript(cx);
    RefPtr<JS::Stencil> stencil(_cache.find(script));

    if (stencil == nullptr)
    {
        labeled_cout()<<"Compiling script\n";
        stencil = compile_script(script, filename, linenumber);
        if (stencil != nullptr)
            _cache.insert(script, stencil);
    } else {
        labeled_cout()<<"Taking script from cache\n";
    }

    if (stencil != nullptr) {
        JS::InstantiateOptions instantiateOptions;
        rscript = JS::InstantiateGlobalStencil(cx, instantiateOptions, stencil);
        if (!rscript) {
            boilerplate::ReportAndClearException(cx);
            return;
        }

        JS::RootedValue val(cx);
        if (!JS_ExecuteScript(cx, rscript, &val)) {
            boilerplate::ReportAndClearException(cx);
            return;
        }
    }
}

RefPtr<JS::Stencil> Job::compile_script(std::string const &script,
    const char* filename, unsigned long linenumber)
{
    JS::SourceText<mozilla::Utf8Unit> source;
    auto *fc = frontend_context();
    if (!fc)
        return nullptr;

    if (!source.init(fc, script.c_str(), script.size(), JS::SourceOwnership::Borrowed))
    {
        labeled_cout()<<"Error initializing JS source\n";
        JS::DestroyFrontendContext(fc);
        return nullptr;
    }

    JS::CompileOptions opts{JS::CompileOptions::ForFrontendContext()};
    opts.setFileAndLine(filename, linenumber);
    opts.setNonSyntacticScope(true);
    JS::CompilationStorage compileStorage;
    RefPtr<JS::Stencil> st =
	    JS::CompileGlobalScriptToStencil(fc, opts, source, compileStorage);

    if (st == nullptr) {
        labeled_cout()<<"Error compiling script, presumably due to a syntax error.\n";
        //need a better report of the error, but we don't have exposed API
    }

    JS::DestroyFrontendContext(fc);
    return st;
}

////////////////////////////////////////////////////////////////////////////////
// Code to illustrate how to use stencils and the cache

static bool Print(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    JS::Rooted<JS::Value> arg(cx, args.get(0));
    JS::Rooted<JSString*> str(cx, JS::ToString(cx, arg));
    if (!str)
        return false;

    JS::UniqueChars chars = JS_EncodeStringToUTF8(cx, str);
    labeled_cout()<<chars.get()<<"\n";

    args.rval().setUndefined();
    return true;
}

bool DefineFunctions(JSContext* cx, JS::Handle<JSObject*> global) {
    return JS_DefineFunction(cx, global, "print", &Print, 0, 0);
}

static void ExecuteExamples(JSContext *cx, Job &job)
{
    static char const *js1 = R"js(
        print(`JS log one: ${new Date()}`);
        )js";

    // a script with a syntactic error
    static char const *js2 = R"js(
        await print(`JS log two: ${new Date()}`);
        )js";

    static char const *js3 = R"js(
        print(`JS log three: ${new Date()}`);
        )js";

    static char const *scripts[] = {js1, js2, js3};

    JS::Rooted<JSObject*> global(cx, boilerplate::CreateGlobal(cx));
    if (!global) {
        labeled_cout()<<"Failed during boilerplate::CreateGlobal\n";
        return;
    }

    JSAutoRealm ar(cx, global);

    if (!DefineFunctions(cx, global)) {
        boilerplate::ReportAndClearException(cx);
        return;
    }

    char const *filename = "none";
    unsigned long linenumber = 0; //should show in error reports
    for (auto code: scripts) {
        job.ExecuteScript(cx, code, filename, ++linenumber);
    }
}

static void ThreadFunction(JSRuntime* parentRuntime, JSCache *cache)
{
    JSContext* cx(JS_NewContext(8L * 1024L * 1024L, parentRuntime));
    Job job(*cache);

    labeled_cout()<<"Child thread started\n";

    if (!JS::InitSelfHostedCode(cx)) {
        labeled_cout()<<"Failed during JS::InitSelfHostedCode\n";
        JS_DestroyContext(cx);
        return;
    }

    for (int i = 0 ; i < 2; ++i)
        ExecuteExamples(cx, job);

    JS_DestroyContext(cx);
}

static bool StencilExample(JSContext* cx) {
    JSCache cache;

    labeled_cout()<<"Main thread started\n";

    std::thread thread(ThreadFunction, JS_GetRuntime(cx), &cache);

    Job job(cache);
    for (int i = 0 ; i < 2; ++i)
        ExecuteExamples(cx, job);

    thread.join();

    return true;
}

int main(int argc, const char* argv[]) {
    if (!boilerplate::RunExample(StencilExample)) {
        return 1;
    }
    return 0;
}

