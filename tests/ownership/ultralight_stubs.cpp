/*
 * Synthetic replacements for the Ultralight/AppCore/WebCore and libpng
 * symbols the plugin links against. Nothing is rendered, no script engine
 * runs and no URL is ever fetched: a view only records what it was asked to
 * load, script evaluation echoes the script source and the bitmap is a
 * synthetic RGBA8 pattern.
 *
 * String16 stores exactly `length` UTF-16 units with no terminator, the
 * length-counted contract of the Ultralight header (data()/length()), so
 * code that treats data() as NUL-terminated reads past the end.
 */

#include <Ultralight/Ultralight.h>
#include <AppCore/JSHelpers.h>
#include "png.h"

#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "ultralight_stubs.h"

namespace stub {
std::vector<ultralight::View *> g_views;
std::vector<std::string> g_loads;
int g_evals = 0;
std::string narrow(const ultralight::String16 &s) {
    std::string r;
    for(size_t i = 0; i < s.length(); i++) r.push_back((char)s.data()[i]);
    return r;
}
}

namespace ultralight {

/* ---------- String16 / String ---------- */

static Char16 *dup16(const Char16 *src, size_t len) {
    if(!len) return nullptr;
    Char16 *d = new Char16[len];
    memcpy(d, src, len * sizeof(Char16));
    return d;
}

String16::String16() : data_(nullptr), length_(0) {}
String16::String16(const char *c_str) : data_(nullptr), length_(c_str ? strlen(c_str) : 0) {
    if(length_) { data_ = new Char16[length_]; for(size_t i = 0; i < length_; i++) data_[i] = (unsigned char)c_str[i]; }
}
String16::String16(const Char16 *str, size_t len) : data_(dup16(str, len)), length_(len) {}
String16::String16(const unsigned short *str, size_t len) : data_(dup16((const Char16 *)str, len)), length_(len) {}
String16::String16(const String16 &other) : data_(dup16(other.data_, other.length_)), length_(other.length_) {}
String16::~String16() { delete[] data_; }
String16 &String16::operator=(const String16 &other) {
    if(this != &other) { delete[] data_; data_ = dup16(other.data_, other.length_); length_ = other.length_; }
    return *this;
}

String::String() {}
String::String(const String16 &str) : str_(str) {}
String::~String() {}

class StubString16Vector : public String16Vector {
public:
    std::vector<String16> items;
    mutable int rc = 1;
    void AddRef() const override { rc++; }
    void Release() const override { if(--rc == 0) delete this; }
    int ref_count() const override { return rc; }
    void push_back(const String16 &val) override { items.push_back(val); }
    String16 *data() override { return items.empty() ? nullptr : &items[0]; }
    size_t size() const override { return items.size(); }
};
String16Vector::String16Vector() {}
String16Vector::~String16Vector() {}
Ref<String16Vector> String16Vector::Create(const String16 *a, size_t len) {
    StubString16Vector *v = new StubString16Vector();
    for(size_t i = 0; i < len; i++) v->items.push_back(a[i]);
    return AdoptRef(*(String16Vector *)v);
}

RefCounted::~RefCounted() {}
FileSystem::~FileSystem() {}
Platform::~Platform() {}
Renderer::~Renderer() {}
View::~View() {}
Bitmap::Bitmap() {}
Bitmap::~Bitmap() {}
RenderTarget::RenderTarget() {}

/* ---------- Platform ---------- */

class StubPlatform : public Platform {
public:
    FileSystem *fs = nullptr;
    void set_config(const Config &) override {}
    const Config &config() const override { abort(); }
    void set_gpu_driver(GPUDriver *) override {}
    GPUDriver *gpu_driver() const override { return nullptr; }
    void set_font_loader(FontLoader *) override {}
    FontLoader *font_loader() const override { return nullptr; }
    void set_file_system(FileSystem *f) override { fs = f; }
    FileSystem *file_system() const override { return fs; }
};

Platform &Platform::instance() { static StubPlatform p; return p; }

/* ---------- Bitmap ---------- */

class StubBitmap : public Bitmap {
public:
    uint32_t w, h;
    std::vector<uint8_t> px;
    mutable int rc = 1;
    StubBitmap(uint32_t w_, uint32_t h_) : w(w_), h(h_), px((size_t)w_ * h_ * 4) {
        for(size_t i = 0; i < px.size(); i++) px[i] = (uint8_t)(i * 7);
    }
    void AddRef() const override { rc++; }
    void Release() const override { if(--rc == 0) delete this; }
    int ref_count() const override { return rc; }
    uint32_t width() const override { return w; }
    uint32_t height() const override { return h; }
    IntRect bounds() const override { return IntRect{ 0, 0, (int)w, (int)h }; }
    BitmapFormat format() const override { return kBitmapFormat_RGBA8; }
    uint32_t bpp() const override { return 4; }
    uint32_t row_bytes() const override { return w * 4; }
    size_t size() const override { return px.size(); }
    bool owns_pixels() const override { return true; }
    void *LockPixels() override { return px.data(); }
    void UnlockPixels() override {}
    const void *LockPixels() const override { return px.data(); }
    void UnlockPixels() const override {}
    void *raw_pixels() override { return px.data(); }
    bool IsEmpty() const override { return px.empty(); }
    void Erase() override {}
    void Set(Ref<Bitmap>) override {}
    bool DrawBitmap(IntRect, IntRect, Ref<Bitmap>, bool) override { return false; }
    bool WritePNG(const char *) override { abort(); }
};

/* ---------- View / Renderer ---------- */

class StubView : public View {
public:
    uint32_t w, h;
    mutable int rc = 1;
    LoadListener *listener = nullptr;
    StubView(uint32_t w_, uint32_t h_) : w(w_), h(h_) { stub::g_views.push_back(this); }
    ~StubView() { for(auto &v : stub::g_views) if(v == this) v = nullptr; }
    void AddRef() const override { rc++; }
    void Release() const override { if(--rc == 0) delete this; }
    int ref_count() const override { return rc; }
    String url() override { return String(); }
    String title() override { return String(); }
    uint32_t width() const override { return w; }
    uint32_t height() const override { return h; }
    bool is_loading() override { return false; }
    RenderTarget render_target() override { return RenderTarget(); }
    bool is_bitmap_dirty() override { return false; }
    RefPtr<Bitmap> bitmap() override { return AdoptRef(*(Bitmap *)new StubBitmap(w, h)); }
    void LoadHTML(const String &html) override { stub::g_loads.push_back("html:" + stub::narrow(html.utf16())); }
    void LoadURL(const String &url) override { stub::g_loads.push_back("url:" + stub::narrow(url.utf16())); }
    void Resize(uint32_t w_, uint32_t h_) override { w = w_; h = h_; }
    JSContextRef js_context() override { return (JSContextRef)this; }
    JSValueRef EvaluateScript(const String &) override { abort(); }
    bool CanGoBack() override { return false; }
    bool CanGoForward() override { return false; }
    void GoBack() override {}
    void GoForward() override {}
    void GoToHistoryOffset(int) override {}
    void Reload() override {}
    void Stop() override {}
    void FireKeyEvent(const KeyEvent &) override {}
    void FireMouseEvent(const MouseEvent &) override {}
    void FireScrollEvent(const ScrollEvent &) override {}
    void set_view_listener(ViewListener *) override {}
    ViewListener *view_listener() const override { return nullptr; }
    void set_load_listener(LoadListener *l) override { listener = l; }
    LoadListener *load_listener() const override { return listener; }
    void set_needs_paint(bool) override {}
    bool needs_paint() const override { return false; }
    RefPtr<View> inspector() override { return nullptr; }
};

class StubRenderer : public Renderer {
public:
    mutable int rc = 1;
    void AddRef() const override { rc++; }
    void Release() const override { if(--rc == 0) delete this; }
    int ref_count() const override { return rc; }
    Ref<View> CreateView(uint32_t w, uint32_t h, bool) override { return AdoptRef(*(View *)new StubView(w, h)); }
    void Update() override {}
    void Render() override {}
    void PurgeMemory() override {}
};

Ref<Renderer> Renderer::Create() { return AdoptRef(*(Renderer *)new StubRenderer()); }

/* ---------- JSHelpers: the "script engine" echoes the script source ---------- */

static JSContextRef g_ctx = nullptr;
void SetJSContext(JSContextRef ctx) { g_ctx = ctx; }

JSString::JSString(const String &str) : instance_((JSStringRef)new String16(str.utf16())) {}
JSString::JSString(const JSString &other) : instance_((JSStringRef)new String16(*(const String16 *)other.instance_)) {}
JSString::~JSString() { delete (String16 *)instance_; }
JSString &JSString::operator=(const JSString &other) {
    if(this != &other) { delete (String16 *)instance_; instance_ = (JSStringRef)new String16(*(const String16 *)other.instance_); }
    return *this;
}
JSString::operator String() { return String(*(const String16 *)instance_); }

JSValue::JSValue(JSString val) : ctx_(g_ctx) { instance_ = (JSValueRef)new String16(String(val).utf16()); }
JSValue::JSValue(const JSValue &other) : ctx_(other.ctx_) { instance_ = (JSValueRef)new String16(*(const String16 *)other.instance_); }
JSValue &JSValue::operator=(const JSValue &other) {
    if(this != &other) { delete (String16 *)instance_; ctx_ = other.ctx_; instance_ = (JSValueRef)new String16(*(const String16 *)other.instance_); }
    return *this;
}
JSValue::~JSValue() { delete (String16 *)instance_; }
JSValueRef JSValue::instance() const { return instance_; }
JSString JSValue::ToString() const { return JSString(String(*(const String16 *)instance_)); }

JSValue JSEval(const JSString &str) {
    if(!g_ctx) abort();
    stub::g_evals++;
    return JSValue(str);
}

} // namespace ultralight

