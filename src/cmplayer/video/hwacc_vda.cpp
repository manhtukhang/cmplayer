#include "hwacc_vda.hpp"

#ifdef Q_OS_MAC
#include "videotexture.hpp"
#include "hwacc_helper.hpp"
#include "opengl/opengltexture2d.hpp"
#include "misc/log.hpp"
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/OpenGL.h>
#include <CoreVideo/CVPixelBuffer.h>
extern "C" {
#include <video/mp_image.h>
#include <libavcodec/vda.h>
}
#ifdef check
#undef check
#endif

DECLARE_LOG_CONTEXT(VDA)

struct HwAccVda::Data {
    vda_context context;
    bool ok = true;
};

HwAccVda::HwAccVda(AVCodecID codec)
: HwAcc(codec), d(new Data) {
    memset(&d->context, 0, sizeof(d->context));
}

HwAccVda::~HwAccVda() {
    freeContext();
    delete d;
}

auto HwAccVda::getImage(mp_image *mpi) -> mp_image*
{
    auto buffer = (CVPixelBufferRef)mpi->planes[3];
    auto release = [] (void *arg) {
        CVPixelBufferRef buffer = (CVPixelBufferRef)arg;
        CVPixelBufferRelease(buffer);
    };
    CVPixelBufferRetain(buffer);
    auto img = null_mp_image(IMGFMT_VDA, size().width(), size().height(), buffer, release);
    mp_image_copy_attributes(img, mpi);
    img->planes[3] = mpi->planes[3];
    return img;
}

auto HwAccVda::isOk() const -> bool
{
    return d->ok;
}

auto HwAccVda::getSurface() -> mp_image*
{
    auto mpi = null_mp_image(IMGFMT_VDA, size().width(), size().height(), nullptr, nullptr);
    mpi->planes[0] = (uchar*)(void*)(uintptr_t)1;
    return mpi;
}

auto HwAccVda::context() const -> void*
{
    return &d->context;
}

auto HwAccVda::freeContext() -> void
{
    if (d->context.decoder) {
        ff_vda_destroy_decoder(&d->context);
        d->context.decoder = nullptr;
    }
}

auto HwAccVda::fillContext(AVCodecContext *avctx, int w, int h) -> bool
{
    freeContext();
    d->context.width = w;
    d->context.height = h;
    d->context.format = 'avc1';
    d->context.use_sync_decoding = 1;
    d->context.use_ref_buffer = 1;
    d->context.cv_pix_fmt_type = kCVPixelFormatType_422YpCbCr8;
    const auto res = ff_vda_create_decoder(&d->context, avctx->extradata,
                                            avctx->extradata_size);
    return d->ok = (kVDADecoderNoErr == res);
}

/*****************************************************************************/

VdaMixer::VdaMixer(const QSize &size)
    : HwAccMixer(size) { }

VdaMixer::~VdaMixer()
{
    CVPixelBufferRelease(m_buf);
}

auto VdaMixer::upload(OpenGLTexture2D &texture,
                      const mp_image *mpi, bool /*deint*/) -> bool
{
    const auto cgl = CGLGetCurrentContext();
    Q_ASSERT(mpi->imgfmt == IMGFMT_VDA);
    CVPixelBufferRelease(m_buf);
    m_buf = (CVPixelBufferRef)mpi->planes[3];
    CVPixelBufferRetain(m_buf);
    const auto surface = CVPixelBufferGetIOSurface(m_buf);
    const auto w = IOSurfaceGetWidthOfPlane(surface, 0);
    const auto h = IOSurfaceGetHeightOfPlane(surface, 0);
    auto err = CGLTexImageIOSurface2D(cgl, texture.target(), texture.format(),
                                      w, h, texture.transfer().format,
                                      texture.transfer().type, surface, 0);
    if (err != kCGLNoError) {
        _Error("CGLError: %%(0x%%)", CGLErrorString(err), _N(err, 16));
        return false;
    }
    return true;
}

#endif
