#include "highqualitytextureitem.hpp"
#include "opengl/interpolator.hpp"
#include "opengl/opengltexture1d.hpp"
#include "opengl/opengltexture2d.hpp"
#include "opengl/opengltexturebinder.hpp"
#include "enum/dithering.hpp"
#include "enum/interpolatortype.hpp"
extern "C" {
#include <video/out/dither.h>
}

struct HighQualityTextureData : public HighQualityTextureItem::ShaderData {
    const OpenGLTexture2D *texture = nullptr;
    const OpenGLTexture2D *overlay = nullptr;
    const OpenGLTexture2D *lutDither = nullptr;
    const OpenGLTexture1D *lutInt[2] = { nullptr, nullptr };
    int depth = 8;
    Dithering dithering = Dithering::None;
    InterpolatorType interpolator = InterpolatorType::Bilinear;
};

struct HighQualityTextureShader : public HighQualityTextureItem::ShaderIface {
    HighQualityTextureShader(Interpolator::Category category,
                             bool dithering, bool rectangle)
        : m_category(category)
        , m_dithering(dithering)
        , m_rectangle(rectangle)
    {
        static const auto common = R"(
            varying vec2 texCoord;
            varying vec2 overlayCoord;
            #define DEC_UNIFORM_DXY
            #if USE_RECTANGLE
            varying vec2 normalizedTexCoord;
            #define sampler2Dg sampler2DRect
            #define texture2Dg texture2DRect
            #else
            #define normalizedTexCoord texCoord
            #define sampler2Dg sampler2D
            #define texture2Dg texture2D
            #endif
        )"_b;

        static const auto vtxCode = R"(
            uniform mat4 qt_Matrix;
            uniform vec2 overlay_factor;
            attribute vec4 aPosition;
            attribute vec2 aTexCoord;
            void main() {
                setLutIntCoord(aTexCoord);
                texCoord = aTexCoord;
                overlayCoord = aTexCoord*overlay_factor;
            #if (USE_RECTANGLE && USE_DITHERING)
                normalizedTexCoord = aTexCoord/tex_size;
            #endif
                gl_Position = qt_Matrix * aPosition;
            }
        )"_b;

        static const auto fragCode = R"(
            uniform sampler2Dg tex;
            uniform sampler2D overlay;
            #if USE_DITHERING
            uniform sampler2D dithering;
            uniform float dith_quant;
            uniform float dith_center;
            uniform vec2 dith_size;
            vec4 ditheringed(const in vec4 color) {
                vec2 dith_pos = normalizedTexCoord.xy / dith_size;
                float dith_val = texture2D(dithering, dith_pos).r + dith_center;
                return floor(color * dith_quant + dith_val) / dith_quant;
            }
            #endif
            void main() {
                vec4 color = interpolated(tex, texCoord);
                vec4 over = texture2D(overlay, overlayCoord);
                float r = 1.0 - over.a;
            #if USE_DITHERING
                color = ditheringed(color);
            #endif
                gl_FragColor = color*r + over;
            }
        )"_b;

        const auto interpolator = Interpolator::shader(m_category);
        QByteArray header;
        auto define = [&] (const QByteArray &name, int value) {
            header += "#define "_b; header += name; header += ' ';
            header += QByteArray::number(value); header += '\n';
        };
        define("USE_DITHERING"_b, dithering);
        define("USE_RECTANGLE"_b, rectangle);
        header += common;
        fragmentShader = header;
        fragmentShader += "#define FRAGMENT\n";
        fragmentShader += interpolator;
        fragmentShader += fragCode;
        vertexShader = header;
        vertexShader += "#define VERTEX\n";
        vertexShader += interpolator;
        vertexShader += vtxCode;
        m_lutIntCount = Interpolator::textures(m_category);
        attributes = OGL::TextureVertex::names();
        Q_ASSERT(0 <= m_lutIntCount && m_lutIntCount < 3);
    }

    void resolve(QOpenGLShaderProgram *prog) final {
        loc_tex = prog->uniformLocation("tex");
        loc_overlay = prog->uniformLocation("overlay");
        loc_overlay_factor = prog->uniformLocation("overlay_factor");
        if (m_lutIntCount > 0) {
            loc_dxy = prog->uniformLocation("dxy");
            loc_tex_size = prog->uniformLocation("tex_size");
        }
        for (int i=0; i<m_lutIntCount; ++i) {
            auto name = "lut_int"_b + QByteArray::number(i+1);
            loc_lut_int[i] = prog->uniformLocation(name);
        }
        if (m_dithering) {
            loc_dithering = prog->uniformLocation("dithering");
            loc_dith_quant
                    = prog->uniformLocation("dith_quant");
            loc_dith_center = prog->uniformLocation("dith_center");
            loc_dith_size = prog->uniformLocation("dith_size");
            Q_ASSERT(loc_dithering != -1);
            Q_ASSERT(loc_dith_quant != -1);
            Q_ASSERT(loc_dith_center != -1);
            Q_ASSERT(loc_dith_size != -1);
        }
    }

    void update(QOpenGLShaderProgram *prog,
                const HighQualityTextureItem::ShaderData *data)
    {
        auto d = static_cast<const HighQualityTextureData*>(data);
        auto f = HighQualityTextureItem::func();
        int textureIndex = 0;
        auto bind = [&] (const OpenGLTextureBase *tex, int loc)
            { tex->bind(prog, loc, textureIndex++); };

        const float w = d->texture->width(), h = d->texture->height();
        QVector2D dxy(1.0, 1.0), overlay_factor(1.0, 1.0);
        if (!m_rectangle)
            dxy = { 1.0f/w, 1.0f/h };
        else
            overlay_factor = { 1.0f/w, 1.0f/h };
        prog->setUniformValue(loc_dxy, dxy);
        prog->setUniformValue(loc_tex_size, QVector2D(w, h));
        prog->setUniformValue(loc_overlay_factor, overlay_factor);

        bind(d->texture, loc_tex);
        bind(d->overlay, loc_overlay);
        for (int i=0; i<m_lutIntCount; ++i)
            bind(d->lutInt[i], loc_lut_int[i]);
        if (m_dithering) {
            auto &dithering = *d->lutDither;
            Q_ASSERT(dithering.width() == dithering.height());
            const int size = dithering.width();
            const auto q = static_cast<float>(1 << d->depth) - 1.f;
            prog->setUniformValue(loc_dith_quant, q);
            prog->setUniformValue(loc_dith_center, 0.5f / size * size);
            prog->setUniformValue(loc_dith_size, QSize(size, size));
            bind(d->lutDither, loc_dithering);
        }
        f->glActiveTexture(GL_TEXTURE0);
    }
