/*
 * Synthetic 4D engine boundary for ownership regressions.
 *
 * Compiles the real plugin entry point (4DPlugin-Ultralight.cpp, included
 * below) and links the real IFileSystem.cpp, support/4DPlugin-JSON.cpp, the
 * built classes of the bundled 4D Plugin API and the unmodified
 * 4DPluginAPI.c. Only gCall4D is replaced. Every engine resource (Unistrings,
 * pictures, objects/collections, handles) is a real heap block tracked in a
 * live set, so leaks are detected exactly and a dispose of something the
 * engine no longer owns (double free) is reported by name.
 *
 * Nothing is rendered or fetched: ultralight_stubs.cpp replaces the
 * Ultralight renderer, JSHelpers and libpng (see that file).
 *
 * Engine semantics modelled (per the SDK contract):
 *  - A longint parameter slot holds exactly one PA_long32.
 *  - PA_CreateVariable(eVK_Unistring/eVK_Picture) allocates an empty value.
 *  - PA_GetStringVariable returns a view borrowed from the variable.
 *  - PA_SetUnistring/PA_ReturnString read a NUL-terminated buffer.
 *  - PA_SetObjectProperty deep-copies strings and pictures and retains
 *    objects/collections; the caller still owns its variable.
 *  - PA_GetObjectProperty returns an owned copy (or a retained object/
 *    collection) that must be cleared; on error the result is left
 *    uninitialised (poisoned here) and must be neither read nor cleared.
 *  - PA_ExecuteCommandByID returns an owned result.
 *  - PA_RunInMainProcess runs the procedure synchronously.
 */

/* Resolved through -I <source-root>, so the same harness tests baseline and fixed trees. */
#include "4DPlugin-Ultralight.cpp"

#include "PrivateTypes.h"
#include "EntryPoints.h"

#include "ultralight_stubs.h"

#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" Call4DProcPtr gCall4D;

namespace {

typedef std::basic_string<PA_Unichar> u16;

struct Obj {
    int rc;
    bool coll;
    std::map<u16, PA_Variable> props;
    std::vector<PA_Variable> items;
};

/* data first: the SDK dereferences array handles directly (*(T**)handle). */
struct Handle { char *data; size_t size; };

std::set<void *> g_strings;
std::map<void *, size_t> g_pictures;
std::set<Obj *> g_objects;
std::set<void *> g_handles;
std::vector<std::string> g_violations;

/* PA_GetObjectProperty for this key fails: eb.fError is set and the result
   is left uninitialised. The poison is readable (tests fail by assertion,
   not by crashing) but never engine-owned, so clearing it is counted. */
std::string g_fail_key;
/* JSON Stringify (1217) fails the same way. */
bool g_fail_stringify = false;
const char kPoisonMark = 0x5A;
int g_poison_clears = 0;
PA_Unichar kPoisonBuf[] = { 'B', 'A', 'D', '!', 0 };
PA_Unichar *const kPoison = kPoisonBuf;

PA_Variable undefined_var();

void poison(PA_Variable *out) {
    *out = undefined_var();
    out->fFiller = kPoisonMark;
    out->fType = eVK_Unistring;
    out->uValue.fString.fString = kPoison;
    out->uValue.fString.fLength = 4;
}

void violation(const std::string &what) {
    fprintf(stderr, "  ENGINE CONTRACT: %s\n", what.c_str());
    g_violations.push_back(what);
}

[[noreturn]] void die(const char *msg) {
    fprintf(stderr, "HARNESS ERROR: %s\n", msg);
    fflush(stderr);
    abort();
}

std::string narrow(const u16 &s) { std::string r; for(PA_Unichar c : s) r.push_back((char)c); return r; }
u16 widen(const std::string &s) { u16 r; for(char c : s) r.push_back((PA_Unichar)(unsigned char)c); return r; }

PA_Unistring new_string(const PA_Unichar *src, size_t len) {
    PA_Unichar *buf = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if(len) memcpy(buf, src, len * sizeof(PA_Unichar));
    buf[len] = 0;
    g_strings.insert(buf);
    PA_Unistring s;
    s.fLength = (PA_long32)len;
    s.fString = buf;
    s.fReserved1 = 0;
    s.fReserved2 = 0;
    return s;
}

void free_string(PA_Unistring *s) {
    if(!s->fString) return;
    if(s->fString == kPoison) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed engine call\n");
    } else if(!g_strings.erase(s->fString)) {
        violation("double_dispose_string");
    } else {
        free(s->fString);
    }
    s->fString = 0;
    s->fLength = 0;
}