/* ---------- libpng: emits only a signature, no image is encoded ---------- */

struct png_struct_def {
    jmp_buf jb;
    png_voidp io;
    png_rw_ptr write;
};
struct png_info_def { int unused; };

extern "C" {

png_structp png_create_write_struct(png_const_charp, png_voidp, png_error_ptr, png_error_ptr) {
    return (png_structp)calloc(1, sizeof(png_struct_def));
}
png_infop png_create_info_struct(png_const_structrp) { return (png_infop)calloc(1, sizeof(png_info_def)); }
jmp_buf *png_set_longjmp_fn(png_structrp p, png_longjmp_ptr, size_t) { return &p->jb; }
void png_set_write_fn(png_structrp p, png_voidp io, png_rw_ptr w, png_flush_ptr) { p->io = io; p->write = w; }
png_voidp png_get_io_ptr(png_const_structrp p) { return p->io; }
void png_set_IHDR(png_const_structrp, png_inforp, png_uint_32, png_uint_32, int, int, int, int, int) {}
void png_set_pHYs(png_const_structrp, png_inforp, png_uint_32, png_uint_32, int) {}
png_voidp png_malloc(png_const_structrp, png_alloc_size_t n) { return malloc(n); }
/* Like libpng, png_free with a NULL png_ptr does nothing. */
void png_free(png_const_structrp p, png_voidp ptr) { if(p) free(ptr); }
void png_write_info(png_structrp, png_const_inforp) {}
void png_set_rows(png_const_structrp, png_inforp, png_bytepp) {}
void png_write_png(png_structrp p, png_inforp, int, png_voidp) {
    static const png_byte sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    p->write(p, (png_bytep)sig, sizeof(sig));
}
void png_write_end(png_structrp, png_inforp) {}
void png_destroy_write_struct(png_structpp p, png_infopp i) {
    if(i && *i) { free(*i); *i = NULL; }
    if(p && *p) { free(*p); *p = NULL; }
}

}