private:
    int m_lutIntCount = 0;
    Interpolator::Category m_category = Interpolator::None;
    bool m_dithering = false, m_rectangle = false;
    int loc_tex = -1, loc_dxy = -1, loc_lut_int[2] = {-1, -1};
    int loc_tex_size = -1, loc_dithering = -1, loc_dith_quant = -1;
    int loc_dith_center = -1, loc_dith_size = -1;
    int loc_overlay = -1, loc_overlay_factor = -1;
};

static QSGMaterialType types[Interpolator::CategoryMax][2][2];

struct HighQualityTextureItem::Data {
    const Interpolator *interpolator = nullptr;
    OpenGLTexture1D lutInt[2];
    OpenGLTexture2D lutDither;
    OpenGLTexture2D overlay, transparent;
    Dithering dithering = Dithering::None;
    int depth = 8;
};

HighQualityTextureItem::HighQualityTextureItem(QQuickItem *parent)
    : SimpleTextureItem(parent)
    , d(new Data)
{
    d->interpolator = Interpolator::get(InterpolatorType::Bilinear);
}

HighQualityTextureItem::~HighQualityTextureItem()
{
    delete d;
}

auto HighQualityTextureItem::supportsHighQualityRendering() -> bool
{
    return OGL::hasExtension(OGL::TextureFloat);
}

auto HighQualityTextureItem::interpolator() const -> InterpolatorType
{
    return d->interpolator->type();
}

auto HighQualityTextureItem::setInterpolator(InterpolatorType type) -> void
{
    if (type != InterpolatorType::Bilinear && !supportsHighQualityRendering())
        type = InterpolatorType::Bilinear;
    if (d->interpolator->type() != type) {
        d->interpolator = Interpolator::get(type);
        rerender();
    }
}