void *new_picture(size_t size) {
    void *p = malloc(16);
    g_pictures[p] = size;
    return p;
}

void free_picture(void *p) {
    if(!p) return;
    if(!g_pictures.erase(p)) { violation("double_dispose_picture"); return; }
    free(p);
}

Handle *new_handle(size_t size) {
    Handle *h = (Handle *)malloc(sizeof(Handle));
    h->size = size;
    h->data = (char *)calloc(1, size ? size : 1);
    g_handles.insert(h);
    return h;
}

void free_handle(Handle *h) {
    if(!h) return;
    if(!g_handles.erase(h)) { violation("double_dispose_handle"); return; }
    free(h->data);
    free(h);
}

Handle *live_handle(void *p) {
    if(!g_handles.count(p)) die("used an unknown handle");
    return (Handle *)p;
}

void release_object(Obj *o);

void clear_var(PA_Variable *v) {
    if(v->fFiller == kPoisonMark) {
        g_poison_clears++;
        fprintf(stderr, "  cleared the uninitialised result of a failed engine call\n");
        v->fType = eVK_Undefined;
        v->fFiller = 0;
        return;
    }
    switch(v->fType) {
        case eVK_Unistring: free_string(&v->uValue.fString); break;
        case eVK_Picture: free_picture(v->uValue.fPicture); break;
        case eVK_Blob: free_handle((Handle *)v->uValue.fBlob.fHandle); break;
        case eVK_Object: if(v->uValue.fObject) release_object((Obj *)v->uValue.fObject); break;
        case eVK_Collection: if(v->uValue.fCollection) release_object((Obj *)v->uValue.fCollection); break;
        default: break;
    }
    v->fType = eVK_Undefined;
}

Obj *live(void *p) {
    if(!g_objects.count((Obj *)p)) die("used a dead object/collection");
    return (Obj *)p;
}

PA_Variable undefined_var() { PA_Variable v; memset(&v, 0, sizeof(v)); v.fType = eVK_Undefined; return v; }

PA_Variable copy_var(const PA_Variable &src) {
    PA_Variable dst = src;
    switch(src.fType) {
        case eVK_Unistring:
            dst.uValue.fString = new_string(src.uValue.fString.fString, src.uValue.fString.fLength);
            break;
        case eVK_Picture: {
            PA_Picture pic = src.uValue.fPicture; /* PA_Variable is packed */
            if(!g_pictures.count(pic)) die("stored a dead picture");
            dst.uValue.fPicture = new_picture(g_pictures[pic]);
            break;
        }
        case eVK_Object:
            if(src.uValue.fObject) live(src.uValue.fObject)->rc++;
            break;
        case eVK_Collection:
            if(src.uValue.fCollection) live(src.uValue.fCollection)->rc++;
            break;
        default: break;
    }
    return dst;
}

Obj *new_object(bool coll = false) {
    Obj *o = new Obj();
    o->rc = 1;
    o->coll = coll;
    g_objects.insert(o);
    return o;
}

void release_object(Obj *o) {
    if(!g_objects.count(o)) { violation("double_release_object"); return; }
    if(--o->rc == 0) {
        for(auto &kv : o->props) clear_var(&kv.second);
        for(auto &v : o->items) clear_var(&v);
        g_objects.erase(o);
        delete o;
    }
}

u16 key_of(const PA_Unistring &k) { return u16(k.fString, k.fLength); }

PA_Variable object_result(Obj *o) {
    PA_Variable v = undefined_var();
    v.fType = o->coll ? eVK_Collection : eVK_Object;
    if(o->coll) v.uValue.fCollection = (PA_CollectionRef)o; else v.uValue.fObject = (PA_ObjectRef)o;
    return v;
}

void execute_command(EngineBlock *eb) {
    PA_Variable **params = (PA_Variable **)eb->fPtr1;
    PA_Variable *ret = (PA_Variable *)eb->fPtr2;
    switch(eb->fParam1) {
        case 1225: { /* OB Copy */
            Obj *src = live(params[0]->uValue.fObject);
            Obj *dst = new_object(false);
            for(auto &kv : src->props) dst->props[kv.first] = copy_var(kv.second);
            *ret = object_result(dst);
            break;
        }
        case 1217: { /* JSON Stringify */
            live(params[0]->uValue.fObject);
            if(g_fail_stringify) { poison(ret); eb->fError = -1; break; }
            u16 json = widen("{}");
            ret->fType = eVK_Unistring;
            ret->uValue.fString = new_string(json.c_str(), json.size());
            break;
        }
        case 1231: { /* OB Is defined */
            Obj *o = live(params[0]->uValue.fObject);
            ret->fType = eVK_Boolean;
            ret->uValue.fBoolean = o->props.count(key_of(params[1]->uValue.fString)) ? 1 : 0;
            break;
        }
        case 1471: *ret = object_result(new_object(false)); break; /* New Object */
        case 1472: *ret = object_result(new_object(true)); break; /* New Collection */
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unexpected PA_ExecuteCommandByID %d", (int)eb->fParam1);
            die(msg);
        }
    }
}

void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_SET_UNISTRING: {
            /* Replaces the content of an engine-owned string with a copy of
               the NUL-terminated source. */
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            PA_Unistring s = new_string(src, len);
            free_string(&eb->fUniString1);
            eb->fUniString1 = s;
            break;
        }
        case EX_DISPOSE_UNISTRING:
            free_string(&eb->fUniString1);
            break;
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if(v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString; /* borrowed */
            break;
        }
        case EX_CREATE_PICTURE:
            eb->fPicture = new_picture((size_t)eb->fParam1);
            break;
        case EX_DISPOSE_PICTURE:
            free_picture(eb->fPicture);
            break;
        case EX_CLEAR_VARIABLE:
            clear_var((PA_Variable *)eb->fPtr1);
            break;
        case EX_GET_INFORMATION:
            eb->fParam2 = 0x00002000; /* 4D v20: PA_CreateObject uses New Object */
            break;
        case EX_RUN_IN_MAIN_PROCESS:
            ((PA_RunInMainProcessProcPtr)eb->fPtr1)(eb->fPtr2);
            break;
        case EX_SET_OBJ_VALUE: {
            Obj *o = live(eb->fPtr1);
            if(eb->fShort == eVK_Collection) die("collection set not modelled");
            PA_Variable value = copy_var(*(PA_Variable *)eb->fPtr2);
            u16 k = key_of(eb->fUniString1);
            auto it = o->props.find(k);
            if(it != o->props.end()) clear_var(&it->second);
            o->props[k] = value;
            break;
        }
        case EX_GET_OBJ_VALUE: {
            if(!eb->fPtr1) { violation("property_of_null_object"); *(PA_Variable *)eb->fPtr2 = undefined_var(); eb->fError = -1; break; }
            Obj *o = live(eb->fPtr1);
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if(eb->fShort == eVK_Collection) die("collection get not modelled");
            u16 k = key_of(eb->fUniString1);
            if(!g_fail_key.empty() && narrow(k) == g_fail_key) {
                poison(out);
                eb->fError = -1;
                break;
            }
            auto it = o->props.find(k);
            *out = it == o->props.end() ? undefined_var() : copy_var(it->second);
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID:
            execute_command(eb);
            break;
        case EX_HANDLE_MANAGER: {
            switch(eb->fParam1) {
                case 1: eb->fHandle = (PA_Handle)new_handle((size_t)eb->fParam2); break;
                case 2: free_handle((Handle *)eb->fHandle); break;
                case 4: eb->fParam2 = (sLONG_PTR)live_handle(eb->fHandle)->size; break;
                case 5: eb->fParam3 = (sLONG_PTR)live_handle(eb->fHandle)->data; break;
                case 6: break;
                default: die("unexpected handle manager op");
            }
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unmodelled engine selector %d", selector);
            die(msg);
        }
    }
}

/* ---------- test helpers ---------- */

PA_Unistring U(const char *ascii) {
    u16 s = widen(ascii);
    return new_string(s.c_str(), s.size());
}

void put(Obj *o, const char *key, PA_Variable v) {
    PA_Unistring k = U(key);
    PA_SetObjectProperty((PA_ObjectRef)o, &k, v);
    PA_DisposeUnistring(&k);
}

void put_text(Obj *o, const char *key, const char *value) {
    PA_Variable v = undefined_var();
    PA_Unistring u = U(value);
    PA_SetStringVariable(&v, &u);
    put(o, key, v);
    PA_ClearVariable(&v);
}

void put_real(Obj *o, const char *key, double d) {
    PA_Variable v = undefined_var();
    PA_SetRealVariable(&v, d);
    put(o, key, v);
}

Obj *put_object(Obj *o, const char *key, bool coll) {
    Obj *c = new_object(coll);
    PA_Variable v = object_result(c);
    put(o, key, v);
    PA_ClearVariable(&v);
    return c;
}