auto HighQualityTextureItem::setDithering(Dithering dithering) -> void
{
    if (dithering == Dithering::Fruit && !supportsHighQualityRendering())
        dithering = Dithering::None;
    if (_Change(d->dithering, dithering))
        rerender();
}

auto HighQualityTextureItem::dithering() const -> Dithering
{
    return d->dithering;
}

auto HighQualityTextureItem::transparentTexture() const -> const OpenGLTexture2D&
{
    return d->transparent;
}

auto HighQualityTextureItem::initializeGL() -> void
{
    SimpleTextureItem::initializeGL();
    d->lutInt[0].create();
    d->lutInt[1].create();
    d->lutDither.create(OGL::Nearest, OGL::Repeat);
    d->transparent.create(OGL::Nearest, OGL::ClampToEdge);
    const quint32 transparent = 0x0;
    OpenGLTextureBinder<OGL::Target2D> binder(&d->transparent);
    d->transparent.initialize(1, 1, OpenGLTextureTransferInfo::get(OGL::BGRA),
                              &transparent);
    d->overlay = d->transparent;
}

auto HighQualityTextureItem::finalizeGL() -> void
{
    SimpleTextureItem::finalizeGL();
    d->lutInt[0].destroy();
    d->lutInt[1].destroy();
    d->lutDither.destroy();
    d->transparent.destroy();
}

auto HighQualityTextureItem::type() const -> Type* {
    return &::types[d->interpolator->category()]
                   [d->dithering > 0]
                   [texture().isRectangle()];
}

auto HighQualityTextureItem::createShader() const -> ShaderIface*
{
    return new HighQualityTextureShader(d->interpolator->category(),
                                        d->dithering > 0,
                                        texture().isRectangle());
}

auto HighQualityTextureItem::createData() const -> ShaderData*
{
    auto data = new HighQualityTextureData;
    data->texture = &texture();
    data->overlay = &d->overlay;
    data->lutInt[0] = &d->lutInt[0];
    data->lutInt[1] = &d->lutInt[1];
    data->lutDither = &d->lutDither;
    return data;
}

static auto makeDitheringTexture(OpenGLTexture2D &tex, Dithering type) ->void
{
    if (type == Dithering::None)
        return;
    const int sizeb = 6;
    int size = 0;
    const void *data = nullptr;
    QByteArray buffer;
    OpenGLTextureTransferInfo info;
    if (type == Dithering::Fruit) {
        Q_ASSERT(OGL::hasExtension(OGL::TextureFloat));
        size = 1 << 6;
        static QVector<GLfloat> fruit;
        if (fruit.size() != size*size) {
            fruit.resize(size*size);
            mp_make_fruit_dither_matrix(fruit.data(), sizeb);
        }
        const bool rg = OGL::hasExtension(OGL::TextureRG);
        info.texture         = rg ? OGL::R16_UNorm : OGL::Luminance16_UNorm;
        info.transfer.format = rg ? OGL::Red : OGL::Luminance;
        info.transfer.type   = OGL::Float32;
        data = fruit.data();
    } else {
        size = 8;
        buffer.resize(size*size);
        mp_make_ordered_dither_matrix((uchar*)buffer.data(), size);
        info = OpenGLTextureTransferInfo::get(OGL::OneComponent);
        data = buffer.data();
    }
    OpenGLTextureBinder<OGL::Target2D> binder(&tex);
    binder->initialize(size, size, info, data);
}

auto HighQualityTextureItem::updateData(ShaderData *sd) -> void
{
    updateTexture(&texture());
    auto data = static_cast<HighQualityTextureData*>(sd);
    data->depth = d->depth;
    if (data->interpolator != d->interpolator->type()) {
        data->interpolator = d->interpolator->type();
        d->interpolator->allocate(&d->lutInt[0], &d->lutInt[1]);
    }
    if (_Change(data->dithering, d->dithering))
        makeDitheringTexture(d->lutDither, d->dithering);
}

auto HighQualityTextureItem::setOverlayTexture(const OpenGLTexture2D &o) -> void
{
    d->overlay = o;
}