bool check(bool ok, const char *what) {
    if(!ok) fprintf(stderr, "  check failed: %s\n", what);
    return ok;
}

bool no_leaks() {
    bool ok = true;
    for(void *p : g_strings) {
        fprintf(stderr, "  LEAK: engine string \"%s\"\n", narrow(u16((PA_Unichar *)p)).c_str());
        ok = false;
    }
    if(!g_pictures.empty()) { fprintf(stderr, "  LEAK: %zu engine picture(s)\n", g_pictures.size()); ok = false; }
    if(!g_objects.empty()) { fprintf(stderr, "  LEAK: %zu engine object(s)/collection(s)\n", g_objects.size()); ok = false; }
    if(!g_handles.empty()) { fprintf(stderr, "  LEAK: %zu engine handle(s)\n", g_handles.size()); ok = false; }
    if(g_poison_clears) { fprintf(stderr, "  %d failed result(s) cleared\n", g_poison_clears); ok = false; }
    if(!g_violations.empty()) ok = false;
    return ok;
}

/* 4D hands every parameter as a pointer to its own slot; a longint slot is
   exactly one PA_long32 (PA_GetLongParameter reads PA_long32). */
struct Call {
    std::vector<void *> args;
    std::vector<PA_long32 *> longs;
    std::vector<PA_Unistring *> texts;
    sLONG_PTR result = 0;
    PA_Unistring text_result;
    PA_Unistring *text_result_ptr = nullptr;

    Call &l(PA_long32 v) { PA_long32 *p = (PA_long32 *)malloc(sizeof(PA_long32)); *p = v; longs.push_back(p); args.push_back(p); return *this; }
    Call &t(const char *s) { PA_Unistring *p = new PA_Unistring(U(s)); texts.push_back(p); args.push_back(p); return *this; }
    Call &returns_text() { text_result = U(""); text_result_ptr = &text_result; result = (sLONG_PTR)text_result_ptr; return *this; }

    void run(PA_long32 selector) {
        PluginBlock pb;
        pb.fParameters = args.empty() ? NULL : &args[0];
        pb.fResult = &result;
        PluginMain(selector, &pb);
    }
    std::string text() const { return narrow(u16(text_result.fString, text_result.fLength)); }
    ~Call() {
        for(PA_long32 *p : longs) free(p);
        for(PA_Unistring *p : texts) { PA_DisposeUnistring(p); delete p; }
        if(text_result_ptr) PA_DisposeUnistring(&text_result);
    }
};

void init_plugin() { PluginMain(kInitPlugin, NULL); }
void deinit_plugin() { PluginMain(kDeinitPlugin, NULL); }

PA_long32 create_view(PA_long32 w, PA_long32 h) {
    Call c;
    c.l(w).l(h).l(0).run(1);
    return (PA_long32)c.result;
}

/* ---------- LIVE defect (Ultralight Load html) ---------- */

/* LIVE: Ultralight Load html declares its first parameter as a longint
   (manifest &L; the view id, like every other command) but read it with
   PA_GetObjectParameter, i.e. a pointer-sized read from a PA_long32 slot,
   then used the resulting garbage as a PA_ObjectRef in ob_get_n. */
bool defect_live_load_html_view_param_read_as_object() {
    init_plugin();
    PA_long32 view = create_view(320, 200);
    {
        Call c;
        c.l(view).t("<p>synthetic</p>").run(2);
    }
    bool ok = check(view == 1, "view created");
    ok &= check(stub::g_loads == std::vector<std::string>({ "html:<p>synthetic</p>" }), "html loaded into the view");
    deinit_plugin();
    return no_leaks() && ok;
}

/* HARDENING (LIVE): Ultralight Evaluate script handed the length-counted
   String16 buffer to PA_ReturnString, which reads up to a NUL. */
bool hardening_live_evaluate_script_result_not_nul_terminated() {
    init_plugin();
    PA_long32 view = create_view(320, 200);
    std::string got;
    {
        Call c;
        c.l(view).t("document.title").returns_text().run(5);
        got = c.text();
    }
    bool ok = check(stub::g_evals == 1 && got == "document.title", "script result returned");
    deinit_plugin();
    return no_leaks() && ok;
}

/* ---------- DORMANT defects (support/4DPlugin-JSON.cpp, no caller) ---------- */

bool defect_dormant_ob_get_a_s_result_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "s", "text");
    CUTF16String s;
    CUTF8String s8;
    bool ra = ob_get_a((PA_ObjectRef)o, L"s", &s);
    bool rs = ob_get_s((PA_ObjectRef)o, L"s", &s8);
    release_object(o);
    bool ok = check(ra && s == widen("text") && rs && s8 == (const uint8_t *)"text", "values read");
    return no_leaks() && ok;
}

bool defect_dormant_ob_get_n_b_wrong_kind_not_cleared() {
    Obj *o = new_object(false);
    put_text(o, "id", "1");
    put_text(o, "flag", "yes");
    double n = ob_get_n((PA_ObjectRef)o, L"id");
    bool b = ob_get_b((PA_ObjectRef)o, L"flag");
    release_object(o);
    bool ok = check(n == 0.0 && !b, "wrong kinds yield 0/false");
    return no_leaks() && ok;
}

/* The retained object/collection reference was never released, so it
   outlived its owner. The returned ref is borrowed from the parent. */
bool defect_dormant_ob_get_o_c_reference_not_released() {
    Obj *o = new_object(false);
    Obj *child = put_object(o, "child", false);
    Obj *items = put_object(o, "items", true);
    PA_ObjectRef r1 = ob_get_o((PA_ObjectRef)o, L"child");
    PA_CollectionRef r2 = ob_get_c((PA_ObjectRef)o, L"items");
    bool ok = check(r1 == (PA_ObjectRef)child && r2 == (PA_CollectionRef)items, "references returned");
    ok &= check(child->rc == 1 && items->rc == 1, "returned references borrowed from the parent");
    release_object(o);
    return no_leaks() && ok;
}

/* ob_set_s/ob_set_a/ob_set_p overwrote the value allocated by
   PA_CreateVariable(eVK_Unistring/eVK_Picture). */
bool defect_dormant_ob_set_empty_owner_leaks() {
    Obj *o = new_object(false);
    ob_set_s((PA_ObjectRef)o, "k1", "v1");
    ob_set_s((PA_ObjectRef)o, L"k2", "v2");
    ob_set_a((PA_ObjectRef)o, L"k3", L"v3");
    CUTF16String v4 = widen("v4");
    ob_set_a((PA_ObjectRef)o, L"k4", &v4);
    PA_Picture pic = PA_CreatePicture(NULL, 0);
    // ob_set_p consumes `pic` (shallow set, deep-copying set, then clear).
    ob_set_p((PA_ObjectRef)o, L"k5", pic);
    bool ok = check(o->props.size() == 5, "five properties set");
    release_object(o);
    return no_leaks() && ok;
}

/* HARDENING: on error PA_GetObjectProperty leaves its result uninitialised. */
bool hardening_ob_get_error_result_untouched() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    put_real(o, "n", 1.0);
    g_fail_key = "a";
    CUTF16String s;
    CUTF8String s8;
    bool ra = ob_get_a((PA_ObjectRef)o, L"a", &s);
    bool rs = ob_get_s((PA_ObjectRef)o, L"a", &s8);
    g_fail_key = "n";
    double d = ob_get_n((PA_ObjectRef)o, L"n");
    g_fail_key.clear();
    release_object(o);
    bool ok = check(!ra && !rs && s.empty() && s8.empty() && d == 0.0, "failed reads yield nothing");
    return no_leaks() && ok;
}

/* HARDENING: ob_stringify cleared the 1217 result without checking the error. */
bool hardening_ob_stringify_error_untouched() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    g_fail_stringify = true;
    CUTF8String json;
    ob_stringify((PA_ObjectRef)o, &json);
    g_fail_stringify = false;
    release_object(o);
    bool ok = check(json.empty(), "failed stringify yields nothing");
    return no_leaks() && ok;
}

/* ---------- controls (pass on both trees) ---------- */

bool control_create_view_ids() {
    init_plugin();
    PA_long32 a = create_view(10, 10);
    PA_long32 b = create_view(20, 20);
    bool ok = check(a == 1 && b == 2, "sequential view ids");
    deinit_plugin();
    return no_leaks() && ok;
}

/* Load path: the borrowed text parameter is neither disposed nor retained. */
bool control_load_path_url() {
    init_plugin();
    PA_long32 view = create_view(10, 10);
    {
        Call c;
        c.l(view).t("http://example.invalid/synthetic").run(3);
    }
    bool ok = check(stub::g_loads == std::vector<std::string>({ "url:http://example.invalid/synthetic" }), "url handed to the view");
    deinit_plugin();
    return no_leaks() && ok;
}

/* Get snapshot: the picture from PA_CreatePicture is handed to 4D. */
bool control_get_snapshot_picture() {
    init_plugin();
    PA_long32 view = create_view(4, 3);
    PA_Picture pic = NULL;
    {
        Call c;
        c.l(view).run(4);
        pic = (PA_Picture)c.result;
    }
    bool ok = check(pic && g_pictures.count(pic) && g_pictures[pic] == 8, "picture returned to 4D");
    PA_DisposePicture(pic);
    deinit_plugin();
    return no_leaks() && ok;
}

/* View 0 is "no view": nothing is loaded, evaluated or returned. */
bool control_no_view_is_ignored() {
    init_plugin();
    std::string got;
    {
        Call c;
        c.l(0).t("1").returns_text().run(5);
        got = c.text();
    }
    {
        Call c;
        c.l(0).run(4);
    }
    bool ok = check(stub::g_evals == 0 && stub::g_loads.empty() && got.empty(), "nothing done without a view");
    deinit_plugin();
    return no_leaks() && ok;
}

bool control_ob_set_get_round_trip() {
    Obj *o = new_object(false);
    ob_set_n((PA_ObjectRef)o, L"n", 2.5);
    ob_set_b((PA_ObjectRef)o, L"b", true);
    ob_set_i((PA_ObjectRef)o, L"i", 7);
    ob_set_o((PA_ObjectRef)o, L"o", PA_CreateObject());
    ob_set_c((PA_ObjectRef)o, L"c", PA_CreateCollection());
    bool ok = check(o->props.size() == 5, "five properties set");
    ok &= check(ob_get_n((PA_ObjectRef)o, L"n") == 2.5 && ob_get_b((PA_ObjectRef)o, L"b"), "real and boolean read back");
    ok &= check(ob_is_defined((PA_ObjectRef)o, L"n") && !ob_is_defined((PA_ObjectRef)o, L"x"), "ob_is_defined");
    release_object(o);
    return no_leaks() && ok;
}

bool control_ob_stringify() {
    Obj *o = new_object(false);
    put_text(o, "a", "x");
    CUTF8String json;
    ob_stringify((PA_ObjectRef)o, &json);
    release_object(o);
    bool ok = check(json == (const uint8_t *)"{}", "stringified");
    return no_leaks() && ok;
}

struct Test { const char *name; bool (*fn)(); };
const Test kTests[] = {
    { "defect_live_load_html_view_param_read_as_object", defect_live_load_html_view_param_read_as_object },
    { "hardening_live_evaluate_script_result_not_nul_terminated", hardening_live_evaluate_script_result_not_nul_terminated },
    { "defect_dormant_ob_get_a_s_result_not_cleared", defect_dormant_ob_get_a_s_result_not_cleared },
    { "defect_dormant_ob_get_n_b_wrong_kind_not_cleared", defect_dormant_ob_get_n_b_wrong_kind_not_cleared },
    { "defect_dormant_ob_get_o_c_reference_not_released", defect_dormant_ob_get_o_c_reference_not_released },
    { "defect_dormant_ob_set_empty_owner_leaks", defect_dormant_ob_set_empty_owner_leaks },
    { "hardening_ob_get_error_result_untouched", hardening_ob_get_error_result_untouched },
    { "hardening_ob_stringify_error_untouched", hardening_ob_stringify_error_untouched },
    { "control_create_view_ids", control_create_view_ids },
    { "control_load_path_url", control_load_path_url },
    { "control_get_snapshot_picture", control_get_snapshot_picture },
    { "control_no_view_is_ignored", control_no_view_is_ignored },
    { "control_ob_set_get_round_trip", control_ob_set_get_round_trip },
    { "control_ob_stringify", control_ob_stringify },
};

} // namespace

int main(int argc, char **argv) {
    @autoreleasepool {
        gCall4D = engine;
        if(argc == 2 && !strcmp(argv[1], "--list")) {
            for(const Test &t : kTests) printf("%s\n", t.name);
            return 0;
        }
        if(argc != 2) { fprintf(stderr, "usage: %s --list | <test>\n", argv[0]); return 2; }
        for(const Test &t : kTests) {
            if(!strcmp(argv[1], t.name)) {
                bool ok = t.fn();
                printf("%s: %s\n", t.name, ok ? "PASS" : "FAIL");
                return ok ? 0 : 1;
            }
        }
        fprintf(stderr, "unknown test %s\n", argv[1]);
        return 2;
    }
}
